#include <vector>
#include <list>
#include <map>
#include <set>
#include <sstream>
#include <algorithm>

#include <windows.h>

#include "common/node.h"
#include "common/library.h"

#include "platform/vfw.h"
#include "platform/console.h"

#include "avisynth/avisynth.h"

#include "aviutl/input.h"
#include "aviutl/filter.h"

#include "singleton.h"
#include "aviutl.h"
#include "loadaui.h"
#include "loadauf.h"
#include "loadauf2.h"
//#include "loadaufMT.h"

void AviUtlPluginInit(IScriptEnvironment *env) {
	LoadAviUtlInputPlugin::PluginInit(env);
	LoadAviUtlFilterPlugin::PluginInit(env);
	LoadAviUtlFilterPlugin2::PluginInit(env);
//	LoadAviUtlFilterPluginMT::PluginInit(env);
	ConvertYUY2ToAviUtlYC::PluginInit(env);
	ConvertAviUtlYCToYUY2::PluginInit(env);
}
