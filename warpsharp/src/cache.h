//////////////////////////////////////////////////////////////////////////////

class FrameCache : public GenericVideoFilter {
	struct State : public Node<State> {
		int n;
		PVideoFrame frame;
	};

	Node<State> _node;
	std::vector<State> _state;
	bool _debug;

	FrameCache(const PClip& clip, int size, bool debug, IScriptEnvironment *env)
		: GenericVideoFilter(clip), _state(size), _debug(debug)
	{
		for(int i = 0; i < _state.size(); ++i)
			_node.LinkPrev(&_state[i]);

		child->SetCacheHints(CACHE_NOTHING, 0);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		PClip child(args[0].AsClip());
		int size = args[1].AsInt(0);
		bool debug = args[2].AsBool(false);

		if(size <= 0)
			return child;

		return new FrameCache(child, size, debug, env);
	}

public:
	static const char *GetName()
	{ return "FrameCache"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "ci[debug]b", Create, NULL); }

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		State *state = _node.GetNext();

		while(state != &_node) {
			if(n == state->n && !!state->frame)
				break;

			state = state->GetNext();
		}

		if(state == &_node) {
			state = _node.GetPrev();
			state->n = n;
			state->frame = child->GetFrame(n, env);

			if(_debug)
				dout << GetName() << ": " << state->n << std::endl;
		}

		state->Unlink();
		_node.LinkNext(state);

		return state->frame;
	}
};

//////////////////////////////////////////////////////////////////////////////

class SetCacheHints {
	static AVSValue __cdecl Apply(AVSValue args, void *user_data, IScriptEnvironment *env) {
		args[0].AsClip()->SetCacheHints(args[1].AsInt(), args[2].AsInt());
		return AVSValue();
	}

public:
	static const char *GetName()
	{ return "SetCacheHints"; }

	static void PluginInit(IScriptEnvironment *env) {
		env->AddFunction(GetName(), "cii", Apply, NULL);
		env->SetGlobalVar("CACHE_NOTHING", CACHE_NOTHING);
		env->SetGlobalVar("CACHE_RANGE", CACHE_RANGE);
		env->SetGlobalVar("CACHE_ALL", CACHE_ALL);
	}
};

//////////////////////////////////////////////////////////////////////////////

class CacheNothing {
	static AVSValue __cdecl Apply(AVSValue args, void *user_data, IScriptEnvironment *env) {
		args[0].AsClip()->SetCacheHints(CACHE_NOTHING, 0);
		return AVSValue();
	}

public:
	static const char *GetName()
	{ return "CacheNothing"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c", Apply, NULL); }
};

//////////////////////////////////////////////////////////////////////////////

class CacheRange {
	static AVSValue __cdecl Apply(AVSValue args, void *user_data, IScriptEnvironment *env) {
		args[0].AsClip()->SetCacheHints(CACHE_RANGE, args[1].AsInt());
		return AVSValue();
	}

public:
	static const char *GetName()
	{ return "CacheRange"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "ci", Apply, NULL); }
};

//////////////////////////////////////////////////////////////////////////////

class CacheAll {
	static AVSValue __cdecl Apply(AVSValue args, void *user_data, IScriptEnvironment *env) {
		args[0].AsClip()->SetCacheHints(CACHE_ALL, 0);
		return AVSValue();
	}

public:
	static const char *GetName()
	{ return "CacheAll"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "c", Apply, NULL); }
};

//////////////////////////////////////////////////////////////////////////////
