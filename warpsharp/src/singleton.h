//////////////////////////////////////////////////////////////////////////////

template<class TYPE> class Singleton {
	static void __cdecl Delete(void *user_data, IScriptEnvironment *env)
	{ delete static_cast<TYPE *>(user_data); }

public:
	static TYPE *GetInstance(IScriptEnvironment *env) {
		TYPE *ptr;

		try {
			char *str = const_cast<char *>(env->GetVar(typeid(TYPE).raw_name()).AsString());
			ptr = reinterpret_cast<TYPE *>(str);
		}
		catch(::IScriptEnvironment::NotFound) {
			ptr = new TYPE();
			env->SetGlobalVar(typeid(TYPE).raw_name(), reinterpret_cast<char *>(ptr));
			env->AtExit(Delete, ptr);
		}

		return ptr;
	}
};

//////////////////////////////////////////////////////////////////////////////
