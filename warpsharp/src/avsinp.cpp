#include <vector>

#include <windows.h>

#include "platform/vfw.h"

#include "aviutl/input.h"

//////////////////////////////////////////////////////////////////////////////

class AVSInput : public AVIFileInitializer {
	IAVIFilePtr _file;
	IAVIStreamPtr _videoStream, _audioStream;
	AVISTREAMINFOW _videoInfo, _audioInfo;
	std::vector<BYTE> _videoFormat, _audioFormat;

	AVSInput(const char *file) {
		try { Init(file); }
		catch(_com_error) {}
	}

	bool operator!() const
	{ return (_videoStream == NULL) && (_audioStream == NULL); }

	void Init(const char *file) {
		if(FAILED(AVIFileOpen(&_file, file, OF_READ, NULL)))
			return;

		AVIFILEINFOW fileInfo;
		if(FAILED(_file->Info(&fileInfo, sizeof(fileInfo))))
			return;

		for(int i = 0; i < fileInfo.dwStreams; ++i) {
			IAVIStreamPtr stream;
			if(FAILED(_file->GetStream(&stream, 0, i)))
				continue;

			AVISTREAMINFOW info;
			if(FAILED(stream->Info(&info, sizeof(info))))
				continue;

			LONG size;
			if(FAILED(stream->ReadFormat(0, NULL, &size)))
				continue;

			std::vector<BYTE> format(size);
			if(FAILED(stream->ReadFormat(0, &format[0], &size)))
				continue;

			if(info.fccType == streamtypeVIDEO) {
				_videoStream = stream;
				_videoInfo = info;
				_videoFormat = format;
			}
			else if(info.fccType == streamtypeAUDIO) {
				_audioStream = stream;
				_audioInfo = info;
				_audioFormat = format;
			}
		}
	}

	//----------------------------------------------------------------------------

	static INPUT_HANDLE __cdecl func_open(LPSTR file) {
		std::auto_ptr<AVSInput> handle(new AVSInput(file));
		return !*handle ? NULL : handle.release();
	}

	static BOOL __cdecl func_close(INPUT_HANDLE ih)
	{ delete static_cast<AVSInput *>(ih); return TRUE; }

	static BOOL __cdecl func_info_get(INPUT_HANDLE ih, INPUT_INFO *iip)
	{ return static_cast<AVSInput *>(ih)->GetInfo(iip); }

	static int __cdecl func_read_video(INPUT_HANDLE ih, int frame, void *buf)
	{ return static_cast<AVSInput *>(ih)->ReadVideo(frame, buf); }

	static int __cdecl func_read_audio(INPUT_HANDLE ih, int start, int length, void *buf)
	{ return static_cast<AVSInput *>(ih)->ReadAudio(start, length, buf); }

	//----------------------------------------------------------------------------

	bool GetInfo(INPUT_INFO *iip) {
		iip->flag = 0;

		if(_videoStream != NULL) {
			iip->flag |= INPUT_INFO_FLAG_VIDEO;
			iip->rate = _videoInfo.dwRate;
			iip->scale = _videoInfo.dwScale;
			iip->n = _videoInfo.dwLength;
			iip->format = reinterpret_cast<BITMAPINFOHEADER *>(&_videoFormat[0]);
			iip->format_size = _videoFormat.size();
			iip->handler = _videoInfo.fccHandler;
		}

		if(_audioStream != NULL) {
			iip->flag |= INPUT_INFO_FLAG_AUDIO;
			iip->audio_n = _audioInfo.dwLength;
			iip->audio_format = reinterpret_cast<WAVEFORMATEX *>(&_audioFormat[0]);
			iip->audio_format_size = _audioFormat.size();
		}

		return true;
	}

	int ReadVideo(int frame, void *buf) {
		LONG bytes, videoSize;

		if(FAILED(_videoStream->Read(frame, 1, NULL, 0, &videoSize, NULL)))
			return 0;

		if(FAILED(_videoStream->Read(frame, 1, buf, videoSize, &bytes, NULL)))
			return 0;

		return bytes;
	}

	int ReadAudio(int start, int length, void *buf) {
		LONG samples, audioSize;

		if(FAILED(_audioStream->Read(start, length, NULL, 0, &audioSize, NULL)))
			return 0;

		if(FAILED(_audioStream->Read(start, length, buf, audioSize, NULL, &samples)))
			return 0;

		return samples;
	}

	//----------------------------------------------------------------------------

public:
	static INPUT_PLUGIN_TABLE *GetInputPluginTable() {
		static INPUT_PLUGIN_TABLE table = {
			INPUT_PLUGIN_FLAG_VIDEO | INPUT_PLUGIN_FLAG_AUDIO,
			"AVISynth Script File Reader",
			"AVISynth Script File (*.avs)\0*.avs\0",
			"AVISynth Script File Reader",
			NULL,
			NULL,
			func_open,
			func_close,
			func_info_get,
			func_read_video,
			func_read_audio,
			NULL,
			NULL,
		};
		return &table;
	}
};

//////////////////////////////////////////////////////////////////////////////

extern "C" __declspec(dllexport)
INPUT_PLUGIN_TABLE * __cdecl GetInputPluginTable()
{ return AVSInput::GetInputPluginTable(); }

//////////////////////////////////////////////////////////////////////////////
