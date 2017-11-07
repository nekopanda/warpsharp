#include <vector>
#include <iostream>
#include <cassert>

#include <windows.h>

#include "common/node.h"
#include "common/align.h"
#include "common/refcount.h"

#include "platform/console.h"

#include "avisynth/avisynth.h"

#include "deint.h"

void DeintPluginInit(IScriptEnvironment *env) {
	Auto24FPS::PluginInit(env);
	AutoDeint::PluginInit(env);
}
