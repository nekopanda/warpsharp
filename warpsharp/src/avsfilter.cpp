#include <sstream>
#include <iterator>

#include <windows.h>
#include <windowsx.h>

#include "aviutl/filter.h"
#include "VirtualDub/filter.h"
#include "VirtualDub/ScriptValue.h"

#include "platform/DialogTemplate.h"

#include "common/thread.h"
#include "avisynth/compatible.h"

namespace CURRENT_VERSION {

#include "singleton.h"
#include "aviutl.h"
#include "plugin.h"

#define IMPLEMENT
#include "avsfilter.h"
#undef IMPLEMENT

} // namespace CURRENT_VERSION

//////////////////////////////////////////////////////////////////////////////

static avisynth::AvisynthDLL _AvisynthDLL("avisynth.dll");

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch(fdwReason) {
	case DLL_PROCESS_ATTACH:
		return !!_AvisynthDLL;

	case DLL_PROCESS_DETACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

typedef std::map<HINSTANCE, avisynth::EnvironmentObjectRef> EnvironmentMap;
static EnvironmentMap _EnvironmentMap;

template<class FILTER, class BUILDER>
static FILTER *PluginInit(HINSTANCE inst) {
	using namespace CURRENT_VERSION;

	EnvironmentMap::iterator it = _EnvironmentMap.find(inst);
	if(it != _EnvironmentMap.end()) return NULL;

	avisynth::EnvironmentObjectRef holder(_AvisynthDLL.CreateScriptEnvironment());
	if(!holder) return NULL;

	CURRENT_VERSION::IScriptEnvironment *env = holder->CreateCompatible();
	if(env == NULL) return NULL;

	LoadPlugin::PluginInit(env);
	BUILDER::PluginInit(env);
	AddFunction::PluginInit(env);
	AddValue::PluginInit(env);
	AddTrack::PluginInit(env);
	AddArray::PluginInit(env);
	AddCheck::PluginInit(env);

	const char ext[] = ".avs";
	char path[MAX_PATH + sizeof(ext)];
	GetModuleFileName(inst, path, MAX_PATH);
	strcat_s(path, ext);

	FILTER *filter = NULL;

	try {
		AVSValue args[] = { path };
		AVSValue result(env->Invoke("Import", AVSValue(args, sizeof(args) / sizeof(*args))));
		filter = FilterObject::FromAVS<BUILDER>(result, path, env)->Freeze();
		_EnvironmentMap[inst] = holder;
	}
	catch(::AvisynthError err) {
		MessageBox(NULL, err.msg, NULL, MB_OK);
	}

	return filter;
}

//////////////////////////////////////////////////////////////////////////////

static CriticalSection _CriticalSection;

extern "C" __declspec(dllexport)
FILTER_DLL * WINAPI InitFilterDLL(HINSTANCE inst) {
	CriticalLock lock(_CriticalSection);
	return PluginInit<FILTER_DLL, CURRENT_VERSION::AvisynthFiltersForAviUtl>(inst);
}

extern "C" __declspec(dllexport)
BOOL WINAPI ExitFilterDLL(HINSTANCE inst) {
	CriticalLock lock(_CriticalSection);
	return !!_EnvironmentMap.erase(inst);
}

extern "C" __declspec(dllexport)
FilterDefinition * WINAPI InitFilterDef(HINSTANCE inst) {
	CriticalLock lock(_CriticalSection);
	return PluginInit<FilterDefinition, CURRENT_VERSION::AvisynthFiltersForVirtualDub>(inst);
}

extern "C" __declspec(dllexport)
BOOL WINAPI ExitFilterDef(HINSTANCE inst) {
	CriticalLock lock(_CriticalSection);
	return !!_EnvironmentMap.erase(inst);
}

//////////////////////////////////////////////////////////////////////////////
