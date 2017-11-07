#include <process.h>

#define THREAD_STAT_RUN 0
#define THREAD_STAT_END 1

typedef struct {
	MULTI_THREAD_FUNC func;
	int active;
	int stat;
	int thread_id;
	int thread_num;
	void *param1;
	void *param2;
	HANDLE hThread;
    HANDLE eventRun;
    HANDLE eventEnd;
} MULTI_THREAD_POOL;

static int g_Thread;
static MULTI_THREAD_POOL *g_ThreadPool = NULL;

/* スレッド関数 */
unsigned int WINAPI DoThread(void *arg){
	MULTI_THREAD_POOL *mtp = (MULTI_THREAD_POOL *)arg;

	while(mtp->active){
		while( mtp->stat != THREAD_STAT_RUN ){
			// 所有権が渡されるまで待機 
			WaitForSingleObject(mtp->eventRun, INFINITE);
			if( mtp->active == 0 ){
				return 0;
			}
		}
		mtp->func(mtp->thread_id, mtp->thread_num, mtp->param1, mtp->param2);
		mtp->stat = THREAD_STAT_END;
		SetEvent(mtp->eventEnd);
	}

	return 0;
}

/* スレッドプール作成 */
MULTI_THREAD_POOL *makeThreadPool(){
	// 既にスレッドプール作成済みであれば、そのまま抜ける。
	if(g_ThreadPool)
		return g_ThreadPool;

	unsigned thread_id;
	MULTI_THREAD_POOL *mtp;

	mtp = (MULTI_THREAD_POOL*)malloc(sizeof(MULTI_THREAD_POOL)*g_Thread);
	if(mtp){
		for(int i=0; i<g_Thread; i++){
			mtp[i].active = 1;
			mtp[i].stat = THREAD_STAT_END;
			mtp[i].thread_id = i;
			mtp[i].thread_num = g_Thread;
			mtp[i].eventRun = CreateEvent(NULL, FALSE, FALSE, NULL);
			mtp[i].eventEnd = CreateEvent(NULL, FALSE, FALSE, NULL);
			mtp[i].hThread = (HANDLE)_beginthreadex(NULL, 0, DoThread, &mtp[i], NULL, &thread_id);
		}
		g_ThreadPool = mtp;
	}

	return g_ThreadPool;
}


void releaseThreadPool(){
	// スレッドプールがなければ、そのまま抜ける
	if(!g_ThreadPool)
		return;

	MULTI_THREAD_POOL *mtp = g_ThreadPool;
	for(int i=0; i<g_Thread; i++){
		mtp[i].active = 0;
		mtp[i].stat = THREAD_STAT_END;

		// 実行中のイベントが待機状態だったら終了させる。
		SetEvent(mtp[i].eventRun);
		CloseHandle(mtp[i].eventRun);

		SetEvent(mtp[i].eventEnd);
		CloseHandle(mtp[i].eventEnd);
		CloseHandle(mtp[i].hThread);
	}

	free(g_ThreadPool);
	g_ThreadPool = NULL;
	return;
}









//////////////////////////////////////////////////////////////////////////////

class ClampFrameRange : public GenericVideoFilter {
public:
	ClampFrameRange(PClip clip) : GenericVideoFilter(clip)
	{ child->SetCacheHints(CACHE_NOTHING, 0); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env)
	{ return child->GetFrame(std::min(std::max(n, 0), vi.num_frames - 1), env); }
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlFrameCache : public GenericVideoFilter {
	struct State : public Node<State> {
		int n;
		PVideoFrame frame;
	};
	typedef std::list<State> StateList;

	Node<State> _node;
	StateList _state;
	int _align;

	std::set<int> _peak;
	bool _expand;

public:
	AviUtlFrameCache(PClip clip, int align)
		: GenericVideoFilter(clip), _align(align), _expand(false)
	{ Resize(vi.width, vi.height, 1); }

	void Resize(int width, int height, int cache) {
		if(width != vi.width || height != vi.height) {
			_state.clear();
			vi.width = width;
			vi.height = height;
		}

		if(cache > _state.size())
			_state.resize(cache);

		for(StateList::iterator it = _state.begin(); it != _state.end(); ++it)
			if(it->IsEmpty())
				_node.LinkPrev(&*it);
	}

	void AutoExpand() {
		_peak.clear();
		_expand = true;
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		if(_expand) {
			_peak.insert(n);
			Resize(vi.width, vi.height, _peak.size());
		}

		State *state = _node.GetNext();

		while(state != &_node) {
			if(n == state->n && !!state->frame)
				break;

			state = state->GetNext();
		}

		if(state == &_node) {
			state = _node.GetPrev();
			state->n = n;

			PVideoFrame src(child->GetFrame(n, env));
			const VideoInfo& vj = child->GetVideoInfo();
			int pitch = (vi.RowSize() + _align - 1) / _align * _align;

			if(vi.width != vj.width || vi.height != vj.height || pitch != src->GetPitch()) {
				PVideoFrame dst(env->NewVideoFrame(vi, _align));

				env->BitBlt(
					dst->GetWritePtr(), dst->GetPitch(),
					src->GetReadPtr(), src->GetPitch(),
					std::min(vi.RowSize(), vj.RowSize()),
					std::min(vi.height, vj.height));

				src = dst;
			}

			state->frame = src;
		}

		state->Unlink();
		_node.LinkNext(state);

		return state->frame;
	}
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlFilterProxyExFunc {
public:
	virtual void *get_ycp(int n) = 0;
	virtual void *get_ycp_ofs(int n, int ofs) = 0;
	virtual void *get_ycp_source_cache(int n, int ofs) = 0;
	virtual void *get_ycp_filtering(int n) = 0;
	virtual void *get_ycp_filtering_cache(int n) = 0;
	virtual PIXEL_YC *get_ycp_filtering_cache_ex(int n, int *w, int *h) = 0;

	virtual bool is_editing() = 0;
	virtual bool is_saving() = 0;
	virtual bool is_saveframe(int n) = 0;
	virtual bool is_keyframe(int n) = 0;
	virtual bool is_recompress(int n) = 0;

	virtual void *get_pixelp(int n) = 0;
	virtual void *get_disp_pixelp(DWORD format) = 0;
	virtual bool get_pixel_filtered(int n, void *pixelp, int *w, int *h) = 0;
	virtual bool get_pixel_source(int n, void *pixelp, DWORD format) = 0;
	virtual bool get_pixel_filtered_ex(int n, void *pixelp, int *w, int *h, DWORD format) = 0;

	virtual int get_audio(int n, void *buf) = 0;
	virtual int get_audio_filtered(int n, void *buf) = 0;
	virtual int get_audio_filtering(int n, void *buf) = 0;

	virtual int get_frame() = 0;
	virtual int get_frame_n() = 0;

	virtual bool get_frame_size(int *w, int *h) = 0;

	virtual int set_frame(int n) = 0;
	virtual int set_frame_n(int n) = 0;

	virtual bool copy_frame(int d, int s) = 0;
	virtual bool copy_video(int d, int s) = 0;
	virtual bool copy_audio(int d, int s) = 0;
	virtual bool paste_clip(HWND hwnd, int n) = 0;

	virtual bool get_sys_info(SYS_INFO *sip) = 0;

	virtual bool get_frame_status(int n, FRAME_STATUS *fsp) = 0;
	virtual bool set_frame_status(int n, FRAME_STATUS *fsp) = 0;

	virtual bool get_file_info(FILE_INFO *fip) = 0;
	virtual bool get_source_file_info(FILE_INFO *fip, int source_file_id) = 0;

	virtual bool get_source_video_number(int n, int *source_file_id, int *source_video_number) = 0;

	virtual const char *get_config_name(int n) = 0;

	virtual bool get_select_frame(int *s, int *e) = 0;
	virtual bool set_select_frame(int s, int e) = 0;
};

class AviUtlFilterPluginExFunc : public FILTER {
public:
	virtual bool set_ycp_filtering_cache_size(int w, int h, int d) = 0;

	virtual bool filter_window_update() = 0;
	virtual bool is_filter_window_disp() = 0;
	virtual bool is_filter_active() = 0;

	virtual int ini_load_int(LPSTR key, int n) = 0;
	virtual int ini_save_int(LPSTR key, int n) = 0;

	virtual bool ini_load_str(LPSTR key, LPSTR str, LPSTR def) = 0;
	virtual bool ini_save_str(LPSTR key, LPSTR str) = 0;
};

//////////////////////////////////////////////////////////////////////////////

template<bool DeBug>
class ExFuncCallback {
	static void * __cdecl get_ycp(void *editp, int n) {
		if(DeBug) dout << "get_ycp(" << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_ycp(n);
	}

	static void __cdecl get_ycp_ofs(void *editp, int n, int ofs) {
		if(DeBug) dout << "get_ycp_ofs(" << editp << "," << n << "," << ofs << ")" << std::endl;
		//return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_ycp_ofs(n, ofs);
	}

	static void * __cdecl get_ycp_source_cache(void *editp, int n, int ofs) {
		if(DeBug) dout << "get_ycp_source_cache(" << editp << "," << n << "," << ofs << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_ycp_source_cache(n, ofs);
	}

	static void * __cdecl get_ycp_filtering(void *fp, void *editp, int n, void *reserve) {
		if(DeBug) dout << "get_ycp_filtering(" << fp << "," << editp << "," << n << "," << reserve << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_ycp_filtering(n);
	}

	static void * __cdecl get_ycp_filtering_cache(void *fp, void *editp, int n) {
		if(DeBug) dout << "get_ycp_filtering_cache(" << fp << "," << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_ycp_filtering_cache(n);
	}

	static PIXEL_YC * __cdecl get_ycp_filtering_cache_ex(void *fp, void *editp, int n, int *w, int *h) {
		if(DeBug) dout << "get_ycp_filtering_cache_ex(" << fp << "," << editp << "," << n << "," << w << "," << h << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_ycp_filtering_cache_ex(n, w, h);
	}


	static BOOL __cdecl is_editing(void *editp) {
		if(DeBug) dout << "is_editing(" << editp << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->is_editing();
	}

	static BOOL __cdecl is_saving(void *editp) {
		if(DeBug) dout << "is_saving(" << editp << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->is_saving();
	}

	static BOOL __cdecl is_saveframe(void *editp, int n) {
		if(DeBug) dout << "is_saveframe(" << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->is_saveframe(n);
	}

	static BOOL __cdecl is_keyframe(void *editp, int n) {
		if(DeBug) dout << "is_keyframe(" << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->is_keyframe(n);
	}

	static BOOL __cdecl is_recompress(void *editp, int n) {
		if(DeBug) dout << "is_recompress(" << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->is_recompress(n);
	}


	static void * __cdecl get_pixelp(void *editp, int n) {
		if(DeBug) dout << "get_pixelp(" << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_pixelp(n);
	}

	static void * __cdecl get_disp_pixelp(void *editp, DWORD format) {
		if(DeBug) dout << "get_disp_pixelp(" << editp << "," << format << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_disp_pixelp(format);
	}

	static BOOL __cdecl get_pixel_filtered(void *editp, int n, void *pixelp, int *w, int *h) {
		if(DeBug) dout << "get_pixel_filtered(" << editp << "," << n << "," << pixelp << "," << w << "," << h << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_pixel_filtered(n, pixelp, w, h);
	}

	static BOOL __cdecl get_pixel_source(void *editp, int n, void *pixelp, DWORD format) {
		if(DeBug) dout << "get_pixel_source(" << editp << "," << n << "," << pixelp << "," << format << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_pixel_source(n, pixelp, format);
	}

	static BOOL __cdecl get_pixel_filtered_ex(void *editp, int n, void *pixelp, int *w, int *h, DWORD format) {
		if(DeBug) dout << "get_pixel_filtered_ex(" << editp << "," << n << "," << pixelp << "," << w << "," << h << "," << format << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_pixel_filtered_ex(n, pixelp, w, h, format);
	}


	static int __cdecl get_audio(void *editp, int n, void *buf) {
		if(DeBug) dout << "get_audio(" << editp << "," << n << "," << buf << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_audio(n, buf);
	}

	static int __cdecl get_audio_filtered(void *editp, int n, void *buf) {
		if(DeBug) dout << "get_audio_filtered(" << editp << "," << n << "," << buf << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_audio_filtered(n, buf);
	}

	static int __cdecl get_audio_filtering(void *fp, void *editp, int n, void *buf) {
		if(DeBug) dout << "get_audio_filtering(" << fp << "," << editp << "," << n << "," << buf << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_audio_filtering(n, buf);
	}


	static int __cdecl get_frame(void *editp) {
		if(DeBug) dout << "get_frame(" << editp << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_frame();
	}

	static int __cdecl get_frame_n(void *editp) {
		if(DeBug) dout << "get_frame_n(" << editp << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_frame_n();
	}

	static BOOL __cdecl get_frame_size(void *editp, int *w, int *h) {
		if(DeBug) dout << "get_frame_size(" << editp << "," << w << "," << h << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_frame_size(w, h);
	}

	static int __cdecl set_frame(void *editp, int n) {
		if(DeBug) dout << "set_frame(" << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->set_frame(n);
	}

	static int __cdecl set_frame_n(void *editp, int n) {
		if(DeBug) dout << "set_frame_n(" << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->set_frame_n(n);
	}


	static BOOL __cdecl copy_frame(void *editp, int d, int s) {
		if(DeBug) dout << "copy_frame(" << editp << "," << d << "," << s << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->copy_frame(d, s);
	}

	static BOOL __cdecl copy_video(void *editp, int d, int s) {
		if(DeBug) dout << "copy_video(" << editp << "," << d << "," << s << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->copy_video(d, s);
	}

	static BOOL __cdecl copy_audio(void *editp, int d, int s) {
		if(DeBug) dout << "copy_audio(" << editp << "," << d << "," << s << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->copy_audio(d, s);
	}

	static BOOL __cdecl paste_clip(HWND hwnd, void *editp, int n) {
		if(DeBug) dout << "paste_clip(" << hwnd << "," << editp << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->paste_clip(hwnd, n);
	}


	static BOOL __cdecl get_sys_info(void *editp, SYS_INFO *sip) {
		if(DeBug) dout << "get_sys_info(" << editp << "," << sip << ")" << std::endl;
		if(editp)
			return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_sys_info(sip);
		else
			return true;
	}


	static BOOL __cdecl get_frame_status(void *editp, int n, FRAME_STATUS *fsp) {
		if(DeBug) dout << "get_frame_status(" << editp << "," << n << "," << fsp << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_frame_status(n, fsp);
	}

	static BOOL __cdecl set_frame_status(void *editp, int n, FRAME_STATUS *fsp) {
		if(DeBug) dout << "set_frame_status(" << editp << "," << n << "," << fsp << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->set_frame_status(n, fsp);
	}


	static BOOL __cdecl get_file_info(void *editp, FILE_INFO *fip) {
		if(DeBug) dout << "get_file_info(" << editp << "," << fip << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_file_info(fip);
	}

	static BOOL __cdecl get_source_file_info(void *editp, FILE_INFO *fip, int source_file_id) {
		if(DeBug) dout << "get_source_file_info(" << editp << "," << fip << "," << source_file_id << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_source_file_info(fip, source_file_id);
	}

	static BOOL __cdecl get_source_video_number(void *editp, int n, int *source_file_id, int *source_video_number) {
		if(DeBug) dout << "get_source_video_number(" << editp << "," << n << "," << source_file_id << "," << source_video_number << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_source_video_number(n, source_file_id, source_video_number);
	}


	static LPSTR __cdecl get_config_name(void *editp, int n) {
		if(DeBug) dout << "get_config_name(" << editp << "," << n << ")" << std::endl;
		return (LPSTR)static_cast<AviUtlFilterProxyExFunc *>(editp)->get_config_name(n);
	}

	static BOOL __cdecl get_select_frame(void *editp, int *s, int *e) {
		if(DeBug) dout << "get_select_frame(" << editp << "," << s << "," << e << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->get_select_frame(s, e);
	}

	static BOOL __cdecl set_select_frame(void *editp, int s, int e) {
		if(DeBug) dout << "set_select_frame(" << editp << "," << s << "," << e << ")" << std::endl;
		return static_cast<AviUtlFilterProxyExFunc *>(editp)->set_select_frame(s, e);
	}


	static BOOL __cdecl set_ycp_filtering_cache_size(void *fp, int w, int h, int d, int flag) {
		if(DeBug) dout << "set_ycp_filtering_cache_size(" << fp << "," << w << "," << h << "," << d << "," << flag << ")" << std::endl;
		return static_cast<AviUtlFilterPluginExFunc *>(static_cast<FILTER *>(fp))->set_ycp_filtering_cache_size(w, h, d);
	}


	static BOOL __cdecl filter_window_update(void *fp) {
		if(DeBug) dout << "filter_window_update(" << fp << ")" << std::endl;
		return static_cast<AviUtlFilterPluginExFunc *>(static_cast<FILTER *>(fp))->filter_window_update();
	}

	static BOOL __cdecl is_filter_window_disp(void *fp) {
		if(DeBug) dout << "is_filter_window_disp(" << fp << ")" << std::endl;
		return static_cast<AviUtlFilterPluginExFunc *>(static_cast<FILTER *>(fp))->is_filter_window_disp();
	}

	static BOOL __cdecl is_filter_active(void *fp) {
		if(DeBug) dout << "is_filter_active(" << fp << ")" << std::endl;
		return static_cast<AviUtlFilterPluginExFunc *>(static_cast<FILTER *>(fp))->is_filter_active();
	}


	static int __cdecl ini_load_int(void *fp, LPSTR key, int n) {
		if(DeBug) dout << "ini_load_int(" << fp << "," << key << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterPluginExFunc *>(static_cast<FILTER *>(fp))->ini_load_int(key, n);
	}

	static int __cdecl ini_save_int(void *fp, LPSTR key, int n) {
		if(DeBug) dout << "ini_save_int(" << fp << "," << key << "," << n << ")" << std::endl;
		return static_cast<AviUtlFilterPluginExFunc *>(static_cast<FILTER *>(fp))->ini_save_int(key, n);
	}

	static BOOL __cdecl ini_load_str(void *fp, LPSTR key, LPSTR str, LPSTR def) {
		if(DeBug) dout << "ini_load_str(" << fp << "," << key << "," << str << "," << def << ")" << std::endl;
		return static_cast<AviUtlFilterPluginExFunc *>(static_cast<FILTER *>(fp))->ini_load_str(key, str, def);
	}

	static BOOL __cdecl ini_save_str(void *fp, LPSTR key, LPSTR str) {
		if(DeBug) dout << "ini_save_str(" << fp << "," << key << "," << str << ")" << std::endl;
		return static_cast<AviUtlFilterPluginExFunc *>(static_cast<FILTER *>(fp))->ini_save_str(key, str);
	}


	static BOOL __cdecl copy_clip(HWND hwnd, void *pixelp, int w, int h) {
		if(DeBug) dout << "copy_clip(" << hwnd << "," << pixelp << "," << w << "," << h <<")" << std::endl;
		return FALSE;
	}


	static BOOL __cdecl rgb2yc(PIXEL_YC *ycp, PIXEL *pixelp, int w) {
		if(DeBug) dout << "rgb2yc(" << ycp << "," << pixelp << "," << w << ")" << std::endl;
		return FALSE;
	}

	static BOOL __cdecl yc2rgb(PIXEL *pixelp, PIXEL_YC *ycp, int w) {
		if(DeBug) dout << "yc2rgb(" << pixelp << "," << ycp << "," << w << ")" << std::endl;
		return FALSE;
	}

	static BOOL __cdecl dlg_get_load_name(LPSTR name, LPSTR filter, LPSTR def) {
		if(DeBug) dout << "dlg_get_load_name(" << name << "," << filter << "," << def << ")" << std::endl;
		return FALSE;
	}

	static BOOL __cdecl dlg_get_save_name(LPSTR name, LPSTR filter, LPSTR def) {
		if(DeBug) dout << "dlg_get_save_name(" << name << "," << filter << "," << def << ")" << std::endl;
		return FALSE;
	}

	static void * __cdecl get_filterp(int filter_id) {
		if(DeBug) dout << "get_filterp(" << filter_id << ")" << std::endl;
		return NULL;
	}

	static BOOL __cdecl exec_multi_thread_func(MULTI_THREAD_FUNC func, void *param1, void *param2 ){
		if(DeBug) dout << "exec_multi_thread_func(" << func << "," << param1 << "," << param2 << ")" << std::endl;

		if (g_Thread < 1)
			return FALSE;

		// ここでスレッドプールの作成
		MULTI_THREAD_POOL *mtp = makeThreadPool();
		if(!mtp)
			return FALSE;

		for(int i=0; i<g_Thread; i++){
			mtp[i].func = func;
			mtp[i].stat = THREAD_STAT_RUN;
			mtp[i].param1 = param1;
			mtp[i].param2 = param2;

			// スレッドに制御権を移譲
			SetEvent(mtp[i].eventRun);
		}

		// スレッドの終了まで待機
		for(int i=0; i<g_Thread; i++){
			WaitForSingleObject(mtp[i].eventEnd, INFINITE);
		}

		return TRUE;
	}

public:
	static EXFUNC *GetExFunc() {
		static EXFUNC exfunc = {
			 get_ycp_ofs, get_ycp, get_pixelp, get_audio,
			 is_editing, is_saving, get_frame, get_frame_n,
			 get_frame_size, set_frame, set_frame_n, copy_frame,
			 copy_video, copy_audio, copy_clip, paste_clip,
			 get_frame_status, set_frame_status, is_saveframe, is_keyframe,
			 is_recompress, filter_window_update, is_filter_window_disp, get_file_info,
			 get_config_name, is_filter_active, get_pixel_filtered, get_audio_filtered,
			 get_select_frame, set_select_frame, rgb2yc, yc2rgb,
			 dlg_get_load_name, dlg_get_save_name, ini_load_int, ini_save_int,
			 ini_load_str, ini_save_str, get_source_file_info, get_source_video_number,
			 get_sys_info, get_filterp, get_ycp_filtering, get_audio_filtering,
			 set_ycp_filtering_cache_size, get_ycp_filtering_cache, get_ycp_source_cache, get_disp_pixelp,
			 get_pixel_source, get_pixel_filtered_ex, get_ycp_filtering_cache_ex,
			 exec_multi_thread_func,
		};
		return &exfunc;
	}
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlFilterPlugin;
typedef Ref<AviUtlFilterPlugin> AviUtlFilterPluginRef;

class AviUtlFilterPluginParams;
typedef Ref<AviUtlFilterPluginParams> AviUtlFilterPluginParamsRef;

class AviUtlFilterPluginParams : public RefCount<AviUtlFilterPluginParams> {
public:
	virtual ~AviUtlFilterPluginParams() {}
	virtual AviUtlFilterPluginRef CreatePlugin() = 0;
	virtual const char *GetName() = 0;
	virtual const char *GetArgs() = 0;
	virtual const char *GetInfo() = 0;
};

class AviUtlFilterPlugin
	: public AviUtlPluginObject<FILTER_DLL>
	, protected AviUtlFilterPluginExFunc
{
	std::string _params;
	std::vector<int> _track, _track_s, _track_e, _check;

	bool _debug;
	int _cache_w, _cache_h, _cache_d;
	int _thread;

	//----------------------------------------------------------------------------

	bool set_ycp_filtering_cache_size(int w, int h, int d)
	{ _cache_w = w; _cache_h = h; _cache_d = d; return true; }


	bool filter_window_update()
	{ return TRUE; }

	bool is_filter_window_disp()
	{ return FALSE; }

	bool is_filter_active()
	{ return TRUE; }

	int ini_load_int(LPSTR key, int n)
	{ return n; }

	int ini_save_int(LPSTR key, int n)
	{ return n; }

	bool ini_load_str(LPSTR key, LPSTR str, LPSTR def)
	{ strcpy(str, def); return TRUE; }

	bool ini_save_str(LPSTR key, LPSTR str)
	{ return TRUE; }

	//----------------------------------------------------------------------------

	bool OnInit() {
		PLUGIN_TABLE *table = GetPluginTable();
		return table->func_init == NULL || table->func_init(static_cast<FILTER *>(this));
	}

	bool OnExit() {
		PLUGIN_TABLE *table = GetPluginTable();
		return table->func_exit == NULL || table->func_exit(static_cast<FILTER *>(this));
	}

public:
	AviUtlFilterPlugin(const char *path, const char *name, bool copy, bool debug, int thread)
		: AviUtlPluginObject<PLUGIN_TABLE>(path, name, copy, "GetFilterTable")
		, _debug(debug), _cache_w(0), _cache_h(0), _cache_d(0), _thread(thread)
	{
		PLUGIN_TABLE *table = GetPluginTable();
		if(table == NULL) return;
		if(debug) dout << "AviUtlFilterPlugin(" << path << "," << name << "," << copy << "," << debug << "," << thread << ")" << std::endl;
		// スレッド数...大きい方を利用する。
		if(g_Thread <= thread)
			g_Thread = thread;

		memcpy(static_cast<FILTER *>(this), table, sizeof(FILTER));

		if(track_n) {
			_track.assign(track_default, track_default + track_n);
			track = &_track[0];

			if(track_s == NULL) {
				_track_s.assign(track_n, 0);
				track_s = &_track_s[0];
			}

			if(track_e == NULL) {
				_track_e.assign(track_n, 256);
				track_e = &_track_e[0];
			}
		}

		if(check_n) {
			_check.assign(check_default, check_default + check_n);
			check = &_check[0];
		}

		exfunc = debug ? ExFuncCallback<true>::GetExFunc() : ExFuncCallback<false>::GetExFunc();
		hwnd = HWND_DESKTOP;
		dll_hinst = GetLibrary()->GetHandle();
		dll_path = "";

		if((flag & FILTER_FLAG_EX_DATA) && ex_data_def != NULL)
			memcpy(ex_data_ptr, ex_data_def, ex_data_size);

		std::ostringstream os;
		os << "c";

		for(int i = 0; i < track_n; ++i) os << "[i" << i << "]i";
		for(int i = 0; i < check_n; ++i) os << "[b" << i << "]b";
		_params = os.str();
	}

	~AviUtlFilterPlugin()
	{ Exit(); }

	const char *GetParams() const
	{ return _params.c_str(); }

	const FILTER *GetFilter() const
	{ return this; }

	void SetArgs(AVSValue args) {
		int i;
		for(i = 0; i < track_n; ++i)
			track[i] = args[i].AsInt(track_default[i]);

		args = AVSValue(&args[i], check_n);

		for(i = 0; i < check_n; ++i)
			check[i] = args[i].AsBool(check_default[i]);
	}

	bool IsDebug() const { return _debug; }
	int GetCacheW(int w) const { return (_cache_w > 0) ? _cache_w : w; }
	int GetCacheH(int h) const { return (_cache_h > 0) ? _cache_h : h; }
	int GetCacheD(int d) const { return (_cache_d > 0) ? _cache_d : d; }
	int GetThread() const { return _thread; }

	bool Proc(FILTER_PROC_INFO *fpip)
	{ return (func_proc != NULL) ? !!func_proc(static_cast<FILTER *>(this), fpip) : true;	}

	bool Update(int status)
	{ return (func_update != NULL) ? !!func_update(static_cast<FILTER *>(this), status) : true; }

	bool SaveStart(int s, int e, AviUtlFilterProxyExFunc *editp)
	{ return (func_save_start != NULL) ? !!func_save_start(static_cast<FILTER *>(this), s, e, editp) : true; }

	bool SaveEnd(AviUtlFilterProxyExFunc *editp)
	{ return (func_save_end != NULL) ? !!func_save_end(static_cast<FILTER *>(this), editp) : true; }
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlFilterProxy
	: public GenericVideoFilter
	, protected AviUtlFilterProxyExFunc
{
	AviUtlFilterPluginRef _plugin;

	IScriptEnvironment *_env;
	int _frameNo;

	bool _writable;
	PVideoFrame _tempFrame;
	PClip _clipYC, _clipEX, _clipYUY, _clipRGB;
	AviUtlFrameCache *_cacheYC, *_cacheEX, *_cacheYUY, *_cacheRGB;

	//----------------------------------------------------------------------------

	void *get_ycp(int n)
	{ return get_ycp_source_cache(n, 0); }

	void *get_ycp_ofs(int n, int ofs)
	{ return get_ycp_source_cache(n, ofs); }

	void *get_ycp_source_cache(int n, int ofs) {
		_writable = false;
		const VideoInfo& vi = _cacheYC->GetVideoInfo();
		return const_cast<BYTE *>(_cacheYC->GetFrame(n + ofs, _env)->GetReadPtr());
	}

	void *get_ycp_filtering(int n)
	{ return get_ycp_source_cache(n, 0); }

	void *get_ycp_filtering_cache(int n)
	{ return get_ycp_filtering_cache_ex(n, NULL, NULL); }

	PIXEL_YC *get_ycp_filtering_cache_ex(int n, int *w, int *h) {
		_writable = false;
		const VideoInfo& vi = _cacheEX->GetVideoInfo();

		_cacheEX->Resize(
			_plugin->GetCacheW(vi.width / 3) * 3,
			_plugin->GetCacheH(vi.height),
			_plugin->GetCacheD(1));

		if(w != NULL) *w = vi.width / 3;
		if(h != NULL) *h = vi.height;

		const BYTE *ptr = _cacheEX->GetFrame(n, _env)->GetReadPtr();
		return reinterpret_cast<PIXEL_YC *>(const_cast<BYTE *>(ptr));
	}


	bool is_editing() { return FALSE; }
	bool is_saving() { return TRUE; }
	bool is_saveframe(int n) { return true; }
	bool is_keyframe(int n) { return false; }
	bool is_recompress(int n) { return false; }


	AviUtlFrameCache *_get_pixel_cache(DWORD format) {
		switch(format) {
		case BI_RGB: case comptypeDIB:
			return _cacheRGB;
		case mmioFOURCC('Y','U','Y','2'):
			return _cacheYUY;
		}
		return NULL;
	}

	void *_get_pixel_ptr(int n, DWORD format) {
		_writable = false;
		AviUtlFrameCache *cache = _get_pixel_cache(format);
		return (cache != NULL) ? const_cast<BYTE *>(cache->GetFrame(n, _env)->GetReadPtr()) : NULL;
	}


	void *get_pixelp(int n)
	{ return _get_pixel_ptr(n, BI_RGB); }

	void *get_disp_pixelp(DWORD format)
	{ return _get_pixel_ptr(_frameNo, format); }

	bool get_pixel_filtered(int n, void *pixelp, int *w, int *h)
	{ return get_pixel_filtered_ex(n, pixelp, w, h, BI_RGB); }

	bool get_pixel_source(int n, void *pixelp, DWORD format)
	{ return get_pixel_filtered_ex(n, pixelp, NULL, NULL, format); }

	bool get_pixel_filtered_ex(int n, void *pixelp, int *w, int *h, DWORD format) {
		_writable = false;
		AviUtlFrameCache *cache = _get_pixel_cache(format);

		if(cache == NULL)
			return false;

		const VideoInfo& vi = cache->GetVideoInfo();

		if(pixelp != NULL) {
			PVideoFrame frame(cache->GetFrame(n, _env));

			_env->BitBlt(static_cast<BYTE *>(pixelp), vi.BMPSize() / vi.height,
				frame->GetReadPtr(), frame->GetPitch(), vi.RowSize(), vi.height);
		}

		if(w != NULL) *w = vi.width;
		if(h != NULL) *h = vi.height;

		return true;
	}


	int get_audio(int n, void *buf)
	{ return get_audio_filtering(n, buf); }

	int get_audio_filtered(int n, void *buf)
	{ return get_audio_filtering(n, buf); }

	int get_audio_filtering(int n, void *buf) {
		int start = vi.AudioSamplesFromFrames(n);
		int length = vi.AudioSamplesFromFrames(1);
		if(buf != NULL) child->GetAudio(buf, start, length, _env);
		return length;
	}


	int get_frame() { return _frameNo; }
	int get_frame_n() { return vi.num_frames; }


	bool get_frame_size(int *w, int *h) {
		if(w != NULL) *w = vi.width / 3;
		if(h != NULL) *h = vi.height;
		return true;
	}


	int set_frame(int n) { return n; }
	int set_frame_n(int n) { return vi.num_frames; }


	bool copy_frame(int d, int s) { return false; }
	bool copy_video(int d, int s) { return false; }
	bool copy_audio(int d, int s) { return false; }
	bool paste_clip(HWND hwnd, int n) { return false; }


	bool get_sys_info(SYS_INFO *sip) {
		memset(sip, 0, sizeof(SYS_INFO));
		long cpu = _env->GetCPUFlags();
		if(cpu & CPUF_SSE) sip->flag |= SYS_INFO_FLAG_USE_SSE;
		if(cpu & CPUF_SSE2) sip->flag |= SYS_INFO_FLAG_USE_SSE2;
		sip->info = "0.99";
		sip->filter_n = 0;
		sip->min_w = 32;
		sip->min_h = 32;
		sip->max_w = vi.width / 3;
		sip->max_h = vi.height;
		sip->max_frame = vi.num_frames;
		sip->edit_name = "";
		sip->project_name = "";
		sip->output_name = "";

		//try {
		//	AVSValue v = _env->GetVar("AfsLogFile");
		//	sip->output_name = (LPSTR)v.AsString("");
		//} catch (IScriptEnvironment::NotFound){
		//	sip->output_name = "";	
		//}
		sip->vram_w = sip->max_w;
		sip->vram_h = vi.height;
		sip->vram_yc_size = sizeof(PIXEL_YC);
		sip->vram_line_size = sizeof(PIXEL_YC) * sip->max_w;
		return true;
	}


	bool get_frame_status(int n, FRAME_STATUS *fsp)
	{ return false; }

	bool set_frame_status(int n, FRAME_STATUS *fsp)
	{ return false; }

	bool get_file_info(FILE_INFO *fip) {
		memset(fip, 0, sizeof(FILE_INFO));
		fip->name = "";

		if(vi.HasVideo()) {
			fip->flag |= FILE_INFO_FLAG_VIDEO;
			fip->w = vi.width / 3;
			fip->h = vi.height;
			fip->video_rate = vi.fps_numerator;
			fip->video_scale = vi.fps_denominator;
		}

		if(vi.HasAudio()) {
			fip->flag |= FILE_INFO_FLAG_AUDIO;
			fip->audio_rate = vi.audio_samples_per_second;
			fip->audio_ch = vi.nchannels;
		}

		return true;
	}

	bool get_source_file_info(FILE_INFO *fip, int source_file_id) {
		if(source_file_id != 0) return false;
		return get_file_info(fip);
	}

	bool get_source_video_number(int n, int *source_file_id, int *source_video_number) {
		if(source_file_id != NULL) *source_file_id = 0;
		if(source_video_number != NULL) *source_video_number = n;
		return true;
	}


	const char *get_config_name(int n)
	{ return ""; }

	bool get_select_frame(int *s, int *e) {
		if(s != NULL) *s = 0;
		if(e != NULL) *e = vi.num_frames - 1;
		return true;
	}

	bool set_select_frame(int s, int e)
	{ return false; }

	//----------------------------------------------------------------------------

	AviUtlFilterProxy(const AviUtlFilterPluginRef& plugin, AVSValue args, IScriptEnvironment *env)
		: GenericVideoFilter(new ClampFrameRange(args[0].AsClip()))
		, _plugin(plugin), _writable(true)
	{
		_clipYC = _cacheYC = new AviUtlFrameCache(child, sizeof(PIXEL_YC));
		_clipEX = _cacheEX = new AviUtlFrameCache(child, sizeof(PIXEL_YC));

		AVSValue argsYUY2[] = { _clipYC };
		_clipYUY = env->Invoke(ConvertAviUtlYCToYUY2::GetName(), AVSValue(argsYUY2, sizeof(argsYUY2) / sizeof(*argsYUY2))).AsClip();

		AVSValue argsRGB[] = { _clipYUY };
		_clipRGB = env->Invoke("ConvertToRGB24", AVSValue(argsRGB, sizeof(argsRGB) / sizeof(*argsRGB))).AsClip();

		_clipYUY = _cacheYUY = new AviUtlFrameCache(_clipYUY, 4);
		_clipRGB = _cacheRGB = new AviUtlFrameCache(_clipRGB, 4);

		plugin->SetArgs(AVSValue(&args[1], args.ArraySize()));
		plugin->Update(FILTER_UPDATE_STATUS_ALL);

		_cacheYC->AutoExpand();
		plugin->SaveStart(0, vi.num_frames - 1, this);
	}

	~AviUtlFilterProxy()
	{ _plugin->SaveEnd(this); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		AviUtlFilterPluginParams *params = static_cast<AviUtlFilterPluginParams *>(user_data);
		AviUtlFilterPluginRef plugin(params->CreatePlugin());

		if(!plugin->Init())
			env->ThrowError("%s: プラグイン初期化エラー", params->GetName());

		PClip child(args[0].AsClip());
		const VideoInfo& vi = child->GetVideoInfo();

		if(!vi.IsYUY2())
			env->ThrowError("%s: YUY2専用", plugin->GetFuncName());

		return new AviUtlFilterProxy(plugin, args, env);
	}

public:
	static void PluginInit(const AviUtlFilterPluginParamsRef& params, IScriptEnvironment *env)
	{ env->AddFunction(params->GetName(), params->GetArgs(), Create, params); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		_env = env;
		_frameNo = n;
		_cacheYC->AutoExpand();

		PVideoFrame editFrame;

		if(_writable)
			editFrame = child->GetFrame(n, env);
		else
			editFrame = _cacheYC->GetFrame(n, env);

		if(!editFrame->IsWritable() || editFrame->GetPitch() != vi.RowSize()) {
			PVideoFrame workFrame(env->NewVideoFrame(vi, sizeof(PIXEL_YC)));
			env->BitBlt(workFrame->GetWritePtr(), workFrame->GetPitch(),
				editFrame->GetReadPtr(), editFrame->GetPitch(), vi.RowSize(), vi.height);
			editFrame = workFrame;
		}

		if(!_tempFrame || !_tempFrame->IsWritable())
			_tempFrame = env->NewVideoFrame(vi, sizeof(PIXEL_YC));

		PIXEL_YC *edit = (PIXEL_YC *)editFrame->GetWritePtr();
		PIXEL_YC *temp = (PIXEL_YC *)_tempFrame->GetWritePtr();

		FILTER_PROC_INFO fpi;
		memset(&fpi, 0, sizeof(fpi));

		if(child->GetParity(n))
			fpi.flag |= FILTER_PROC_INFO_FLAG_INVERT_FIELD_ORDER;

		fpi.ycp_edit = edit;
		fpi.ycp_temp = temp;
		fpi.w = vi.width / 3;
		fpi.h = vi.height;
		fpi.max_w = fpi.w;
		fpi.max_h = vi.height;
		fpi.frame = n;
		fpi.frame_n = vi.num_frames;
		fpi.org_w = fpi.w;
		fpi.org_h = vi.height;
		fpi.audiop = NULL;
		fpi.audio_n = 0;
		fpi.audio_ch = 0;
		fpi.pixelp = NULL;
		fpi.editp = static_cast<AviUtlFilterProxyExFunc *>(this);
		fpi.yc_size = sizeof(PIXEL_YC);
		fpi.line_size = sizeof(PIXEL_YC) * fpi.w;

		_plugin->Proc(&fpi);

		PVideoFrame result(editFrame);

		if(fpi.ycp_edit != edit) {
			result = _tempFrame;
			_tempFrame = editFrame;
		}

		return result;
	}
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlFilterPluginByRef : public AviUtlFilterPluginParams {
	AviUtlFilterPluginRef _plugin;

public:
	AviUtlFilterPluginByRef(const AviUtlFilterPluginRef& plugin)
		: _plugin(plugin) {}

	AviUtlFilterPluginRef CreatePlugin()
	{ return _plugin; }

	const char *GetName() { return _plugin->GetFuncName(); }
	const char *GetArgs() { return _plugin->GetParams(); }
	const char *GetInfo() { return _plugin->GetInformation(); }
};

class AviUtlFilterPluginByCopy : public AviUtlFilterPluginParams {
	std::string _path, _name, _args, _info;
	bool _debug;
	int _thread;

public:
	AviUtlFilterPluginByCopy(const AviUtlFilterPluginRef& plugin)
		: _path(plugin->GetLibrary()->GetPath())
		, _name(plugin->GetFuncName())
		, _args(plugin->GetParams())
		, _info(plugin->GetInformation())
		, _debug(plugin->IsDebug())
		, _thread(plugin->GetThread()) {}

	AviUtlFilterPluginRef CreatePlugin()
	{ return new AviUtlFilterPlugin(_path.c_str(), _name.c_str(), true, _debug, _thread); }

	const char *GetName() { return _name.c_str(); }
	const char *GetArgs() { return _args.c_str(); }
	const char *GetInfo() { return _info.c_str(); }
};

//////////////////////////////////////////////////////////////////////////////

class LoadAviUtlFilterPlugin {
	std::map<std::string, AviUtlFilterPluginParamsRef> _params;

//	static void __cdecl Delete(void *user_data, IScriptEnvironment *env)
//	{ delete static_cast<LoadAviUtlFilterPlugin *>(user_data); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env)
	{ return Singleton<LoadAviUtlFilterPlugin>::GetInstance(env)->AddPlugin(args, env); }
//	{ return static_cast<LoadAviUtlFilterPlugin *>(user_data)->AddPlugin(args, env); }

	AVSValue AddPlugin(AVSValue args, IScriptEnvironment *env) {
		const char *path = args[0].AsString();
		const char *name = args[1].AsString();
		bool copy = args[2].AsBool(false);
		bool debug = args[3].AsBool(false);
		int thread = args[4].AsInt(1);
		if(thread < 1)
			thread = 1;

		if(_params.find(name) != _params.end())
			return AVSValue();

		AviUtlFilterPluginRef plugin(new AviUtlFilterPlugin(path, name, false, debug, thread));

		if(!plugin)
			env->ThrowError("%s: プラグイン読込エラー", GetName());

		if(plugin->GetFilter()->flag & FILTER_FLAG_AUDIO_FILTER)
			env->ThrowError("%s: ビデオフィルター専用", plugin->GetFuncName());

		AviUtlFilterPluginParamsRef& params = _params[name];

		if(copy)
			params = new AviUtlFilterPluginByCopy(plugin);
		else
			params = new AviUtlFilterPluginByRef(plugin);

		AviUtlFilterProxy::PluginInit(params, env);

		return params->GetInfo();
	}

public:

	~LoadAviUtlFilterPlugin(){
		// スレッドの停止
		releaseThreadPool();
	}

	static const char *GetName()
	{ return "LoadAviUtlFilterPlugin"; }

	static void PluginInit(IScriptEnvironment *env) {
		LoadAviUtlFilterPlugin *self = new LoadAviUtlFilterPlugin();
//		env->AtExit(Delete, self);
		env->AddFunction(GetName(), "ss[copy]b[debug]b[thread]i", Create, self);
	}
};

//////////////////////////////////////////////////////////////////////////////
