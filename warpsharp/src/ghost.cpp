#include <vector>
#include <cassert>
#include <fstream>
#include <algorithm>

#include <windows.h>

#include "common/node.h"
#include "common/align.h"
#include "common/refcount.h"

#include "avisynth/avisynth.h"

#include "slicer.h"

#include "ghost.h"

void GhostPluginInit(IScriptEnvironment *env) {
	EraseGhost::PluginInit(env);
	SearchGhost::PluginInit(env);
	EraseGhostV::PluginInit(env);
	SearchGhostV::PluginInit(env);
}
