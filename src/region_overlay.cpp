#include "region_overlay.h"

#include <shellscalingapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "region_logic.h"

namespace oc {
namespace {

// ---- Visual constants (96 DPI values, measured from the reference) --------
constexpr uint32_t kXorMask = 0x0000FFFF;  // XOR with RGB(0,255,255): G and B inverted

constexpr int kPanelW = 244;
constexpr int kPanelH = 363;
constexpr int kZoomX = 2, kZoomY = 2, kZoomW = 240, kZoomH = 200;
constexpr int kZoomFactor = 5;  // 48 x 40 source pixels
constexpr int kBoxY = 202;      // raised box holding the info bar and help text
constexpr int kInfoH = 18;
constexpr int kPanelMargin = 90;  // cursor distance that makes the panel jump corners
constexpr int kHelpItemGap = 2;  // extra pixels between help items

constexpr COLORREF kWhite = RGB(255, 255, 255);
constexpr COLORREF kFrameGray = RGB(160, 160, 160);
constexpr COLORREF kInfoBack = RGB(200, 200, 200);
constexpr COLORREF kInfoText = RGB(0, 0, 128);
constexpr COLORREF kHelpBack = RGB(51, 103, 155);
constexpr COLORREF kHelpText = RGB(255, 255, 255);
constexpr COLORREF kLabelBack = RGB(255, 255, 175);
constexpr COLORREF kLabelShadow = RGB(150, 150, 150);
constexpr COLORREF kLabelText = RGB(0, 0, 0);

// Our own wording of the on-screen help (same meaning as the reference).
const wchar_t* const kHelpLines[] = {
    L"- F1: hide / show this panel",
    L"- Drag with the left mouse button",
    L"- Or click one corner, then click the opposite corner",
    L"- Enter or Space: fix / accept the corners",
    L"- Arrows: move 1 pixel. Tab: switch corners",
    L"- Esc or right-click: cancel",
};

const wchar_t kClassName[] = L"OpenCapture.RegionOverlay";
bool g_active = false;

int Scale(int v, int dpi) { return MulDiv(v, dpi, 96); }

RECT ToRECT(const Rc& r) { return RECT{r.left, r.top, r.right, r.bottom}; }

class Overlay {
public:
    explicit Overlay(HINSTANCE inst) : inst_(inst) {}
    ~Overlay();

    RegionCaptureResult Run();

    static LRESULT CALLBACK WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);

private:
    LRESULT WndProc(UINT msg, WPARAM wp, LPARAM lp);

    Pt ClientToScreenPt(LPARAM lp) const;
    Pt ClampToScreen(Pt p) const;
    void MoveCursorTo(Pt p);
    void OnPointer(Pt p);
    void Finish(bool ok);

    // Rendering
    void Render();
    void UpdateMonitor(Pt p);
    void RestoreRect(const RECT& r);
    void XorHLine(int y, int x0, int x1);
    void XorVLine(int x, int y0, int y1);
    void XorRectOutline(const Rc& r);
    RECT DrawPanel(Pt cursor);
    RECT DrawSizeLabel();
    void Present(const std::vector<RECT>& rects);
    void EnsureFonts(int dpi);
    void FillRectColor(const RECT& r, COLORREF c);

    HINSTANCE inst_;
    HWND hwnd_ = nullptr;
    RECT virt_{};  // virtual screen
    DibSurface frozen_;
    DibSurface back_;
    RegionSelector sel_;
    bool done_ = false;
    bool ok_ = false;

    PanelCorner corner_ = PanelCorner::BottomRight;
    bool panelVisible_ = true;
    Rc monitor_{};
    int dpi_ = 96;
    std::vector<RECT> drawn_;  // overlay elements currently composited into back_

    int fontDpi_ = 0;
    HFONT labelFont_ = nullptr;
    HFONT infoFont_ = nullptr;
    HFONT helpFont_ = nullptr;
};

Overlay::~Overlay() {
    if (hwnd_) DestroyWindow(hwnd_);
    for (HFONT f : {labelFont_, infoFont_, helpFont_})
        if (f) DeleteObject(f);
}

void Overlay::EnsureFonts(int dpi) {
    if (fontDpi_ == dpi) return;
    for (HFONT* f : {&labelFont_, &infoFont_, &helpFont_}) {
        if (*f) DeleteObject(*f);
        *f = nullptr;
    }
    auto make = [dpi](int pt, int weight, const wchar_t* face) {
        return CreateFontW(-MulDiv(pt, dpi, 72), 0, 0, 0, weight, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                           OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
    };
    labelFont_ = make(9, FW_NORMAL, L"Segoe UI");
    infoFont_ = make(8, FW_BOLD, L"Tahoma");
    helpFont_ = make(9, FW_NORMAL, L"Segoe UI");
    fontDpi_ = dpi;
}

RegionCaptureResult Overlay::Run() {
    RegionCaptureResult result;
    virt_ = VirtualScreenRect();
    if (!GrabScreen(virt_, frozen_)) return result;
    const int w = frozen_.Width(), h = frozen_.Height();
    if (!back_.Create(w, h)) return result;
    std::memcpy(back_.Bits(), frozen_.Bits(), static_cast<size_t>(w) * h * 4);

    POINT cp;
    GetCursorPos(&cp);
    sel_ = RegionSelector(ClampToScreen({cp.x, cp.y}));
    UpdateMonitor(sel_.Cursor());
    // The panel starts bottom-right unless the cursor is already next to it.
    corner_ = UpdatePanelCorner(PanelCorner::BottomRight, sel_.Cursor(), monitor_, Scale(kPanelW, dpi_),
                                Scale(kPanelH, dpi_), Scale(kPanelMargin, dpi_));

    WNDCLASSEXW wc{sizeof(wc)};
    wc.style = CS_DBLCLKS;
    wc.lpfnWndProc = WndProcThunk;
    wc.hInstance = inst_;
    wc.lpszClassName = kClassName;
    RegisterClassExW(&wc);  // fails harmlessly if already registered

    hwnd_ = CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, kClassName, L"OpenCapture region", WS_POPUP,
                            virt_.left, virt_.top, w, h, nullptr, nullptr, inst_, this);
    if (!hwnd_) return result;

    Render();  // compose the first frame before the window appears
    ShowWindow(hwnd_, SW_SHOW);
    SetForegroundWindow(hwnd_);
    SetFocus(hwnd_);
    UpdateWindow(hwnd_);

    MSG msg;
    while (!done_) {
        const BOOL r = GetMessageW(&msg, nullptr, 0, 0);
        if (r == 0) {  // WM_QUIT: let the outer loop see it too
            PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (r < 0) break;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    ShowWindow(hwnd_, SW_HIDE);
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;

    if (ok_) {
        const Rc sel = sel_.Selection();
        result.ok = true;
        result.screenRect = ToRECT(sel);
        result.image = CropToImage(frozen_, sel.left - virt_.left, sel.top - virt_.top, sel.Width(), sel.Height());
    }
    return result;
}

LRESULT CALLBACK Overlay::WndProcThunk(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Overlay* self;
    if (msg == WM_NCCREATE) {
        self = static_cast<Overlay*>(reinterpret_cast<CREATESTRUCTW*>(lp)->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->hwnd_ = hwnd;
    } else {
        self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }
    return self ? self->WndProc(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

Pt Overlay::ClientToScreenPt(LPARAM lp) const {
    return ClampToScreen({GET_X_LPARAM(lp) + virt_.left, GET_Y_LPARAM(lp) + virt_.top});
}

Pt Overlay::ClampToScreen(Pt p) const {
    p.x = std::clamp(p.x, static_cast<int>(virt_.left), static_cast<int>(virt_.right) - 1);
    p.y = std::clamp(p.y, static_cast<int>(virt_.top), static_cast<int>(virt_.bottom) - 1);
    return p;
}

void Overlay::MoveCursorTo(Pt p) {
    p = ClampToScreen(p);
    SetCursorPos(p.x, p.y);
    OnPointer(p);  // don't wait for the synthesized WM_MOUSEMOVE
}

void Overlay::OnPointer(Pt p) {
    sel_.MouseMove(p);
    Render();
}

void Overlay::Finish(bool ok) {
    ok_ = ok;
    done_ = true;
    if (GetCapture() == hwnd_) ReleaseCapture();
}

LRESULT Overlay::WndProc(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
        case WM_SETCURSOR:
            SetCursor(nullptr);  // the XOR crosshair *is* the pointer
            return TRUE;
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(hwnd_, &ps);
            const RECT& r = ps.rcPaint;
            BitBlt(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, back_.Dc(), r.left, r.top, SRCCOPY);
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_MOUSEMOVE:
            OnPointer(ClientToScreenPt(lp));
            return 0;
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
            SetCapture(hwnd_);
            sel_.LeftDown(ClientToScreenPt(lp));
            Render();
            return 0;
        case WM_LBUTTONUP:
            sel_.LeftUp(ClientToScreenPt(lp));
            if (sel_.State() == SelState::Finished) {
                Finish(true);
            } else {
                if (GetCapture() == hwnd_) ReleaseCapture();
                Render();
            }
            return 0;
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            return 0;  // cancel on release so the click doesn't reach the window below
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL:
            sel_.Cancel();
            Finish(false);
            return 0;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN: {
            const Pt c = sel_.Cursor();
            switch (wp) {
                case VK_ESCAPE:
                    sel_.Cancel();
                    Finish(false);
                    return 0;
                case VK_RETURN:
                case VK_SPACE:
                    sel_.Accept();
                    if (sel_.State() == SelState::Finished)
                        Finish(true);
                    else
                        Render();
                    return 0;
                case VK_LEFT: MoveCursorTo({c.x - 1, c.y}); return 0;
                case VK_RIGHT: MoveCursorTo({c.x + 1, c.y}); return 0;
                case VK_UP: MoveCursorTo({c.x, c.y - 1}); return 0;
                case VK_DOWN: MoveCursorTo({c.x, c.y + 1}); return 0;
                case VK_TAB: {
                    Pt np;
                    if (sel_.SwapCorners(&np)) MoveCursorTo(np);
                    return 0;
                }
                case VK_F1:
                    panelVisible_ = !panelVisible_;
                    Render();
                    return 0;
            }
            break;
        }
        case WM_DISPLAYCHANGE:
            sel_.Cancel();
            Finish(false);
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

// ---------------------------------------------------------------------------
// Rendering. `back_` always equals `frozen_` plus the elements listed in
// `drawn_`; each frame restores those areas, draws the new elements and blits
// only the union of old and new areas to the window.
// ---------------------------------------------------------------------------

void Overlay::UpdateMonitor(Pt p) {
    HMONITOR mon = MonitorFromPoint(POINT{p.x, p.y}, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfoW(mon, &mi);
    const Rc newMon{mi.rcMonitor.left, mi.rcMonitor.top, mi.rcMonitor.right, mi.rcMonitor.bottom};
    UINT dx = 96, dy = 96;
    if (FAILED(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &dx, &dy))) dx = 96;
    if (!(newMon == monitor_)) {
        monitor_ = newMon;
        dpi_ = static_cast<int>(dx);
    }
}

void Overlay::RestoreRect(const RECT& r) {
    const int x0 = std::max(0L, r.left), y0 = std::max(0L, r.top);
    const int x1 = std::min<long>(back_.Width(), r.right), y1 = std::min<long>(back_.Height(), r.bottom);
    if (x1 <= x0) return;
    for (int y = y0; y < y1; ++y)
        std::memcpy(back_.Row(y) + x0, frozen_.Row(y) + x0, static_cast<size_t>(x1 - x0) * 4);
}

void Overlay::XorHLine(int y, int x0, int x1) {
    if (y < 0 || y >= back_.Height()) return;
    x0 = std::max(x0, 0);
    x1 = std::min(x1, back_.Width());
    uint32_t* row = back_.Row(y);
    for (int x = x0; x < x1; ++x) row[x] ^= kXorMask;
}

void Overlay::XorVLine(int x, int y0, int y1) {
    if (x < 0 || x >= back_.Width()) return;
    y0 = std::max(y0, 0);
    y1 = std::min(y1, back_.Height());
    for (int y = y0; y < y1; ++y) back_.Row(y)[x] ^= kXorMask;
}

// XOR every outline pixel exactly once (corners included).
void Overlay::XorRectOutline(const Rc& r) {
    XorHLine(r.top, r.left, r.right);
    if (r.Height() > 1) XorHLine(r.bottom - 1, r.left, r.right);
    if (r.Height() > 2) {
        XorVLine(r.left, r.top + 1, r.bottom - 1);
        if (r.Width() > 1) XorVLine(r.right - 1, r.top + 1, r.bottom - 1);
    }
}

void Overlay::FillRectColor(const RECT& r, COLORREF c) {
    SetDCBrushColor(back_.Dc(), c);
    FillRect(back_.Dc(), &r, static_cast<HBRUSH>(GetStockObject(DC_BRUSH)));
}

RECT Overlay::DrawPanel(Pt cursor) {
    const int s = dpi_;
    const int W = Scale(kPanelW, s), H = Scale(kPanelH, s);
    const Rc pr = PanelRect(corner_, monitor_, W, H);
    const int ox = pr.left - virt_.left, oy = pr.top - virt_.top;
    auto R = [&](int l, int t, int r, int b) { return RECT{ox + l, oy + t, ox + r, oy + b}; };
    const int one = std::max(1, Scale(1, s)), two = std::max(2, Scale(2, s));

    // Etched outer frame: white then gray on every side.
    FillRectColor(R(0, 0, W, H), kFrameGray);
    FillRectColor(R(0, 0, W, one), kWhite);
    FillRectColor(R(0, 0, one, H), kWhite);
    FillRectColor(R(W - two, 0, W - one, H), kWhite);
    FillRectColor(R(0, H - two, W, H - one), kWhite);

    // Zoomed view: kZoomFactor x nearest-neighbour, black outside the screen.
    const int zx = Scale(kZoomX, s), zy = Scale(kZoomY, s);
    const int zw = Scale(kZoomW, s), zh = Scale(kZoomH, s);
    const int srcW = zw / kZoomFactor, srcH = zh / kZoomFactor;
    const int srcX = cursor.x - virt_.left - srcW / 2, srcY = cursor.y - virt_.top - srcH / 2;
    // Copy the source block first: the panel never overlaps it (90px margin),
    // but reading before writing keeps that independent of the layout.
    std::vector<uint32_t> src(static_cast<size_t>(srcW) * srcH, 0);
    for (int y = 0; y < srcH; ++y) {
        const int by = srcY + y;
        if (by < 0 || by >= back_.Height()) continue;
        for (int x = 0; x < srcW; ++x) {
            const int bx = srcX + x;
            if (bx >= 0 && bx < back_.Width()) src[static_cast<size_t>(y) * srcW + x] = back_.Row(by)[bx];
        }
    }
    for (int y = 0; y < srcH * kZoomFactor; ++y) {
        const int dy = oy + zy + y;
        if (dy < 0 || dy >= back_.Height()) continue;
        uint32_t* row = back_.Row(dy);
        const uint32_t* srow = src.data() + static_cast<size_t>(y / kZoomFactor) * srcW;
        for (int x = 0; x < srcW * kZoomFactor; ++x) {
            const int dx = ox + zx + x;
            if (dx >= 0 && dx < back_.Width()) row[dx] = srow[x / kZoomFactor];
        }
    }

    // Raised box holding the info bar and the help text.
    const int by = Scale(kBoxY, s), bb = H - two;
    FillRectColor(R(zx, by, zx + zw, bb), kFrameGray);
    FillRectColor(R(zx, by, zx + zw - two, by + two), kWhite);
    FillRectColor(R(zx, by, zx + two, bb - two), kWhite);
    const int ix0 = zx + two, ix1 = zx + zw - two;
    const int iy0 = by + two, iy1 = iy0 + Scale(kInfoH, s);
    const RECT info = R(ix0, iy0, ix1, iy1);
    const RECT help = R(ix0, iy1, ix1, bb - two);
    FillRectColor(info, kInfoBack);
    FillRectColor(help, kHelpBack);

    EnsureFonts(s);
    HDC dc = back_.Dc();
    SetBkMode(dc, TRANSPARENT);

    wchar_t buf[128];
    int selW = 0, selH = 0;
    if (sel_.HasAnchor()) {
        const Rc r = sel_.Selection();
        selW = r.Width();
        selH = r.Height();
    }
    std::swprintf(buf, 128, L"X , Y = %d , %d    W x H = %d x %d", cursor.x, cursor.y, selW, selH);
    HGDIOBJ oldFont = SelectObject(dc, infoFont_);
    SetTextColor(dc, kInfoText);
    RECT ir = info;
    DrawTextW(dc, buf, -1, &ir, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

    SelectObject(dc, helpFont_);
    SetTextColor(dc, kHelpText);
    RECT hr = help;
    hr.left += Scale(8, s);
    hr.right -= Scale(4, s);
    hr.top += Scale(3, s);
    for (const wchar_t* line : kHelpLines) {
        RECT calc = hr;
        DrawTextW(dc, line, -1, &calc, DT_WORDBREAK | DT_NOPREFIX | DT_CALCRECT);
        DrawTextW(dc, line, -1, &hr, DT_WORDBREAK | DT_NOPREFIX);
        hr.top += calc.bottom - calc.top + Scale(kHelpItemGap, s);
        if (hr.top >= hr.bottom) break;
    }
    SelectObject(dc, oldFont);
    return R(0, 0, W, H);
}

RECT Overlay::DrawSizeLabel() {
    const Rc sel = sel_.Selection();
    wchar_t text[64];
    std::swprintf(text, 64, L"%d x %d", sel.Width(), sel.Height());

    EnsureFonts(dpi_);
    HDC dc = back_.Dc();
    HGDIOBJ oldFont = SelectObject(dc, labelFont_);
    SIZE ext{};
    GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &ext);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    // Body = text extent + 7px wide, exactly one text line high (measured).
    const int padX = Scale(4, dpi_), padY = 0;
    const int w = ext.cx + Scale(7, dpi_);
    const int h = tm.tmHeight;

    const Rc bounds{virt_.left, virt_.top, virt_.right, virt_.bottom};
    const LabelPlacement p = PlaceSizeLabel(sel_.Anchor(), sel_.Cursor(), w, h, bounds, ScaledLabelMetrics(dpi_));
    if (!p.visible) {
        SelectObject(dc, oldFont);
        return RECT{0, 0, 0, 0};
    }
    const int x = p.topLeft.x - virt_.left, y = p.topLeft.y - virt_.top;
    const int sh = std::max(1, Scale(1, dpi_));
    FillRectColor(RECT{x + sh, y + sh, x + w + sh, y + h + sh}, kLabelShadow);
    FillRectColor(RECT{x, y, x + w, y + h}, kLabelBack);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, kLabelText);
    TextOutW(dc, x + padX, y + padY, text, static_cast<int>(wcslen(text)));
    SelectObject(dc, oldFont);
    return RECT{x, y, x + w + sh, y + h + sh};
}

void Overlay::Render() {
    const Pt c = sel_.Cursor();
    UpdateMonitor(c);
    corner_ = UpdatePanelCorner(corner_, c, monitor_, Scale(kPanelW, dpi_), Scale(kPanelH, dpi_),
                                Scale(kPanelMargin, dpi_));

    std::vector<RECT> dirty = drawn_;
    for (const RECT& r : drawn_) RestoreRect(r);
    drawn_.clear();

    const int bw = back_.Width(), bh = back_.Height();
    const int cx = c.x - virt_.left, cy = c.y - virt_.top;

    // 1) XOR graphics: crosshair while idle, selection outline afterwards.
    if (!sel_.HasAnchor()) {
        XorHLine(cy, 0, bw);
        XorVLine(cx, 0, bh);
        drawn_.push_back(RECT{0, cy, bw, cy + 1});
        drawn_.push_back(RECT{cx, 0, cx + 1, bh});
    } else {
        Rc r = sel_.Selection();
        r = {r.left - virt_.left, r.top - virt_.top, r.right - virt_.left, r.bottom - virt_.top};
        XorRectOutline(r);
        drawn_.push_back(RECT{r.left, r.top, r.right, r.top + 1});
        drawn_.push_back(RECT{r.left, r.bottom - 1, r.right, r.bottom});
        drawn_.push_back(RECT{r.left, r.top, r.left + 1, r.bottom});
        drawn_.push_back(RECT{r.right - 1, r.top, r.right, r.bottom});
    }

    // 2) Magnifier panel (its zoom samples the XOR graphics, not the label).
    if (panelVisible_) drawn_.push_back(DrawPanel(c));

    // 3) Size label on top.
    if (sel_.HasAnchor()) {
        const RECT lr = DrawSizeLabel();
        if (lr.right > lr.left) drawn_.push_back(lr);
    }

    dirty.insert(dirty.end(), drawn_.begin(), drawn_.end());
    if (hwnd_ && IsWindowVisible(hwnd_)) Present(dirty);
}

void Overlay::Present(const std::vector<RECT>& rects) {
    GdiFlush();
    HDC dc = GetDC(hwnd_);
    for (const RECT& r : rects) {
        if (r.right <= r.left || r.bottom <= r.top) continue;
        BitBlt(dc, r.left, r.top, r.right - r.left, r.bottom - r.top, back_.Dc(), r.left, r.top, SRCCOPY);
    }
    ReleaseDC(hwnd_, dc);
}

}  // namespace

bool IsRegionCaptureActive() { return g_active; }

RegionCaptureResult CaptureRectangularRegion(HINSTANCE instance) {
    if (g_active) return {};
    g_active = true;
    RegionCaptureResult r;
    {
        Overlay overlay(instance);
        r = overlay.Run();
    }
    g_active = false;
    return r;
}

}  // namespace oc
