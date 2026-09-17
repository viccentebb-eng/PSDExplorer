#include "Handlers.h"
#include "ComStreamAdapter.h"
#include "WicUtil.h"
#include <algorithm>
#include <string>
#include <vector>
#include <cmath>

PreviewHandler::PreviewHandler() {
    DllAddRef();
    rect_ = RECT{0,0,0,0};
    ZeroMemory(&font_, sizeof(font_));
    font_.lfHeight = -16;
    wcscpy_s(font_.lfFaceName, LF_FACESIZE, L"Segoe UI");
}

PreviewHandler::~PreviewHandler() {
    Unload();
    if (site_) site_->Release();
    DllRelease();
}

HRESULT PreviewHandler::QueryInterface(REFIID riid, void** ppv) {
    if (!ppv) return E_POINTER;
    *ppv = nullptr;
    if (riid == IID_IUnknown || riid == IID_IPreviewHandler) *ppv = static_cast<IPreviewHandler*>(this);
    else if (riid == IID_IInitializeWithStream) *ppv = static_cast<IInitializeWithStream*>(this);
    else if (riid == IID_IPreviewHandlerVisuals) *ppv = static_cast<IPreviewHandlerVisuals*>(this);
    else if (riid == IID_IOleWindow) *ppv = static_cast<IOleWindow*>(this);
    else if (riid == IID_IObjectWithSite) *ppv = static_cast<IObjectWithSite*>(this);
    else return E_NOINTERFACE;
    AddRef(); return S_OK;
}
ULONG PreviewHandler::AddRef() { return static_cast<ULONG>(InterlockedIncrement(&refs_)); }
ULONG PreviewHandler::Release() {
    ULONG r = static_cast<ULONG>(InterlockedDecrement(&refs_));
    if (!r) delete this;
    return r;
}

HRESULT PreviewHandler::Initialize(IStream* pstream, DWORD) {
    if (!pstream) return E_INVALIDARG;
    if (stream_) return HRESULT_FROM_WIN32(ERROR_ALREADY_INITIALIZED);
    stream_ = pstream; stream_->AddRef();
    return S_OK;
}

HRESULT PreviewHandler::SetWindow(HWND hwnd, const RECT* prc) {
    if (!hwnd || !prc) return E_INVALIDARG;
    parent_ = hwnd; rect_ = *prc;
    if (child_) {
        SetParent(child_, parent_);
        SetWindowPos(child_, nullptr, rect_.left, rect_.top,
                     rect_.right-rect_.left, rect_.bottom-rect_.top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
    }
    return S_OK;
}

HRESULT PreviewHandler::SetRect(const RECT* prc) {
    if (!prc) return E_INVALIDARG;
    rect_ = *prc;
    if (child_) SetWindowPos(child_, nullptr, rect_.left, rect_.top,
                             rect_.right-rect_.left, rect_.bottom-rect_.top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
    return S_OK;
}

ATOM PreviewHandler::EnsureWindowClass() {
    static ATOM atom = 0;
    if (atom) return atom;
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = &PreviewHandler::WndProc;
    wc.hInstance = g_module;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.lpszClassName = L"PSDExplorerPreviewWindow";
    atom = RegisterClassExW(&wc);
    if (!atom && GetLastError() == ERROR_CLASS_ALREADY_EXISTS) atom = 1;
    return atom;
}

LRESULT CALLBACK PreviewHandler::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    PreviewHandler* self = reinterpret_cast<PreviewHandler*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<PreviewHandler*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    if (self) {
        switch (msg) {
            case WM_PAINT: {
                PAINTSTRUCT ps{}; HDC dc = BeginPaint(hwnd, &ps); self->Paint(dc); EndPaint(hwnd, &ps); return 0;
            }
            case WM_ERASEBKGND: return 1;
            case WM_SETFOCUS: return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HRESULT PreviewHandler::RenderBitmap() {
    if (bitmap_) { DeleteObject(bitmap_); bitmap_ = nullptr; }
    if (!stream_) return E_UNEXPECTED;
    UINT w = static_cast<UINT>(std::max<LONG>(1, rect_.right - rect_.left));
    UINT h = static_cast<UINT>(std::max<LONG>(1, rect_.bottom - rect_.top));
    UINT edge = std::min<UINT>(4096, std::max(w, h) * 2u);
    edge = std::max<UINT>(edge, 512);

    ComStreamAdapter adapter(stream_);
    if (!adapter.valid()) return E_FAIL;
    std::vector<uint8_t> jpeg;
    uint32_t jw=0,jh=0; std::string detail;
    auto st = psdx::ExtractEmbeddedJpeg(adapter, jpeg, jw, jh, &detail);
    if (st == psdx::DecodeStatus::Ok && !jpeg.empty()) {
        HRESULT hr = DecodeJpegToHBitmap(jpeg, edge, &bitmap_);
        if (SUCCEEDED(hr)) return hr;
    }
    if (!adapter.seek(0)) return E_FAIL;
    psdx::ImageBGRA image;
    st = psdx::DecodeCompositeThumbnail(adapter, edge, image, &detail);
    if (st != psdx::DecodeStatus::Ok) return E_FAIL;
    return ImageToHBitmap(image, &bitmap_);
}

HRESULT PreviewHandler::DoPreview() {
    if (previewed_) return S_OK;
    if (!parent_ || !stream_) return E_UNEXPECTED;
    if (!EnsureWindowClass()) return HRESULT_FROM_WIN32(GetLastError());
    child_ = CreateWindowExW(0, L"PSDExplorerPreviewWindow", L"", WS_CHILD | WS_VISIBLE,
                             rect_.left, rect_.top, rect_.right-rect_.left, rect_.bottom-rect_.top,
                             parent_, nullptr, g_module, this);
    if (!child_) return HRESULT_FROM_WIN32(GetLastError());
    HRESULT hr = RenderBitmap();
    previewed_ = true;
    InvalidateRect(child_, nullptr, TRUE);
    return SUCCEEDED(hr) ? S_OK : hr;
}

void PreviewHandler::DestroyPreviewWindow() {
    if (child_) { DestroyWindow(child_); child_ = nullptr; }
}

HRESULT PreviewHandler::Unload() {
    DestroyPreviewWindow();
    if (bitmap_) { DeleteObject(bitmap_); bitmap_ = nullptr; }
    if (stream_) { stream_->Release(); stream_ = nullptr; }
    previewed_ = false;
    return S_OK;
}

HRESULT PreviewHandler::SetFocus() {
    if (!child_) return S_FALSE;
    ::SetFocus(child_); return S_OK;
}
HRESULT PreviewHandler::QueryFocus(HWND* phwnd) {
    if (!phwnd) return E_POINTER;
    *phwnd = GetFocus(); return S_OK;
}
HRESULT PreviewHandler::TranslateAccelerator(MSG* pmsg) {
    if (!pmsg) return E_POINTER;
    if (site_) {
        IPreviewHandlerFrame* frame = nullptr;
        if (SUCCEEDED(site_->QueryInterface(IID_PPV_ARGS(&frame)))) {
            HRESULT hr = frame->TranslateAccelerator(pmsg);
            frame->Release();
            return hr;
        }
    }
    return S_FALSE;
}
HRESULT PreviewHandler::SetBackgroundColor(COLORREF color) { background_ = color; if(child_) InvalidateRect(child_,nullptr,TRUE); return S_OK; }
HRESULT PreviewHandler::SetFont(const LOGFONTW* plf) { if(!plf) return E_POINTER; font_ = *plf; return S_OK; }
HRESULT PreviewHandler::SetTextColor(COLORREF color) { textColor_ = color; if(child_) InvalidateRect(child_,nullptr,TRUE); return S_OK; }

HRESULT PreviewHandler::GetWindow(HWND* phwnd) { if(!phwnd) return E_POINTER; *phwnd = child_ ? child_ : parent_; return *phwnd ? S_OK : E_FAIL; }
HRESULT PreviewHandler::ContextSensitiveHelp(BOOL) { return E_NOTIMPL; }

HRESULT PreviewHandler::SetSite(IUnknown* punkSite) {
    if (site_) { site_->Release(); site_ = nullptr; }
    if (punkSite) { site_ = punkSite; site_->AddRef(); }
    return S_OK;
}
HRESULT PreviewHandler::GetSite(REFIID riid, void** ppvSite) {
    if (!ppvSite) return E_POINTER;
    *ppvSite = nullptr;
    return site_ ? site_->QueryInterface(riid, ppvSite) : E_FAIL;
}

void PreviewHandler::Paint(HDC hdc) {
    RECT client{}; GetClientRect(child_, &client);
    HBRUSH brush = CreateSolidBrush(background_);
    FillRect(hdc, &client, brush); DeleteObject(brush);
    if (!bitmap_) {
        SetBkMode(hdc, TRANSPARENT); ::SetTextColor(hdc, textColor_);
        HFONT font = CreateFontIndirectW(&font_); HFONT old = static_cast<HFONT>(SelectObject(hdc,font));
        DrawTextW(hdc, L"No se pudo generar la vista previa de este PSD/PSB.", -1, &client,
                  DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        SelectObject(hdc,old); DeleteObject(font);
        return;
    }
    BITMAP bm{}; GetObject(bitmap_, sizeof(bm), &bm);
    if (bm.bmWidth <= 0 || bm.bmHeight <= 0) return;
    int cw = client.right-client.left, ch=client.bottom-client.top;
    double scale = std::min(double(cw)/bm.bmWidth, double(ch)/bm.bmHeight);
    int dw = std::max(1, int(std::lround(bm.bmWidth*scale)));
    int dh = std::max(1, int(std::lround(bm.bmHeight*scale)));
    int x = (cw-dw)/2, y=(ch-dh)/2;
    HDC mem = CreateCompatibleDC(hdc); HGDIOBJ old = SelectObject(mem,bitmap_);
    SetStretchBltMode(hdc, HALFTONE);
    StretchBlt(hdc,x,y,dw,dh,mem,0,0,bm.bmWidth,bm.bmHeight,SRCCOPY);
    SelectObject(mem,old); DeleteDC(mem);
}
