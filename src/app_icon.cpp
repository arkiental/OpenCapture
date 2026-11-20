#include "app_icon.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace oc {

// A dashed selection rectangle with solid corner brackets over a rounded blue
// tile: "capture a region".
HICON CreateAppIcon(int size) {
    const int n = std::max(size, 16);
    std::vector<uint32_t> px(static_cast<size_t>(n) * n, 0);
    auto put = [&](int x, int y, uint32_t argb) {
        if (x >= 0 && y >= 0 && x < n && y < n) px[static_cast<size_t>(y) * n + x] = argb;
    };
    const uint32_t tile = 0xFF2F6FB5, white = 0xFFFFFFFF, accent = 0xFF7FE6FF;
    const int radius = n / 6;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const int dx = std::max({radius - x, x - (n - 1 - radius), 0});
            const int dy = std::max({radius - y, y - (n - 1 - radius), 0});
            if (dx * dx + dy * dy <= radius * radius) put(x, y, tile);
        }
    const int m = n / 5, l = m, t = m, r = n - 1 - m, b = n - 1 - m;
    const int th = std::max(1, n / 16), dash = std::max(2, n / 8);
    for (int i = l; i <= r; ++i)
        for (int k = 0; k < th; ++k)
            if (((i - l) / dash) % 2 == 0) {
                put(i, t + k, white);
                put(i, b - k, white);
            }
    for (int i = t; i <= b; ++i)
        for (int k = 0; k < th; ++k)
            if (((i - t) / dash) % 2 == 0) {
                put(l + k, i, white);
                put(r - k, i, white);
            }
    const int arm = std::max(3, n / 4), bt = std::max(2, n / 10);
    for (int i = 0; i < arm; ++i)
        for (int k = 0; k < bt; ++k) {
            put(l + i, t + k, accent), put(l + k, t + i, accent);
            put(r - i, b - k, accent), put(r - k, b - i, accent);
        }

    BITMAPV5HEADER bh{};
    bh.bV5Size = sizeof(bh);
    bh.bV5Width = n;
    bh.bV5Height = -n;
    bh.bV5Planes = 1;
    bh.bV5BitCount = 32;
    bh.bV5Compression = BI_BITFIELDS;
    bh.bV5RedMask = 0x00FF0000;
    bh.bV5GreenMask = 0x0000FF00;
    bh.bV5BlueMask = 0x000000FF;
    bh.bV5AlphaMask = 0xFF000000;
    HDC dc = GetDC(nullptr);
    void* bits = nullptr;
    HBITMAP color = CreateDIBSection(dc, reinterpret_cast<BITMAPINFO*>(&bh), DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, dc);
    if (!color) return nullptr;
    std::copy(px.begin(), px.end(), static_cast<uint32_t*>(bits));
    HBITMAP mask = CreateBitmap(n, n, 1, 1, nullptr);
    ICONINFO ii{};
    ii.fIcon = TRUE;
    ii.hbmColor = color;
    ii.hbmMask = mask;
    HICON icon = CreateIconIndirect(&ii);
    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

}  // namespace oc
