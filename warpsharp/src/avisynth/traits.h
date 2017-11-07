#ifndef _AVISYNTH_H_
#define _AVISYNTH_H_

#include <vector>
#include <list>
#include <crtdbg.h>

//////////////////////////////////////////////////////////////////////////////

namespace avisynth1 {
#include "1/avisynth.h"
} // namespace avisynth1

#undef __AVISYNTH_H__
#undef FRAME_ALIGN

namespace avisynth2 {
#include "2/avisynth.h"
} // namespace avisynth2

#ifndef CURRENT_VERSION
#define CURRENT_VERSION avisynth2
#endif

//////////////////////////////////////////////////////////////////////////////

class AvisynthError {
public:
	const char* const msg;
	AvisynthError(const char* _msg) : msg(_msg) {}
};

class IScriptEnvironment {
public:
	class NotFound {};
};

//////////////////////////////////////////////////////////////////////////////

namespace avisynth1 {

struct Traits {
	enum { AVISYNTH_INTERFACE_VERSION = AVISYNTH_INTERFACE_VERSION };

	typedef IClip IClip;
	typedef IScriptEnvironment IScriptEnvironment;

	typedef PClip PClip;
	typedef PVideoFrame PVideoFrame;

	typedef VideoInfo VideoInfo;
	typedef VideoFrame VideoFrame;

	typedef AVSValue AVSValue;
	typedef std::vector<AVSValue> AVSValueArray;
	typedef std::list<AVSValueArray> AVSValueStack;
};
} // namespace avisynth1

//////////////////////////////////////////////////////////////////////////////

namespace avisynth2 {

struct Traits {
	enum { AVISYNTH_INTERFACE_VERSION = AVISYNTH_INTERFACE_VERSION };

	typedef IClip IClip;
	typedef IScriptEnvironment IScriptEnvironment;

	typedef PClip PClip;
	typedef PVideoFrame PVideoFrame;

	typedef VideoInfo VideoInfo;
	typedef VideoFrame VideoFrame;

	typedef AVSValue AVSValue;
	typedef std::vector<AVSValue> AVSValueArray;
	typedef std::list<AVSValueArray> AVSValueStack;
};

} // namespace avisynth2

//////////////////////////////////////////////////////////////////////////////

#endif // _AVISYNTH_H_
