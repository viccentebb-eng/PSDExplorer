void StartPrewarm() {
    if (g_working.exchange(true)) return;
    HRESULT activationHr = VerifyComActivation();
    if (FAILED(activationHr)) {
        g_working = false;
        SetTrayState(L"PSD Explorer - Integración COM con error");
        std::wstring msg = L"No iniciaré el procesamiento porque Windows no puede activar el proveedor de miniaturas (" +
                           HexHr(activationHr) + L").\n\nPulsa Inicio > Instalar / Reparar, después Verificar integración y Reiniciar Explorer.";
        MessageBoxW(g_main, msg.c_str(), kAppName, MB_ICONERROR);
        return;
    }
    wchar_t folderBuf[32768]{};
    GetWindowTextW(g_folderEdit, folderBuf, static_cast<int>(std::size(folderBuf)));
    std::wstring folder = folderBuf;
    if (folder.empty() || GetFileAttributesW(folder.c_str()) == INVALID_FILE_ATTRIBUTES) {
        g_working = false;
        MessageBoxW(g_main, L"Selecciona una carpeta válida primero.", kAppName, MB_ICONINFORMATION);
        return;
    }
    int edge = 512;
    int sel = static_cast<int>(SendMessageW(g_sizeCombo, CB_GETCURSEL, 0, 0));
    if (sel == 0) edge = 256; else if (sel == 2) edge = 1024;
    bool recursive = SendMessageW(g_recursive, BM_GETCHECK, 0, 0) == BST_CHECKED;

    if (g_worker.joinable()) g_worker.join();
    g_cancel = false;
    g_totalFiles = 0; g_failedFiles = 0;
    SetTrayState(L"PSD Explorer - Buscando PSD/PSB...");
    EnableWindow(g_generate, FALSE); EnableWindow(g_cancelBtn, TRUE);
    SendMessageW(g_log, LB_RESETCONTENT, 0, 0);
    SendMessageW(g_progress, PBM_SETPOS, 0, 0);
    SetWindowTextW(g_progressText, L"Buscando archivos PSD/PSB...");

    g_worker = std::thread([folder, recursive, edge]() {
        HRESULT workerCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        auto files = EnumerateFiles(folder, recursive);
        PostMessageW(g_main, WM_APP_TOTAL, static_cast<WPARAM>(files.size()), 0);
        size_t done = 0;
        for (const auto& file : files) {
            if (g_cancel.load()) break;
            HRESULT hr = RequestShellThumbnail(file, edge);
            auto* payload = new ProgressPayload{file, hr, SUCCEEDED(hr)};
            ++done;
            PostMessageW(g_main, WM_APP_PROGRESS_ITEM, static_cast<WPARAM>(done), reinterpret_cast<LPARAM>(payload));
        }
        if (SUCCEEDED(workerCo)) CoUninitialize();
        PostMessageW(g_main, WM_APP_DONE, static_cast<WPARAM>(done), g_cancel.load() ? 1 : 0);
    });
}

class FileByteStream final : public psdx::ByteStream {
public:
    explicit FileByteStream(const std::wstring& path) {
        h_ = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, nullptr);
        if (h_ != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER li{};
            if (GetFileSizeEx(h_, &li) && li.QuadPart >= 0) size_ = static_cast<uint64_t>(li.QuadPart);
            else { CloseHandle(h_); h_ = INVALID_HANDLE_VALUE; }
        }
    }
    ~FileByteStream() override { if (h_ != INVALID_HANDLE_VALUE) CloseHandle(h_); }
    bool valid() const { return h_ != INVALID_HANDLE_VALUE; }
    bool seek(uint64_t absolute) override {
        if (!valid() || absolute > size_ || absolute > static_cast<uint64_t>(LLONG_MAX)) return false;
        LARGE_INTEGER li{}; li.QuadPart = static_cast<LONGLONG>(absolute);
        if (!SetFilePointerEx(h_, li, nullptr, FILE_BEGIN)) return false;
        pos_ = absolute; return true;
    }
    uint64_t tell() const override { return pos_; }
    uint64_t size() const override { return size_; }
    bool read(void* dst, size_t bytes) override {
        if (!valid() || bytes > size_ - pos_) return false;
        auto* p = static_cast<uint8_t*>(dst);
        size_t left = bytes;
        while (left) {
            DWORD chunk = static_cast<DWORD>(std::min<size_t>(left, 1u << 20));
            DWORD got = 0;
            if (!ReadFile(h_, p, chunk, &got, nullptr) || got != chunk) return false;
            p += got; left -= got; pos_ += got;
        }
        return true;
    }
private:
    HANDLE h_{INVALID_HANDLE_VALUE};
    uint64_t size_{};
    uint64_t pos_{};
};

const wchar_t* ColorModeName(uint16_t m) {
    switch (m) {
        case 0: return L"Bitmap"; case 1: return L"Escala de grises"; case 2: return L"Indexado";
        case 3: return L"RGB"; case 4: return L"CMYK"; case 7: return L"Multicanal";
        case 8: return L"Duotono"; case 9: return L"Lab"; default: return L"Desconocido";
    }
}

const wchar_t* CompressionName(uint16_t c) {
    switch (c) { case 0: return L"RAW"; case 1: return L"RLE / PackBits"; case 2: return L"ZIP"; case 3: return L"ZIP Prediction"; default: return L"Desconocida"; }
}

std::wstring Utf8ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    if (!n) return std::wstring(s.begin(), s.end());
    std::wstring w(n, L'\0'); MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), w.data(), n); return w;
}

void InspectSelectedFile() {
    wchar_t buf[32768]{};
    GetWindowTextW(g_fileEdit, buf, static_cast<int>(std::size(buf)));
    std::wstring path = buf;
    if (!FileExists(path)) { MessageBoxW(g_main, L"Selecciona un PSD/PSB válido.", kAppName, MB_ICONINFORMATION); return; }
    FileByteStream stream(path);
    if (!stream.valid()) { SetWindowTextW(g_inspectText, L"No se pudo abrir el archivo."); return; }
    auto r = psdx::Inspect(stream);
    if (r.status != psdx::DecodeStatus::Ok) {
        std::wstring msg = L"Archivo no reconocido o no compatible.\r\n\r\nEstado: " + Utf8ToWide(psdx::StatusText(r.status)) +
                           L"\r\nDetalle: " + Utf8ToWide(r.detail);
        SetWindowTextW(g_inspectText, msg.c_str()); return;
    }
    std::wstring name = fs::path(path).filename().wstring();
    std::wostringstream os;
    os << name << L"\r\n\r\n"
       << L"Formato: " << (r.header.version == 2 ? L"PSB" : L"PSD") << L"\r\n"
       << L"Dimensiones: " << r.header.width << L" × " << r.header.height << L" px\r\n"
       << L"Profundidad: " << r.header.depth << L" bits\r\n"
       << L"Modo: " << ColorModeName(r.header.colorMode) << L"\r\n"
       << L"Canales: " << r.header.channels << L"\r\n"
       << L"Tamaño del archivo: " << FormatBytes(stream.size()) << L"\r\n\r\n"
       << L"Preview JPEG incrustado: " << (r.hasEmbeddedJpeg ? L"Sí" : L"No") << L"\r\n";
    if (r.hasEmbeddedJpeg) os << L"Preview: " << r.embeddedJpegWidth << L" × " << r.embeddedJpegHeight << L" px\r\n";
    os << L"Compresión composite: " << CompressionName(r.compositeCompression) << L"\r\n\r\n";

    if (r.hasEmbeddedJpeg) {
        os << L"Diagnóstico: COMPATIBLE. PSD Explorer puede usar el preview JPEG sin decodificar las capas/composite.";
    } else if ((r.compositeCompression == 0 || r.compositeCompression == 1) &&
               (r.header.depth == 8 || r.header.depth == 16) &&
               (r.header.colorMode == 1 || r.header.colorMode == 3 || r.header.colorMode == 4)) {
        os << L"Diagnóstico: COMPATIBLE por composite " << CompressionName(r.compositeCompression) << L".";
    } else if (r.compositeCompression == 2 || r.compositeCompression == 3) {
        os << L"Diagnóstico: este archivo NO trae preview JPEG y usa " << CompressionName(r.compositeCompression)
           << L". La 1.1 identifica correctamente este caso, pero el decoder ZIP todavía no está implementado; "
              L"por eso puede seguir apareciendo el icono PSB/PSD.";
    } else {
        os << L"Diagnóstico: el formato es válido, pero el fallback actual no cubre esta combinación de profundidad/modo/compresión.";
    }
    os << L"\r\n\r\nDetalle del parser: " << Utf8ToWide(r.detail);
    SetWindowTextW(g_inspectText, os.str().c_str());
}

HWND MakeLabel(HWND parent, const wchar_t* text, int x, int y, int w, int h, bool bold = false) {
    HWND c = CreateWindowW(L"STATIC", text, WS_CHILD | WS_VISIBLE | SS_LEFT, x, y, w, h, parent, nullptr, nullptr, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(bold ? g_fontBold : g_font), TRUE); return c;
}

HWND MakeButton(HWND parent, const wchar_t* text, int id, int x, int y, int w, int h, bool def = false) {
    HWND c = CreateWindowW(L"BUTTON", text, WS_CHILD | WS_VISIBLE | (def ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON),
                           x, y, w, h, parent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)), nullptr, nullptr);
    SendMessageW(c, WM_SETFONT, reinterpret_cast<WPARAM>(g_font), TRUE); return c;
}

