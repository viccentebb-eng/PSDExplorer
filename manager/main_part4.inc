LRESULT CALLBACK PageProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_COMMAND || msg == WM_NOTIFY) {
        return g_main ? SendMessageW(g_main, msg, wp, lp) : 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

HWND CreatePage(HWND parent) {
    HWND p = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE, 12, 44, 880, 552, parent, nullptr, nullptr, nullptr);
    return p;
}

void CreatePages(HWND hwnd) {
    RECT r{}; GetClientRect(hwnd, &r);
    g_tab = CreateWindowW(WC_TABCONTROLW, L"", WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
                          18, 18, r.right - 36, r.bottom - 36, hwnd,
                          reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TAB)), nullptr, nullptr);
    SendMessageW(g_tab, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    const wchar_t* tabs[] = {L"Inicio", L"Miniaturas", L"Caché", L"Diagnóstico"};
    for (int i = 0; i < 4; ++i) { TCITEMW ti{}; ti.mask = TCIF_TEXT; ti.pszText = const_cast<LPWSTR>(tabs[i]); TabCtrl_InsertItem(g_tab, i, &ti); }
    RECT tr{}; GetClientRect(g_tab, &tr); TabCtrl_AdjustRect(g_tab, FALSE, &tr);
    for (auto& page : g_pages) page = CreateWindowW(L"PSDExplorerPage", L"", WS_CHILD | WS_VISIBLE, tr.left, tr.top,
                                                     tr.right - tr.left, tr.bottom - tr.top, g_tab, nullptr, nullptr, nullptr);

    // Inicio
    MakeLabel(g_pages[0], L"PSD Explorer", 24, 18, 480, 34, true);
    MakeLabel(g_pages[0], L"Miniaturas y panel de vista previa para PSD / PSB en Windows 10/11 x64.", 24, 54, 760, 26);
    HWND group = CreateWindowW(L"BUTTON", L"Estado de integración", WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                               24, 92, 805, 195, g_pages[0], nullptr, nullptr, nullptr);
    SendMessageW(group, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    g_homeStatus = CreateWindowW(L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_LEFT, 42, 122, 770, 145,
                                 g_pages[0], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_HOME_STATUS)), nullptr, nullptr);
    SendMessageW(g_homeStatus, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    MakeButton(g_pages[0], L"Instalar / Reparar", ID_INSTALL, 24, 315, 170, 38, true);
    MakeButton(g_pages[0], L"Verificar integración", ID_VERIFY, 208, 315, 175, 38);
    MakeButton(g_pages[0], L"Reiniciar Explorer", ID_RESTART, 397, 315, 160, 38);
    MakeButton(g_pages[0], L"Desinstalar", ID_UNINSTALL, 571, 315, 130, 38);
    MakeLabel(g_pages[0], L"Consejo: si un PSB muestra el icono de Photoshop, abre Diagnóstico y analiza ese archivo. Ahí verás si tiene preview incrustado y qué compresión usa.", 24, 380, 805, 60);

    // Miniaturas
    MakeLabel(g_pages[1], L"Preprocesar carpeta", 24, 18, 450, 30, true);
    MakeLabel(g_pages[1], L"Pide las miniaturas a Explorer una por una para llenar la caché y muestra el avance real.", 24, 50, 780, 24);
    g_folderEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                                   24, 88, 645, 30, g_pages[1], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FOLDER_EDIT)), nullptr, nullptr);
    SendMessageW(g_folderEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    MakeButton(g_pages[1], L"Elegir carpeta…", ID_FOLDER_PICK, 684, 86, 145, 34);
    g_recursive = CreateWindowW(L"BUTTON", L"Incluir subcarpetas", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                24, 134, 170, 26, g_pages[1], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_RECURSIVE)), nullptr, nullptr);
    SendMessageW(g_recursive, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    SendMessageW(g_recursive, BM_SETCHECK, BST_CHECKED, 0);
    MakeLabel(g_pages[1], L"Tamaño de precalentamiento:", 220, 135, 190, 24);
    g_sizeCombo = CreateWindowW(L"COMBOBOX", L"", WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
                                410, 131, 130, 200, g_pages[1], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SIZE_COMBO)), nullptr, nullptr);
    SendMessageW(g_sizeCombo, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    SendMessageW(g_sizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"256 px"));
    SendMessageW(g_sizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"512 px"));
    SendMessageW(g_sizeCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(L"1024 px"));
    SendMessageW(g_sizeCombo, CB_SETCURSEL, 1, 0);
    g_generate = MakeButton(g_pages[1], L"Generar / precargar", ID_GENERATE, 565, 129, 170, 34, true);
    g_cancelBtn = MakeButton(g_pages[1], L"Cancelar", ID_CANCEL, 745, 129, 84, 34);
    EnableWindow(g_cancelBtn, FALSE);
    g_progress = CreateWindowExW(0, PROGRESS_CLASSW, L"", WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
                                 24, 181, 805, 22, g_pages[1], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_PROGRESS)), nullptr, nullptr);
    SendMessageW(g_progress, PBM_SETRANGE32, 0, 100);
    g_progressText = MakeLabel(g_pages[1], L"Listo.", 24, 211, 805, 24);
    g_log = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT,
                            24, 244, 805, 244, g_pages[1], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_LOG)), nullptr, nullptr);
    SendMessageW(g_log, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    MakeLabel(g_pages[1], L"OK = Explorer obtuvo una miniatura. ERROR = Windows no pudo producirla; revisa ese archivo en Diagnóstico.", 24, 500, 805, 30);

    // Cache
    MakeLabel(g_pages[2], L"Caché de miniaturas", 24, 18, 450, 30, true);
    MakeLabel(g_pages[2], L"PSD Explorer entrega imágenes al sistema; Windows las conserva en su propia caché compartida.", 24, 50, 780, 24);
    g_cacheInfo = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
                                  24, 90, 805, 245, g_pages[2], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CACHE_INFO)), nullptr, nullptr);
    SendMessageW(g_cacheInfo, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    MakeButton(g_pages[2], L"Actualizar tamaño", ID_CACHE_REFRESH, 24, 358, 155, 36);
    MakeButton(g_pages[2], L"Limpiar caché de Windows", ID_CACHE_CLEAR, 194, 358, 205, 36);
    MakeLabel(g_pages[2], L"Limpiar no desinstala PSD Explorer. Sí borra miniaturas de otros formatos porque Microsoft usa las mismas bases de datos para todo Explorer.", 24, 420, 805, 55);

    // Diagnostico
    MakeLabel(g_pages[3], L"Inspector PSD / PSB", 24, 18, 450, 30, true);
    MakeLabel(g_pages[3], L"Comprueba cabecera, preview incrustado, modo de color y compresión del composite.", 24, 50, 780, 24);
    g_fileEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"", WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY,
                                 24, 88, 645, 30, g_pages[3], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_FILE_EDIT)), nullptr, nullptr);
    SendMessageW(g_fileEdit, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);
    MakeButton(g_pages[3], L"Elegir archivo…", ID_FILE_PICK, 684, 86, 145, 34);
    MakeButton(g_pages[3], L"Analizar", ID_INSPECT, 24, 132, 120, 34, true);
    MakeButton(g_pages[3], L"Probar miniatura", ID_TEST_THUMB, 156, 132, 145, 34);
    g_inspectText = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"Selecciona un PSD o PSB para analizarlo.",
                                    WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
                                    24, 184, 805, 322, g_pages[3], reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_INSPECT_TEXT)), nullptr, nullptr);
    SendMessageW(g_inspectText, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE);

    for (int i = 1; i < 4; ++i) ShowWindow(g_pages[i], SW_HIDE);
    UpdateHomeStatus(); UpdateCacheInfo();
}

void SwitchPage(int index) {
    if (index < 0 || index > 3) return;
    for (int i = 0; i < 4; ++i) ShowWindow(g_pages[i], i == index ? SW_SHOW : SW_HIDE);
    if (index == 2) UpdateCacheInfo();
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_CREATE: CreatePages(hwnd); AddTrayIcon(); return 0;
        case WM_TRAYICON:
            if (LOWORD(lp) == WM_LBUTTONDBLCLK || LOWORD(lp) == NIN_SELECT) {
                ShowWindow(hwnd, SW_RESTORE);
                SetForegroundWindow(hwnd);
                return 0;
            }
            break;
        case WM_NOTIFY: {
            auto* nm = reinterpret_cast<NMHDR*>(lp);
            if (nm && nm->idFrom == ID_TAB && nm->code == TCN_SELCHANGE) {
                SwitchPage(TabCtrl_GetCurSel(g_tab)); return 0;
            }
            break;
        }
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case ID_INSTALL: {
                    std::wstring err;
                    if (InstallIntegration(err)) MessageBoxW(hwnd, L"Integración instalada/reparada y activación COM verificada. Reinicia Explorer antes de precargar carpetas.", kAppName, MB_ICONINFORMATION);
                    else MessageBoxW(hwnd, err.c_str(), kAppName, MB_ICONERROR);
                    UpdateHomeStatus(); return 0;
                }
                case ID_UNINSTALL: {
                    if (MessageBoxW(hwnd, L"¿Retirar la integración de PSD/PSB para este usuario?", kAppName, MB_ICONQUESTION | MB_YESNO) == IDYES) {
                        std::wstring err; UninstallIntegration(err); UpdateHomeStatus();
                    }
                    return 0;
                }
                case ID_VERIFY: {
                    std::wstring report = VerifyIntegrationReport();
                    MessageBoxW(hwnd, report.c_str(), kAppName, MB_ICONINFORMATION);
                    UpdateHomeStatus();
                    return 0;
                }
                case ID_RESTART: RestartExplorer(hwnd); return 0;
                case ID_FOLDER_PICK: {
                    std::wstring p = PickFolder(hwnd); if (!p.empty()) SetWindowTextW(g_folderEdit, p.c_str()); return 0;
                }
                case ID_GENERATE: StartPrewarm(); return 0;
                case ID_CANCEL: g_cancel = true; SetWindowTextW(g_progressText, L"Cancelando después del archivo actual…"); return 0;
                case ID_CACHE_REFRESH: UpdateCacheInfo(); return 0;
                case ID_CACHE_CLEAR: {
                    std::wstring report; if (ClearThumbnailCache(hwnd, report)) MessageBoxW(hwnd, report.c_str(), kAppName, MB_ICONINFORMATION); return 0;
                }
                case ID_FILE_PICK: {
                    std::wstring p = PickPsdFile(hwnd); if (!p.empty()) { SetWindowTextW(g_fileEdit, p.c_str()); InspectSelectedFile(); } return 0;
                }
                case ID_INSPECT: InspectSelectedFile(); return 0;
                case ID_TEST_THUMB: {
                    wchar_t p[32768]{}; GetWindowTextW(g_fileEdit, p, static_cast<int>(std::size(p)));
                    if (!FileExists(p)) { MessageBoxW(hwnd, L"Selecciona un archivo primero.", kAppName, MB_ICONINFORMATION); return 0; }
                    HRESULT comHr = VerifyComActivation();
                    HRESULT directHr = RequestProviderThumbnailDirect(p, 512);
                    HRESULT shellHr = RequestShellThumbnail(p, 512);
                    std::wostringstream os;
                    os << L"Activación COM: " << (SUCCEEDED(comHr) ? L"OK" : L"ERROR") << L" (" << HexHr(comHr) << L")\r\n"
                       << L"Proveedor directo: " << (SUCCEEDED(directHr) ? L"OK" : L"ERROR") << L" (" << HexHr(directHr) << L")\r\n"
                       << L"Explorer / caché: " << (SUCCEEDED(shellHr) ? L"OK" : L"ERROR") << L" (" << HexHr(shellHr) << L")\r\n\r\n";
                    if (comHr == REGDB_E_CLASSNOTREG)
                        os << L"La clase COM no está registrada/visible para Windows. El parser del archivo puede ser compatible y aun así Explorer no podrá usarlo.";
                    else if (SUCCEEDED(directHr) && FAILED(shellHr))
                        os << L"El proveedor sí genera la miniatura directamente; el fallo está en la asociación/caché del Shell.";
                    else if (FAILED(directHr))
                        os << L"La integración COM carga, pero el proveedor no pudo producir la miniatura para este archivo.";
                    else
                        os << L"La ruta completa está funcionando.";
                    MessageBoxW(hwnd, os.str().c_str(), kAppName, SUCCEEDED(shellHr) ? MB_ICONINFORMATION : MB_ICONWARNING); return 0;
                }
            }
            break;
        case WM_APP_TOTAL: {
            size_t total = static_cast<size_t>(wp);
            g_totalFiles = total;
            SendMessageW(g_progress, PBM_SETRANGE32, 0, static_cast<LPARAM>(std::max<size_t>(1, total)));
            std::wstring s = total ? (L"Encontrados " + std::to_wstring(total) + L" archivos. Generando…") : L"No se encontraron PSD/PSB.";
            SetWindowTextW(g_progressText, s.c_str()); return 0;
        }
        case WM_APP_PROGRESS_ITEM: {
            size_t done = static_cast<size_t>(wp);
            auto* p = reinterpret_cast<ProgressPayload*>(lp);
            SendMessageW(g_progress, PBM_SETPOS, static_cast<WPARAM>(done), 0);
            std::wstring name = p ? fs::path(p->path).filename().wstring() : L"";
            std::wstring row = (p && p->ok ? L"OK     " : L"ERROR  ") + name;
            if (p && !p->ok) {
                ++g_failedFiles;
                std::wostringstream hs; hs << L"   [0x" << std::hex << std::uppercase << static_cast<unsigned long>(p->hr) << L"]"; row += hs.str();
            }
            SendMessageW(g_log, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(row.c_str()));
            int cnt = static_cast<int>(SendMessageW(g_log, LB_GETCOUNT, 0, 0));
            if (cnt > 0) SendMessageW(g_log, LB_SETTOPINDEX, cnt - 1, 0);
            SetWindowTextW(g_progressText, (L"Procesados: " + std::to_wstring(done) + L" / " + std::to_wstring(g_totalFiles) + L"   |   " + name).c_str());
            SetTrayState(L"PSD Explorer - Procesando " + std::to_wstring(done) + L" / " + std::to_wstring(g_totalFiles));
            delete p; return 0;
        }
        case WM_APP_DONE: {
            bool cancelled = lp != 0;
            g_working = false;
            EnableWindow(g_generate, TRUE); EnableWindow(g_cancelBtn, FALSE);
            std::wstring s = cancelled ? L"Proceso cancelado." : (L"Terminado. Archivos procesados: " + std::to_wstring(static_cast<size_t>(wp)) + L".");
            SetWindowTextW(g_progressText, s.c_str());
            std::wstring tray = cancelled ? L"PSD Explorer - Proceso cancelado" :
                (L"PSD Explorer - Terminado: " + std::to_wstring(static_cast<size_t>(wp)) + L" archivos, " +
                 std::to_wstring(g_failedFiles) + L" errores");
            SetTrayState(tray, !cancelled);
            UpdateCacheInfo(); return 0;
        }
        case WM_CLOSE:
            if (g_working.load()) {
                if (MessageBoxW(hwnd, L"Hay un proceso de miniaturas en curso. ¿Cancelar y cerrar?", kAppName, MB_ICONQUESTION | MB_YESNO) != IDYES) return 0;
                g_cancel = true;
            }
            DestroyWindow(hwnd); return 0;
        case WM_DESTROY:
            RemoveTrayIcon();
            g_cancel = true;
            if (g_worker.joinable()) g_worker.join();
            if (g_font) DeleteObject(g_font); if (g_fontBold) DeleteObject(g_fontBold);
            PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}
} // namespace

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, LPWSTR, int show) {
    SetProcessDPIAware();
    HRESULT co = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX ic{sizeof(ic), ICC_STANDARD_CLASSES | ICC_PROGRESS_CLASS | ICC_TAB_CLASSES};
    InitCommonControlsEx(&ic);

    NONCLIENTMETRICSW ncm{sizeof(ncm)};
    SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0);
    lstrcpyW(ncm.lfMessageFont.lfFaceName, L"Segoe UI");
    ncm.lfMessageFont.lfHeight = -16;
    g_font = CreateFontIndirectW(&ncm.lfMessageFont);
    LOGFONTW bold = ncm.lfMessageFont; bold.lfWeight = FW_BOLD; bold.lfHeight = -24;
    g_fontBold = CreateFontIndirectW(&bold);

    WNDCLASSEXW pageWc{sizeof(pageWc)};
    pageWc.lpfnWndProc = PageProc; pageWc.hInstance = hInst; pageWc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    pageWc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); pageWc.lpszClassName = L"PSDExplorerPage";
    RegisterClassExW(&pageWc);

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc; wc.hInstance = hInst; wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION); wc.hIconSm = wc.hIcon;
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1); wc.lpszClassName = L"PSDExplorerManager111";
    RegisterClassExW(&wc);

    g_main = CreateWindowExW(0, wc.lpszClassName, kAppName,
                             WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                             CW_USEDEFAULT, CW_USEDEFAULT, 930, 675, nullptr, nullptr, hInst, nullptr);
    if (!g_main) { if (SUCCEEDED(co)) CoUninitialize(); return 1; }
    ShowWindow(g_main, show); UpdateWindow(g_main);
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (SUCCEEDED(co)) CoUninitialize();
    return static_cast<int>(msg.wParam);
}
