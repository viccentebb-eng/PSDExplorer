bool InstallIntegration(std::wstring& error) {
    std::wstring src = GetExeDir() + L"\\PSDExplorerShell.dll";
    if (!FileExists(src)) { error = L"No encuentro PSDExplorerShell.dll junto a PSDExplorerSetup.exe."; return false; }
    std::wstring local = GetLocalAppData();
    if (local.empty()) { error = L"No pude localizar LocalAppData."; return false; }
    std::wstring root = local + L"\\PSDExplorer";
    std::wstring appDir = root + L"\\" + kVersion;
    EnsureDir(root);
    if (!EnsureDir(appDir)) { error = L"No pude crear la carpeta de instalación."; return false; }
    std::wstring dst = appDir + L"\\PSDExplorerShell.dll";
    if (!CopyFileW(src.c_str(), dst.c_str(), FALSE)) {
        DWORD e = GetLastError();
        if (e != ERROR_SHARING_VIOLATION && e != ERROR_ACCESS_DENIED) {
            error = L"No pude copiar la DLL. Código: " + std::to_wstring(e); return false;
        }
        if (!FileExists(dst)) { error = L"La DLL está bloqueada y no existe una copia instalada utilizable."; return false; }
    }
    if (!FilesEqual(src, dst)) {
        error = L"La DLL instalada no coincide con la versión nueva. Probablemente Windows mantiene la anterior abierta. "
                L"Reinicia Explorer y vuelve a pulsar Instalar / Reparar.";
        return false;
    }

    std::wstring thumb = Classes(std::wstring(L"CLSID\\") + CLSID_PSDExplorerThumbnail_Str);
    std::wstring prev = Classes(std::wstring(L"CLSID\\") + CLSID_PSDExplorerPreview_Str);
    if (SetRegString(HKEY_CURRENT_USER, thumb, nullptr, L"PSD Explorer Thumbnail Provider") != ERROR_SUCCESS ||
        SetRegString(HKEY_CURRENT_USER, thumb + L"\\InprocServer32", nullptr, dst) != ERROR_SUCCESS ||
        SetRegString(HKEY_CURRENT_USER, thumb + L"\\InprocServer32", L"ThreadingModel", L"Apartment") != ERROR_SUCCESS) {
        error = L"Falló el registro COM del proveedor de miniaturas."; return false;
    }
    if (SetRegString(HKEY_CURRENT_USER, prev, nullptr, L"PSD Explorer Preview Handler") != ERROR_SUCCESS ||
        SetRegString(HKEY_CURRENT_USER, prev, L"AppID", PSDExplorer_PreviewHostAppId) != ERROR_SUCCESS ||
        SetRegString(HKEY_CURRENT_USER, prev + L"\\InprocServer32", nullptr, dst) != ERROR_SUCCESS ||
        SetRegString(HKEY_CURRENT_USER, prev + L"\\InprocServer32", L"ThreadingModel", L"Apartment") != ERROR_SUCCESS) {
        error = L"Falló el registro COM del panel de vista previa."; return false;
    }

    AssociateExt(L".psd");
    AssociateExt(L".psb");
    SetRegString(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers",
                 CLSID_PSDExplorerPreview_Str, L"PSD Explorer Preview Handler");
    SetRegString(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                 CLSID_PSDExplorerThumbnail_Str, L"PSD Explorer Thumbnail Provider");
    SetRegString(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved",
                 CLSID_PSDExplorerPreview_Str, L"PSD Explorer Preview Handler");
    SetRegString(HKEY_CURRENT_USER, L"Software\\PSDExplorer", L"Version", kVersion);
    SetRegString(HKEY_CURRENT_USER, L"Software\\PSDExplorer", L"InstallPath", dst);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);

    HRESULT verifyHr = VerifyComActivation();
    if (FAILED(verifyHr)) {
        error = L"Las claves se escribieron, pero Windows no pudo activar el proveedor COM (" + HexHr(verifyHr) +
                L"). Usa el botón Verificar integración para ver el punto exacto del fallo.";
        return false;
    }
    return true;
}

bool UninstallIntegration(std::wstring& error) {
    std::wstring thumbKey = Classes(std::wstring(L"CLSID\\") + CLSID_PSDExplorerThumbnail_Str + L"\\InprocServer32");
    std::wstring installedDll = GetRegString(HKEY_CURRENT_USER, thumbKey, nullptr);
    UnassociateExt(L".psd"); UnassociateExt(L".psb");
    DeleteValueSafe(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\PreviewHandlers", CLSID_PSDExplorerPreview_Str);
    DeleteValueSafe(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", CLSID_PSDExplorerThumbnail_Str);
    DeleteValueSafe(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved", CLSID_PSDExplorerPreview_Str);
    DeleteTree(HKEY_CURRENT_USER, Classes(std::wstring(L"CLSID\\") + CLSID_PSDExplorerThumbnail_Str));
    DeleteTree(HKEY_CURRENT_USER, Classes(std::wstring(L"CLSID\\") + CLSID_PSDExplorerPreview_Str));
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    if (!installedDll.empty() && FileExists(installedDll)) {
        if (!DeleteFileW(installedDll.c_str())) MoveFileExW(installedDll.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
    error.clear();
    return true;
}

bool IsInstalled(std::wstring* path = nullptr) {
    auto key = Classes(std::wstring(L"CLSID\\") + CLSID_PSDExplorerThumbnail_Str + L"\\InprocServer32");
    std::wstring dll = GetRegString(HKEY_CURRENT_USER, key, nullptr);
    if (path) *path = dll;
    return !dll.empty() && FileExists(dll);
}

void RestartExplorer(HWND owner, bool ask = true) {
    if (ask && MessageBoxW(owner,
        L"Esto cerrará las ventanas actuales del Explorador y volverá a iniciar explorer.exe.\n\n¿Continuar?",
        kAppName, MB_ICONWARNING | MB_YESNO) != IDYES) return;
    STARTUPINFOW si{sizeof(si)};
    PROCESS_INFORMATION pi{};
    wchar_t cmd[] = L"taskkill.exe /F /IM explorer.exe";
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 6000);
        CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    Sleep(700);
    ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOWNORMAL);
}

std::wstring FormatBytes(uint64_t bytes) {
    static const wchar_t* u[] = {L"B", L"KB", L"MB", L"GB", L"TB"};
    double v = static_cast<double>(bytes);
    int i = 0;
    while (v >= 1024.0 && i < 4) { v /= 1024.0; ++i; }
    std::wostringstream os;
    os << std::fixed << std::setprecision(i == 0 ? 0 : 1) << v << L" " << u[i];
    return os.str();
}

std::wstring CacheFolder() {
    std::wstring local = GetLocalAppData();
    return local.empty() ? L"" : local + L"\\Microsoft\\Windows\\Explorer";
}

uint64_t ThumbnailCacheSize(size_t* count = nullptr) {
    uint64_t total = 0; size_t n = 0;
    std::wstring folder = CacheFolder();
    if (folder.empty()) { if (count) *count = 0; return 0; }
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((folder + L"\\thumbcache_*.db").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                total += (uint64_t(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
                ++n;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    if (count) *count = n;
    return total;
}

void UpdateCacheInfo() {
    if (!g_cacheInfo) return;
    size_t count = 0;
    uint64_t bytes = ThumbnailCacheSize(&count);
    std::wstring folder = CacheFolder();
    std::wostringstream os;
    os << L"Caché de miniaturas de Windows\r\n\r\n"
       << L"Tamaño actual: " << FormatBytes(bytes) << L"\r\n"
       << L"Bases de datos: " << count << L"\r\n"
       << L"Ubicación: " << folder << L"\r\n\r\n"
       << L"Importante: esta caché es compartida por Windows para fotos, videos, PDF y demás formatos. "
          L"Al limpiarla, Explorer volverá a crear las miniaturas conforme abras carpetas.";
    SetWindowTextW(g_cacheInfo, os.str().c_str());
}

bool ClearThumbnailCache(HWND owner, std::wstring& report) {
    if (MessageBoxW(owner,
        L"Se borrará la caché GLOBAL de miniaturas de Windows, no solo la de PSD/PSB.\n"
        L"Explorer se reiniciará y Windows reconstruirá las vistas conforme abras carpetas.\n\n¿Continuar?",
        kAppName, MB_ICONWARNING | MB_YESNO) != IDYES) {
        report = L"Cancelado."; return false;
    }

    STARTUPINFOW si{sizeof(si)}; PROCESS_INFORMATION pi{};
    wchar_t cmd[] = L"taskkill.exe /F /IM explorer.exe";
    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 6000); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    }
    Sleep(900);

    std::wstring folder = CacheFolder();
    size_t deleted = 0, pending = 0;
    WIN32_FIND_DATAW fd{};
    HANDLE h = FindFirstFileW((folder + L"\\thumbcache_*.db").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::wstring file = folder + L"\\" + fd.cFileName;
                if (DeleteFileW(file.c_str())) ++deleted;
                else if (MoveFileExW(file.c_str(), nullptr, MOVEFILE_DELAY_UNTIL_REBOOT)) ++pending;
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    ShellExecuteW(nullptr, L"open", L"explorer.exe", nullptr, nullptr, SW_SHOWNORMAL);
    std::wostringstream os;
    os << L"Caché procesada. Archivos borrados: " << deleted;
    if (pending) os << L". Pendientes para el próximo reinicio: " << pending;
    os << L".";
    report = os.str();
    UpdateCacheInfo();
    return true;
}

std::wstring HexHr(HRESULT hr) {
    std::wostringstream os;
    os << L"0x" << std::hex << std::uppercase << static_cast<unsigned long>(hr);
    return os.str();
}

bool RegDefaultEquals(const std::wstring& subkey, const std::wstring& expected) {
    return GetRegString(HKEY_CURRENT_USER, subkey, nullptr) == expected;
}

HRESULT VerifyComActivation() {
    IThumbnailProvider* provider = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_PSDExplorerThumbnail, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&provider));
    if (provider) provider->Release();
    return hr;
}

std::wstring VerifyIntegrationReport() {
    std::wostringstream os;
    std::wstring dll;
    bool installed = IsInstalled(&dll);
    os << L"DLL instalada: " << (installed ? L"OK" : L"ERROR") << L"\r\n";
    if (!dll.empty()) os << L"Ruta: " << dll << L"\r\n";

    bool loadOk = false, exportOk = false;
    if (installed) {
        HMODULE m = LoadLibraryW(dll.c_str());
        loadOk = m != nullptr;
        if (m) {
            exportOk = GetProcAddress(m, "DllGetClassObject") != nullptr;
            FreeLibrary(m);
        }
    }
    os << L"Carga directa de la DLL: " << (loadOk ? L"OK" : L"ERROR") << L"\r\n";
    os << L"Export DllGetClassObject: " << (exportOk ? L"OK" : L"ERROR") << L"\r\n";

    HRESULT comHr = VerifyComActivation();
    os << L"Activación COM: " << (SUCCEEDED(comHr) ? L"OK" : L"ERROR")
       << L" (" << HexHr(comHr) << L")\r\n";

    auto assoc = [](const wchar_t* ext) {
        std::wstring sys = Classes(std::wstring(L"SystemFileAssociations\\") + ext + L"\\ShellEx\\" + Shell_ThumbnailProviderGuid);
        return RegDefaultEquals(sys, CLSID_PSDExplorerThumbnail_Str);
    };
    os << L"Asociación PSD: " << (assoc(L".psd") ? L"OK" : L"ERROR") << L"\r\n";
    os << L"Asociación PSB: " << (assoc(L".psb") ? L"OK" : L"ERROR") << L"\r\n";
    std::wstring psdProg = EffectiveProgId(L".psd");
    std::wstring psbProg = EffectiveProgId(L".psb");
    os << L"ProgID efectivo PSD: " << (psdProg.empty() ? L"(sin detectar)" : psdProg) << L"\r\n";
    os << L"ProgID efectivo PSB: " << (psbProg.empty() ? L"(sin detectar)" : psbProg) << L"\r\n";

    if (comHr == REGDB_E_CLASSNOTREG) {
        os << L"\r\nDiagnóstico: Windows devuelve REGDB_E_CLASSNOTREG. El parser puede leer el PSD/PSB, "
              L"pero Windows no está pudiendo activar el proveedor COM. Pulsa Instalar / Reparar y después Reiniciar Explorer.";
    } else if (FAILED(comHr)) {
        os << L"\r\nDiagnóstico: la clase está registrada, pero la activación COM falló. El HRESULT permite distinguir "
              L"registro, carga de DLL o dependencias.";
    } else if (!assoc(L".psd") || !assoc(L".psb")) {
        os << L"\r\nDiagnóstico: la DLL se activa, pero falta una asociación de archivo. Instalar / Reparar la volverá a escribir.";
    } else {
        os << L"\r\nDiagnóstico: registro y activación COM correctos.";
    }
    return os.str();
}

void UpdateHomeStatus() {
    if (!g_homeStatus) return;
    std::wstring path;
    bool ok = IsInstalled(&path);
    std::wostringstream os;
    os << (ok ? L"● Integración activa\r\n" : L"○ Integración no instalada\r\n");
    if (ok) {
        HRESULT comHr = VerifyComActivation();
        os << L"PSD: registrado para miniaturas y vista previa\r\n"
           << L"PSB: registrado para miniaturas y vista previa\r\n"
           << L"Activación COM: " << (SUCCEEDED(comHr) ? L"OK" : (L"ERROR " + HexHr(comHr))) << L"\r\n"
           << L"DLL: " << path << L"\r\n\r\n"
           << L"Si Activación COM muestra ERROR, el problema es la integración con Windows, no el parser PSD/PSB.";
    } else {
        os << L"Pulsa Instalar / Reparar. La instalación es por usuario y no cambia Photoshop como aplicación predeterminada.";
    }
    SetWindowTextW(g_homeStatus, os.str().c_str());
}

std::wstring PickFolder(HWND owner) {
    IFileOpenDialog* dlg = nullptr;
    std::wstring out;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        FILEOPENDIALOGOPTIONS opts{};
        if (SUCCEEDED(dlg->GetOptions(&opts))) dlg->SetOptions(opts | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        dlg->SetTitle(L"Selecciona la carpeta con PSD / PSB");
        if (SUCCEEDED(dlg->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR p = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = p; CoTaskMemFree(p); }
                item->Release();
            }
        }
        dlg->Release();
    }
    return out;
}

std::wstring PickPsdFile(HWND owner) {
    IFileOpenDialog* dlg = nullptr;
    std::wstring out;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        COMDLG_FILTERSPEC specs[] = {{L"Photoshop PSD / PSB", L"*.psd;*.psb"}, {L"Todos los archivos", L"*.*"}};
        dlg->SetFileTypes(static_cast<UINT>(std::size(specs)), specs);
        dlg->SetTitle(L"Selecciona un PSD o PSB");
        FILEOPENDIALOGOPTIONS opts{};
        if (SUCCEEDED(dlg->GetOptions(&opts))) dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST);
        if (SUCCEEDED(dlg->Show(owner))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dlg->GetResult(&item))) {
                PWSTR p = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) { out = p; CoTaskMemFree(p); }
                item->Release();
            }
        }
        dlg->Release();
    }
    return out;
}

bool IsPsdLike(const fs::path& p) {
    std::wstring ext = p.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return ext == L".psd" || ext == L".psb";
}

std::vector<std::wstring> EnumerateFiles(const std::wstring& folder, bool recursive) {
    std::vector<std::wstring> files;
    std::error_code ec;
    if (recursive) {
        fs::recursive_directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec), end;
        for (; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (it->is_regular_file(ec) && !ec && IsPsdLike(it->path())) files.push_back(it->path().wstring());
        }
    } else {
        fs::directory_iterator it(folder, fs::directory_options::skip_permission_denied, ec), end;
        for (; it != end; it.increment(ec)) {
            if (ec) { ec.clear(); continue; }
            if (it->is_regular_file(ec) && !ec && IsPsdLike(it->path())) files.push_back(it->path().wstring());
        }
    }
    return files;
}

HRESULT RequestShellThumbnail(const std::wstring& path, int edge) {
    IShellItemImageFactory* f = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(path.c_str(), nullptr, IID_PPV_ARGS(&f));
    if (FAILED(hr)) return hr;
    HBITMAP hb = nullptr;
    SIZE s{edge, edge};
    hr = f->GetImage(s, static_cast<SIIGBF>(SIIGBF_THUMBNAILONLY | SIIGBF_BIGGERSIZEOK), &hb);
    if (hb) DeleteObject(hb);
    f->Release();
    return hr;
}

HRESULT RequestProviderThumbnailDirect(const std::wstring& path, int edge) {
    IThumbnailProvider* provider = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_PSDExplorerThumbnail, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&provider));
    if (FAILED(hr)) return hr;

    IInitializeWithStream* init = nullptr;
    hr = provider->QueryInterface(IID_PPV_ARGS(&init));
    IStream* stream = nullptr;
    if (SUCCEEDED(hr)) {
        hr = SHCreateStreamOnFileEx(path.c_str(), STGM_READ | STGM_SHARE_DENY_NONE,
                                    FILE_ATTRIBUTE_NORMAL, FALSE, nullptr, &stream);
    }
    if (SUCCEEDED(hr)) hr = init->Initialize(stream, STGM_READ);
    HBITMAP hb = nullptr;
    WTS_ALPHATYPE alpha = WTSAT_UNKNOWN;
    if (SUCCEEDED(hr)) hr = provider->GetThumbnail(static_cast<UINT>(edge), &hb, &alpha);
    if (hb) DeleteObject(hb);
    if (stream) stream->Release();
    if (init) init->Release();
    provider->Release();
    return hr;
}
