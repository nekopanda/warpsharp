//////////////////////////////////////////////////////////////////////////////

class ScriptSite : public IActiveScriptSite {
	typedef std::map<bstr_t, IUnknownPtr> Items;

	LONG _refcount;
	Items _items;

public:
	ScriptSite()
		: _refcount(1) {}

	void AddItem(const bstr_t& name, const IUnknownPtr& item)
	{ _items.insert(Items::value_type(name, item)); }

	IActiveScriptPtr CreateScript(const bstr_t& type) {
		using _com_util::CheckError;

		IActiveScriptPtr script(static_cast<wchar_t *>(type));
		CheckError(script->SetScriptSite(this));

		for(Items::iterator it(_items.begin()); it != _items.end(); ++it)
			CheckError(script->AddNamedItem(it->first, SCRIPTITEM_GLOBALMEMBERS | SCRIPTITEM_ISVISIBLE));

		return script;
	}

	// IUnknown

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) {
		if(ppvObj == NULL) return E_POINTER;
		*ppvObj = NULL;

		if(riid == IID_IUnknown)
			*ppvObj = static_cast<IUnknown *>(this);
		else if(riid == IID_IActiveScriptSite)
			*ppvObj = static_cast<IActiveScriptSite *>(this);

		return (*ppvObj != NULL) ? (AddRef(), S_OK) : E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() {
		InterlockedIncrement(&_refcount);
		return _refcount;
	}

	ULONG STDMETHODCALLTYPE Release() {
		if(InterlockedDecrement(&_refcount) == 0) {
			delete this;
			return 0;
		}
		return _refcount;
	}

	// IActiveScriptSite

	HRESULT STDMETHODCALLTYPE GetLCID(LCID *plcid)
	{ return E_NOTIMPL; }

	HRESULT STDMETHODCALLTYPE GetItemInfo(
		LPCOLESTR pstrName, DWORD dwReturnMask,
		IUnknown **ppiunkItem, ITypeInfo **ppti)
	{
		if(pstrName == NULL)
			return E_INVALIDARG;

		if(dwReturnMask & SCRIPTINFO_ITYPEINFO)
			return E_NOTIMPL;

		Items::iterator it(_items.find(pstrName));

		if(it == _items.end())
			return E_INVALIDARG;

		if(dwReturnMask & SCRIPTINFO_IUNKNOWN) {
			if(ppiunkItem == NULL)
				return E_INVALIDARG;

			*ppiunkItem = it->second;
			(*ppiunkItem)->AddRef();
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE GetDocVersionString(BSTR *pbstrVersion)
	{ return E_NOTIMPL; }

	HRESULT STDMETHODCALLTYPE OnScriptTerminate(const VARIANT *pvarResult, const EXCEPINFO *pexcepinfo)
	{ return S_OK; }

	HRESULT STDMETHODCALLTYPE OnStateChange(SCRIPTSTATE ssScriptState)
	{ return S_OK; }

	HRESULT STDMETHODCALLTYPE OnScriptError(IActiveScriptError *pscripterror)
	{ return S_OK; }

	HRESULT STDMETHODCALLTYPE OnEnterScript()
	{ return S_OK; }

	HRESULT STDMETHODCALLTYPE OnLeaveScript()
	{ return S_OK; }
};

//////////////////////////////////////////////////////////////////////////////

class __declspec(uuid("BFA27F50-8638-4A7A-ADDC-2FAE9140BF65")) AvisynthClip;
_COM_SMARTPTR_TYPEDEF(AvisynthClip, __uuidof(AvisynthClip));

class AvisynthClip : public IUnknown {
	LONG _refcount;
	PClip _clip;

public:
	AvisynthClip(const PClip& clip)
		: _refcount(1), _clip(clip) {}

	const PClip& GetClip() const
	{ return _clip; }

	// IUnknown

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) {
		if(ppvObj == NULL) return E_POINTER;
		*ppvObj = NULL;

		if(riid == IID_IUnknown)
			*ppvObj = static_cast<IUnknown *>(this);
		else if(riid == __uuidof(AvisynthClip))
			*ppvObj = static_cast<AvisynthClip *>(this);

		return (*ppvObj != NULL) ? (AddRef(), S_OK) : E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() {
		InterlockedIncrement(&_refcount);
		return _refcount;
	}

	ULONG STDMETHODCALLTYPE Release() {
		if(InterlockedDecrement(&_refcount) == 0) {
			delete this;
			return 0;
		}
		return _refcount;
	}
};

//////////////////////////////////////////////////////////////////////////////

class Converter {
	typedef std::set<std::string> Strings;
	typedef std::vector<AVSValue> Values;
	typedef std::list<Values> Stack;

	Strings _strings;
	Stack _stack;

	const char *Intern(const char *str)
	{ return _strings.insert(str).first->c_str(); }

	void AVSToSafeArray(const AVSValue& val, variant_t& var) {
		int size = val.ArraySize();

		SAFEARRAYBOUND bound;
		bound.lLbound = 0;
		bound.cElements = size;

		SAFEARRAY *array = SafeArrayCreate(VT_VARIANT, 1, &bound);
		if(array == NULL)
			return;

		V_VT(&var) = VT_VARIANT | VT_ARRAY;
		V_ARRAY(&var) = array;

		variant_t *data;
		if(FAILED(SafeArrayAccessData(array, reinterpret_cast<LPVOID *>(&data))))
			return;

		for(int i = 0; i < size; ++i) {
			try {
				AVSToCOM(val[i], data[i]);
			}
			catch(...) {
			}
		}

		SafeArrayUnaccessData(array);
	}

	void SafeArrayToAVSArray(const variant_t& var, AVSValue& val) {
		SAFEARRAY *array = V_ISBYREF(&var) ? *V_ARRAYREF(&var) : V_ARRAY(&var);
		if(SafeArrayGetDim(array) != 1)
			return;

		LONG lb, ub;
		SafeArrayGetLBound(array, 1, &lb);
		SafeArrayGetUBound(array, 1, &ub);

		_stack.resize(_stack.size() + 1);
		Values& values = _stack.back();

		values.resize(ub - lb);
		val = AVSValue(&values.front(), values.size());

		if(SUCCEEDED(SafeArrayLock(array)))
			return;

		variant_t ref;
		V_VT(&ref) = (V_VT(&var) & ~VT_ARRAY) | VT_BYREF;

		for(LONG i = lb; i <= ub; ++i) {
			try {
				SafeArrayPtrOfIndex(array, &i, &V_BYREF(&ref));
				COMToAVS(ref, values[i - lb]);
			}
			catch(...) {
			}
		}

		SafeArrayUnlock(array);
	}

public:
	void AVSToCOM(const AVSValue& val, variant_t& var) {
		if(val.IsArray())
			;//AVSToSafeArray(val, var);

		else if(val.IsInt())    var = val.AsInt();
		else if(val.IsFloat())  var = val.AsFloat();
		else if(val.IsBool())   var = val.AsBool();
		else if(val.IsString()) var = val.AsString();
		else if(val.IsClip())   var = variant_t(new AvisynthClip(val.AsClip()), false);
	}

	void COMToAVS(const variant_t& var, AVSValue& val) {
		if(V_ISARRAY(&var)) {
			//SafeArrayToAVSArray(var, val);
			return;
		}

		switch(V_VT(&var) & VT_TYPEMASK) {
		case VT_I1: val = V_ISBYREF(&var) ? *V_I1REF(&var) : V_I1(&var); break;
		case VT_I2: val = V_ISBYREF(&var) ? *V_I2REF(&var) : V_I2(&var); break;
		case VT_I4: val = V_ISBYREF(&var) ? *V_I4REF(&var) : V_I4(&var); break;
		case VT_INT: val = V_ISBYREF(&var) ? *V_INTREF(&var) : V_INT(&var); break;
		case VT_UI1: val = V_ISBYREF(&var) ? *V_UI1REF(&var) : V_UI1(&var); break;
		case VT_UI2: val = V_ISBYREF(&var) ? *V_UI2REF(&var) : V_UI2(&var); break;

		case VT_I8: val = int(V_ISBYREF(&var) ? *V_I8REF(&var) : V_I8(&var)); break;
		case VT_UI4: val = int(V_ISBYREF(&var) ? *V_UI4REF(&var) : V_UI4(&var)); break;
		case VT_UI8: val = int(V_ISBYREF(&var) ? *V_UI8REF(&var) : V_UI8(&var)); break;
		case VT_UINT: val = int(V_ISBYREF(&var) ? *V_UINTREF(&var) : V_UINT(&var)); break;

		case VT_R4: val = V_ISBYREF(&var) ? *V_R4REF(&var) : V_R4(&var); break;
		case VT_R8: val = V_ISBYREF(&var) ? *V_R8REF(&var) : V_R8(&var); break;

		case VT_BOOL: val = bool(V_ISBYREF(&var) ? *V_BOOLREF(&var) : V_BOOL(&var)); break;
		case VT_BSTR: val = Intern(bstr_t(V_ISBYREF(&var) ? *V_BSTRREF(&var) : V_BSTR(&var))); break;

		case VT_VARIANT: COMToAVS(*static_cast<variant_t *>(V_VARIANTREF(&var)), val); break;
		case VT_DATE: val = V_ISBYREF(&var) ? *V_DATEREF(&var) : V_DATE(&var); break;
		case VT_ERROR: val = V_ISBYREF(&var) ? *V_ERRORREF(&var) : V_ERROR(&var); break;

		case VT_UNKNOWN:
			try {
				val = AvisynthClipPtr(V_ISBYREF(&var) ? *V_UNKNOWNREF(&var) : V_UNKNOWN(&var))->GetClip();
			}
			catch(::_com_error) {
			}
			break;
/*
		case VT_CY: val = V_ISBYREF(&var) ? *V_CYREF(&var) : V_CY(&var); break;
		case VT_DECIMAL: val = V_ISBYREF(&var) ? *V_DECIMALREF(&var) : V_DECIMAL(&var); break;
		case VT_DISPATCH:
		case VT_EMPTY:
		case VT_NULL:
		case VT_HRESULT:
		case VT_VOID:
		case VT_PTR:
		case VT_SAFEARRAY:
		case VT_CARRAY:
		case VT_USERDEFINED:
		case VT_LPSTR:
		case VT_LPWSTR:
		case VT_FILETIME:
		case VT_BLOB:
		case VT_STREAM:
		case VT_STORAGE:
		case VT_STREAMED_OBJECT:
		case VT_STORED_OBJECT:
		case VT_BLOB_OBJECT:
		case VT_CF:
		case VT_CLSID:
			break;
*/
		}
	}
};

//////////////////////////////////////////////////////////////////////////////

class AVS : public IDispatch {
	typedef std::map<bstr_t, unsigned> IDMap;
	typedef std::vector<bstr_t> Names;

	LONG _refcount;
	Converter *_conv;
	IScriptEnvironment *_env;

	IDMap _idmap;
	Names _names;

public:
	AVS(Converter *conv, IScriptEnvironment *env)
		: _refcount(1), _conv(conv), _env(env) {}

	// IUnknown

	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) {
		if(ppvObj == NULL) return E_POINTER;
		*ppvObj = NULL;

		if(riid == IID_IUnknown)
			*ppvObj = static_cast<IUnknown *>(this);
		else if(riid == IID_IDispatch)
			*ppvObj = static_cast<IDispatch *>(this);

		return (*ppvObj != NULL) ? (AddRef(), S_OK) : E_NOINTERFACE;
	}

	ULONG STDMETHODCALLTYPE AddRef() {
		InterlockedIncrement(&_refcount);
		return _refcount;
	}

	ULONG STDMETHODCALLTYPE Release() {
		if(InterlockedDecrement(&_refcount) == 0) {
			delete this;
			return 0;
		}
		return _refcount;
	}

	// IDispatch

	HRESULT STDMETHODCALLTYPE GetTypeInfoCount(UINT *pctinfo)
	{ return E_NOTIMPL; }

	HRESULT STDMETHODCALLTYPE GetTypeInfo(UINT iTInfo, LCID lcid, ITypeInfo **ppTInfo)
	{ return E_NOTIMPL; }

	HRESULT STDMETHODCALLTYPE GetIDsOfNames(
		REFIID riid, LPOLESTR *rgszNames, UINT cNames, LCID lcid, DISPID *rgDispId)
	{
		if(rgszNames == NULL || rgDispId == NULL)
			return E_POINTER;

		for(int i= 0; i < cNames; ++i) {
			bstr_t name(rgszNames[i]);
			LCMapStringW(lcid, LCMAP_LOWERCASE, name, name.length(), name, name.length());

			std::pair<IDMap::iterator, bool> result(_idmap.insert(IDMap::value_type(name, _names.size())));
			if(result.second) _names.push_back(result.first->first);

			rgDispId[i] = result.first->second;
		}

		return S_OK;
	}

	HRESULT STDMETHODCALLTYPE Invoke(
		DISPID dispIdMember, REFIID riid, LCID lcid, WORD wFlags,
		DISPPARAMS *pDispParams, VARIANT *pVarResult,
		EXCEPINFO *pExcepInfo, UINT *puArgErr)
	{
		if(pDispParams == NULL)
			return E_POINTER;

		if(unsigned(dispIdMember) >= _names.size())
			return DISP_E_MEMBERNOTFOUND;

		try {
			const variant_t *params = static_cast<variant_t *>(pDispParams->rgvarg);
			variant_t *result = static_cast<variant_t *>(pVarResult);

			if(wFlags & DISPATCH_METHOD) {
				std::vector<AVSValue> args(pDispParams->cArgs);

				for(int i = 0; i < pDispParams->cArgs; ++i)
					_conv->COMToAVS(params[pDispParams->cArgs - i - 1], args[i]);

				AVSValue value(_env->Invoke(_names[dispIdMember], AVSValue(&args.front(), args.size())));

				if(pVarResult != NULL)
					_conv->AVSToCOM(value, *result);
			}
			else if(wFlags & DISPATCH_PROPERTYGET) {
				if(pVarResult == NULL)
					return E_INVALIDARG;

				AVSValue value(_env->GetVar(_names[dispIdMember]));
				_conv->AVSToCOM(value, *result);
			}
			else if(wFlags & DISPATCH_PROPERTYPUT) {
				AVSValue value;
				_conv->COMToAVS(*params, value);
				_env->SetVar(_names[dispIdMember], value);
			}
			else {
				return E_NOTIMPL;
			}
		}
		catch(::_com_error e) {
			if(pExcepInfo != NULL) {
				pExcepInfo->wCode = e.WCode();
				pExcepInfo->bstrSource = e.Source().copy();
				pExcepInfo->bstrDescription = e.Description().copy();
				pExcepInfo->bstrHelpFile = e.HelpFile().copy();
				pExcepInfo->dwHelpContext = e.HelpContext();
				pExcepInfo->scode = e.Error();
			}
			return e.Error();
		}
		catch(::AvisynthError e) {
			if(pExcepInfo != NULL) {
				pExcepInfo->wCode = 0x0201;
				pExcepInfo->bstrSource = _names[dispIdMember].copy();
				pExcepInfo->bstrDescription = bstr_t(e.msg).copy();
				pExcepInfo->scode = DISP_E_EXCEPTION;
			}
			return DISP_E_EXCEPTION;
		}
		catch(::IScriptEnvironment::NotFound) {
			if(pExcepInfo != NULL) {
				pExcepInfo->wCode = 0x0201;
				pExcepInfo->bstrSource = _names[dispIdMember].copy();
				pExcepInfo->bstrDescription = bstr_t(typeid(::IScriptEnvironment::NotFound).name()).copy();
				pExcepInfo->scode = DISP_E_MEMBERNOTFOUND;
			}
			return DISP_E_MEMBERNOTFOUND;
		}

		return S_OK;
	}
};

//////////////////////////////////////////////////////////////////////////////

class WScript : public CoInitializer {
	static CriticalSection _critical;
	static std::set<WScript *> _objects;

	Converter *_conv;
	IActiveScriptPtr _script;
	AVSValue _result;

	static bstr_t LoadScript(const char *file, IScriptEnvironment *env) {
		std::ifstream ifs(file);
		if(!ifs.is_open())
			env->ThrowError("%s: ファイルオープンエラー", GetName());

		char data;
		std::ostringstream oss;
		while(ifs.get(data) && oss.put(data));

		return oss.str().c_str();
	}

	static WScript *Create(const char *file, const char *type, IScriptEnvironment *env) {
		using _com_util::CheckError;

		Converter *conv = Singleton<Converter>::GetInstance(env);
		bstr_t text(LoadScript(file, env));

		ScriptSite *site = new ScriptSite();
		IActiveScriptSitePtr site_ptr(site, false);
		site->AddItem(L"AVS", IUnknownPtr(new AVS(conv, env), false));

		IActiveScriptPtr script(site->CreateScript(type));
		IActiveScriptParsePtr parse(script);
		variant_t varres;

		CheckError(parse->InitNew());
		CheckError(parse->ParseScriptText(text,
			NULL, NULL, NULL, 0, 0, SCRIPTTEXT_ISPERSISTENT, &varres, NULL));
		CheckError(script->SetScriptState(SCRIPTSTATE_CONNECTED));

		AVSValue valres;
		conv->COMToAVS(varres, valres);

		WScript *wscript = new WScript(conv, script, valres);
		env->AtExit(Delete, wscript);

		return wscript;
	}

	WScript(Converter *conv, const IActiveScriptPtr& script, const AVSValue& result)
		: _conv(conv), _script(script), _result(result)
	{
		CriticalLock lock(_critical);
		_objects.insert(this);
	}

	~WScript() {
		CriticalLock lock(_critical);
		_objects.erase(this);
	}

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		try {
			return ToAVS(Create(args[0].AsString(), args[1].AsString(), env));
		}
		catch(::_com_error e) {
			env->ThrowError("%s: %s", GetName(), e.ErrorMessage());
		}

		return AVSValue();
	}

	static void __cdecl Delete(void *user_data, IScriptEnvironment *env)
	{ delete static_cast<WScript *>(user_data); }

public:
	static AVSValue ToAVS(WScript *ptr)
	{ return reinterpret_cast<char *>(ptr); }

	static WScript *FromAVS(const AVSValue& value) {
		CriticalLock lock(_critical);
		char *str = const_cast<char *>(value.AsString());
		WScript *ptr = reinterpret_cast<WScript *>(str);
		return (_objects.find(ptr) != _objects.end()) ? ptr : NULL;
	}

	static WScript *FromAVS(AVSValue value, const char *name, IScriptEnvironment *env) {
		WScript *ptr = FromAVS(value);
		if(ptr == NULL) env->ThrowError("%s: 不正なオブジェクト", name);
		return ptr;
	}

	static const char *GetName()
	{ return "WScript"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "ss", Create, 0); }

	Converter *GetConverter()
	{ return _conv; }

	const IActiveScriptPtr& GetScript() const
	{ return _script; }

	const AVSValue& GetResult() const
	{ return _result; }
};

#ifdef IMPLEMENT
CriticalSection WScript::_critical;
std::set<WScript *> WScript::_objects;
#endif

//////////////////////////////////////////////////////////////////////////////

class WSResult {
	static AVSValue __cdecl Apply(AVSValue args, void *user_data, IScriptEnvironment *env)
	{ return WScript::FromAVS(args[0])->GetResult(); }

public:
	static const char *GetName()
	{ return "WSResult"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "s", Apply, 0); }
};

//////////////////////////////////////////////////////////////////////////////

class WSInvoke {
	static AVSValue Apply(WScript *script, const bstr_t& name, const AVSValue& args, IScriptEnvironment *env) {
		using _com_util::CheckError;
		Converter *conv = script->GetConverter();

		IDispatchPtr disp;
		CheckError(script->GetScript()->GetScriptDispatch(NULL, &disp));

		DISPID id;
		LPOLESTR names[] = { name };
		CheckError(disp->GetIDsOfNames(IID_NULL, names, 1, LOCALE_USER_DEFAULT, &id));

		int argc = args.ArraySize();
		std::vector<variant_t> argv(argc);

		for(int i = 0; i < argc; ++i)
			conv->AVSToCOM(args[i], argv[argc - i - 1]);

		DISPPARAMS params;
		memset(&params, 0, sizeof(params));
		params.cArgs = argc;
		params.rgvarg = &argv.front();

		variant_t var;
		CheckError(disp->Invoke(id, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD, &params, &var, NULL, NULL));

		AVSValue val;
		conv->COMToAVS(var, val);

		return val;
	}

	static AVSValue __cdecl Apply(AVSValue args, void *user_data, IScriptEnvironment *env) {
		try {
			return Apply(WScript::FromAVS(args[0]), args[1].AsString(), args[2], env);
		}
		catch(::_com_error e) {
			env->ThrowError("%s: %s", GetName(), e.ErrorMessage());
		}

		return AVSValue();
	}

public:
	static const char *GetName()
	{ return "WSInvoke"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "ss.*", Apply, 0); }
};

//////////////////////////////////////////////////////////////////////////////
