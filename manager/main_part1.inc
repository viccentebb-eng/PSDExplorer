#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <thumbcache.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <uxtheme.h>
#include <string>
#include <vector>
#include <filesystem>
#include <thread>
#include <atomic>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cwctype>
#include <iterator>
#include <climits>
#include "Guids.h"
#include "PsdCore.h"

#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace fs = std::filesystem;

namespace {
constexpr wchar_t kAppName[] = L"PSD Explorer 1.1.2";
constexpr wchar_t kVersion[] = L"1.1.2";

constexpr int ID_TAB = 100;
constexpr int ID_INSTALL = 1001;
constexpr int ID_UNINSTALL = 1002;
constexpr int ID_RESTART = 1003;
constexpr int ID_HOME_STATUS = 1004;
constexpr int ID_VERIFY = 1005;

constexpr int ID_FOLDER_EDIT = 2001;
constexpr int ID_FOLDER_PICK = 2002;
constexpr int ID_RECURSIVE = 2003;
constexpr int ID_SIZE_COMBO = 2004;
constexpr int ID_GENERATE = 2005;
constexpr int ID_CANCEL = 2006;
constexpr int ID_PROGRESS = 2007;
constexpr int ID_PROGRESS_TEXT = 2008;
constexpr int ID_LOG = 2009;

constexpr int ID_CACHE_INFO = 3001;
constexpr int ID_CACHE_REFRESH = 3002;
constexpr int ID_CACHE_CLEAR = 3003;

constexpr int ID_FILE_EDIT = 4001;
constexpr int ID_FILE_PICK = 4002;
constexpr int ID_INSPECT = 4003;
constexpr int ID_TEST_THUMB = 4004;
constexpr int ID_INSPECT_TEXT = 4005;

constexpr UINT WM_APP_TOTAL = WM_APP + 10;
constexpr UINT WM_APP_PROGRESS_ITEM = WM_APP + 11;
constexpr UINT WM_APP_DONE = WM_APP + 12;
constexpr UINT WM_TRAYICON = WM_APP + 30;
constexpr UINT TRAY_UID = 1;

HWND g_main{};
HWND g_tab{};
HWND g_pages[4]{};
HWND g_homeStatus{};
HWND g_folderEdit{};
HWND g_recursive{};
HWND g_sizeCombo{};
HWND g_generate{};
HWND g_cancelBtn{};
HWND g_progress{};
HWND g_progressText{};
HWND g_log{};
HWND g_cacheInfo{};
HWND g_fileEdit{};
HWND g_inspectText{};
HFONT g_font{};
HFONT g_fontBold{};
std::thread g_worker;
std::atomic_bool g_cancel{false};
std::atomic_bool g_working{false};
size_t g_totalFiles{};
size_t g_failedFiles{};
NOTIFYICONDATAW g_tray{};


void SetTrayState(const std::wstring& text, bool notify = false) {
    if (!g_main) return;
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_main;
    g_tray.uID = TRAY_UID;
    g_tray.uFlags = NIF_TIP;
    wcsncpy_s(g_tray.szTip, text.c_str(), _TRUNCATE);
    Shell_NotifyIconW(NIM_MODIFY, &g_tray);
    if (notify) {
        g_tray.uFlags = NIF_INFO;
        wcsncpy_s(g_tray.szInfoTitle, kAppName, _TRUNCATE);
        wcsncpy_s(g_tray.szInfo, text.c_str(), _TRUNCATE);
        g_tray.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &g_tray);
    }
}

void AddTrayIcon() {
    ZeroMemory(&g_tray, sizeof(g_tray));
    g_tray.cbSize = sizeof(g_tray);
    g_tray.hWnd = g_main;
    g_tray.uID = TRAY_UID;
    g_tray.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    g_tray.uCallbackMessage = WM_TRAYICON;
    g_tray.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wcsncpy_s(g_tray.szTip, L"PSD Explorer 1.1.2 - Inactivo", _TRUNCATE);
    if (Shell_NotifyIconW(NIM_ADD, &g_tray)) {
        g_tray.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &g_tray);
    }
}

void RemoveTrayIcon() {
    if (!g_main) return;
    NOTIFYICONDATAW n{}; n.cbSize = sizeof(n); n.hWnd = g_main; n.uID = TRAY_UID;
    Shell_NotifyIconW(NIM_DELETE, &n);
}

struct ProgressPayload {
    std::wstring path;
    HRESULT hr{};
    bool ok{};
};

std::wstring GetExeDir() {
    wchar_t path[32768]{};
    DWORD n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    std::wstring s(path, n);
    auto pos = s.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : s.substr(0, pos);
}

std::wstring GetLocalAppData() {
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, path))) return L"";
    return path;
}

bool EnsureDir(const std::wstring& path) {
    int r = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_FILE_EXISTS || r == ERROR_ALREADY_EXISTS;
}

bool FileExists(const std::wstring& path) {
    DWORD a = GetFileAttributesW(path.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}


bool FilesEqual(const std::wstring& a, const std::wstring& b) {
    HANDLE ha = CreateFileW(a.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE hb = CreateFileW(b.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                            nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (ha == INVALID_HANDLE_VALUE || hb == INVALID_HANDLE_VALUE) {
        if (ha != INVALID_HANDLE_VALUE) CloseHandle(ha);
        if (hb != INVALID_HANDLE_VALUE) CloseHandle(hb);
        return false;
    }
    LARGE_INTEGER sa{}, sb{};
    bool same = GetFileSizeEx(ha, &sa) && GetFileSizeEx(hb, &sb) && sa.QuadPart == sb.QuadPart;
    std::vector<unsigned char> ba(64 * 1024), bb(64 * 1024);
    while (same) {
        DWORD ra = 0, rb = 0;
        if (!ReadFile(ha, ba.data(), static_cast<DWORD>(ba.size()), &ra, nullptr) ||
            !ReadFile(hb, bb.data(), static_cast<DWORD>(bb.size()), &rb, nullptr)) {
            same = false; break;
        }
        if (ra != rb || (ra && memcmp(ba.data(), bb.data(), ra) != 0)) { same = false; break; }
        if (ra == 0) break;
    }
    CloseHandle(ha); CloseHandle(hb);
    return same;
}

LONG SetRegString(HKEY root, const std::wstring& subkey, const wchar_t* valueName, const std::wstring& value) {
    HKEY key{};
    LONG r = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (r != ERROR_SUCCESS) return r;
    r = RegSetValueExW(key, valueName, 0, REG_SZ, reinterpret_cast<const BYTE*>(value.c_str()),
                      static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return r;
}

std::wstring GetRegString(HKEY root, const std::wstring& subkey, const wchar_t* valueName) {
    HKEY key{};
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_QUERY_VALUE, &key) != ERROR_SUCCESS) return L"";
    DWORD type = 0, bytes = 0;
    if (RegQueryValueExW(key, valueName, nullptr, &type, nullptr, &bytes) != ERROR_SUCCESS ||
        (type != REG_SZ && type != REG_EXPAND_SZ) || bytes < sizeof(wchar_t)) {
        RegCloseKey(key); return L"";
    }
    std::vector<wchar_t> buf(bytes / sizeof(wchar_t) + 1);
    if (RegQueryValueExW(key, valueName, nullptr, &type, reinterpret_cast<BYTE*>(buf.data()), &bytes) != ERROR_SUCCESS) {
        RegCloseKey(key); return L"";
    }
    RegCloseKey(key);
    return buf.data();
}

void DeleteTree(HKEY root, const std::wstring& subkey) { RegDeleteTreeW(root, subkey.c_str()); }
void DeleteValueSafe(HKEY root, const std::wstring& subkey, const wchar_t* name) {
    HKEY key{};
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegDeleteValueW(key, name); RegCloseKey(key);
    }
}

std::wstring Classes(const std::wstring& s) { return L"Software\\Classes\\" + s; }

std::wstring EffectiveProgId(const wchar_t* ext) {
    std::wstring userChoice = std::wstring(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\") + ext + L"\\UserChoice";
    std::wstring progId = GetRegString(HKEY_CURRENT_USER, userChoice, L"ProgId");
    if (!progId.empty()) return progId;
    return GetRegString(HKEY_CLASSES_ROOT, ext, nullptr);
}

void AssociateExt(const wchar_t* ext) {
    std::wstring base = Classes(ext);
    SetRegString(HKEY_CURRENT_USER, base + L"\\ShellEx\\" + Shell_ThumbnailProviderGuid, nullptr, CLSID_PSDExplorerThumbnail_Str);
    SetRegString(HKEY_CURRENT_USER, base + L"\\ShellEx\\" + Shell_PreviewHandlerGuid, nullptr, CLSID_PSDExplorerPreview_Str);
    std::wstring sys = Classes(std::wstring(L"SystemFileAssociations\\") + ext);
    SetRegString(HKEY_CURRENT_USER, sys + L"\\ShellEx\\" + Shell_ThumbnailProviderGuid, nullptr, CLSID_PSDExplorerThumbnail_Str);
    SetRegString(HKEY_CURRENT_USER, sys + L"\\ShellEx\\" + Shell_PreviewHandlerGuid, nullptr, CLSID_PSDExplorerPreview_Str);

    std::wstring progId = EffectiveProgId(ext);
    if (!progId.empty()) {
        std::wstring prog = Classes(progId);
        SetRegString(HKEY_CURRENT_USER, prog + L"\\ShellEx\\" + Shell_ThumbnailProviderGuid, nullptr, CLSID_PSDExplorerThumbnail_Str);
        SetRegString(HKEY_CURRENT_USER, prog + L"\\ShellEx\\" + Shell_PreviewHandlerGuid, nullptr, CLSID_PSDExplorerPreview_Str);
        SetRegString(HKEY_CURRENT_USER, L"Software\\PSDExplorer", std::wstring(ext) == L".psd" ? L"PsdProgId" : L"PsbProgId", progId);
    }
}

HRESULT VerifyComActivation();
std::wstring HexHr(HRESULT hr);

void UnassociateExt(const wchar_t* ext) {
    std::wstring base = Classes(ext);
    DeleteTree(HKEY_CURRENT_USER, base + L"\\ShellEx\\" + Shell_ThumbnailProviderGuid);
    DeleteTree(HKEY_CURRENT_USER, base + L"\\ShellEx\\" + Shell_PreviewHandlerGuid);
    std::wstring sys = Classes(std::wstring(L"SystemFileAssociations\\") + ext);
    DeleteTree(HKEY_CURRENT_USER, sys + L"\\ShellEx\\" + Shell_ThumbnailProviderGuid);
    DeleteTree(HKEY_CURRENT_USER, sys + L"\\ShellEx\\" + Shell_PreviewHandlerGuid);

    const wchar_t* valueName = std::wstring(ext) == L".psd" ? L"PsdProgId" : L"PsbProgId";
    std::wstring storedProgId = GetRegString(HKEY_CURRENT_USER, L"Software\\PSDExplorer", valueName);
    std::wstring currentProgId = EffectiveProgId(ext);
    if (!storedProgId.empty()) {
        std::wstring prog = Classes(storedProgId);
        DeleteTree(HKEY_CURRENT_USER, prog + L"\\ShellEx\\" + Shell_ThumbnailProviderGuid);
        DeleteTree(HKEY_CURRENT_USER, prog + L"\\ShellEx\\" + Shell_PreviewHandlerGuid);
    }
    if (!currentProgId.empty() && currentProgId != storedProgId) {
        std::wstring prog = Classes(currentProgId);
        DeleteTree(HKEY_CURRENT_USER, prog + L"\\ShellEx\\" + Shell_ThumbnailProviderGuid);
        DeleteTree(HKEY_CURRENT_USER, prog + L"\\ShellEx\\" + Shell_PreviewHandlerGuid);
    }
}
