#include <windows.h>

#include "avisynth/avisynth.h"

#define IMPLEMENT
#include "platform/console.h"
#undef IMPLEMENT

extern void FocusPluginInit(IScriptEnvironment *env);
extern void GhostPluginInit(IScriptEnvironment *env);
extern void DeintPluginInit(IScriptEnvironment *env);
extern void AviUtlPluginInit(IScriptEnvironment *env);
extern void MiscPluginInit(IScriptEnvironment *env);

extern "C" __declspec(dllexport)
const char * __stdcall AvisynthPluginInit2(IScriptEnvironment *env) {
	FocusPluginInit(env);
	GhostPluginInit(env);
	DeintPluginInit(env);
	AviUtlPluginInit(env);
	MiscPluginInit(env);
	return NULL;
}
