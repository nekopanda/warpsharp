#include <vector>
#include <cmath>
#include <cassert>
#include <algorithm>

#include <windows.h>

#include "common/node.h"
#include "common/align.h"
#include "common/refcount.h"

#include "avisynth/avisynth.h"

#include "slicer.h"

#define USE_NATIVE 1

#include "warpsharp.h"
#include "unsharp.h"
#include "xsharpen.h"
#include "kenkun.h"

void FocusPluginInit(IScriptEnvironment *env) {
	WarpSharpBump::PluginInit(env);
	WarpSharpBlur::PluginInit(env);
	WarpSharp::PluginInit(env);
	UnsharpMask::PluginInit(env);
	Xsharpen::PluginInit(env);
	KenKunNRT::PluginInit(env);
	KenKunNR::PluginInit(env);
}
