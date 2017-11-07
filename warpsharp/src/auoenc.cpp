#include <vector>
#include <map>
#include <iostream>
#include <sstream>
#include <ctime>
#include <algorithm>
#include <process.h>

#include <signal.h>

#include <windows.h>

#include "common/library.h"
#include "common/thread.h"

#include "platform/vfw.h"

#include "avisynth/compatible.h"

#include "aviutl/filter.h"
#include "aviutl/output.h"

namespace CURRENT_VERSION {

#include "aviutl.h"

#define IMPLEMENT
#include "loadauo.h"
#undef IMPLEMENT

class OutputCallback : public AviUtlOutputCallback {
	const char *_auo, *_cfg, *_avs, *_out;
	int _now, _total;
	bool _abort, _result, _null;

	IScriptEnvironment *_env;
	time_t _start, _finish;

	AviUtlOutputPluginRef _plugin;
	avisynth::AvisynthDLLRef _avisynth;
	avisynth::EnvironmentObjectRef _holder;
	PClip _clip, _yuy2, _rgb24;
	PVideoFrame _videoBuf;
	std::vector<BYTE> _audioBuf;

	static const char *GetImporter(const char *file) {
		using namespace std;

		const char *importer = "Import";
		DWORD header[3] = { 0, 0, 0 };

		ifstream ifs(file, ios_base::binary);
		ifs.read(reinterpret_cast<char *>(header), sizeof(header));

		if(header[0] == mmioFOURCC('R','I','F','F') && header[2] == mmioFOURCC('A','V','I',' '))
			importer = "OpenDMLSource";

		return importer;
	}

protected:
	bool Init() {
		using namespace std;

		_plugin = new AviUtlOutputPlugin(_auo, "", false);

		if(!_plugin) {
			cout << "プラグイン読込エラー" << endl;
			return false;
		}

		if(!_plugin->Init()) {
			cout << "プラグイン初期化エラー" << endl;
			return false;
		}

		if(!_plugin->Config(_cfg)) {
			cout << "プラグイン設定エラー" << endl;
			return false;
		}

		_avisynth = new avisynth::AvisynthDLL("avisynth.dll");

		if(!_avisynth) {
			cout << "avisynth.dllを読込めない" << endl;
			return false;
		}

		_holder = _avisynth->CreateScriptEnvironment();

		if(!_holder) {
			cout << "ScriptEnvironmentを構築出来ない" << endl;
			return false;
		}

		_env = _holder->CreateCompatible();

		if(_env == NULL) {
			cout << "ScriptEnvironmentの互換性を保てない" << endl;
			return false;
		}

		const char *importer = GetImporter(_avs);

		try {
			AVSValue args[] = { _avs };
			_clip = _env->Invoke(importer, AVSValue(args, sizeof(args) / sizeof(*args))).AsClip();
		}
		catch(...) {
			cout << "クリップ構築中に例外処理発生" << endl;
			return false;
		}

		if(!_clip) {
			cout << "クリップを取得出来ない" << endl;
			return false;
		}

		try {
			AVSValue args[] = { _clip };
			_yuy2 = _env->Invoke("ConvertToYUY2", AVSValue(args, sizeof(args) / sizeof(*args))).AsClip();
			_rgb24 = _env->Invoke("ConvertToRGB24", AVSValue(args, sizeof(args) / sizeof(*args))).AsClip();
		}
		catch(...) {
			cout << "コンバータ構築中に例外処理発生" << endl;
			return false;
		}

		return true;
	}

	void Exit() {
		_audioBuf.clear();
		_videoBuf = PVideoFrame();
		_rgb24 = PClip();
		_yuy2 = PClip();
		_clip = PClip();
		_holder = avisynth::EnvironmentObjectRef();
		_avisynth = avisynth::AvisynthDLLRef();
		_plugin = AviUtlOutputPluginRef();
	}

	void Start() {
		using namespace std;

		cout << "プラグイン: " << _auo << endl;
		cout << "設定ファイル: " << _cfg << endl;
		cout << "入力ファイル: " << _avs << endl;
		cout << "出力ファイル: " << _out << endl;
		cout << endl;

		time(&_start);
		cout << "エンコード開始: " << ctime(&_start) << endl;
	}

	void Finish(bool result) {
		using namespace std;

		time(&_finish);
		cout << "エンコード終了: " << ctime(&_finish) << endl;

		int hour = difftime(_finish, _start);
		int second = hour % 60; hour /= 60;
		int minute = hour % 60; hour /= 60;

		cout << "経過時間: " << hour << ":" << minute << ":" << second << endl;

		_result = result;
	}

	void GetOutputInfo(OUTPUT_INFO *oip) {
		const VideoInfo& vi = _rgb24->GetVideoInfo();
		oip->savefile = const_cast<char *>(_out);

		if(vi.HasVideo()) {
			oip->flag |= OUTPUT_INFO_FLAG_VIDEO;
			oip->w = vi.width;
			oip->h = vi.height;
			oip->rate = vi.fps_numerator;
			oip->scale = vi.fps_denominator;
			oip->n = vi.num_frames;
			oip->size = vi.BMPSize();
		}

		if(vi.HasAudio()) {
			oip->flag |= OUTPUT_INFO_FLAG_AUDIO;
			oip->audio_rate = vi.SamplesPerSecond();
			oip->audio_ch = vi.AudioChannels();
			oip->audio_n = vi.num_audio_samples;
			oip->audio_size = vi.BytesPerAudioSample();
		}
	}

	const AviUtlOutputPluginRef& GetPlugin() {
		return _plugin;
	}

	void *GetVideo(int frame, DWORD format) {
		PClip clip;

		switch(format) {
		case mmioFOURCC('Y','U','Y','2'):
			clip = _yuy2;
			break;

		case BI_RGB: case comptypeDIB:
			clip = _rgb24;
			break;

		default:
			return NULL;
		}

		const VideoInfo& vi = clip->GetVideoInfo();
		_videoBuf = clip->GetFrame(frame, _env);

		if(_videoBuf->GetPitch() != ((vi.RowSize() + 3) & ~3)) {
			PVideoFrame dst(_env->NewVideoFrame(vi, 4));

			_env->BitBlt(dst->GetWritePtr(), dst->GetPitch(),
				_videoBuf->GetReadPtr(), _videoBuf->GetPitch(),
				_videoBuf->GetRowSize(), _videoBuf->GetHeight());

			_videoBuf = dst;
		}

		return const_cast<BYTE *>(_videoBuf->GetReadPtr());
	}

	void *GetAudio(int start, int length, int *readed) {
		const VideoInfo& vi = _clip->GetVideoInfo();

		_audioBuf.resize(vi.BytesFromAudioSamples(length));
		_clip->GetAudio(&_audioBuf[0], start, length, _env);

		if(readed != NULL)
			*readed = length;

		return &_audioBuf[0];
	}

	bool IsAbort() {
		return _abort;
	}

	bool RestTimeDisp(int now, int total) {
		_now = now;
		_total = total;
		return true;
	}

	int GetFlag(int frame) {
		return 0;
	}

	bool UpdatePreview() {
		return true;
	}

public:
	OutputCallback(const char *auo, const char *cfg, const char *avs, const char *out)
		: _auo(auo), _cfg(cfg), _avs(avs), _out(out)
		, _now(0), _total(std::numeric_limits<int>::max())
		, _abort(false), _result(false) {}

	void Abort()
	{ _abort = true; }

	void SetNull()
	{ _null = true; }

	template<class T>
	void GetProgress(T& now, T& total) const
	{ now = _now; total = _total; }

	bool GetResult() const
	{ return _result; }
};

} // namespace CURRENT_VERSION

//////////////////////////////////////////////////////////////////////////////

namespace CURRENT_VERSION {

std::auto_ptr<CURRENT_VERSION::OutputCallback> _callback;

static void OnSignal(int signal) {
	switch(signal) {
	case SIGINT:
		_callback->Abort();
		break;
	}
}

} // namespace CURRENT_VERSION

//////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {
	using namespace CURRENT_VERSION;
	using namespace std;

	static const DWORD priority[] = {
		IDLE_PRIORITY_CLASS, BELOW_NORMAL_PRIORITY_CLASS,
		NORMAL_PRIORITY_CLASS, ABOVE_NORMAL_PRIORITY_CLASS,
		HIGH_PRIORITY_CLASS, REALTIME_PRIORITY_CLASS,
	};
	const int prioritySize = sizeof(priority) / sizeof(*priority);

	if(argc != 6) {
		const char *command = strrchr(argv[0], '\\');

		if(command == NULL)
			command = argv[0];

		cout << "Usage: " << command;
		cout << " AUOﾌｧｲﾙ 設定ﾌｧｲﾙ 入力ﾌｧｲﾙ 出力ﾌｧｲﾙ ﾌﾟﾗｲｵﾘﾃｨ(0-";
		cout << (prioritySize - 1) << ")" << endl;

		return EXIT_FAILURE;
	}

	if(isdigit(argv[5][0]) && argv[5][1] == '\0') {
		int index = (argv[5][0] - '0') % prioritySize;
		SetPriorityClass(GetCurrentProcess(), priority[index]);
	}

	_callback.reset(new OutputCallback(argv[1], argv[2], argv[3], argv[4]));
	signal(SIGINT, OnSignal);

	AviUtlOutputContextRef context(new AviUtlOutputContext(_callback.get()));
	context->Start();

	long	startTime	= 0;
	long	prevTime	= 0;
	long	prevFrame	= 0;
	double	fps			= 0.0;
	double	avrFps		= 0.0;
	bool	bRet		= false;
	int		now			= 0;
	int		total		= 0;

	while(!bRet) {
		bRet = context->Wait(2000);
		_callback->GetProgress(now, total);
		if (bRet){
			now = total;
		}

		if (now && !startTime){
			startTime = GetTickCount() - 10000;
			prevTime = GetTickCount() - 10000;
		}

		ostringstream os;
		os << now << "/" << total;

		if(total > 0){
			os << " (" << (now * 100 / total) << "%)";
		}


		if(prevFrame != now && startTime){
			fps    = ((now-prevFrame) * 10000 / (GetTickCount() - prevTime )) * 0.1;
			avrFps =  (now            * 10000 / (GetTickCount() - startTime)) * 0.1;
			prevFrame = now;
			prevTime = GetTickCount();
		}
		os << " [ " << fps << "fps - avr. " << avrFps << "fps ]";

		SetConsoleTitle(os.str().c_str());
	}

	if(!_callback->GetResult()) {
		cout << "エンコードは失敗しました" << endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

//////////////////////////////////////////////////////////////////////////////
