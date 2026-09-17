#include <windows.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include "Guids.h"

namespace {

LONG SetString(HKEY root, const std::wstring& subkey, const wchar_t* valueName, const std::wstring& value) {
    HKEY key{};
    LONG r = RegCreateKeyExW(root, subkey.c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &key, nullptr);
    if (r != ERROR_SUCCESS) return r;
    r = RegSetValueExW(key, valueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),
        static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(key);
    return r;
}

std::wstring GetExeDir() {
    wchar_t path[32768]{};
    DWORD n = GetModuleFileNameW(nullptr, path, static_cast<DWORD>(std::size(path)));
    std::wstring s(path, n);
    auto pos = s.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : s.substr(0, pos);
}

bool EnsureDir(const std::wstring& path) {
    int r = SHCreateDirectoryExW(nullptr, path.c_str(), nullptr);
    return r == ERROR_SUCCESS || r == ERROR_FILE_EXISTS || r == ERROR_ALREADY_EXISTS;
}

std::wstring ProgramFilesDir() {
    wchar_t path[MAX_PATH]{};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_PROGRAM_FILES, nullptr, SHGFP_TYPE_CURRENT, path))) return L"";
    return path;
}

bool RegisterMachine() {
    const std::wstring src = GetExeDir() + L"\\PSDExplorerShell.dll";
    if (GetFileAttributesW(src.c_str()) == INVALID_FILE_ATTRIBUTES) return false;

    const std::wstring pf = ProgramFilesDir();
    if (pf.empty()) return false;
    const std::wstring root = pf + L"\\PSDExplorer";
    const std::wstring appDir = root + L"\\Shell-1.1.2";
    if (!EnsureDir(root) || !EnsureDir(appDir)) return false;
    const std::wstring dst = appDir + L"\\PSDExplorerShell.dll";

    if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
        DWORD e = GetLastError();
        if (e != ERROR_SHARING_VIOLATION && e != ERROR_ACCESS_DENIED) return false;
        if (GetFileAttributesW(dst.c_str()) == INVALID_FILE_ATTRIBUTES) return false;
    }

    const std::wstring classes = L"Software\\Classes\\CLSID\\";
    const std::wstring thumb = classes + CLSID_PSDExplorerThumbnail_Str;
    const std::wstring prev = classes + CLSID_PSDExplorerPreview_Str;

    if (SetString(HKEY_LOCAL_MACHINE, thumb, nullptr, L"PSD Explorer Thumbnail Provider") != ERROR_SUCCESS ||
        SetString(HKEY_LOCAL_MACHINE, thumb + L"\\InprocServer32", nullptr, dst) != ERROR_SUCCESS ||
        SetString(HKEY_LOCAL_MACHINE, thumb + L"\\InprocServer32", L"ThreadingModel", L"Apartment") != ERROR_SUCCESS) return false;

    if (SetString(HKEY_LOCAL_MACHINE, prev, nullptr, L"PSD Explorer Preview Handler") != ERROR_SUCCESS ||
        SetString(HKEY_LOCAL_MACHINE, prev, L"AppID", PSDExplorer_PreviewHostAppId) != ERROR_SUCCESS ||
        SetString(HKEY_LOCAL_MACHINE, prev + L"\\InprocServer32", nullptr, dst) != ERROR_SUCCESS ||
        SetString(HKEY_LOCAL_MACHINE, prev + L"\\InprocServer32", L"ThreadingModel", L"Apartment") != ERROR_SUCCESS) return false;

    if (SetString(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
        CLSID_PSDExplorerThumbnail_Str, L"PSD Explorer Thumbnail Provider") != ERROR_SUCCESS) return false;
    if (SetString(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
        CLSID_PSDExplorerPreview_Str, L"PSD Explorer Preview Handler") != ERROR_SUCCESS) return false;
    if (SetString(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
        CLSID_PSDExplorerPreview_Str, L"PSD Explorer Preview Handler") != ERROR_SUCCESS) return false;

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

void DeleteValueSafe(HKEY root, const std::wstring& subkey, const wchar_t* valueName) {
    HKEY key{};
    if (RegOpenKeyExW(root, subkey.c_str(), 0, KEY_SET_VALUE, &key) == ERROR_SUCCESS) {
        RegDeleteValueW(key, valueName);
        RegCloseKey(key);
    }
}

bool UnregisterMachine() {
    const std::wstring classes = L"Software\\Classes\\CLSID\\";
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, (classes + CLSID_PSDExplorerThumbnail_Str).c_str());
    RegDeleteTreeW(HKEY_LOCAL_MACHINE, (classes + CLSID_PSDExplorerPreview_Str).c_str());
    DeleteValueSafe(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
        CLSID_PSDExplorerThumbnail_Str);
    DeleteValueSafe(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
        CLSID_PSDExplorerPreview_Str);
    DeleteValueSafe(HKEY_LOCAL_MACHINE,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
        CLSID_PSDExplorerPreview_Str);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return true;
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR cmdLine, int) {
    if (cmdLine && wcsstr(cmdLine, L"--unregister")) return UnregisterMachine() ? 0 : 2;
    if (cmdLine && wcsstr(cmdLine, L"--register")) return RegisterMachine() ? 0 : 1;
    return 3;
}
