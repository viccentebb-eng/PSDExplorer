bool RunAdminHelper(const wchar_t* action, std::wstring& error) {
    const std::wstring helper = GetExeDir() + L"\\PSDExplorerAdmin.exe";
    if (!FileExists(helper)) {
        error = L"Falta PSDExplorerAdmin.exe junto al instalador. Vuelve a compilar/extraer el paquete completo.";
        return false;
    }

    SHELLEXECUTEINFOW sei{};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
    sei.hwnd = g_main;
    sei.lpVerb = L"runas";
    sei.lpFile = helper.c_str();
    sei.lpParameters = action;
    sei.nShow = SW_HIDE;

    if (!ShellExecuteExW(&sei)) {
        DWORD e = GetLastError();
        if (e == ERROR_CANCELLED) {
            error = L"Windows necesita permiso de administrador para registrar el proveedor COM a nivel del sistema. "
                    L"La reparación por usuario quedó aplicada, pero Explorer puede seguir mostrando 0x80040154.";
        } else {
            error = L"No se pudo iniciar la reparación elevada. Código: " + std::to_wstring(e);
        }
        return false;
    }

    WaitForSingleObject(sei.hProcess, 30000);
    DWORD code = 1;
    GetExitCodeProcess(sei.hProcess, &code);
    CloseHandle(sei.hProcess);
    if (code != 0) {
        error = L"La reparación de integración del sistema terminó con código " + std::to_wstring(code) + L".";
        return false;
    }
    return true;
}

bool InstallIntegration(std::wstring& error) {
    if (!InstallIntegrationUser(error)) return false;

    std::wstring machineError;
    if (!RunAdminHelper(L"--register", machineError)) {
        error = L"La integración por usuario quedó instalada, pero la reparación para el proceso aislado de Explorer no se completó.\n\n" + machineError;
        return false;
    }

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    error.clear();
    return true;
}

bool UninstallIntegration(std::wstring& error) {
    std::wstring machineError;
    if (!RunAdminHelper(L"--unregister", machineError)) {
        error = L"No se pudo retirar el registro COM del sistema.\n\n" + machineError;
        return false;
    }
    return UninstallIntegrationUser(error);
}
