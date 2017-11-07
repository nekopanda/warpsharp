#ifndef _VFW_H_
#define _VFW_H_

#include <vfw.h>
#include <comdef.h>

//////////////////////////////////////////////////////////////////////////////

class  __declspec(uuid("E6D6B700-124D-11D4-86F3-DB80AFD98778")) AVIFileSynth;
struct __declspec(uuid("00020020-0000-0000-C000-000000000046")) IAVIFile;
struct __declspec(uuid("00020021-0000-0000-C000-000000000046")) IAVIStream;
struct __declspec(uuid("E6D6B708-124D-11D4-86F3-DB80AFD98778")) IAvisynthClipInfo;

_COM_SMARTPTR_TYPEDEF(IAVIFile,          __uuidof(IAVIFile));
_COM_SMARTPTR_TYPEDEF(IAVIStream,        __uuidof(IAVIStream));
_COM_SMARTPTR_TYPEDEF(IAvisynthClipInfo, __uuidof(IAvisynthClipInfo));

class AVIFileInitializer {
public:
	AVIFileInitializer()
	{ AVIFileInit(); }

	~AVIFileInitializer()
	{ AVIFileExit(); }
};

class CoInitializer {
public:
	CoInitializer()
	{ CoInitialize(NULL); }

	~CoInitializer()
	{ CoUninitialize(); }
};

//////////////////////////////////////////////////////////////////////////////

class CompressVars : public COMPVARS {
	void Init() {
		memset(static_cast<COMPVARS *>(this), 0, sizeof(COMPVARS));
		cbSize = sizeof(COMPVARS);
	}

	void Exit() {
		if(dwFlags & ICMF_COMPVARS_VALID) {
			ICSeqCompressFrameEnd(this);
			ICCompressorFree(this);
		}
	}

public:
	CompressVars()
	{ Init(); }

	~CompressVars()
	{ Exit(); }

	void Release()
	{ Exit(); Init(); }
};

//////////////////////////////////////////////////////////////////////////////

#endif // _VFW_H_
