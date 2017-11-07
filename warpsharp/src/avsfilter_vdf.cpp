#include <windows.h>
#include "VirtualDub/filter.h"

extern "C" __declspec(dllexport)
FilterDefinition * WINAPI InitFilterDef(HINSTANCE inst);

extern "C" __declspec(dllexport)
BOOL WINAPI ExitFilterDef(HINSTANCE inst);

//////////////////////////////////////////////////////////////////////////////

static FilterDefinition *_FilterDefinition = NULL;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch(fdwReason) {
	case DLL_PROCESS_ATTACH:
		if(_FilterDefinition == NULL)
			_FilterDefinition = InitFilterDef(hinstDLL);
		return _FilterDefinition != NULL;

	case DLL_PROCESS_DETACH:
		if(_FilterDefinition != NULL && ExitFilterDef(hinstDLL))
			_FilterDefinition = NULL;
		return _FilterDefinition == NULL;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

static FilterDefinition *_FilterDefinition2 = NULL;

extern "C" __declspec(dllexport)
int __cdecl VirtualdubFilterModuleInit2(FilterModule *fm, const FilterFunctions *ff, int& vdfd_ver, int& vdfd_compat) {
	_FilterDefinition2 = ff->addFilter(fm, _FilterDefinition, sizeof(FilterDefinition));

	if(_FilterDefinition2 == NULL)
		return 1;

	vdfd_ver = VIRTUALDUB_FILTERDEF_VERSION;
	vdfd_compat = VIRTUALDUB_FILTERDEF_COMPATIBLE;

	return 0;
}

extern "C" __declspec(dllexport)
void __cdecl VirtualdubFilterModuleDeinit(FilterModule *fm, const FilterFunctions *ff) {
	ff->removeFilter(_FilterDefinition2);
	_FilterDefinition2 = NULL;
}

//////////////////////////////////////////////////////////////////////////////
