#include "stdafx.h"

#include "MainDlg.h"

#include "symbol_enum.h"

#define WSH_VERSION L"1.0.1"

namespace {

struct InitialUIValues {
    CString engineDir;
    CString symbolsDir;
    CString symbolServer;
    CString targetExecutable;
};

InitialUIValues GetInitialUIValues() {
    InitialUIValues values;

    std::filesystem::path iniFilePath = wil::GetModuleFileName<std::wstring>();
    iniFilePath.replace_filename(L"windhawk-symbol-helper.ini");

    std::filesystem::path fallbackIniFilePath1 = iniFilePath;
    fallbackIniFilePath1.replace_filename(L"windhawk.ini");

    std::filesystem::path fallbackIniFilePath2 =
        wil::ExpandEnvironmentStrings<std::wstring>(
            LR"(%ProgramFiles%\Windhawk\windhawk.ini)");

    WCHAR buffer[MAX_PATH];
    constexpr DWORD bufferSize = static_cast<DWORD>(std::size(buffer));

    if (GetPrivateProfileString(L"Config", L"EnginePath", L"", buffer,
                                bufferSize, iniFilePath.c_str())) {
        values.engineDir =
            wil::ExpandEnvironmentStrings<std::wstring>(buffer).c_str();
    } else if (GetPrivateProfileString(L"Storage", L"EnginePath", L"", buffer,
                                       bufferSize,
                                       fallbackIniFilePath1.c_str())) {
        auto expanded = wil::ExpandEnvironmentStrings<std::wstring>(buffer);
        values.engineDir =
            (fallbackIniFilePath1.parent_path() / expanded).c_str();
    } else if (GetPrivateProfileString(L"Storage", L"EnginePath", L"", buffer,
                                       bufferSize,
                                       fallbackIniFilePath2.c_str())) {
        auto expanded = wil::ExpandEnvironmentStrings<std::wstring>(buffer);
        values.engineDir =
            (fallbackIniFilePath2.parent_path() / expanded).c_str();
    } else {
        values.engineDir = LR"(C:\Program Files\Windhawk\Engine\1.3.1)";
    }

    if (GetPrivateProfileString(L"Config", L"SymbolsPath", L"", buffer,
                                bufferSize, iniFilePath.c_str())) {
        values.symbolsDir =
            wil::ExpandEnvironmentStrings<std::wstring>(buffer).c_str();
    } else if (GetPrivateProfileString(L"Storage", L"AppDataPath", L"", buffer,
                                       bufferSize,
                                       fallbackIniFilePath1.c_str())) {
        auto expanded = wil::ExpandEnvironmentStrings<std::wstring>(buffer);
        values.symbolsDir = (fallbackIniFilePath1.parent_path() / expanded /
                             L"Engine" / L"Symbols")
                                .c_str();
    } else if (GetPrivateProfileString(L"Storage", L"AppDataPath", L"", buffer,
                                       bufferSize,
                                       fallbackIniFilePath2.c_str())) {
        auto expanded = wil::ExpandEnvironmentStrings<std::wstring>(buffer);
        values.symbolsDir = (fallbackIniFilePath2.parent_path() / expanded /
                             L"Engine" / L"Symbols")
                                .c_str();
    } else {
        values.symbolsDir = LR"(C:\ProgramData\Windhawk\Engine\Symbols)";
    }

    if (GetPrivateProfileString(L"Config", L"SymbolServer", L"", buffer,
                                bufferSize, iniFilePath.c_str())) {
        values.symbolServer = buffer;
    } else {
        values.symbolServer = L"https://msdl.microsoft.com/download/symbols";
    }

    if (GetPrivateProfileString(L"Config", L"TargetExecutable", L"", buffer,
                                bufferSize, iniFilePath.c_str())) {
        values.targetExecutable =
            wil::ExpandEnvironmentStrings<std::wstring>(buffer).c_str();
    } else {
        values.targetExecutable = LR"(C:\Windows\Explorer.exe)";
    }

    return values;
}

void OpenUrl(HWND hWnd, PCWSTR url) {
    if ((INT_PTR)ShellExecute(hWnd, L"open", url, nullptr, nullptr,
                              SW_SHOWNORMAL) <= 32) {
        CString errorMsg;
        errorMsg.Format(
            L"Failed to open link, you can copy it with Ctrl+C and open it in "
            L"a browser manually:\n\n%s",
            url);
        MessageBox(hWnd, errorMsg, nullptr, MB_ICONHAND);
    }
}

}  // namespace

BOOL CMainDlg::PreTranslateMessage(MSG* pMsg) {
    if (m_resultsEdit.PreTranslateMessage(pMsg)) {
        return TRUE;
    }

    if (m_accelerator && ::TranslateAccelerator(m_hWnd, m_accelerator, pMsg)) {
        return TRUE;
    }

    return CWindow::IsDialogMessage(pMsg);
}

BOOL CMainDlg::OnInitDialog(CWindow wndFocus, LPARAM lInitParam) {
    // Center the dialog on the screen.
    CenterWindow();

    // Set icons.
    HICON hIcon = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR,
                                   ::GetSystemMetrics(SM_CXICON),
                                   ::GetSystemMetrics(SM_CYICON));
    SetIcon(hIcon, TRUE);
    HICON hIconSmall = AtlLoadIconImage(IDR_MAINFRAME, LR_DEFAULTCOLOR,
                                        ::GetSystemMetrics(SM_CXSMICON),
                                        ::GetSystemMetrics(SM_CYSMICON));
    SetIcon(hIconSmall, FALSE);

    // Bind keys...
    m_accelerator = AtlLoadAccelerators(IDR_MAINFRAME);

    // Register object for message filtering and idle updates.
    CMessageLoop* pLoop = _Module.GetMessageLoop();
    ATLASSERT(pLoop != nullptr);
    pLoop->AddMessageFilter(this);

    // Populate values.
    auto initialUIValues = GetInitialUIValues();
    CEdit(GetDlgItem(IDC_ENGINE_DIR)).SetWindowText(initialUIValues.engineDir);
    CEdit(GetDlgItem(IDC_SYMBOLS_DIR))
        .SetWindowText(initialUIValues.symbolsDir);
    CEdit(GetDlgItem(IDC_SYMBOL_SERVER))
        .SetWindowText(initialUIValues.symbolServer);
    CEdit(GetDlgItem(IDC_TARGET_EXECUTABLE))
        .SetWindowText(initialUIValues.targetExecutable);

    CButton(GetDlgItem(IDC_UNDECORATED)).SetCheck(BST_CHECKED);

    // Init edit control.
    auto resultsPlaceholderControl = GetDlgItem(IDC_RESULTS_PLACEHOLDER);

    CRect rc;
    resultsPlaceholderControl.GetClientRect(&rc);
    resultsPlaceholderControl.MapWindowPoints(m_hWnd, rc);
    m_resultsEdit.Create(m_hWnd, rc, nullptr,
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_MULTILINE |
                             ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN |
                             ES_NOHIDESEL | WS_VSCROLL | WS_HSCROLL,
                         WS_EX_CLIENTEDGE, IDC_RESULTS);

    CLogFont fontAttributes(AtlGetDefaultGuiFont());
    wcscpy_s(fontAttributes.lfFaceName, L"Courier New");
    fontAttributes.SetHeight(10);
    m_resultsEditFont = fontAttributes.CreateFontIndirect();
    m_resultsEdit.SetFont(m_resultsEditFont);

    // Init resizing.
    DlgResize_Init();

    return TRUE;
}

void CMainDlg::OnDestroy() {
    m_enumSymbolsThread.reset();

    PostQuitMessage(0);
}

void CMainDlg::OnDropFiles(HDROP hDropInfo) {
    if (DragQueryFile(hDropInfo, 0xFFFFFFFF, NULL, 0) == 1) {
        WCHAR fileName[MAX_PATH];
        DragQueryFile(hDropInfo, 0, fileName, MAX_PATH);

        SetDlgItemText(IDC_TARGET_EXECUTABLE, fileName);
    } else {
        MessageBox(L"Please drop one file at a time", L"Unsupported",
                   MB_ICONINFORMATION);
    }

    DragFinish(hDropInfo);
}

void CMainDlg::OnAppAbout(UINT uNotifyCode, int nID, CWindow wndCtl) {
    PCWSTR content =
        L"A tool to get symbols from executables the same way Windhawk mods do "
        L"with the symbols API.\n\n"
        L"The tool was created to help with Windhawk mod development.\n\n"
        L"Windhawk can be downloaded at <A "
        L"HREF=\"https://windhawk.net\">windhawk.net</A>.";

    TASKDIALOGCONFIG taskDialogConfig{
        .cbSize = sizeof(taskDialogConfig),
        .hwndParent = m_hWnd,
        .hInstance = _Module.GetModuleInstance(),
        .dwFlags = TDF_ENABLE_HYPERLINKS | TDF_ALLOW_DIALOG_CANCELLATION |
                   TDF_POSITION_RELATIVE_TO_WINDOW,
        .pszWindowTitle = L"About",
        .pszMainIcon = MAKEINTRESOURCE(IDR_MAINFRAME),
        .pszMainInstruction = L"Windhawk Symbol Helper v" WSH_VERSION,
        .pszContent = content,
        .pfCallback = [](HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam,
                         LONG_PTR lpRefData) -> HRESULT {
            switch (msg) {
                case TDN_HYPERLINK_CLICKED:
                    OpenUrl(hwnd, (PCWSTR)lParam);
                    break;
            }

            return S_OK;
        },
    };

    ::TaskDialogIndirect(&taskDialogConfig, nullptr, nullptr, nullptr);
}

void CMainDlg::OnOK(UINT uNotifyCode, int nID, CWindow wndCtl) {
    if (m_enumSymbolsThread) {
        m_enumSymbolsThread->request_stop();
        GetDlgItem(IDOK).EnableWindow(FALSE);
        return;
    }

    struct {
        CString engineDir;
        CString symbolsDir;
        CString symbolServer;
        CString targetExecutable;
        bool undecorated;
        bool decorated;
        bool log;
    } threadParams;

    GetDlgItemText(IDC_ENGINE_DIR, threadParams.engineDir);
    GetDlgItemText(IDC_SYMBOLS_DIR, threadParams.symbolsDir);
    GetDlgItemText(IDC_SYMBOL_SERVER, threadParams.symbolServer);
    GetDlgItemText(IDC_TARGET_EXECUTABLE, threadParams.targetExecutable);
    threadParams.undecorated =
        CButton(GetDlgItem(IDC_UNDECORATED)).GetCheck() != BST_UNCHECKED;
    threadParams.decorated =
        CButton(GetDlgItem(IDC_DECORATED)).GetCheck() != BST_UNCHECKED;
    threadParams.log = CButton(GetDlgItem(IDC_LOG)).GetCheck() != BST_UNCHECKED;

    SetDlgItemText(IDOK, L"Cancel");

    m_enumSymbolsThread = std::jthread([threadParams = std::move(threadParams),
                                        &enumSymbolsResult =
                                            m_enumSymbolsResult,
                                        hWnd =
                                            m_hWnd](std::stop_token stopToken) {
        CStringA logOutput;

        try {
            CString result;
            int count = 0;

            {
                SymbolEnum::Callbacks callbacks{
                    .queryCancel =
                        [&stopToken]() { return stopToken.stop_requested(); },
                    .notifyProgress =
                        [hWnd](int progress) {
                            CWindow(hWnd).PostMessage(UWM_PROGRESS,
                                                      (WPARAM)progress);
                        },
                };

                if (threadParams.log) {
                    callbacks.notifyLog = [&logOutput](PCSTR message) {
                        logOutput += message;
                        logOutput += "\r\n";
                    };
                }

                SymbolEnum symbolEnum(threadParams.targetExecutable.GetString(),
                                      0, threadParams.engineDir.GetString(),
                                      threadParams.symbolsDir.GetString(),
                                      threadParams.symbolServer.GetString(),
                                      callbacks);

                while (auto iter = symbolEnum.GetNextSymbol(false)) {
                    if (stopToken.stop_requested()) {
                        throw std::runtime_error("Cancelled");
                    }

                    CString addressPrefix;
                    addressPrefix.Format(L"[%08" TEXT(PRIXPTR) L"] ",
                                         iter->address);

                    if (threadParams.undecorated) {
                        result += addressPrefix;
                        result += iter->name;
                        result += L"\r\n";
                    }

                    if (threadParams.decorated) {
                        result += addressPrefix;
                        result += iter->nameDecorated;
                        result += L"\r\n";
                    }

                    count++;
                }
            }

            enumSymbolsResult.Format(L"Found %d symbols\r\n%S%s", count,
                                     logOutput.GetString(), result.GetString());
        } catch (const std::exception& e) {
            enumSymbolsResult.Format(L"Error: %S\r\n%S", e.what(),
                                     logOutput.GetString());
        }

        CWindow(hWnd).PostMessage(UWM_ENUM_SYMBOLS_DONE);
    });
}

void CMainDlg::OnCancel(UINT uNotifyCode, int nID, CWindow wndCtl) {
    DestroyWindow();
}

LRESULT CMainDlg::OnProgress(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    int progress = (int)wParam;

    CString text;
    text.Format(L"[%d%%] Cancel", progress);

    SetDlgItemText(IDOK, text);

    return 0;
}

LRESULT CMainDlg::OnEnumSymbolsDone(UINT uMsg, WPARAM wParam, LPARAM lParam) {
    m_enumSymbolsThread.reset();

    SetDlgItemText(IDC_RESULTS, m_enumSymbolsResult);
    m_enumSymbolsResult.Empty();

    SetDlgItemText(IDOK, L"Get &symbols");
    GetDlgItem(IDOK).EnableWindow(TRUE);

    return 0;
}
