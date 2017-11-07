//////////////////////////////////////////////////////////////////////////////

class AviUtlInputPlugin;
typedef Ref<AviUtlInputPlugin> AviUtlInputPluginRef;

class AviUtlInputPluginParams;
typedef Ref<AviUtlInputPluginParams> AviUtlInputPluginParamsRef;

class AviUtlInputPluginParams : public Ref<AviUtlInputPluginParams> {
public:
	virtual ~AviUtlInputPluginParams() {}
	virtual AviUtlInputPluginRef CreatePlugin() = 0;
	virtual const char *GetName() = 0;
	virtual const char *GetInfo() = 0;
};

class AviUtlInputPlugin : public AviUtlPluginObject<INPUT_PLUGIN_TABLE> {
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
	AviUtlInputPlugin(const char *path, const char *name, bool copy)
		: AviUtlPluginObject<PLUGIN_TABLE>(path, name, copy, "GetInputPluginTable") {}

	~AviUtlInputPlugin()
	{ Exit(); }

	INPUT_HANDLE Open(const char *file) const {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_open != NULL) ? table->func_open(const_cast<char *>(file)) : NULL;
	}

	bool Close(INPUT_HANDLE ih) const {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_close != NULL) ? table->func_close(ih) : false;
	}

	bool GetInputInfo(INPUT_HANDLE ih, INPUT_INFO *iip) const {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_info_get != NULL) ? table->func_info_get(ih, iip) : false;
	}

	int ReadVideo(INPUT_HANDLE ih, int frame, void *buf) const {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_read_video != NULL) ? table->func_read_video(ih, frame, buf) : 0;
	}

	int ReadAudio(INPUT_HANDLE ih, int start, int length, void *buf) {
		PLUGIN_TABLE *table = GetPluginTable();
		return (table->func_read_audio != NULL) ? table->func_read_audio(ih, start, length, buf) : 0;
	}
};

/////////////////////////////////////////////////////////////////////////////

class AviUtlInputProxy : public IClip {
	AviUtlInputPluginRef _plugin;
	INPUT_HANDLE _handle;
	VideoInfo _vi;

	static void GetVideoInfo(VideoInfo& vi, const INPUT_INFO& info) {
		memset(&vi, 0, sizeof(vi));

		bool video = info.flag & INPUT_INFO_FLAG_VIDEO;
		bool audio = info.flag & INPUT_INFO_FLAG_AUDIO;

		if(video) {
			switch(info.format->biCompression) {
			case mmioFOURCC('Y','U','Y','2'):
				vi.pixel_type = VideoInfo::CS_YUY2;
				break;
			case BI_RGB: case comptypeDIB:
				switch(info.format->biBitCount) {
				case 24: vi.pixel_type = VideoInfo::CS_BGR24; break;
				case 32: vi.pixel_type = VideoInfo::CS_BGR32; break;
				default: video = false; break;
				}
				break;
			default:
				video = false;
				break;
			}
		}

		if(audio) {
			switch(info.audio_format->wBitsPerSample) {
			case  8: vi.sample_type = SAMPLE_INT8;  break;
			case 16: vi.sample_type = SAMPLE_INT16; break;
			case 24: vi.sample_type = SAMPLE_INT24; break;
			case 32: vi.sample_type = SAMPLE_INT32; break;
			default: audio = false; break;
			}
		}

		if(video) {
			vi.width = info.format->biWidth;
			vi.height = info.format->biHeight;
			vi.fps_numerator = info.rate;
			vi.fps_denominator = info.scale;
			vi.num_frames = info.n;
		}

		if(audio) {
			vi.audio_samples_per_second = info.audio_format->nSamplesPerSec;
			vi.num_audio_samples = info.audio_n;
			vi.nchannels = info.audio_format->nChannels;
		}
	}

	AviUtlInputProxy(const AviUtlInputPluginRef& plugin, INPUT_HANDLE handle, IScriptEnvironment *env)
		: _plugin(plugin), _handle(handle)
	{
		INPUT_INFO info;
		memset(&info, 0, sizeof(info));
		plugin->GetInputInfo(_handle, &info);
		GetVideoInfo(_vi, info);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		AviUtlInputPluginParams *params = static_cast<AviUtlInputPluginParams *>(user_data);
		AviUtlInputPluginRef plugin(params->CreatePlugin());

		if(!plugin->Init())
			env->ThrowError("%s: プラグイン初期化エラー", params->GetName());

		const char *name = args[0].AsString();
		INPUT_HANDLE handle = plugin->Open(name);

		if(handle == NULL)
			env->ThrowError("%s: ファイルオープンエラー", plugin->GetFuncName());

		return new AviUtlInputProxy(plugin, handle, env);
	}

public:
	static void PluginInit(const AviUtlInputPluginParamsRef& params, IScriptEnvironment *env)
	{ env->AddFunction(params->GetName(), "s", Create, params); }

	~AviUtlInputProxy()
	{ _plugin->Close(_handle); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame frame(env->NewVideoFrame(_vi, 4));
		_plugin->ReadVideo(_handle, n, frame->GetWritePtr());
		return frame;
	}

	void __stdcall GetAudio(void *buf, __int64 start, __int64 count, IScriptEnvironment *env)
	{ _plugin->ReadAudio(_handle, start, count, buf); }

	void __stdcall SetCacheHints(int cachehints, int frame_range)
	{}

	const VideoInfo& __stdcall GetVideoInfo()
	{ return _vi; }

	bool __stdcall GetParity(int n)
	{ return false; }
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlInputPluginByRef : public AviUtlInputPluginParams {
	AviUtlInputPluginRef _plugin;

public:
	AviUtlInputPluginByRef(const AviUtlInputPluginRef& plugin)
		: _plugin(plugin) {}

	AviUtlInputPluginRef CreatePlugin()
	{ return _plugin; }

	const char *GetName() { return _plugin->GetFuncName(); }
	const char *GetInfo() { return _plugin->GetInformation(); }
};

class AviUtlInputPluginByCopy : public AviUtlInputPluginParams {
	std::string _path, _name, _info;

public:
	AviUtlInputPluginByCopy(const AviUtlInputPluginRef& plugin)
		: _path(plugin->GetLibrary()->GetPath())
		, _name(plugin->GetFuncName())
		, _info(plugin->GetInformation()) {}

	AviUtlInputPluginRef CreatePlugin()
	{ return new AviUtlInputPlugin(_path.c_str(), _name.c_str(), true); }

	const char *GetName() { return _name.c_str(); }
	const char *GetInfo() { return _info.c_str(); }
};

//////////////////////////////////////////////////////////////////////////////

class LoadAviUtlInputPlugin {
	std::map<std::string, AviUtlInputPluginParamsRef> _params;

//	static void __cdecl Delete(void *user_data, IScriptEnvironment *env)
//	{ delete static_cast<LoadAviUtlInputPlugin *>(user_data); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env)
	{ return Singleton<LoadAviUtlInputPlugin>::GetInstance(env)->AddPlugin(args, env); }
//	{ return static_cast<LoadAviUtlInputPlugin *>(user_data)->AddPlugin(args, env); }

	AVSValue AddPlugin(AVSValue args, IScriptEnvironment *env) {
		const char *path = args[0].AsString();
		const char *name = args[1].AsString();
		bool copy = args[2].AsBool(false);

		if(_params.find(name) != _params.end())
			return AVSValue();

		AviUtlInputPluginRef plugin(new AviUtlInputPlugin(path, name, false));

		if(!plugin)
			env->ThrowError("%s: プラグイン読込エラー", GetName());

		AviUtlInputPluginParamsRef& params = _params[name];

		if(copy)
			params = new AviUtlInputPluginByCopy(plugin);
		else
			params = new AviUtlInputPluginByRef(plugin);

		AviUtlInputProxy::PluginInit(params, env);

		return params->GetInfo();
	}

public:
	static const char *GetName()
	{ return "LoadAviUtlInputPlugin"; }

	static void PluginInit(IScriptEnvironment *env) {
		LoadAviUtlInputPlugin *self = new LoadAviUtlInputPlugin();
//		env->AtExit(Delete, self);
		env->AddFunction(GetName(), "ss[copy]b", Create, self);
	}
};

//////////////////////////////////////////////////////////////////////////////
