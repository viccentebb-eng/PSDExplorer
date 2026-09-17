#include "WicUtil.h"
#include <wincodec.h>
#include <objidl.h>
#include <algorithm>
#include <vector>
#include <cmath>

namespace {

template<class T> void SafeRelease(T*& p) { if (p) { p->Release(); p = nullptr; } }

HRESULT PixelsToDib(const uint8_t* pixels, UINT w, UINT h, UINT stride, HBITMAP* bitmap) {
    if (!pixels || !w || !h || !bitmap) return E_INVALIDARG;
    *bitmap = nullptr;
    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = static_cast<LONG>(w);
    bmi.bmiHeader.biHeight = -static_cast<LONG>(h); // top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    HBITMAP hb = CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!hb || !bits) return HRESULT_FROM_WIN32(GetLastError());
    for (UINT y = 0; y < h; ++y) {
        memcpy(static_cast<uint8_t*>(bits) + size_t(y) * w * 4,
               pixels + size_t(y) * stride, size_t(w) * 4);
    }
    *bitmap = hb;
    return S_OK;
}

}

HRESULT ImageToHBitmap(const psdx::ImageBGRA& image, HBITMAP* bitmap) {
    if (image.pixels.size() < size_t(image.width) * image.height * 4) return E_INVALIDARG;
    return PixelsToDib(image.pixels.data(), image.width, image.height, image.width * 4, bitmap);
}

HRESULT DecodeJpegToHBitmap(const std::vector<uint8_t>& jpeg, UINT maxEdge, HBITMAP* bitmap,
                            UINT* outWidth, UINT* outHeight) {
    if (bitmap) *bitmap = nullptr;
    if (!bitmap || jpeg.empty()) return E_INVALIDARG;
    if (maxEdge == 0) maxEdge = 256;
    maxEdge = std::min<UINT>(maxEdge, 4096);

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    bool uninit = SUCCEEDED(hr);
    if (hr == RPC_E_CHANGED_MODE) hr = S_OK;
    if (FAILED(hr)) return hr;

    IWICImagingFactory* factory = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapSource* source = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* converter = nullptr;
    IStream* mem = nullptr;
    HGLOBAL hg = nullptr;

    do {
        hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(&factory));
        if (FAILED(hr)) break;

        hg = GlobalAlloc(GMEM_MOVEABLE, jpeg.size());
        if (!hg) { hr = E_OUTOFMEMORY; break; }
        void* p = GlobalLock(hg);
        if (!p) { hr = HRESULT_FROM_WIN32(GetLastError()); break; }
        memcpy(p, jpeg.data(), jpeg.size());
        GlobalUnlock(hg);
        hr = CreateStreamOnHGlobal(hg, TRUE, &mem); // stream owns hg
        if (FAILED(hr)) break;
        hg = nullptr;

        hr = factory->CreateDecoderFromStream(mem, nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
        if (FAILED(hr)) break;
        hr = decoder->GetFrame(0, &frame);
        if (FAILED(hr)) break;

        UINT sw=0, sh=0;
        hr = frame->GetSize(&sw,&sh);
        if (FAILED(hr) || !sw || !sh) { if (SUCCEEDED(hr)) hr = E_FAIL; break; }
        UINT dw=sw, dh=sh;
        if (std::max(sw,sh) > maxEdge) {
            double scale = double(maxEdge) / double(std::max(sw,sh));
            dw = std::max<UINT>(1, static_cast<UINT>(std::lround(sw*scale)));
            dh = std::max<UINT>(1, static_cast<UINT>(std::lround(sh*scale)));
            hr = factory->CreateBitmapScaler(&scaler);
            if (FAILED(hr)) break;
            hr = scaler->Initialize(frame, dw, dh, WICBitmapInterpolationModeFant);
            if (FAILED(hr)) break;
            source = scaler; source->AddRef();
        } else {
            source = frame; source->AddRef();
        }

        hr = factory->CreateFormatConverter(&converter);
        if (FAILED(hr)) break;
        hr = converter->Initialize(source, GUID_WICPixelFormat32bppBGRA,
                                   WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom);
        if (FAILED(hr)) break;
        const UINT stride = dw * 4;
        std::vector<uint8_t> pixels(size_t(stride) * dh);
        hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
        if (FAILED(hr)) break;
        hr = PixelsToDib(pixels.data(), dw, dh, stride, bitmap);
        if (SUCCEEDED(hr)) {
            if (outWidth) *outWidth = dw;
            if (outHeight) *outHeight = dh;
        }
    } while(false);

    SafeRelease(converter); SafeRelease(source); SafeRelease(scaler); SafeRelease(frame);
    SafeRelease(decoder); SafeRelease(mem); SafeRelease(factory);
    if (hg) GlobalFree(hg);
    if (uninit) CoUninitialize();
    return hr;
}
