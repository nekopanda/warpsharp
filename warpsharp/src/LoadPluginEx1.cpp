#include <sstream>
#include <iterator>

#include <windows.h>

#define CURRENT_VERSION avisynth1
#include "avisynth/compatible.h"

namespace CURRENT_VERSION {

#include "singleton.h"
#include "plugin.h"

} // namespace CURRENT_VERSION

extern "C" __declspec(dllexport)
const char * __stdcall AvisynthPluginInit(avisynth1::IScriptEnvironment *env) {
	CURRENT_VERSION::LoadPlugin::PluginInit(avisynth::CompatibleTraits
		<avisynth1::Traits, CURRENT_VERSION::Traits>::ValidateEnvironment(env));
	return NULL;
}
