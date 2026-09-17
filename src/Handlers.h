#pragma once
#include <windows.h>
#include <thumbcache.h>
#include <shobjidl.h>
#include <objidl.h>
#include "Globals.h"

class ThumbnailProvider final : public IThumbnailProvider, public IInitializeWithStream {
public:
    ThumbnailProvider();
    ~ThumbnailProvider();
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP Initialize(IStream* pstream, DWORD grfMode) override;
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha) override;
private:
    long refs_{1};
    IStream* stream_{};
};

class PreviewHandler final : public IPreviewHandler,
                             public IInitializeWithStream,
                             public IPreviewHandlerVisuals,
                             public IOleWindow,
                             public IObjectWithSite {
public:
    PreviewHandler();
    ~PreviewHandler();

    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;

    IFACEMETHODIMP Initialize(IStream* pstream, DWORD grfMode) override;

    IFACEMETHODIMP SetWindow(HWND hwnd, const RECT* prc) override;
    IFACEMETHODIMP SetRect(const RECT* prc) override;
    IFACEMETHODIMP DoPreview() override;
    IFACEMETHODIMP Unload() override;
    IFACEMETHODIMP SetFocus() override;
    IFACEMETHODIMP QueryFocus(HWND* phwnd) override;
    IFACEMETHODIMP TranslateAccelerator(MSG* pmsg) override;
    IFACEMETHODIMP SetBackgroundColor(COLORREF color) override;
    IFACEMETHODIMP SetFont(const LOGFONTW* plf) override;
    IFACEMETHODIMP SetTextColor(COLORREF color) override;

    IFACEMETHODIMP GetWindow(HWND* phwnd) override;
    IFACEMETHODIMP ContextSensitiveHelp(BOOL fEnterMode) override;

    IFACEMETHODIMP SetSite(IUnknown* punkSite) override;
    IFACEMETHODIMP GetSite(REFIID riid, void** ppvSite) override;

private:
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    static ATOM EnsureWindowClass();
    HRESULT RenderBitmap();
    void Paint(HDC hdc);
    void DestroyPreviewWindow();

    long refs_{1};
    IStream* stream_{};
    IUnknown* site_{};
    HWND parent_{};
    HWND child_{};
    RECT rect_{};
    HBITMAP bitmap_{};
    COLORREF background_{RGB(32,32,32)};
    COLORREF textColor_{RGB(230,230,230)};
    LOGFONTW font_{};
    bool previewed_{};
};

class ClassFactory final : public IClassFactory {
public:
    explicit ClassFactory(bool preview);
    ~ClassFactory();
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP CreateInstance(IUnknown* outer, REFIID riid, void** ppv) override;
    IFACEMETHODIMP LockServer(BOOL lock) override;
private:
    long refs_{1};
    bool preview_{};
};
