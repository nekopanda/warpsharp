//////////////////////////////////////////////////////////////////////////////

class LoadPlugin {
	std::map<HINSTANCE, LibraryObjectRef> _plugins;

//	static void __cdecl Delete(void *user_data, IScriptEnvironment *env)
//	{ delete static_cast<LoadPlugin *>(user_data); }

	static AVSValue __cdecl CreateSV(AVSValue args, void *user_data, IScriptEnvironment *env)
	{ return Singleton<LoadPlugin>::GetInstance(env)->AddPlugins(args[0], env); }
//	{ return static_cast<LoadPlugin *>(user_data)->AddPlugins(args[0], env); }

	static AVSValue __cdecl CreateSI(AVSValue args, void *user_data, IScriptEnvironment *env)
	{ return Singleton<LoadPlugin>::GetInstance(env)->AddPlugin(args[0].AsString(), args[1].AsInt(), env); }
//	{ return static_cast<LoadPlugin *>(user_data)->AddPlugin(args[0].AsString(), args[1].AsInt(), env); }

	AVSValue AddPlugins(const AVSValue& args, IScriptEnvironment *env) {
		std::vector<const char *> log;
		log.reserve(args.ArraySize());

		for(int i = 0; i < args.ArraySize(); ++i) {
			AVSValue result(AddPlugin(args[i].AsString(), 0, env));
			if(result.IsString()) log.push_back(result.AsString());
		}

		if(log.empty())
			return AVSValue();

		if(log.size() == 1)
			return log.front();

		std::ostringstream os;
		std::ostream_iterator<const char *> it(os, ";");
		std::copy(log.begin(), log.end(), it);

		return env->SaveString(os.str().c_str());
	}

	AVSValue AddPlugin(const char *name, int version, IScriptEnvironment *env) {
		LibraryObjectRef lib(new Library(name));
		if(!lib) env->ThrowError("%s: プラグイン読込エラー", GetName());

		if(_plugins.find(lib->GetHandle()) != _plugins.end())
			return AVSValue();

		typedef const char *(__stdcall *Func1)(avisynth1::IScriptEnvironment *);
		typedef const char *(__stdcall *Func2)(avisynth2::IScriptEnvironment *);
		void *func = NULL;

		if(func == NULL) {
			func = lib->GetAddress("AvisynthPluginInit2");

			if(func == NULL)
				func = lib->GetAddress("_AvisynthPluginInit2@4");

			if(func != NULL && version == 0)
				version = avisynth2::AVISYNTH_INTERFACE_VERSION;
		}

		if(func == NULL) {
			func = lib->GetAddress("AvisynthPluginInit");

			if(func == NULL)
				func = lib->GetAddress("_AvisynthPluginInit@4");

			if(func != NULL && version == 0)
				version = avisynth1::AVISYNTH_INTERFACE_VERSION;
		}

		if(func == NULL)
			env->ThrowError("%s: プラグイン初期化エラー", GetName());

		_plugins[lib->GetHandle()] = lib;
		const char *result = NULL;

		switch(version) {
		case avisynth1::AVISYNTH_INTERFACE_VERSION:
			result = static_cast<Func1>(func)(avisynth::CompatibleTraits<Traits, avisynth1::Traits>::ValidateEnvironment(env));
			break;
		case avisynth2::AVISYNTH_INTERFACE_VERSION:
			result = static_cast<Func2>(func)(avisynth::CompatibleTraits<Traits, avisynth2::Traits>::ValidateEnvironment(env));
			break;
		}

		return (result != NULL) ? AVSValue(result) : AVSValue();
	}

public:
	static const char *GetName()
	{ return "LoadPlugin"; }

	static void PluginInit(IScriptEnvironment *env) {
		LoadPlugin *self = new LoadPlugin();
//		env->AtExit(Delete, self);
		env->AddFunction(GetName(), "s+", CreateSV, self);
		env->AddFunction(GetName(), "si", CreateSI, self);
	}
};

//////////////////////////////////////////////////////////////////////////////
