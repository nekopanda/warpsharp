#ifndef _COMPATIBLE_H_
#define _COMPATIBLE_H_

#include <set>
#include <map>
#include <limits>
#include "traits.h"
#include "../common/library.h"
#include "../common/thread.h"

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

template<class TraitsA, class TraitsB>
struct CompatibleTraits;

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

template<class TraitsA, class TraitsB>
class CompatibleEnvironment : public TraitsB::IScriptEnvironment {
	typedef typename TraitsA::IScriptEnvironment IScriptEnvironmentA;
	typedef typename TraitsB::IScriptEnvironment IScriptEnvironmentB;

	typedef typename IScriptEnvironmentB::ApplyFunc ApplyFuncB;
	typedef typename IScriptEnvironmentB::ShutdownFunc ShutdownFuncB;

	typedef typename TraitsA::VideoInfo VideoInfoA;
	typedef typename TraitsB::VideoInfo VideoInfoB;

	typedef typename TraitsA::VideoFrame VideoFrameA;
	typedef typename TraitsB::VideoFrame VideoFrameB;

	typedef typename TraitsA::PVideoFrame PVideoFrameA;
	typedef typename TraitsB::PVideoFrame PVideoFrameB;

	typedef typename TraitsA::AVSValue AVSValueA;
	typedef typename TraitsB::AVSValue AVSValueB;

	typedef typename TraitsA::AVSValueStack AVSValueStackA;
	typedef typename TraitsB::AVSValueStack AVSValueStackB;

	AVSValueStackB _stack;
	IScriptEnvironmentA *_env;

	class CompatibleApplyFunc {
		AVSValueStackA _stack;
		ApplyFuncB _apply;
		void *_user_data;

		static void __cdecl Delete(void *user_data, IScriptEnvironmentA *env)
		{ delete static_cast<CompatibleApplyFunc *>(user_data); }

		static AVSValueA __cdecl Callback(AVSValueA args, void *user_data, IScriptEnvironmentA *env)
		{ return static_cast<CompatibleApplyFunc *>(user_data)->Apply(args, env); }

		AVSValueA Apply(AVSValueA args, IScriptEnvironmentA *env) {
			IScriptEnvironmentB *eB = CompatibleTraits<TraitsA, TraitsB>::ValidateEnvironment(env);
			AVSValueStackB sB;
			AVSValueB vB(CompatibleTraits<TraitsA, TraitsB>::ValidateAVSValue(args, sB));
			vB = _apply(vB, _user_data, eB);
			return CompatibleTraits<TraitsB, TraitsA>::ValidateAVSValue(vB, _stack);
		}

	public:
		CompatibleApplyFunc(
			const char *name, const char *params,
			ApplyFuncB apply, void *user_data,
			IScriptEnvironmentA *env) : _apply(apply), _user_data(user_data)
		{
			env->AtExit(Delete, this);
			env->AddFunction(name, params, Callback, this);
		}
	};

	class CompatibleShutdownFunc {
		ShutdownFuncB _shutdown;
		void *_user_data;

		static void __cdecl Callback(void *user_data, IScriptEnvironmentA *env)
		{ static_cast<CompatibleShutdownFunc *>(user_data)->Shutdown(env); }

		void Shutdown(IScriptEnvironmentA *env) {
			IScriptEnvironmentB *eB = CompatibleTraits<TraitsA, TraitsB>::ValidateEnvironment(env);
			_shutdown(_user_data, eB);
			delete this;
		}

	public:
		CompatibleShutdownFunc(
			ShutdownFuncB shutdown, void *user_data,
			IScriptEnvironmentA *env) : _shutdown(shutdown), _user_data(user_data)
		{ env->AtExit(Callback, this); }
	};

	typedef std::set<CompatibleEnvironment *> ObjectSet;
	typedef std::map<IScriptEnvironmentA *, CompatibleEnvironment *> ObjectMap;

	static CriticalSection _CriticalSection;
	static ObjectSet _ObjectSet;
	static ObjectMap _ObjectMap;

	static void __cdecl Delete(void *user_data, IScriptEnvironmentA *env) {
		CriticalLock lock(_CriticalSection);
		delete static_cast<CompatibleEnvironment *>(user_data);
	}

	CompatibleEnvironment(IScriptEnvironmentA *env) : _env(env) {
		_ObjectSet.insert(this);
		_ObjectMap.insert(ObjectMap::value_type(env, this));
		env->AtExit(Delete, this);
	}

	~CompatibleEnvironment() {
		_ObjectMap.erase(_env);
		_ObjectSet.erase(this);
	}

public:
	static CompatibleEnvironment *Create(IScriptEnvironmentA *env) {
		CriticalLock lock(_CriticalSection);
		ObjectMap::iterator i = _ObjectMap.find(env);
		return i == _ObjectMap.end() ? new CompatibleEnvironment(env) : i->second;
	}

	static IScriptEnvironmentA *Validate(IScriptEnvironmentB *env) {
		CriticalLock lock(_CriticalSection);
		CompatibleEnvironment *e = static_cast<CompatibleEnvironment *>(env);
		return _ObjectSet.find(e) == _ObjectSet.end() ? CompatibleEnvironment<TraitsB, TraitsA>::Create(env) : e->_env;
	}

	long __stdcall GetCPUFlags()
	{ return _env->GetCPUFlags(); }

	char *__stdcall SaveString(const char *s, int length)
	{ return _env->SaveString(s, length); }

	char *__stdcall Sprintf(const char *fmt, ...) {
		va_list val;
		va_start(val, fmt);
		char *result = VSprintf(fmt, val);
		va_end(val);
		return result;
	}

	char *__stdcall VSprintf(const char *fmt, void *val)
	{ return _env->VSprintf(fmt, val); }

	__declspec(noreturn) void __stdcall ThrowError(const char *fmt, ...) {
		va_list val;
		va_start(val, fmt);
		char *result = VSprintf(fmt, val);
		va_end(val);
		throw ::AvisynthError(result);
	}

	void __stdcall AddFunction(const char *name, const char *params, ApplyFuncB apply, void *user_data)
	{ new CompatibleApplyFunc(name, params, apply, user_data, _env); }

	bool __stdcall FunctionExists(const char *name)
	{ return _env->FunctionExists(name); }

	AVSValueB __stdcall Invoke(const char *name, const AVSValueB args, const char **arg_names) {
		AVSValueStackA sA;
		AVSValueA vA(CompatibleTraits<TraitsB, TraitsA>::ValidateAVSValue(args, sA));
		vA = _env->Invoke(name, vA, arg_names);
		return CompatibleTraits<TraitsA, TraitsB>::ValidateAVSValue(vA, _stack);
	}

	AVSValueB __stdcall GetVar(const char *name)
	{ return CompatibleTraits<TraitsA, TraitsB>::ValidateAVSValue(_env->GetVar(name), _stack); }

	bool __stdcall SetVar(const char *name, const AVSValueB& val) {
		AVSValueStackA sA;
		AVSValueA vA(CompatibleTraits<TraitsB, TraitsA>::ValidateAVSValue(val, sA));
		return _env->SetVar(name, vA);
	}

	bool __stdcall SetGlobalVar(const char *name, const AVSValueB& val) {
		AVSValueStackA sA;
		AVSValueA vA(CompatibleTraits<TraitsB, TraitsA>::ValidateAVSValue(val, sA));
		return _env->SetGlobalVar(name, vA);
	}

	void __stdcall PushContext(int level)
	{ _env->PushContext(level); }

	void __stdcall PopContext()
	{ _env->PopContext(); }

	PVideoFrameB __stdcall NewVideoFrame(const VideoInfoB& vi, int align) {
		VideoInfoA iA;
		CompatibleTraits<TraitsB, TraitsA>::ValidateVideoInfo(iA, vi);
		PVideoFrameA dst(_env->NewVideoFrame(iA, align));
		VideoFrameA *fA = dst.operator->();
		return reinterpret_cast<VideoFrameB *>(fA);
	}

	bool __stdcall MakeWritable(PVideoFrameB *pvf)
	{ return _env->MakeWritable(reinterpret_cast<PVideoFrameA *>(pvf)); }

	void __stdcall BitBlt(BYTE *dstp, int dst_pitch, const BYTE *srcp, int src_pitch, int row_size, int height)
	{ _env->BitBlt(dstp, dst_pitch, srcp, src_pitch, row_size, height); }

	void __stdcall AtExit(ShutdownFuncB function, void *user_data)
	{ new CompatibleShutdownFunc(function, user_data, _env); }

	void __stdcall CheckVersion(int version)
	{ _env->CheckVersion(version); }

	PVideoFrameB __stdcall Subframe(PVideoFrameB src, int rel_offset, int new_pitch, int new_row_size, int new_height) {
		VideoFrameB *fB = src.operator->();
		PVideoFrameA dst(_env->Subframe(reinterpret_cast<VideoFrameA *>(fB), rel_offset, new_pitch, new_row_size, new_height));
		VideoFrameA *fA = dst.operator->();
		return reinterpret_cast<VideoFrameB *>(fA);
	}

	int __stdcall SetMemoryMax(int mem)
	{ return _env->SetMemoryMax(mem); }

	int __stdcall SetWorkingDir(const char *newdir)
	{ return _env->SetWorkingDir(newdir); }
};

CriticalSection CompatibleEnvironment<avisynth1::Traits, avisynth2::Traits>::_CriticalSection;
CompatibleEnvironment<avisynth1::Traits, avisynth2::Traits>::ObjectSet CompatibleEnvironment<avisynth1::Traits, avisynth2::Traits>::_ObjectSet;
CompatibleEnvironment<avisynth1::Traits, avisynth2::Traits>::ObjectMap CompatibleEnvironment<avisynth1::Traits, avisynth2::Traits>::_ObjectMap;

CriticalSection CompatibleEnvironment<avisynth2::Traits, avisynth1::Traits>::_CriticalSection;
CompatibleEnvironment<avisynth2::Traits, avisynth1::Traits>::ObjectSet CompatibleEnvironment<avisynth2::Traits, avisynth1::Traits>::_ObjectSet;
CompatibleEnvironment<avisynth2::Traits, avisynth1::Traits>::ObjectMap CompatibleEnvironment<avisynth2::Traits, avisynth1::Traits>::_ObjectMap;

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

template<class TraitsA, class TraitsB>
class CompatibleClip : public TraitsB::IClip {
	typedef typename TraitsA::IScriptEnvironment IScriptEnvironmentA;
	typedef typename TraitsB::IScriptEnvironment IScriptEnvironmentB;

	typedef typename TraitsB::VideoInfo VideoInfoB;
	typedef typename TraitsA::PClip PClipA;

	typedef typename TraitsA::VideoFrame VideoFrameA;
	typedef typename TraitsB::VideoFrame VideoFrameB;

	typedef typename TraitsA::PVideoFrame PVideoFrameA;
	typedef typename TraitsB::PVideoFrame PVideoFrameB;

	PClipA child;
	VideoInfoB vi;

public:
	CompatibleClip(const PClipA& clip) : child(clip)
	{ CompatibleTraits<TraitsA, TraitsB>::ValidateVideoInfo(vi, clip->GetVideoInfo()); }

	PVideoFrameB __stdcall GetFrame(int n, IScriptEnvironmentB *env) {
		IScriptEnvironmentA *eA = CompatibleTraits<TraitsB, TraitsA>::ValidateEnvironment(env);
		PVideoFrameA dst(child->GetFrame(n, eA));
		VideoFrameA *fA = dst.operator->();
		return reinterpret_cast<VideoFrameB *>(fA);
	}

	void __stdcall GetAudio(void *buf, int start, int count, IScriptEnvironmentB *env)
	{ GetAudio(buf, __int64(start), __int64(count), env); }

	void __stdcall GetAudio(void *buf, __int64 start, __int64 count, IScriptEnvironmentB *env) {
		IScriptEnvironmentA *eA = CompatibleTraits<TraitsB, TraitsA>::ValidateEnvironment(env);
		child->GetAudio(buf, start, count, eA);
	}

	const VideoInfoB& __stdcall GetVideoInfo()
	{ return vi; }

	bool __stdcall GetParity(int n)
	{ return child->GetParity(n); }

	void __stdcall SetCacheHints(int cachehints, int frame_range)
	{ CompatibleTraits<TraitsA, TraitsA>::SetCacheHints(child, cachehints, frame_range); }
};

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

template<> struct CompatibleTraits<avisynth1::Traits, avisynth1::Traits> {
	static avisynth1::IScriptEnvironment *ValidateEnvironment(avisynth1::IScriptEnvironment *env)
	{ return env; }

	static void SetCacheHints(const avisynth1::PClip& clip, int cachehints, int frame_range)
	{}
};

template<> struct CompatibleTraits<avisynth2::Traits, avisynth2::Traits> {
	static avisynth2::IScriptEnvironment *ValidateEnvironment(avisynth2::IScriptEnvironment *env)
	{ return env; }
};

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

template<> struct CompatibleTraits<avisynth1::Traits, avisynth2::Traits> {
	static avisynth2::IScriptEnvironment *ValidateEnvironment(avisynth1::IScriptEnvironment *env)
	{ return CompatibleEnvironment<avisynth2::Traits, avisynth1::Traits>::Validate(env); }

	static avisynth2::AVSValue ValidateAVSValue(const avisynth1::AVSValue& v1, avisynth2::Traits::AVSValueStack& stack) {
		avisynth2::AVSValue v2;

		if(v1.IsArray()) {
			stack.resize(stack.size() + 1);
			avisynth2::Traits::AVSValueArray& array = stack.back();
			array.resize(v1.ArraySize());

			for(int i = 0; i < v1.ArraySize(); ++i)
				array[i] = ValidateAVSValue(v1[i], stack);

			v2 = avisynth2::AVSValue(&array[0], array.size());
		}
		else if(v1.IsClip())   v2 = new avisynth::CompatibleClip<avisynth1::Traits, avisynth2::Traits>(v1.AsClip());
		else if(v1.IsBool())   v2 = v1.AsBool();
		else if(v1.IsInt())    v2 = v1.AsInt();
		else if(v1.IsFloat())  v2 = v1.AsFloat();
		else if(v1.IsString()) v2 = v1.AsString();

		return v2;
	}

	static void ValidateVideoInfo(avisynth2::VideoInfo& v2, const avisynth1::VideoInfo& v1) {
		memset(&v2, 0, sizeof(v2));

		if(v1.HasVideo()) {
			v2.width = v1.width;
			v2.height = v1.height;
			v2.fps_numerator = v1.fps_numerator;
			v2.fps_denominator = v1.fps_denominator;
			v2.num_frames = v1.num_frames;

			if(v1.IsRGB24())
				v2.pixel_type = avisynth2::VideoInfo::CS_BGR24;
			else if(v1.IsRGB32())
				v2.pixel_type = avisynth2::VideoInfo::CS_BGR32;
			else if(v1.IsYUY2())
				v2.pixel_type = avisynth2::VideoInfo::CS_YUY2;

			v2.SetFieldBased(v1.field_based);
		}

		if(v1.HasAudio()) {
			v2.audio_samples_per_second = v1.audio_samples_per_second;
			v2.sample_type = (v1.sixteen_bit ? avisynth2::SAMPLE_INT16 : avisynth2::SAMPLE_INT8);
			v2.num_audio_samples = v1.num_audio_samples;
			v2.nchannels = (v1.stereo ? 2 : 1);
		}
	}
};

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

template<> struct CompatibleTraits<avisynth2::Traits, avisynth1::Traits> {
	static avisynth1::IScriptEnvironment *ValidateEnvironment(avisynth2::IScriptEnvironment *env)
	{ return CompatibleEnvironment<avisynth1::Traits, avisynth2::Traits>::Validate(env); }

	static avisynth1::AVSValue ValidateAVSValue(const avisynth2::AVSValue& v2, avisynth1::Traits::AVSValueStack& stack) {
		avisynth1::AVSValue v1;

		if(v2.IsArray()) {
			stack.resize(stack.size() + 1);
			avisynth1::Traits::AVSValueArray& array = stack.back();
			array.resize(v2.ArraySize());

			for(int i = 0; i < v2.ArraySize(); ++i)
				array[i] = ValidateAVSValue(v2[i], stack);

			v1 = avisynth1::AVSValue(&array[0], array.size());
		}
		else if(v2.IsClip())   v1 = new avisynth::CompatibleClip<avisynth2::Traits, avisynth1::Traits>(v2.AsClip());
		else if(v2.IsBool())   v1 = v2.AsBool();
		else if(v2.IsInt())    v1 = v2.AsInt();
		else if(v2.IsFloat())  v1 = v2.AsFloat();
		else if(v2.IsString()) v1 = v2.AsString();

		return v1;
	}

	static void ValidateVideoInfo(avisynth1::VideoInfo& v1, const avisynth2::VideoInfo& v2) {
		memset(&v1, 0, sizeof(v1));

		bool video = v2.HasVideo();
		bool audio = v2.HasAudio();

		if(video) {
			if(!v2.IsRGB24() && !v2.IsRGB32() && !v2.IsYUY2())
				video = false;
		}

		if(audio) {
			if(v2.num_audio_samples > std::numeric_limits<int>::max())
				audio = false;

			if(v2.nchannels > 2 || v2.BytesPerChannelSample() > 2)
				audio = false;
		}

		if(video) {
			v1.width = v2.width;
			v1.height = v2.height;
			v1.fps_numerator = v2.fps_numerator;
			v1.fps_denominator = v2.fps_denominator;
			v1.num_frames = v2.num_frames;

			if(v2.IsRGB24())
				v1.pixel_type = avisynth1::VideoInfo::BGR24;
			else if(v2.IsRGB32())
				v1.pixel_type = avisynth1::VideoInfo::BGR32;
			else if(v2.IsYUY2())
				v1.pixel_type = avisynth1::VideoInfo::YUY2;

			v1.field_based = v2.IsFieldBased();
		}

		if(audio) {
			v1.audio_samples_per_second = v2.audio_samples_per_second;
			v1.num_audio_samples = v2.num_audio_samples;
			v1.stereo = (v2.nchannels == 2);
			v1.sixteen_bit = (v2.BytesPerChannelSample() == 2);
		}
	}
};

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

class IScriptEnvironmentPtr {
	union {
		CURRENT_VERSION::IScriptEnvironment *env;
		avisynth1::IScriptEnvironment *env1;
		avisynth2::IScriptEnvironment *env2;
	};

public:
	IScriptEnvironmentPtr() : env(NULL) {}
	IScriptEnvironmentPtr(avisynth1::IScriptEnvironment *p) : env1(p) {}
	IScriptEnvironmentPtr(avisynth2::IScriptEnvironment *p) : env2(p) {}

	void operator=(avisynth1::IScriptEnvironment *p) { env1 = p; }
	void operator=(avisynth2::IScriptEnvironment *p) { env2 = p; }

	operator avisynth1::IScriptEnvironment *() const { return env1; }
	operator avisynth2::IScriptEnvironment *() const { return env2; }

	CURRENT_VERSION::IScriptEnvironment *operator->() const { return env; }
};

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

class EnvironmentObject : public RefCount<EnvironmentObject> {
public:
	virtual ~EnvironmentObject() {}
	virtual bool operator!() const = 0;
	virtual int GetVersion() const = 0;
	virtual IScriptEnvironmentPtr GetInterface() = 0;
	virtual CURRENT_VERSION::IScriptEnvironment *CreateCompatible() = 0;
};

typedef Ref<EnvironmentObject> EnvironmentObjectRef;

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

template<class Traits>
class EnvironmentHolder : public EnvironmentObject {
	typedef typename Traits::IScriptEnvironment IScriptEnvironment;

	IScriptEnvironment *_env;
	std::auto_ptr<IScriptEnvironment> _owner;

public:
	EnvironmentHolder(IScriptEnvironment *env)
		: _env(env) {}

	EnvironmentHolder(std::auto_ptr<IScriptEnvironment> env)
		: _env(env.get()), _owner(env) {}

	bool operator!() const
	{ return _env == NULL; }

	int GetVersion() const
	{ return Traits::AVISYNTH_INTERFACE_VERSION; }

	IScriptEnvironmentPtr GetInterface()
	{ return _env; }

	CURRENT_VERSION::IScriptEnvironment *CreateCompatible()
	{ return CompatibleTraits<Traits, CURRENT_VERSION::Traits>::ValidateEnvironment(_env); }
};

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

namespace avisynth {

class AvisynthDLL;
typedef Ref<AvisynthDLL> AvisynthDLLRef;

class AvisynthDLL : public RefCount<AvisynthDLL> {
	Library _lib;
	void *_func;

public:
	AvisynthDLL(const char *path) : _lib(path), _func(NULL)
	{ if(!!_lib) _func = _lib.GetAddress("CreateScriptEnvironment"); }

	bool operator!() const
	{ return _func == NULL; }

	EnvironmentObjectRef CreateScriptEnvironment() const {
		typedef avisynth2::IScriptEnvironment *(__stdcall *Func2)(int);
		std::auto_ptr<avisynth2::IScriptEnvironment> env2
			(static_cast<Func2>(_func)(avisynth2::AVISYNTH_INTERFACE_VERSION));

		if(env2.get() != NULL)
			return new EnvironmentHolder<avisynth2::Traits>(env2);

		typedef avisynth1::IScriptEnvironment *(__stdcall *Func1)(int);
		std::auto_ptr<avisynth1::IScriptEnvironment> env1
			(static_cast<Func1>(_func)(avisynth1::AVISYNTH_INTERFACE_VERSION));

		if(env1.get() != NULL)
			return new EnvironmentHolder<avisynth1::Traits>(env1);

		return EnvironmentObjectRef();
	}
};

} // namespace avisynth

//////////////////////////////////////////////////////////////////////////////

#endif // _COMPATIBLE_H_
