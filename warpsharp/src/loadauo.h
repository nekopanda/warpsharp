//////////////////////////////////////////////////////////////////////////////

#define _context_idx 0

class AviUtlOutputPlugin;
typedef Ref<AviUtlOutputPlugin> AviUtlOutputPluginRef;

class AviUtlOutputPlugin : public AviUtlPluginObject<OUTPUT_PLUGIN_TABLE> {
protected:
	bool OnInit() {
		PLUGIN_TABLE *table = GetPluginTable();
		return table->func_init == NULL || table->func_init();
	}

	bool OnExit() {
		PLUGIN_TABLE *table = GetPluginTable();
		return table->func_exit == NULL || table->func_exit();
	}

public:
	AviUtlOutputPlugin(const char *path, const char *name, bool copy)
		: AviUtlPluginObject<PLUGIN_TABLE>(path, name, copy, "GetOutputPluginTable") {}

	~AviUtlOutputPlugin()
	{ Exit(); }

	bool Output(OUTPUT_INFO *oip) {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_output != NULL) ? !!table->func_output(oip) : false;
	}

	bool Config(HWND hwnd = HWND_DESKTOP) {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_config != NULL) ? !!table->func_config(hwnd, GetLibrary()->GetHandle()) : true;
	}

	int GetConfigSize()
	{ return GetConfig(NULL, 0); }

	int GetConfig(void *data, int size) {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_config_get != NULL) ? table->func_config_get(data, size) : 0;
	}

	int SetConfig(void *data, int size) {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_config_set != NULL) ? table->func_config_set(data, size) : 0;
	}

	bool ReadConfig(std::istream& is) {
		int size;
		is.read(reinterpret_cast<char *>(&size), sizeof(size));

		std::vector<BYTE> config(size);
		is.read(reinterpret_cast<char *>(&config[0]), size);

		if(size != is.gcount())
			return false;

		return (size == SetConfig(&config[0], size));
	}

	bool WriteConfig(std::ostream& os) {
		int size = GetConfigSize();

		std::vector<BYTE> config(size);
		GetConfig(&config[0], size);

		os.write(reinterpret_cast<char *>(&size), sizeof(size));
		os.write(reinterpret_cast<char *>(&config[0]), size);

		return !!os;
	}

	bool Config(const char *config, HWND hwnd = HWND_DESKTOP) {
		std::ifstream ifs(config, std::ios_base::binary);

		if(ifs.is_open())
			return ReadConfig(ifs);

		if(!Config())
			return false;

		std::ofstream ofs(config, std::ios_base::binary);

		if(!ofs.is_open())
			return false;

		return WriteConfig(ofs);
	}
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlOutputCallback {
public:
	virtual ~AviUtlOutputCallback() {}
	virtual bool Init() = 0;
	virtual void Exit() = 0;
	virtual void Start() = 0;
	virtual void Finish(bool result) = 0;
	virtual void GetOutputInfo(OUTPUT_INFO *oip) = 0;
	virtual const AviUtlOutputPluginRef& GetPlugin() = 0;

	virtual void *GetVideo(int frame, DWORD format) = 0;
	virtual void *GetAudio(int start, int length, int *readed) = 0;
	virtual bool IsAbort() = 0;
	virtual bool RestTimeDisp(int now, int total) = 0;
	virtual int GetFlag(int frame) = 0;
	virtual bool UpdatePreview() = 0;
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlOutputContext;
typedef Ref<AviUtlOutputContext> AviUtlOutputContextRef;

class AviUtlOutputContext : public RefCount<AviUtlOutputContext>, public ThreadProc {
	static CriticalSection _critical;
	static std::map<DWORD, AviUtlOutputContext *> _context;

	AviUtlOutputCallback *_callback;
	ThreadRef _thread;
	EventRef _event;

	//----------------------------------------------------------------------------

	void *GetVideo(int frame, DWORD format)
	{ return _callback->GetVideo(frame, format); }

	void *GetAudio(int start, int length, int *readed)
	{ return _callback->GetAudio(start, length, readed); }

	bool IsAbort()
	{ return _callback->IsAbort() || _thread->IsAbort(); }

	bool RestTimeDisp(int now, int total)
	{ return _callback->RestTimeDisp(now, total); }

	int GetFlag(int frame)
	{ return _callback->GetFlag(frame); }

	bool UpdatePreview()
	{ return _callback->UpdatePreview(); }

	//----------------------------------------------------------------------------

	static AviUtlOutputContext *GetContext() {
		CriticalLock lock(_critical);
		//return _context[GetCurrentThreadId()];
		return _context[_context_idx];
	}

	static void * __cdecl func_get_video(int frame)
	{ return GetContext()->GetVideo(frame, NULL); }

	static void * __cdecl func_get_audio(int start, int length, int *readed)
	{ return GetContext()->GetAudio(start, length, readed); }

	static BOOL __cdecl func_is_abort()
	{ return GetContext()->IsAbort(); }

	static BOOL __cdecl func_rest_time_disp(int now, int total)
	{ return GetContext()->RestTimeDisp(now, total); }

	static int __cdecl func_get_flag(int frame)
	{ return GetContext()->GetFlag(frame); }

	static BOOL __cdecl func_update_preview()
	{ return GetContext()->UpdatePreview(); }

	static void * __cdecl func_get_video_ex(int frame, DWORD format)
	{ return GetContext()->GetVideo(frame, format); }

	//----------------------------------------------------------------------------

	static void AttachContext(AviUtlOutputContext *self) {
		CriticalLock lock(_critical);
		//_context[GetCurrentThreadId()] = self;
		_context[_context_idx] = self;
	}

	static void DetachContext() {
		CriticalLock lock(_critical);
		//_context.erase(GetCurrentThreadId());
		_context.erase(_context_idx);
	}

	void OnThreadProc(Thread *thread) {
		AttachContext(this);

		if(_callback->Init())
			Encode();

		_callback->Exit();
		_event->Signal();

		DetachContext();
	}

	void Encode() {
		OUTPUT_INFO oi;
		memset((void *)&oi, 0, sizeof(oi));
		_callback->GetOutputInfo(&oi);

		oi.func_get_video = func_get_video;
		oi.func_get_audio = func_get_audio;
		oi.func_is_abort = func_is_abort;
		oi.func_rest_time_disp = func_rest_time_disp;
		oi.func_get_flag = func_get_flag;
		oi.func_update_preview = func_update_preview;
		oi.func_get_video_ex = func_get_video_ex;

		_callback->Start();
		_callback->Finish(_callback->GetPlugin()->Output(&oi));
	}

	//----------------------------------------------------------------------------

public:
	AviUtlOutputContext(AviUtlOutputCallback *callback)
		: _callback(callback) {}

	~AviUtlOutputContext()
	{ _thread = ThreadRef(); }

	void Start() {
		_event = new Event();
		_thread = new Thread(this);
	}

	bool Wait(int time) {
		if(!_event)
			return true;

		if(!_event->Wait(time))
			return false;

		_event = EventRef();
		return true;
	}
};

#ifdef IMPLEMENT
CriticalSection AviUtlOutputContext::_critical;
std::map<DWORD, AviUtlOutputContext *> AviUtlOutputContext::_context;
#endif

//////////////////////////////////////////////////////////////////////////////
