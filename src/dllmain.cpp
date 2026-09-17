#include <windows.h>
#include "Globals.h"
#include "Guids.h"
#include "Handlers.h"
#include <new>

HMODULE g_module = nullptr;
volatile long g_dllRef = 0;

BOOL WINAPI DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        g_module = hinst;
        DisableThreadLibraryCalls(hinst);
    }
    return TRUE;
}

// Use the Windows SDK COM signatures exactly. The symbols are exported through
// PSDExplorerShell.def so we do not redeclare the SDK functions with a
// conflicting dllexport linkage on newer MSVC/Windows SDK versions.
STDAPI DllGetClassObject(REFCLSID clsid, REFIID riid, LPVOID* ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;

    bool preview = false;
    if (IsEqualCLSID(clsid, CLSID_PSDExplorerThumbnail)) {
        preview = false;
    } else if (IsEqualCLSID(clsid, CLSID_PSDExplorerPreview)) {
        preview = true;
    } else {
        return CLASS_E_CLASSNOTAVAILABLE;
    }

    auto* factory = new (std::nothrow) ClassFactory(preview);
    if (!factory) return E_OUTOFMEMORY;

    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllCanUnloadNow(void) {
    return g_dllRef == 0 ? S_OK : S_FALSE;
}
