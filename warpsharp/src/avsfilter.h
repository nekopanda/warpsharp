//////////////////////////////////////////////////////////////////////////////

class FilterObject {
	typedef std::set<FilterObject *> ObjectSet;

	static CriticalSection _CriticalSection;
	static ObjectSet _ObjectSet;

protected:
	FilterObject() {
		CriticalLock lock(_CriticalSection);
		_ObjectSet.insert(this);
	}

public:
	virtual ~FilterObject() {
		CriticalLock lock(_CriticalSection);
		_ObjectSet.erase(this);
	}

	static AVSValue ToAVS(FilterObject *obj)
	{ return reinterpret_cast<char *>(obj); }

	template<class T>
	static T *FromAVS(const AVSValue& value) {
		CriticalLock lock(_CriticalSection);
		char *str = const_cast<char *>(value.AsString());
		FilterObject *obj = reinterpret_cast<FilterObject *>(str);

		if(_ObjectSet.find(obj) != _ObjectSet.end())
			return dynamic_cast<T *>(obj);

		return NULL;
	}

	template<class T>
	static T *FromAVS(AVSValue value, const char *name, IScriptEnvironment *env) {
		T *ptr = FromAVS<T>(value);

		if(ptr == NULL)
			env->ThrowError("%s: 不正なオブジェクト", name);

		return ptr;
	}
};

#ifdef IMPLEMENT
CriticalSection FilterObject::_CriticalSection;
FilterObject::ObjectSet FilterObject::_ObjectSet;
#endif

//////////////////////////////////////////////////////////////////////////////

class FilterParam;
typedef Ref<FilterParam> FilterParamRef;

class FilterParam
	: public FilterObject
	, public RefCount<FilterParam>
{
	std::string _name;

protected:
	FilterParam(const char *name) : _name(name) {}

public:
	const char *GetName() const
	{ return _name.c_str(); }

	void ToStream(std::ostream& os) {
		AVSValue value = GetValue();

		if(value.IsInt())
			os << value.AsInt();
		else if(value.IsFloat())
			os << value.AsFloat();
		else if(value.IsString())
			os << "\"" << value.AsString() << "\"";
		else if(value.IsBool())
			os << value.AsBool();
	}

	virtual FilterParamRef Clone() = 0;
	virtual AVSValue GetValue() = 0;
};

//////////////////////////////////////////////////////////////////////////////

class FilterTrack;
typedef Ref<FilterTrack> FilterTrackRef;

class FilterTrack : public FilterParam {
protected:
	FilterTrack(const char *name) : FilterParam(name) {}

public:
	virtual void SetTrack(int value) = 0;
	virtual int GetTrack() = 0;
	virtual int GetDefault() = 0;
	virtual int GetStart() = 0;
	virtual int GetEnd() = 0;
};

//////////////////////////////////////////////////////////////////////////////

class FilterCheck;
typedef Ref<FilterCheck> FilterCheckRef;

class FilterCheck : public FilterParam {
protected:
	FilterCheck(const char *name) : FilterParam(name) {}

public:
	virtual void SetCheck(bool value) = 0;
	virtual bool GetCheck() = 0;
	virtual bool GetDefault() = 0;
};

//////////////////////////////////////////////////////////////////////////////

class FilterValue;
typedef Ref<FilterValue> FilterValueRef;

class FilterValue : public FilterParam {
	AVSValue _value;

public:
	FilterValue(const AVSValue& value)
		: FilterParam(""), _value(value) {}

	FilterParamRef Clone()
	{ return new FilterValue(*this); }

	AVSValue GetValue()
	{ return _value; }
};

//////////////////////////////////////////////////////////////////////////////

class FilterInt;
typedef Ref<FilterInt> FilterIntRef;

class FilterInt : public FilterTrack {
protected:
	int _value, _default, _start, _end;

public:
	FilterInt(const char *name, int def, int start, int end)
		: FilterTrack(name), _default(def), _start(start), _end(end)
	{ SetTrack(def); }

	FilterParamRef Clone()
	{ return new FilterInt(*this); }

	AVSValue GetValue()
	{ return AVSValue(_value); }

	void SetTrack(int value)
	{ _value = std::min(std::max(value, _start), _end); }

	int GetTrack()   { return _value; }
	int GetDefault() { return _default; }
	int GetStart()   { return _start; }
	int GetEnd()     { return _end; }
};

//////////////////////////////////////////////////////////////////////////////

class FilterFloat;
typedef Ref<FilterFloat> FilterFloatRef;

class FilterFloat : public FilterInt {
	double _offset, _scale;

public:
	FilterFloat(const char *name, int def, int start, int end, double start_f, double end_f)
		: FilterInt(name, def, start, end)
	{
		_offset = start_f;
		_scale = (end_f - start_f) / (_end - _start);
	}

	FilterParamRef Clone()
	{ return new FilterFloat(*this); }

	AVSValue GetValue()
	{ return AVSValue((_value - _start) * _scale + _offset); }
};

//////////////////////////////////////////////////////////////////////////////

class FilterArray;
typedef Ref<FilterArray> FilterArrayRef;

class FilterArray : public FilterInt {
	std::vector<AVSValue> _array;

public:
	FilterArray(const char *name, int def, AVSValue array)
		: FilterInt(name, def, 0, array.ArraySize() - 1)
	{
		_array.reserve(array.ArraySize());
		for(int i = 0; i < array.ArraySize(); ++i)
			_array.push_back(array[i]);
	}

	FilterParamRef Clone()
	{ return new FilterArray(*this); }

	AVSValue GetValue()
	{ return _array[_value]; }
};

//////////////////////////////////////////////////////////////////////////////

class FilterPair;
typedef Ref<FilterPair> FilterPairRef;

class FilterPair : public FilterCheck {
	bool _value, _default;
	AVSValue _false, _true;

public:
	FilterPair(const char *name, bool def, AVSValue f, AVSValue t)
		: FilterCheck(name), _value(def), _default(def), _false(f), _true(t) {}

	FilterParamRef Clone()
	{ return new FilterPair(*this); }

	AVSValue GetValue()
	{ return _value ? _true : _false; }

	void SetCheck(bool value)
	{ _value = value; }

	bool GetCheck()   { return _value; }
	bool GetDefault() { return _default; }
};

//////////////////////////////////////////////////////////////////////////////

class FilterFunction;
typedef Ref<FilterFunction> FilterFunctionRef;

class FilterFunction
	: public FilterObject
	, public RefCount<FilterFunction>
{
	std::string _name;
	std::vector<FilterParamRef> _params;
	bool _hidden;

public:
	FilterFunction(const char *name, bool hidden)
		: _name(name), _hidden(hidden) {}

	const char *GetName() const
	{ return _name.c_str(); }

	const std::vector<FilterParamRef>& GetParams() const
	{ return _params; }

	void AddParam(const FilterParamRef& param)
	{ _params.push_back(param); }

	bool IsHidden() const
	{ return _hidden; }

	FilterFunctionRef Clone() {
		FilterFunctionRef function(new FilterFunction(*this));

		for(int i = 0; i < function->_params.size(); ++i)
			function->_params[i] = function->_params[i]->Clone();

		return function;
	}

	PClip CreateClip(const PClip& child, IScriptEnvironment *env) const {
		std::vector<AVSValue> args;
		args.reserve(_params.size() + 1);
		args.push_back(child);

		for(int i = 0; i < _params.size(); ++i)
			args.push_back(_params[i]->GetValue());

		return env->Invoke(_name.c_str(), AVSValue(&args[0], args.size())).AsClip();
	}

	void ToStream(std::ostream& os) const {
		os << _name << "(";

		for(int i = 0; i < _params.size(); ++i) {
			if(i != 0) os << ",";
			_params[i]->ToStream(os);
		}

		os << ")";
	}
};

//////////////////////////////////////////////////////////////////////////////

class AviUtlSource : public IClip {
	VideoInfo _vi;
	FILTER *_fp;
	FILTER_PROC_INFO *_fpip;

	static void GetVideoInfo(VideoInfo& vi, FILTER *fp, FILTER_PROC_INFO *fpip) {
		memset(&vi, 0, sizeof(vi));

		FILE_INFO info;
		fp->exfunc->get_file_info(fpip->editp, &info);

		if(info.flag & FILE_INFO_FLAG_VIDEO) {
			vi.width = fpip->w;
			vi.height = fpip->h;
			vi.fps_numerator = info.video_scale;
			vi.fps_denominator = info.video_rate;
			vi.num_frames = fpip->frame_n;
			vi.pixel_type = VideoInfo::CS_YUY2;
		}

		if(info.flag & FILE_INFO_FLAG_AUDIO) {
			vi.audio_samples_per_second = info.audio_rate;
			vi.num_audio_samples = fpip->audio_n;
			vi.nchannels = fpip->audio_ch;
			vi.sample_type = SAMPLE_INT16;
		}
	}

public:
	AviUtlSource(FILTER *fp, FILTER_PROC_INFO *fpip)
	{ Reset(fp, fpip); }

	bool Reset(FILTER *fp, FILTER_PROC_INFO *fpip) {
		_fp = fp;
		_fpip = fpip;

		VideoInfo vi;
		GetVideoInfo(vi, fp, fpip);

		bool result = !memcmp(&vi, &_vi, sizeof(vi));
		_vi = vi;

		return result;
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		PVideoFrame frame(env->NewVideoFrame(_vi));
		const PIXEL_YC *ycp = _fpip->ycp_edit;

		n = std::min(std::max(n, 0), _fpip->frame_n - 1);

		if(n != _fpip->frame)
			ycp = static_cast<PIXEL_YC *>(_fp->exfunc->get_ycp_filtering(_fp, _fpip->editp, n, NULL));

		bool flag = ConvertAviUtlYCToYUY2::Convert(frame->GetWritePtr(), frame->GetPitch(),
			ycp, _fpip->max_w * sizeof(PIXEL_YC), _vi.width, _vi.height, cpu_detect());//env->GetCPUFlags());

		if(flag)
#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
			_mm_empty();
#else
			{}
#endif

		return frame;
	}

	void __stdcall GetAudio(void *buf, __int64 start, __int64 count, IScriptEnvironment *env)
	{}

	void __stdcall SetCacheHints(int cachehints, int frame_range)
	{}

	const VideoInfo& __stdcall GetVideoInfo()
	{ return _vi; }

	bool __stdcall GetParity(int n)
	{ return !!(_fpip->flag & FILTER_PROC_INFO_FLAG_INVERT_FIELD_ORDER); }
};

//////////////////////////////////////////////////////////////////////////////

class VirtualDubSource : public IClip {
	VideoInfo _vi;
	PVideoFrame _frame;
	const VBitmap *_bitmap;

	static void GetVideoInfo(VideoInfo& vi, const VBitmap *bitmap) {
		memset(&vi, 0, sizeof(vi));
		vi.width = bitmap->w;
		vi.height = bitmap->h;
		vi.fps_numerator = 2997;
		vi.fps_denominator = 100;
		vi.num_frames = 1;
		vi.pixel_type = VideoInfo::CS_BGR32;
	}

public:
	VirtualDubSource(const VBitmap *bitmap)
	{ Reset(bitmap); }

	bool Reset(const VBitmap *bitmap) {
		_frame = NULL;
		_bitmap = bitmap;

		VideoInfo vi;
		GetVideoInfo(vi, bitmap);

		bool result = !memcmp(&vi, &_vi, sizeof(vi));
		_vi = vi;

		return result;
	}

	PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment *env) {
		if(!_frame) {
			_frame = env->NewVideoFrame(_vi);
			env->BitBlt(_frame->GetWritePtr(), _frame->GetPitch(), reinterpret_cast<BYTE *>(_bitmap->data), _bitmap->pitch, _bitmap->w * 4, _bitmap->h);
		}
		return _frame;
	}

	void __stdcall GetAudio(void *buf, __int64 start, __int64 count, IScriptEnvironment *env)
	{}

	void __stdcall SetCacheHints(int cachehints, int frame_range)
	{}

	const VideoInfo& __stdcall GetVideoInfo()
	{ return _vi; }

	bool __stdcall GetParity(int n)
	{ return false; }
};

//////////////////////////////////////////////////////////////////////////////

class AvisynthFilters : public FilterObject {
protected:
	IScriptEnvironment *_env;
	const char *_name, *_info;
	std::vector<FilterFunctionRef> _functions;

	AvisynthFilters(const char *name, const char *info, IScriptEnvironment *env)
		: _env(env), _name(name), _info(info) {}

public:
	static const char *GetName()
	{ return "AvisynthFilters"; }

	FilterFunctionRef AddFunction(const char *name, bool hidden) {
		FilterFunctionRef function(new FilterFunction(name, hidden));
		_functions.push_back(function);
		return function;
	}

	void ToClipboard(HWND hwnd) {
		if(!OpenClipboard(hwnd))
			return;

		std::ostringstream os;
		os << std::boolalpha;

		for(int i = 0; i < _functions.size(); ++i) {
			if(!_functions[i]->IsHidden()) {
				_functions[i]->ToStream(os);
				os << "\r\n";
			}
		}

		std::string format(os.str());
		HGLOBAL text = GlobalAlloc(GMEM_DDESHARE | GMEM_MOVEABLE, format.size() + 1);

		strcpy(static_cast<char *>(GlobalLock(text)), format.c_str());
		GlobalUnlock(text);

		EmptyClipboard();
		SetClipboardData(CF_TEXT, text);
		CloseClipboard();
	}
};

//////////////////////////////////////////////////////////////////////////////

class AvisynthFiltersForVirtualDub : public AvisynthFilters {
	PClip _clip;
	VirtualDubSource *_source;
	IFilterPreview *_preview;
	bool _enable;

	std::vector<FilterArrayRef> _array_params;
	std::vector<FilterTrackRef> _track_params;
	std::vector<FilterCheckRef> _check_params;

	enum {
		ID_TRACK = 0x0100,
		ID_ARRAY = 0x0200,
		ID_CHECK = 0x0300,
		ID_TRACK_NUMBER = 0x0400,

		ID_ENABLE = 0x0500,
		ID_RESET,
		ID_COPY,
		ID_PREVIEW,
	};

	//----------------------------------------------------------------------------

	static int FilterInitProc(FilterActivation *fa, const FilterFunctions *ff)
	{ return static_cast<AvisynthFiltersForVirtualDub *>(fa->filter->private_data)->InitProc(fa, ff); }

	static void FilterDeinitProc(FilterActivation *fa, const FilterFunctions *ff)
	{ static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->DeinitProc(fa, ff); }

	static int FilterRunProc(const FilterActivation *fa, const FilterFunctions *ff)
	{ return static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->RunProc(fa, ff); }

	static long FilterParamProc(FilterActivation *fa, const FilterFunctions *ff)
	{ return static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->ParamProc(fa, ff); }

	static int FilterConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hWnd)
	{ return static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->ConfigProc(fa, ff, hWnd); }

	static void FilterStringProc(const FilterActivation *fa, const FilterFunctions *ff, char *buf)
	{ static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->StringProc(fa, ff, buf); }

	static int FilterStartProc(FilterActivation *fa, const FilterFunctions *ff)
	{ return static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->StartProc(fa, ff); }

	static int FilterEndProc(FilterActivation *fa, const FilterFunctions *ff)
	{ return static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->EndProc(fa, ff); }

	static bool FilterScriptStrProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int len)
	{ return static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->ScriptStrProc(fa, ff, buf, len); }

	static void FilterScriptFunction(IScriptInterpreter *isi, void *lpVoid, CScriptValue *argv, int argc) {
		FilterActivation *fa = static_cast<FilterActivation *>(lpVoid);
		static_cast<AvisynthFiltersForVirtualDub *>(fa->filter_data)->ScriptFunction(isi, argv, argc);
	}

	//----------------------------------------------------------------------------

	int InitProc(FilterActivation *fa, const FilterFunctions *ff) {
		(new(fa->filter_data) AvisynthFiltersForVirtualDub(*this))->CloneContext();
		return 0;
	}

	void DeinitProc(FilterActivation *fa, const FilterFunctions *ff) {
		this->~AvisynthFiltersForVirtualDub();
	}

	int RunProc(const FilterActivation *fa, const FilterFunctions *ff) {
		PVideoFrame frame;
		ResetSource(&fa->src);

		try {
			if(_enable || _preview == NULL)
				frame = _clip->GetFrame(0, _env);
			else
				frame = _source->GetFrame(0, _env);
		} catch(::AvisynthError) {}

		if(!frame) return 1;
		const VideoInfo& vi = _clip->GetVideoInfo();

		_env->BitBlt(reinterpret_cast<BYTE *>(fa->dst.data), fa->dst.pitch, frame->GetReadPtr(), frame->GetPitch(),
			std::min<int>(fa->dst.w * 4, frame->GetRowSize()), std::min<int>(fa->dst.h, frame->GetHeight()));

		return 0;
	}

	long ParamProc(FilterActivation *fa, const FilterFunctions *ff) {
		ResetSource(&fa->src);
		const VideoInfo& vi = _clip->GetVideoInfo();

		fa->dst.w = vi.width;
		fa->dst.h = vi.height;
		fa->dst.AlignTo8();

		return FILTERPARAM_SWAP_BUFFERS;
	}

	int ConfigProc(FilterActivation *fa, const FilterFunctions *ff, HWND hWnd) {
		using namespace DialogTemplate;
		DialogRef dlg(new Dialog(_name));

		{
			ColsRef cols(new Cols());
			cols->AddFrame(new Check("Enable", ID_ENABLE, 120));
			cols->AddFrame(new Button("Reset", ID_RESET, 40));
			dlg->AddFrame(cols);
		}

		for(int i = 0; i < _track_params.size(); ++i) {
			ColsRef cols(new Cols());
			cols->AddFrame(new Static(_track_params[i]->GetName(), -1, 36));
			cols->AddFrame(new ScrollBar("", ID_TRACK + i, 100));
			cols->AddFrame(new Static("", ID_TRACK_NUMBER + i, 24));
			dlg->AddFrame(cols);
		}

		for(int i = 0; i < _array_params.size(); ++i) {
			ColsRef cols(new Cols());
			ComboBoxRef combo(new ComboBox("", ID_ARRAY + i, 124));
			cols->AddFrame(new Static(_array_params[i]->GetName(), -1, 36));
			cols->AddFrame(combo);
			combo->SetStyle(WS_VSCROLL | CBS_DROPDOWNLIST);
			dlg->AddFrame(cols);
		}

		for(int i = 0; i < _check_params.size(); ++i)
			dlg->AddFrame(new Check(_check_params[i]->GetName(), ID_CHECK + i, 160));

		{
			ColsRef cols(new Cols());
			cols->AddFrame(new Button("Copy", ID_COPY, 40));
			cols->AddFrame(new Button("Preview", ID_PREVIEW, 40));
			cols->AddFrame(new Button("OK", IDOK, 40));
			cols->AddFrame(new Button("Cancel", IDCANCEL, 40));
			dlg->AddFrame(cols);
		}

		std::ostringstream os;
		dlg->Write(os, 0, 0);

		_preview = fa->ifp;
		INT_PTR result = DialogBoxIndirectParam(fa->filter->module->hInstModule,
			(LPDLGTEMPLATE)os.str().c_str(), hWnd, DialogProc, reinterpret_cast<LPARAM>(this));
		_preview = NULL;
		return result;
	}

	void StringProc(const FilterActivation *fa, const FilterFunctions *ff, char *buf) {
		std::ostringstream os;
		os << std::boolalpha;

		for(int i = 0; i < _functions.size(); ++i)
			if(!_functions[i]->IsHidden())
				_functions[i]->ToStream(os);

		strcpy(buf, os.str().c_str());
	}

	int StartProc(FilterActivation *fa, const FilterFunctions *ff)
	{ return 0; }

	int EndProc(FilterActivation *fa, const FilterFunctions *ff)
	{ return 0; }

	bool ScriptStrProc(FilterActivation *fa, const FilterFunctions *ff, char *buf, int len) {
		std::vector<int> values;

		for(int i = 0; i < _track_params.size(); ++i)
			values.push_back(_track_params[i]->GetTrack());

		for(int i = 0; i < _array_params.size(); ++i)
			values.push_back(_array_params[i]->GetTrack());

		for(int i = 0; i < _check_params.size(); ++i)
			values.push_back(_check_params[i]->GetCheck());

		std::ostringstream os;
		os << "Config(";

		for(int i = 0; i < values.size(); ++i) {
			if(i) os << ",";
			os << values[i];
		}

		os << ")";
		strcpy(buf, os.str().c_str());

		return true;
	}

	void ScriptFunction(IScriptInterpreter *isi, CScriptValue *argv, int argc) {
		int n = 0;

		for(int i = 0; i < _track_params.size(); ++i, ++n)
			_track_params[i]->SetTrack(argv[n].asInt());

		for(int i = 0; i < _array_params.size(); ++i, ++n)
			_array_params[i]->SetTrack(argv[n].asInt());

		for(int i = 0; i < _check_params.size(); ++i, ++n)
			_check_params[i]->SetCheck(argv[n].asInt());
	}

	//----------------------------------------------------------------------------

	static INT_PTR CALLBACK DialogProc(HWND hdlg, UINT msg, WPARAM wParam, LPARAM lParam) {
		if(msg == WM_INITDIALOG) SetWindowLongPtr(hdlg, DWLP_USER, lParam);
		AvisynthFiltersForVirtualDub *self = reinterpret_cast<AvisynthFiltersForVirtualDub *>(GetWindowLongPtr(hdlg, DWLP_USER));

		switch(msg) {
		case WM_INITDIALOG: return HANDLE_WM_INITDIALOG(hdlg, wParam, lParam, self->OnInitDialog);
		case WM_COMMAND:    return HANDLE_WM_COMMAND   (hdlg, wParam, lParam, self->OnCommand);
		case WM_HSCROLL:    return HANDLE_WM_HSCROLL   (hdlg, wParam, lParam, self->OnHScroll);
		}

		return FALSE;
	}

	BOOL OnInitDialog(HWND hwnd, HWND hwndFocus, LPARAM lParam) {
		InitDialog(hwnd);
		return TRUE;
	}

	void OnHScroll(HWND hwnd, HWND hwndCtl, UINT code, int pos) {
		int id = GetDlgCtrlID(hwndCtl);
		if(id >= ID_TRACK && id < ID_TRACK + _track_params.size())
			OnNotifyTrack(hwnd, id - ID_TRACK, hwndCtl, code, pos);
	}

	void OnCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
		switch(id) {
		case IDOK:     EndDialog(hwnd, 0); return;
		case IDCANCEL: EndDialog(hwnd, 1); return;

		case ID_RESET:
			ClearSource();
			SetDefault();
			InitDialog(hwnd);
			_preview->RedoFrame();
			return;

		case ID_COPY:
			ToClipboard(hwnd);
			return;

		case ID_PREVIEW:
			_preview->Toggle(hwnd);
			return;
		}

		switch(MAKELONG(id, codeNotify)) {
		case MAKELONG(ID_ENABLE, BN_CLICKED):
			ClearSource();
			_enable = Button_GetCheck(hwndCtl);
			_preview->RedoFrame();
			return;
		}

		if(id >= ID_ARRAY && id < ID_ARRAY + _array_params.size())
			OnNotifyArray(hwnd, id - ID_ARRAY, hwndCtl, codeNotify);

		else if(id >= ID_CHECK && id < ID_CHECK + _check_params.size())
			OnNotifyCheck(hwnd, id - ID_CHECK, hwndCtl, codeNotify);
	}

	void OnNotifyTrack(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify, int pos) {
		FilterTrackRef param(_track_params[id]);
		int old = param->GetTrack();

		switch(codeNotify) {
		case SB_LEFT:  pos = param->GetStart(); break;
		case SB_RIGHT: pos = param->GetEnd(); break;

		case SB_THUMBPOSITION:
		case SB_THUMBTRACK: break;

		default:
			pos = ScrollBar_GetPos(hwndCtl);
			switch(codeNotify) {
			case SB_LINELEFT:  case SB_PAGELEFT:  pos -= 1; break;
			case SB_LINERIGHT: case SB_PAGERIGHT: pos += 1; break;
			}
			break;
		}

		param->SetTrack(pos);
		pos = param->GetTrack();
		if(pos == old) return;

		ClearSource();
		ScrollBar_SetPos(hwndCtl, pos, true);

		std::ostringstream os;
		os << std::boolalpha;
		param->ToStream(os);
		Static_SetText(GetDlgItem(hwnd, ID_TRACK_NUMBER + id), os.str().c_str());

		_preview->RedoFrame();
	}

	void OnNotifyArray(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
		FilterArrayRef param(_array_params[id]);
		int old = param->GetTrack(), cur = old;

		switch(codeNotify) {
		case CBN_SELCHANGE:
			cur = ComboBox_GetCurSel(hwndCtl);
			break;
		}

		param->SetTrack(cur);
		cur = param->GetTrack();
		if(cur == old) return;

		ClearSource();
		param->SetTrack(cur);

		_preview->RedoFrame();
	}

	void OnNotifyCheck(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify) {
		FilterCheckRef param(_check_params[id]);
		bool old = param->GetCheck(), cur = old;

		switch(codeNotify) {
		case BN_CLICKED:
			cur = Button_GetCheck(hwndCtl);
			break;
		}

		param->SetCheck(cur);
		cur = param->GetCheck();
		if(cur == old) return;

		ClearSource();
		param->SetCheck(cur);

		_preview->RedoFrame();
	}

	//----------------------------------------------------------------------------

	void SetDefault() {
		for(int i = 0; i < _track_params.size(); ++i)
			_track_params[i]->SetTrack(_track_params[i]->GetDefault());

		for(int i = 0; i < _array_params.size(); ++i)
			_array_params[i]->SetTrack(_array_params[i]->GetDefault());

		for(int i = 0; i < _check_params.size(); ++i)
			_check_params[i]->SetCheck(_check_params[i]->GetDefault());
	}

	void InitDialog(HWND hwnd) {
		Button_SetCheck(GetDlgItem(hwnd, ID_ENABLE), _enable);

		for(int i = 0; i < _track_params.size(); ++i) {
			FilterTrackRef param(_track_params[i]);
			HWND track = GetDlgItem(hwnd, ID_TRACK + i);
			HWND number = GetDlgItem(hwnd, ID_TRACK_NUMBER + i);

			ScrollBar_SetRange(track, param->GetStart(), param->GetEnd(), false);
			ScrollBar_SetPos(track, param->GetTrack(), true);

			std::ostringstream os;
			os << std::boolalpha;
			param->ToStream(os);
			Static_SetText(number, os.str().c_str());
		}

		for(int i = 0; i < _array_params.size(); ++i) {
			FilterArrayRef param(_array_params[i]);
			HWND combo = GetDlgItem(hwnd, ID_ARRAY + i);
			int value = param->GetTrack();

			ComboBox_ResetContent(combo);

			for(int j = 0; j <= param->GetEnd(); ++j) {
				std::ostringstream os;
				os << std::boolalpha;
				param->SetTrack(j);
				param->ToStream(os);
				ComboBox_AddString(combo, os.str().c_str());
			}

			param->SetTrack(value);
			ComboBox_SetCurSel(combo, value);
		}

		for(int i = 0; i < _check_params.size(); ++i) {
			FilterCheckRef param(_check_params[i]);
			HWND check = GetDlgItem(hwnd, ID_CHECK + i);
			Button_SetCheck(check, param->GetCheck());
		}

		_preview->InitButton(GetDlgItem(hwnd, ID_PREVIEW));
	}

	void CloneContext() {
		for(int i = 0; i < _functions.size(); ++i)
			_functions[i] = _functions[i]->Clone();

		SplitParams();
	}

	void SplitParams() {
		_track_params.clear();
		_array_params.clear();
		_check_params.clear();

		for(int i = 0; i < _functions.size(); ++i) {
			if(_functions[i]->IsHidden()) continue;
			const std::vector<FilterParamRef>& params = _functions[i]->GetParams();

			for(int j = 0; j < params.size(); ++j) {
				FilterArrayRef array(params[j]);
				FilterTrackRef track(params[j]);
				FilterCheckRef check(params[j]);

				if(!!array)
					_array_params.push_back(array);
				else if(!!track)
					_track_params.push_back(track);
				else if(!!check)
					_check_params.push_back(check);
			}
		}
	}

	void ResetSource(const VBitmap *bitmap) {
		if(!!_clip && _source != NULL && _source->Reset(bitmap))
			return;

		_clip = _source = new VirtualDubSource(bitmap);

		for(int i = 0; i < _functions.size(); ++i) {
			try {
				PClip clip(_functions[i]->CreateClip(_clip, _env));
				if(!!clip) _clip = clip;
			}
			catch(::AvisynthError err) {
				if(_preview != NULL) MessageBox(NULL, err.msg, NULL, MB_OK);
			}
			catch(::IScriptEnvironment::NotFound) {
				if(_preview != NULL) MessageBox(NULL, "NotFound", NULL, MB_OK);
			}
		}
	}

	void ClearSource() {
		_clip = NULL;
		_source = NULL;
	}

	//----------------------------------------------------------------------------

	static void __cdecl Delete(void *user_data, IScriptEnvironment *env)
	{ delete static_cast<AvisynthFiltersForVirtualDub *>(user_data); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		AvisynthFiltersForVirtualDub *self = new AvisynthFiltersForVirtualDub(args[0].AsString(), args[1].AsString(""), env);
		env->AtExit(Delete, self);
		return FilterObject::ToAVS(self);
	}

	AvisynthFiltersForVirtualDub(const char *name, const char *info, IScriptEnvironment *env)
		: AvisynthFilters(name, info, env), _preview(NULL), _enable(true) {}

public:
	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "s.", Create, NULL); }

	FilterDefinition *Freeze() {
		SplitParams();
		int items = _track_params.size() + _array_params.size() + _check_params.size();

		ScriptFunctionDef scrFunc[2];
		memset((void*)&scrFunc, 0, sizeof(scrFunc));
		scrFunc[0].func_ptr = reinterpret_cast<ScriptFunctionPtr>(FilterScriptFunction);
		scrFunc[0].name = "Config";
		scrFunc[0].arg_list = _env->SaveString(("0" + std::string(items, 'i')).c_str());

		CScriptObject scrObj;
		memset((void*)&scrObj, 0, sizeof(scrObj));
		scrObj.func_list = reinterpret_cast<ScriptFunctionDef *>
			(_env->SaveString(reinterpret_cast<char *>(scrFunc), sizeof(scrFunc)));

		FilterDefinition filter;
		memset((void*)&filter, 0, sizeof(filter));

		filter.name = _name;
		filter.desc = _info;
		filter.private_data = this;
		filter.inst_data_size = sizeof(AvisynthFiltersForVirtualDub);

		filter.initProc = FilterInitProc;
		filter.deinitProc = FilterDeinitProc;
		filter.runProc = FilterRunProc;
		filter.paramProc = FilterParamProc;
		filter.configProc = FilterConfigProc;
		filter.stringProc = FilterStringProc;
		filter.startProc = FilterStartProc;
		filter.endProc = FilterEndProc;
		filter.fssProc = FilterScriptStrProc;
		filter.script_obj = reinterpret_cast<CScriptObject *>
			(_env->SaveString(reinterpret_cast<char *>(&scrObj), sizeof(scrObj)));

		return reinterpret_cast<FilterDefinition *>
			(_env->SaveString(reinterpret_cast<char *>(&filter), sizeof(filter)));
	}
};

//////////////////////////////////////////////////////////////////////////////

class AvisynthFiltersForAviUtl : public AvisynthFilters {
	PClip _clip;
	AviUtlSource *_source;

	FILTER_DLL _filter;
	bool _processing;

	std::vector<FilterTrackRef> _track_params;
	std::vector<const char *> _track_name;
	std::vector<int> _track_default, _track_start, _track_end;

	std::vector<FilterCheckRef> _check_params;
	std::vector<const char *> _check_name;
	std::vector<int> _check_default;

	typedef bool (AvisynthFiltersForAviUtl::*CommandProc)(HWND hwnd, HWND ctrl, void *editp, FILTER *fp);
	typedef std::map<int, CommandProc> CommandMap;
	CommandMap _commands;

	//----------------------------------------------------------------------------

	static BOOL func_proc(FILTER *fp, FILTER_PROC_INFO *fpip)
	{ return static_cast<AvisynthFiltersForAviUtl *>(fp->ex_data_ptr)->Proc(fp, fpip); }

	static BOOL func_update(FILTER *fp, int status)
	{ return static_cast<AvisynthFiltersForAviUtl *>(fp->ex_data_ptr)->Update(fp, status); }

	static BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp)
	{ return static_cast<AvisynthFiltersForAviUtl *>(fp->ex_data_ptr)->WndProc(hwnd, message, wparam, lparam, editp, fp); }

	static BOOL func_save_start(FILTER *fp, int s, int e, void *editp)
	{ return static_cast<AvisynthFiltersForAviUtl *>(fp->ex_data_ptr)->SaveStart(fp, s, e, editp); }

	//----------------------------------------------------------------------------

	bool Proc(FILTER *fp, FILTER_PROC_INFO *fpip) {
		ResetSource(fp, fpip);

		const VideoInfo& vi = _clip->GetVideoInfo();
		int n = std::min(std::max(fpip->frame, 0), vi.num_frames - 1);

		PVideoFrame frame;
		_processing = true;

		try {
			frame = _clip->GetFrame(n, _env);
		} catch(::AvisynthError) {}

		_processing = false;
		if(!frame) return false;

		fpip->w = std::min(fpip->max_w, vi.width);
		fpip->h = std::min(fpip->max_h, vi.height);

		bool flag = ConvertYUY2ToAviUtlYC::Convert(fpip->ycp_edit, fpip->max_w * sizeof(PIXEL_YC),
			frame->GetReadPtr(), frame->GetPitch(), fpip->w, fpip->h, cpu_detect());//_env->GetCPUFlags());

		if(flag)
#if !(defined(_M_X64) || defined(USE_SIMD_INTRINSICS))
			_mm_empty();
#else
			{}
#endif

		return true;
	}

	bool Update(FILTER *fp, int status) {
		bool clear = false, update = false;

		for(int i = 0; i < _track_params.size(); ++i) {
			FilterTrackRef track(_track_params[i]);
			if(fp->track[i] == track->GetTrack())
				continue;

			clear = true;
			track->SetTrack(fp->track[i]);

			int value = track->GetTrack();
			if(fp->track[i] == value)
				continue;

			update = true;
			fp->track[i] = value;
		}

		for(int i = 0; i < _check_params.size(); ++i) {
			FilterCheckRef check(_check_params[i]);
			if(bool(fp->check[i]) == check->GetCheck())
				continue;

			clear = true;
			check->SetCheck(fp->check[i]);

			bool value = check->GetCheck();
			if(bool(fp->check[i]) == value)
				continue;

			update = true;
			fp->check[i] = value;
		}

		if(clear)
			ClearSource();

		if(update)
			fp->exfunc->filter_window_update(fp);

		return true;
	}

	bool WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, void *editp, FILTER *fp) {
		switch(message) {
		case WM_COMMAND:
			return OnCommand(hwnd, wparam, (HWND)lparam, editp, fp);
		case WM_FILTER_CHANGE_ACTIVE:
			return OnChangeActive(hwnd, editp, fp);
		}
		return false;
	}

	bool SaveStart(FILTER *fp, int s, int e, void *editp) {
		ClearSource();
		return true;
	}

	//----------------------------------------------------------------------------

	bool OnCommand(HWND hwnd, WPARAM id, HWND ctrl, void *editp, FILTER *fp) {
		CommandMap::iterator it = _commands.find(id);

		if(it != _commands.end())
			return (this->*it->second)(hwnd, ctrl, editp, fp);

		return false;
	}

	bool OnChangeActive(HWND hwnd, void *editp, FILTER *fp) {
		if(!fp->exfunc->is_filter_active(fp))
			ClearSource();

		return false;
	}

	bool OnClipboard(HWND hwnd, HWND ctrl, void *editp, FILTER *fp) {
		ToClipboard(hwnd);
		return false;
	}

	//----------------------------------------------------------------------------

	void ResetSource(FILTER *fp, FILTER_PROC_INFO *fpip) {
		if(!!_clip && _source != NULL && _source->Reset(fp, fpip))
			return;

		bool silent = !!fp->exfunc->is_saving(fpip->editp);
		_clip = _source = new AviUtlSource(fp, fpip);

		AVSValue args[] = { _clip };
		_clip = _env->Invoke("Cache", AVSValue(args, sizeof(args) / sizeof(*args))).AsClip();

		for(int i = 0; i < _functions.size(); ++i) {
			try {
				PClip clip(_functions[i]->CreateClip(_clip, _env));

				if(!!clip) {
					AVSValue args[] = { clip };
					_clip = _env->Invoke("Cache", AVSValue(args, sizeof(args) / sizeof(*args))).AsClip();
				}
			}
			catch(::AvisynthError err) {
				if(!silent)
					MessageBox(NULL, err.msg, NULL, MB_OK);
			}
			catch(::IScriptEnvironment::NotFound) {
				if(!silent)
					MessageBox(NULL, "NotFound", NULL, MB_OK);
			}
		}
	}

	void ClearSource() {
		if(_processing)
			_source = NULL;
		else
			_clip = NULL;
	}

	//----------------------------------------------------------------------------

	static void __cdecl Delete(void *user_data, IScriptEnvironment *env)
	{ delete static_cast<AvisynthFiltersForAviUtl *>(user_data); }

	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		AvisynthFiltersForAviUtl *self = new AvisynthFiltersForAviUtl(args[0].AsString(), args[1].AsString(""), env);
		env->AtExit(Delete, self);
		return FilterObject::ToAVS(self);
	}

	AvisynthFiltersForAviUtl(const char *name, const char *info, IScriptEnvironment *env)
		: AvisynthFilters(name, info, env), _processing(false) {}

public:
	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "s.", Create, NULL); }

	FILTER_DLL *Freeze() {
		FILTER_DLL *fp = &_filter;
		memset((void*)fp, 0, sizeof(FILTER_DLL));

		fp->name = const_cast<char *>(_name);
		fp->func_proc = func_proc;
		fp->func_update = func_update;
		fp->func_WndProc = func_WndProc;
		fp->func_save_start = func_save_start;
		fp->ex_data_ptr = this;

		if(*_info == '\0') {
			fp->flag |= FILTER_FLAG_EX_INFORMATION;
			fp->information = const_cast<char *>(_info);
		}

		for(int i = 0; i < _functions.size(); ++i) {
			if(_functions[i]->IsHidden()) continue;
			const std::vector<FilterParamRef>& params = _functions[i]->GetParams();

			for(int j = 0; j < params.size(); ++j) {
				FilterTrackRef track(params[j]);
				FilterCheckRef check(params[j]);

				if(!!track) {
					_track_params.push_back(track);
					_track_name.push_back(track->GetName());
					_track_default.push_back(track->GetDefault());
					_track_start.push_back(track->GetStart());
					_track_end.push_back(track->GetEnd());
				}
				else if(!!check) {
					_check_params.push_back(check);
					_check_name.push_back(check->GetName());
					_check_default.push_back(check->GetDefault());
				}
			}
		}

		int mid = MID_FILTER_BUTTON + _check_name.size();

		_commands[mid++] = &AvisynthFiltersForAviUtl::OnClipboard;
		_check_name.push_back("書式をｸﾘｯﾌﾟﾎﾞｰﾄﾞへｺﾋﾟｰ");
		_check_default.push_back(-1);

		if(!_track_name.empty()) {
			fp->track_n = _track_name.size();
			fp->track_name = reinterpret_cast<TCHAR **>(const_cast<char **>(&_track_name[0]));
			fp->track_default = &_track_default[0];
			fp->track_s = &_track_start[0];
			fp->track_e = &_track_end[0];
		}

		if(!_check_name.empty()) {
			fp->check_n = _check_name.size();
			fp->check_name = reinterpret_cast<TCHAR **>(const_cast<char **>(&_check_name[0]));
			fp->check_default = &_check_default[0];
		}

		return fp;
	}
};

//////////////////////////////////////////////////////////////////////////////

class AddFunction {
	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		AvisynthFilters *filters = FilterObject::FromAVS<AvisynthFilters>(args[0], GetName(), env);
		FilterFunctionRef function(filters->AddFunction(args[1].AsString(), args[2].AsBool(false)));
		return FilterObject::ToAVS(function.operator->());
	}

public:
	static const char *GetName()
	{ return "AddFunction"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "ss[hidden]b", Create, NULL); }
};

//////////////////////////////////////////////////////////////////////////////

class AddValue {
	static AVSValue __cdecl CreateV(AVSValue args, void *user_data, IScriptEnvironment *env) {
		FilterFunction *function = FilterObject::FromAVS<FilterFunction>(args[0], GetName(), env);
		FilterParamRef param(new FilterValue(args[1]));
		function->AddParam(param);
		return FilterObject::ToAVS(param.operator->());
	}

public:
	static const char *GetName()
	{ return "AddValue"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "s.", CreateV, NULL); }
};

//////////////////////////////////////////////////////////////////////////////

class AddTrack {
	static AVSValue __cdecl CreateIII(AVSValue args, void *user_data, IScriptEnvironment *env) {
		FilterFunction *function = FilterObject::FromAVS<FilterFunction>(args[0], GetName(), env);
		FilterParamRef param(new FilterInt(args[1].AsString(), args[2].AsInt(), args[3].AsInt(), args[4].AsInt()));
		function->AddParam(param);
		return FilterObject::ToAVS(param.operator->());
	}

	static AVSValue __cdecl CreateIIIFF(AVSValue args, void *user_data, IScriptEnvironment *env) {
		FilterFunction *function = FilterObject::FromAVS<FilterFunction>(args[0], GetName(), env);
		FilterParamRef param(new FilterFloat(args[1].AsString(), args[2].AsInt(), args[3].AsInt(), args[4].AsInt(), args[5].AsFloat(), args[6].AsFloat()));
		function->AddParam(param);
		return FilterObject::ToAVS(param.operator->());
	}

public:
	static const char *GetName()
	{ return "AddTrack"; }

	static void PluginInit(IScriptEnvironment *env) {
		env->AddFunction(GetName(), "ssiii", CreateIII, NULL);
		env->AddFunction(GetName(), "ssiiiff", CreateIIIFF, NULL);
	}
};

//////////////////////////////////////////////////////////////////////////////

class AddArray {
	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		FilterFunction *function = FilterObject::FromAVS<FilterFunction>(args[0], GetName(), env);
		FilterParamRef param(new FilterArray(args[1].AsString(), args[2].AsInt(), args[3]));
		function->AddParam(param);
		return FilterObject::ToAVS(param.operator->());
	}

public:
	static const char *GetName()
	{ return "AddArray"; }

	static void PluginInit(IScriptEnvironment *env)
	{ env->AddFunction(GetName(), "ssi.+", Create, NULL); }
};

//////////////////////////////////////////////////////////////////////////////

class AddCheck {
	static AVSValue __cdecl Create(AVSValue args, void *user_data, IScriptEnvironment *env) {
		FilterFunction *function = FilterObject::FromAVS<FilterFunction>(args[0], GetName(), env);
		FilterParamRef param(new FilterPair(args[1].AsString(), args[2].AsBool(), false, true));
		function->AddParam(param);
		return FilterObject::ToAVS(param.operator->());
	}

	static AVSValue __cdecl CreateV(AVSValue args, void *user_data, IScriptEnvironment *env) {
		FilterFunction *function = FilterObject::FromAVS<FilterFunction>(args[0], GetName(), env);
		FilterParamRef param(new FilterPair(args[1].AsString(), args[2].AsBool(), args[3], args[4]));
		function->AddParam(param);
		return FilterObject::ToAVS(param.operator->());
	}

public:
	static const char *GetName()
	{ return "AddCheck"; }

	static void PluginInit(IScriptEnvironment *env) {
		env->AddFunction(GetName(), "ssb", Create, NULL);
		env->AddFunction(GetName(), "ssb..", CreateV, NULL);
	}
};

//////////////////////////////////////////////////////////////////////////////
