// OpenCapture entry point: single-instance tray application with a global
// hotkey (Ctrl+PrtSc) for the Rectangular Region capture.
//
// Command line:
//   OpenCapture.exe            start (or keep) the tray application
//   OpenCapture.exe --region   start a Rectangular Region capture now
//   OpenCapture.exe --exit     ask a running instance to quit
#include <objbase.h>
#include <shellapi.h>

#include <string>

#include "app_icon.h"
#include "version.h"
#include "region_overlay.h"
#include "viewer.h"
#include "win.h"

namespace {

const wchar_t kHostClass[] = L"OpenCapture.Host";
const wchar_t kMutexName[] = L"OpenCapture.SingleInstance";
constexpr UINT WM_TRAY = WM_APP + 1;
constexpr UINT WM_START_REGION = WM_APP + 2;
constexpr int kHotkeyRegion = 1;
constexpr UINT kTrayId = 1;
constexpr ULONG_PTR kCopyDataCommand = 0x4F43;  // 'OC'

enum : UINT { kMenuRegion = 2001, kMenuAbout, kMenuExit };

HINSTANCE g_inst = nullptr;
HICON g_iconSmall = nullptr;
HICON g_iconLarge = nullptr;
UINT g_taskbarCreated = 0;
bool g_hotkeyOk = false;

void AddTrayIcon(HWND hwnd, bool notifyHotkeyFailure) {
    NOTIFYICONDATAW nid{sizeof(nid)};
    nid.hWnd = hwnd;
    nid.uID = kTrayId;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon = g_iconSmall;
    wcscpy_s(nid.szTip, L"OpenCapture");
    Shell_NotifyIconW(NIM_ADD, &nid);
    nid.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &nid);

    if (notifyHotkeyFailure) {
        nid.uFlags = NIF_INFO;
        wcscpy_s(nid.szInfoTitle, L"OpenCapture");
        wcscpy_s(nid.szInfo,
                 L"Ctrl+PrtSc is already used by another program, so the hotkey is disabled. "
                 L"Use the tray menu or \"OpenCapture --region\".");
        nid.dwInfoFlags = NIIF_WARNING;
        Shell_NotifyIconW(NIM_MODIFY, &nid);
    }
}

void RemoveTrayIcon(HWND hwnd) {
    NOTIFYICONDATAW nid{sizeof(nid)};
    nid.hWnd = hwnd;
    nid.uID = kTrayId;
    Shell_NotifyIconW(NIM_DELETE, &nid);
}

void StartRegionCapture() {
    if (oc::IsRegionCaptureActive()) return;
    oc::RegionCaptureResult r = oc::CaptureRectangularRegion(g_inst);
    if (r.ok && !r.image.Empty()) {
        HWND v = oc::ShowCaptureViewer(g_inst, std::move(r.image));
        if (v) {
            SendMessageW(v, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_iconSmall));
            SendMessageW(v, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_iconLarge));
        }
    }
}

void ShowTrayMenu(HWND hwnd) {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kMenuRegion,
                g_hotkeyOk ? L"Capture &Rectangular Region\tCtrl+PrtSc" : L"Capture &Rectangular Region");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kMenuAbout, L"&About OpenCapture");
    AppendMenuW(menu, MF_STRING, kMenuExit, L"E&xit");
    SetMenuDefaultItem(menu, kMenuRegion, FALSE);
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);  // required so the menu closes when clicking elsewhere
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

void HandleCommandLine(HWND hwnd, const std::wstring& cmd) {
    if (cmd.find(L"--exit") != std::wstring::npos) {
        PostMessageW(hwnd, WM_CLOSE, 0, 0);
    } else if (cmd.find(L"--region") != std::wstring::npos) {
        // Post instead of running inline so WM_COPYDATA returns immediately.
        PostMessageW(hwnd, WM_START_REGION, 0, 0);
    }
}

LRESULT CALLBACK HostProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == g_taskbarCreated && g_taskbarCreated != 0) {  // Explorer restarted
        AddTrayIcon(hwnd, false);
        return 0;
    }
    switch (msg) {
        case WM_HOTKEY:
            if (wp == kHotkeyRegion) StartRegionCapture();
            return 0;
        case WM_START_REGION:
            StartRegionCapture();
            return 0;
        case WM_TRAY:
            switch (LOWORD(lp)) {
                case NIN_SELECT:
                case NIN_KEYSELECT:
                    PostMessageW(hwnd, WM_START_REGION, 0, 0);
                    break;
                case WM_CONTEXTMENU:
                case WM_RBUTTONUP:
                    ShowTrayMenu(hwnd);
                    break;
            }
            return 0;
        case WM_COMMAND:
            switch (LOWORD(wp)) {
                case kMenuRegion: PostMessageW(hwnd, WM_START_REGION, 0, 0); break;
                case kMenuAbout:
                    MessageBoxW(hwnd,
                                L"OpenCapture " OC_VERSION_W L"\n\nOpen-source screen capture tool.\n"
                                L"Ctrl+PrtSc: capture a rectangular region.\n\nMIT License.",
                                L"About OpenCapture", MB_ICONINFORMATION);
                    break;
                case kMenuExit: PostMessageW(hwnd, WM_CLOSE, 0, 0); break;
            }
            return 0;
        case WM_COPYDATA: {
            const auto* cds = reinterpret_cast<const COPYDATASTRUCT*>(lp);
            if (cds->dwData == kCopyDataCommand && cds->lpData) {
                HandleCommandLine(hwnd, std::wstring(static_cast<const wchar_t*>(cds->lpData),
                                                     cds->cbData / sizeof(wchar_t)));
            }
            return TRUE;
        }
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            UnregisterHotKey(hwnd, kHotkeyRegion);
            RemoveTrayIcon(hwnd);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE inst, HINSTANCE, PWSTR cmdLine, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    g_inst = inst;
    const std::wstring cmd = cmdLine ? cmdLine : L"";

    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Forward the command line to the running instance.
        HWND other = FindWindowW(kHostClass, nullptr);
        if (other && !cmd.empty()) {
            COPYDATASTRUCT cds{kCopyDataCommand, static_cast<DWORD>((cmd.size() + 1) * sizeof(wchar_t)),
                               const_cast<wchar_t*>(cmd.c_str())};
            DWORD_PTR res;
            SendMessageTimeoutW(other, WM_COPYDATA, 0, reinterpret_cast<LPARAM>(&cds), SMTO_ABORTIFHUNG, 3000, &res);
        }
        if (mutex) CloseHandle(mutex);
        return 0;
    }
    if (cmd.find(L"--exit") != std::wstring::npos) {  // nothing running: nothing to do
        if (mutex) CloseHandle(mutex);
        return 0;
    }

    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    g_iconSmall = oc::CreateAppIcon(GetSystemMetricsForDpi(SM_CXSMICON, GetDpiForSystem()));
    g_iconLarge = oc::CreateAppIcon(GetSystemMetricsForDpi(SM_CXICON, GetDpiForSystem()));
    g_taskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");

    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = HostProc;
    wc.hInstance = inst;
    wc.lpszClassName = kHostClass;
    RegisterClassExW(&wc);
    // A hidden top-level window (not message-only) so FindWindow and
    // TaskbarCreated broadcasts reach it.
    HWND host = CreateWindowExW(WS_EX_TOOLWINDOW, kHostClass, L"OpenCapture", WS_POPUP, 0, 0, 0, 0, nullptr,
                                nullptr, inst, nullptr);
    if (!host) return 1;

    g_hotkeyOk = RegisterHotKey(host, kHotkeyRegion, MOD_CONTROL | MOD_NOREPEAT, VK_SNAPSHOT) != FALSE;
    // Don't cover a capture that starts right away with the warning toast.
    AddTrayIcon(host, !g_hotkeyOk && cmd.find(L"--region") == std::wstring::npos);
    HandleCommandLine(host, cmd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    if (g_iconSmall) DestroyIcon(g_iconSmall);
    if (g_iconLarge) DestroyIcon(g_iconLarge);
    CoUninitialize();
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return static_cast<int>(msg.wParam);
}
