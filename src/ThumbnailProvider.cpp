#include "Handlers.h"
#include "ComStreamAdapter.h"
#include "WicUtil.h"
#include <vector>
#include <string>

ThumbnailProvider::ThumbnailProvider() { DllAddRef(); }
ThumbnailProvider::~ThumbnailProvider() {
    if (stream_) stream_->Release();
    DllRelease();
}

HRESULT ThumbnailProvider::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_IThumbnailProvider) *ppv = static_cast<IThumbnailProvider*>(this);
    else if (riid == IID_IInitializeWithStream) *ppv = static_cast<IInitializeWithStream*>(this);
    else return E_NOINTERFACE;
    AddRef(); return S_OK;
}
ULONG ThumbnailProvider::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
ULONG ThumbnailProvider::Release() {
    ULONG r = static_cast<ULONG>(InterlockedDecrement(&refs_));
    if (!r) delete this;
    return r;
}

HRESULT ThumbnailProvider::Initialize(IStream* pstream, DWORD) {
    if (!pstream) return E_INVALIDARG;
    if (stream_) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    stream_ = pstream; stream_->AddRef();
    return S_OK;
}

HRESULT ThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) {
    if (!phbmp || !pdwAlpha) return E_POINTER;
    *phbmp = nullptr; *pdwAlpha = WTSAT_UNKNOWN;
    if (!stream_) return E_UNEXPECTED;
    cx = cx ? cx : 256;
    cx = (cx > 4096) ? 4096 : cx;

    ComStreamAdapter adapter(stream_);
    if (!adapter.valid()) return E_FAIL;

    std::vector<uint8_t> jpeg;
    uint32_t jw=0,jh=0;
    std::string detail;
    auto st = psdx::ExtractEmbeddedJpeg(adapter, jpeg, jw, jh, &detail);
    if (st == psdx::DecodeStatus::Ok && !jpeg.empty()) {
        HRESULT hr = DecodeJpegToHBitmap(jpeg, cx, phbmp);
        if (SUCCEEDED(hr)) {
            *pdwAlpha = WTSAT_RGB;
            return S_OK;
        }
    }

    if (!adapter.seek(0)) return E_FAIL;
    psdx::ImageBGRA image;
    st = psdx::DecodeCompositeThumbnail(adapter, cx, image, &detail);
    if (st != psdx::DecodeStatus::Ok) return E_FAIL;
    HRESULT hr = ImageToHBitmap(image, phbmp);
    if (SUCCEEDED(hr)) *pdwAlpha = image.hasAlpha ? WTSAT_ARGB : WTSAT_RGB;
    return hr;
}
