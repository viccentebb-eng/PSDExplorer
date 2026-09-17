#include "Handlers.h"
#include <new>

ClassFactory::ClassFactory(bool preview) : preview_(preview) { DllAddRef(); }
ClassFactory::~ClassFactory() { DllRelease(); }

HRESULT ClassFactory::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_IClassFactory) *ppv = static_cast<IClassFactory*>(this);
    else return E_NOINTERFACE;
    AddRef(); return S_OK;
}
ULONG ClassFactory::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
ULONG ClassFactory::Release() {
    ULONG r = static_cast<ULONG>(InterlockedDecrement(&refs_));
    if (!r) delete this;
    return r;
}
HRESULT ClassFactory::CreateInstance(IUnknown* outer, REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (outer) return CLASS_E_NOAGGREGATION;
    IUnknown* obj = preview_ ? static_cast<IUnknown*>(static_cast<IPreviewHandler*>(new (std::nothrow) PreviewHandler()))
                             : static_cast<IUnknown*>(static_cast<IThumbnailProvider*>(new (std::nothrow) ThumbnailProvider()));
    if (!obj) return E_OUTOFMEMORY;
    HRESULT hr = obj->QueryInterface(riid, ppv);
    obj->Release();
    return hr;
}
HRESULT ClassFactory::LockServer(BOOL lock) {
    if (lock) DllAddRef(); else DllRelease();
    return S_OK;
}
