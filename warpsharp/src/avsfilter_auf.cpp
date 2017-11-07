#include <windows.h>
#include "aviutl/filter.h"

extern "C" __declspec(dllimport)
FILTER_DLL * WINAPI InitFilterDLL(HINSTANCE inst);

extern "C" __declspec(dllimport)
BOOL WINAPI ExitFilterDLL(HINSTANCE inst);

//////////////////////////////////////////////////////////////////////////////

static FILTER_DLL *_FilterDLL = NULL;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	switch(fdwReason) {
	case DLL_PROCESS_ATTACH:
		if(_FilterDLL == NULL)
			_FilterDLL = InitFilterDLL(hinstDLL);
		return _FilterDLL != NULL;

	case DLL_PROCESS_DETACH:
		if(_FilterDLL != NULL && ExitFilterDLL(hinstDLL))
			_FilterDLL = NULL;
		return _FilterDLL == NULL;

	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
		break;
	}

	return TRUE;
}

extern "C" __declspec(dllexport)
FILTER_DLL * __cdecl GetFilterTable()
{ return _FilterDLL; }

//////////////////////////////////////////////////////////////////////////////
