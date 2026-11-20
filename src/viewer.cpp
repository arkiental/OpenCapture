#include "viewer.h"

#include <commdlg.h>

#include <algorithm>
#include <cstdio>
#include <string>

#include "image_io.h"

namespace oc {
namespace {

const wchar_t kViewerClass[] = L"OpenCapture.Viewer";
constexpr COLORREF kBackground = RGB(200, 200, 200);
constexpr int kStatusH = 22;  // 96-DPI height of the status strip

enum : UINT { kCmdSave = 1001, kCmdCopy, kCmdClose };

int g_captureCounter = 0;

std::wstring DefaultFileName() {
    SYSTEMTIME t;
    GetLocalTime(&t);
    wchar_t buf[64];
    swprintf(buf, 64, L"Capture_%04u-%02u-%02u_%02u%02u%02u.png", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
             t.wSecond);
    return buf;
}

class Viewer {
public:
    Viewer(Image img, int number) : img_(std::move(img)), number_(number) {}

    static LRESULT CALLBACK Proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
        Viewer* self;
        if (msg == WM_NCCREATE) {
            self = static_cast<Viewer*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
            self->hwnd_ = hwnd;
        } else {
            self = reinterpret_cast<Viewer*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }
        if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
        const LRESULT r = self->Handle(msg, wp, lp);
        if (msg == WM_NCDESTROY) delete self;
        return r;
    }

    std::wstring Title() const {
        wchar_t buf[128];
        swprintf(buf, 128, L"Capture %d (%d x %d) - OpenCapture", number_, img_.width, img_.height);
        return buf;
    }

    const Image& Img() const { return img_; }

private:
    int Dpi() const { return static_cast<int>(GetDpiForWindow(hwnd_)); }
    int StatusH() const { return MulDiv(kStatusH, Dpi(), 96); }

    RECT ViewArea() const {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        rc.bottom = std::max(rc.top, rc.bottom - StatusH());
        return rc;
    }

    void UpdateScrollBars() {
        const RECT va = ViewArea();
        const int vw = va.right - va.left, vh = va.bottom - va.top;
        SCROLLINFO si{sizeof(si)};
        si.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
        si.nMin = 0;
        si.nMax = img_.width - 1;
        si.nPage = static_cast<UINT>(std::max(vw, 0));
        scrollX_ = std::clamp(scrollX_, 0, std::max(0, img_.width - vw));
        si.nPos = scrollX_;
        SetScrollInfo(hwnd_, SB_HORZ, &si, TRUE);
        si.nMax = img_.height - 1;
        si.nPage = static_cast<UINT>(std::max(vh, 0));
        scrollY_ = std::clamp(scrollY_, 0, std::max(0, img_.height - vh));
        si.nPos = scrollY_;
        SetScrollInfo(hwnd_, SB_VERT, &si, TRUE);
    }

    void Scroll(int bar, int code) {
        SCROLLINFO si{sizeof(si)};
        si.fMask = SIF_ALL;
        GetScrollInfo(hwnd_, bar, &si);
        int pos = si.nPos;
        const int line = MulDiv(20, Dpi(), 96);
        switch (code) {
            case SB_LINEUP: pos -= line; break;
            case SB_LINEDOWN: pos += line; break;
            case SB_PAGEUP: pos -= static_cast<int>(si.nPage); break;
            case SB_PAGEDOWN: pos += static_cast<int>(si.nPage); break;
            case SB_THUMBTRACK:
            case SB_THUMBPOSITION: pos = si.nTrackPos; break;
            case SB_TOP: pos = si.nMin; break;
            case SB_BOTTOM: pos = si.nMax; break;
        }
        (bar == SB_HORZ ? scrollX_ : scrollY_) = pos;
        UpdateScrollBars();
        InvalidateRect(hwnd_, nullptr, FALSE);
    }

    void Paint(HDC dc) {
        RECT rc;
        GetClientRect(hwnd_, &rc);
        const int w = rc.right, h = rc.bottom;
        if (w <= 0 || h <= 0) return;
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, w, h);
        HGDIOBJ old = SelectObject(mem, bmp);

        SetDCBrushColor(mem, kBackground);
        FillRect(mem, &rc, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));

        const RECT va = ViewArea();
        const int vw = va.right - va.left, vh = va.bottom - va.top;
        const int x = img_.width < vw ? (vw - img_.width) / 2 : -scrollX_;
        const int y = img_.height < vh ? (vh - img_.height) / 2 : -scrollY_;
        BITMAPINFO bi{};
        bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bi.bmiHeader.biWidth = img_.width;
        bi.bmiHeader.biHeight = -img_.height;
        bi.bmiHeader.biPlanes = 1;
        bi.bmiHeader.biBitCount = 32;
        bi.bmiHeader.biCompression = BI_RGB;
        IntersectClipRect(mem, va.left, va.top, va.right, va.bottom);
        SetDIBitsToDevice(mem, x, y, img_.width, img_.height, 0, 0, 0, img_.height, img_.pixels.data(), &bi,
                          DIB_RGB_COLORS);
        SelectClipRgn(mem, nullptr);

        // Status strip.
        RECT sr{0, va.bottom, w, h};
        SetDCBrushColor(mem, GetSysColor(COLOR_BTNFACE));
        FillRect(mem, &sr, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
        NONCLIENTMETRICSW ncm{sizeof(ncm)};
        SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0, Dpi());
        HFONT font = CreateFontIndirectW(&ncm.lfStatusFont);
        HGDIOBJ oldFont = SelectObject(mem, font);
        SetBkMode(mem, TRANSPARENT);
        SetTextColor(mem, GetSysColor(COLOR_BTNTEXT));
        wchar_t status[128];
        swprintf(status, 128, L"Size: %d x %d", img_.width, img_.height);
        sr.left += MulDiv(8, Dpi(), 96);
        DrawTextW(mem, status, -1, &sr, DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        SelectObject(mem, oldFont);
        DeleteObject(font);

        BitBlt(dc, 0, 0, w, h, mem, 0, 0, SRCCOPY);
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
    }

    void SaveAs() {
        wchar_t file[MAX_PATH];
        wcsncpy(file, DefaultFileName().c_str(), MAX_PATH);
        file[MAX_PATH - 1] = 0;
        OPENFILENAMEW ofn{sizeof(ofn)};
        ofn.hwndOwner = hwnd_;
        ofn.lpstrFilter = L"PNG image (*.png)\0*.png\0JPEG image (*.jpg)\0*.jpg;*.jpeg\0Bitmap (*.bmp)\0*.bmp\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFile = file;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrDefExt = L"png";
        ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
        if (!GetSaveFileNameW(&ofn)) return;
        std::wstring path = file;
        ImageFormat fmt = ofn.nFilterIndex == 2 ? ImageFormat::Jpeg
                          : ofn.nFilterIndex == 3 ? ImageFormat::Bmp
                                                  : FormatFromPath(path);
        std::wstring err;
        if (!SaveImage(img_, path, fmt, 90, &err)) {
            MessageBoxW(hwnd_, (L"Could not save the image:\n" + err).c_str(), L"OpenCapture", MB_ICONERROR);
        }
    }

    LRESULT Handle(UINT msg, WPARAM wp, LPARAM lp) {
        switch (msg) {
            case WM_CREATE:
                UpdateScrollBars();
                return 0;
            case WM_SIZE:
                UpdateScrollBars();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            case WM_ERASEBKGND:
                return 1;
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC dc = BeginPaint(hwnd_, &ps);
                Paint(dc);
                EndPaint(hwnd_, &ps);
                return 0;
            }
            case WM_HSCROLL:
                Scroll(SB_HORZ, LOWORD(wp));
                return 0;
            case WM_VSCROLL:
                Scroll(SB_VERT, LOWORD(wp));
                return 0;
            case WM_MOUSEWHEEL: {
                const int delta = GET_WHEEL_DELTA_WPARAM(wp);
                const bool horiz = (GET_KEYSTATE_WPARAM(wp) & MK_SHIFT) != 0;
                (horiz ? scrollX_ : scrollY_) -= MulDiv(delta, MulDiv(60, Dpi(), 96), WHEEL_DELTA);
                UpdateScrollBars();
                InvalidateRect(hwnd_, nullptr, FALSE);
                return 0;
            }
            case WM_KEYDOWN: {
                const bool ctrl = GetKeyState(VK_CONTROL) < 0;
                if (wp == VK_ESCAPE) PostMessageW(hwnd_, WM_CLOSE, 0, 0);
                else if (ctrl && wp == 'S') SaveAs();
                else if (ctrl && wp == 'C') CopyImageToClipboard(hwnd_, img_);
                return 0;
            }
            case WM_COMMAND:
                switch (LOWORD(wp)) {
                    case kCmdSave: SaveAs(); break;
                    case kCmdCopy: CopyImageToClipboard(hwnd_, img_); break;
                    case kCmdClose: PostMessageW(hwnd_, WM_CLOSE, 0, 0); break;
                }
                return 0;
            case WM_DPICHANGED: {
                const RECT* r = reinterpret_cast<const RECT*>(lp);
                SetWindowPos(hwnd_, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                             SWP_NOZORDER | SWP_NOACTIVATE);
                return 0;
            }
        }
        return DefWindowProcW(hwnd_, msg, wp, lp);
    }

    HWND hwnd_ = nullptr;
    Image img_;
    int number_;
    int scrollX_ = 0;
    int scrollY_ = 0;
};

HMENU BuildMenu() {
    HMENU file = CreatePopupMenu();
    AppendMenuW(file, MF_STRING, kCmdSave, L"&Save As...\tCtrl+S");
    AppendMenuW(file, MF_STRING, kCmdCopy, L"&Copy to Clipboard\tCtrl+C");
    AppendMenuW(file, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file, MF_STRING, kCmdClose, L"&Close\tEsc");
    HMENU bar = CreateMenu();
    AppendMenuW(bar, MF_POPUP, reinterpret_cast<UINT_PTR>(file), L"&File");
    return bar;
}

}  // namespace

HWND ShowCaptureViewer(HINSTANCE instance, Image img) {
    static bool registered = false;
    if (!registered) {
        WNDCLASSEXW wc{sizeof(wc)};
        wc.lpfnWndProc = Viewer::Proc;
        wc.hInstance = instance;
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hIcon = LoadIcon(instance, MAKEINTRESOURCE(1));
        wc.lpszClassName = kViewerClass;
        RegisterClassExW(&wc);
        registered = true;
    }

    auto* viewer = new Viewer(std::move(img), ++g_captureCounter);
    const Image& im = viewer->Img();

    // Size the window around the image, limited to 90% of the work area of
    // the monitor under the cursor, and centre it there.
    POINT cp;
    GetCursorPos(&cp);
    HMONITOR mon = MonitorFromPoint(cp, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    const RECT wa = mi.rcWork;
    const UINT dpi = GetDpiForSystem();
    const DWORD style = WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL;
    RECT r{0, 0, std::max(im.width + 80, 420), im.height + 80 + MulDiv(kStatusH, dpi, 96)};
    AdjustWindowRectExForDpi(&r, style, TRUE, 0, dpi);
    const int maxW = (wa.right - wa.left) * 9 / 10, maxH = (wa.bottom - wa.top) * 9 / 10;
    const int w = std::min<int>(r.right - r.left, maxW), h = std::min<int>(r.bottom - r.top, maxH);
    const int x = wa.left + ((wa.right - wa.left) - w) / 2, y = wa.top + ((wa.bottom - wa.top) - h) / 2;

    const std::wstring title = viewer->Title();
    HWND hwnd = CreateWindowExW(0, kViewerClass, title.c_str(), style, x, y, w, h, nullptr, BuildMenu(), instance,
                                viewer);
    if (!hwnd) {
        delete viewer;
        return nullptr;
    }
    ShowWindow(hwnd, SW_SHOWNORMAL);
    SetForegroundWindow(hwnd);
    return hwnd;
}

}  // namespace oc
