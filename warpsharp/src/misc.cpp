#include <set>
#include <map>
#include <list>
#include <vector>
#include <fstream>
#include <sstream>

#include <activscp.h>
#define __IActiveScriptParse_INTERFACE_DEFINED__

#include <comdef.h>
#include <comutil.h>

#include "common/node.h"
#include "common/thread.h"

#include "platform/vfw.h"
#include "platform/console.h"

#include "avisynth/avisynth.h"

#include "singleton.h"

#include "uvtiming.h"
#include "cache.h"

#define IMPLEMENT
#include "script.h"
#undef IMPLEMENT

void MiscPluginInit(IScriptEnvironment *env) {
	UVTimingH::PluginInit(env);
	UVTimingV::PluginInit(env);

	FrameCache::PluginInit(env);
//	SetCacheHints::PluginInit(env);
	CacheNothing::PluginInit(env);
	CacheRange::PluginInit(env);
	CacheAll::PluginInit(env);

	WScript::PluginInit(env);
	WSResult::PluginInit(env);
	WSInvoke::PluginInit(env);
}
