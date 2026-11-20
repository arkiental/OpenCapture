// Unit tests for src/region_logic.*. Every expectation below is a measurement
// taken from the reference application on a 1920x1080 monitor at 100% scale
// (see docs/research/2026-11-20-rectangular-region-observations.md).
#include "../src/region_logic.h"

#include <cstdio>

using namespace oc;

static int g_failures = 0;
static int g_checks = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        ++g_checks;                                                        \
        if (!(cond)) {                                                     \
            ++g_failures;                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                  \
    } while (0)

static const Rc kScreen{0, 0, 1920, 1080};

static void ExpectLabel(Pt anchor, Pt cursor, int w, int h, int ex, int ey) {
    LabelPlacement p = PlaceSizeLabel(anchor, cursor, w, h, kScreen);
    ++g_checks;
    if (!p.visible || p.topLeft.x != ex || p.topLeft.y != ey) {
        ++g_failures;
        std::printf("FAIL label anchor(%d,%d) cursor(%d,%d): got %s (%d,%d), want (%d,%d)\n", anchor.x, anchor.y,
                    cursor.x, cursor.y, p.visible ? "visible" : "hidden", p.topLeft.x, p.topLeft.y, ex, ey);
    }
}

static void ExpectLabelHidden(Pt anchor, Pt cursor, int w, int h) {
    LabelPlacement p = PlaceSizeLabel(anchor, cursor, w, h, kScreen);
    ++g_checks;
    if (p.visible) {
        ++g_failures;
        std::printf("FAIL label anchor(%d,%d) cursor(%d,%d) should be hidden, got (%d,%d)\n", anchor.x, anchor.y,
                    cursor.x, cursor.y, p.topLeft.x, p.topLeft.y);
    }
}

static void TestRectFromCorners() {
    Rc r = RectFromCorners({600, 300}, {1000, 650});
    CHECK(r.Width() == 401 && r.Height() == 351);  // "401 x 351"
    CHECK(RectFromCorners({1000, 650}, {600, 300}) == r);
    Rc one = RectFromCorners({5, 5}, {5, 5});
    CHECK(one.Width() == 1 && one.Height() == 1);  // "1 x 1"
}

static void TestLabelDirections() {
    const Pt a{1000, 500};
    ExpectLabel(a, {1300, 700}, 54, 15, 1314, 715);  // down-right
    ExpectLabel(a, {700, 300}, 54, 15, 631, 270);    // up-left
    ExpectLabel(a, {1300, 300}, 54, 15, 1314, 270);  // up-right
    ExpectLabel(a, {700, 700}, 54, 15, 631, 730);    // down-left (+30 below)
    ExpectLabel(a, {900, 600}, 54, 15, 831, 630);
    ExpectLabel(a, {999, 501}, 30, 15, 954, 531);
    ExpectLabel(a, {1000, 500}, 30, 15, 1014, 515);  // no movement: down-right
    ExpectLabel(a, {1000, 600}, 42, 15, 1014, 615);  // straight down
    ExpectLabel(a, {1100, 500}, 42, 15, 1114, 515);  // straight right
    ExpectLabel(a, {900, 500}, 42, 15, 843, 470);    // straight left -> up-left
    ExpectLabel(a, {1000, 400}, 42, 15, 943, 370);   // straight up   -> up-left
    ExpectLabel({600, 300}, {1000, 650}, 54, 15, 1014, 665);
}

static void TestLabelEdges() {
    const Pt a{1000, 500};
    // right edge: flips to just left of the cursor
    ExpectLabel(a, {1851, 700}, 54, 15, 1865, 715);
    ExpectLabel(a, {1852, 700}, 54, 15, 1866, 715);
    ExpectLabel(a, {1853, 700}, 54, 15, 1799, 715);
    ExpectLabel(a, {1900, 700}, 54, 15, 1846, 715);
    ExpectLabel(a, {1919, 700}, 54, 15, 1865, 715);
    ExpectLabel({1500, 700}, {1919, 300}, 54, 15, 1865, 270);
    // bottom edge
    ExpectLabel(a, {1300, 1049}, 54, 15, 1314, 1064);
    ExpectLabel(a, {1300, 1050}, 54, 15, 1314, 1065);
    ExpectLabel(a, {1300, 1070}, 54, 15, 1314, 1055);
    ExpectLabel(a, {1300, 1079}, 54, 15, 1314, 1064);
    ExpectLabel(a, {700, 1034}, 54, 15, 631, 1064);
    ExpectLabel(a, {700, 1035}, 54, 15, 631, 1065);
    ExpectLabel(a, {700, 1070}, 54, 15, 631, 1055);
    // left edge
    ExpectLabel(a, {40, 300}, 54, 15, 39, 270);
    ExpectLabel(a, {10, 300}, 54, 15, 9, 270);
    ExpectLabel(a, {0, 300}, 60, 15, -1, 270);  // 1px off-screen: 59px visible
    // top edge
    ExpectLabel(a, {700, 30}, 54, 15, 631, 0);
    ExpectLabel(a, {700, 29}, 54, 15, 631, 29);
    ExpectLabel(a, {700, 5}, 54, 15, 631, 5);
    ExpectLabel(a, {700, 0}, 54, 15, 631, 0);
    // both axes flipped -> hidden
    ExpectLabelHidden(a, {5, 5}, 60, 15);
    ExpectLabelHidden({1500, 700}, {1919, 1079}, 54, 15);
    ExpectLabelHidden({1500, 700}, {0, 0}, 60, 15);
}

static void TestPanelCorner() {
    const int W = 244, H = 363, M = 90;
    CHECK(PanelRect(PanelCorner::BottomRight, kScreen, W, H) == (Rc{1676, 717, 1920, 1080}));
    CHECK(PanelRect(PanelCorner::TopLeft, kScreen, W, H) == (Rc{0, 0, 244, 363}));

    auto br = PanelCorner::BottomRight;
    auto tl = PanelCorner::TopLeft;
    // Approaching the bottom-right panel.
    CHECK(UpdatePanelCorner(br, {1585, 1000}, kScreen, W, H, M) == br);
    CHECK(UpdatePanelCorner(br, {1586, 1000}, kScreen, W, H, M) == tl);
    CHECK(UpdatePanelCorner(br, {1800, 626}, kScreen, W, H, M) == br);
    CHECK(UpdatePanelCorner(br, {1800, 627}, kScreen, W, H, M) == tl);
    CHECK(UpdatePanelCorner(br, {1500, 400}, kScreen, W, H, M) == br);
    // Approaching the top-left panel.
    CHECK(UpdatePanelCorner(tl, {335, 100}, kScreen, W, H, M) == tl);
    CHECK(UpdatePanelCorner(tl, {334, 100}, kScreen, W, H, M) == br);
    CHECK(UpdatePanelCorner(tl, {100, 454}, kScreen, W, H, M) == tl);
    CHECK(UpdatePanelCorner(tl, {100, 453}, kScreen, W, H, M) == br);
    // Hysteresis: far from both, the panel stays wherever it is.
    CHECK(UpdatePanelCorner(tl, {1000, 500}, kScreen, W, H, M) == tl);
    CHECK(UpdatePanelCorner(br, {1000, 500}, kScreen, W, H, M) == br);
}

static void TestDragFinishes() {
    RegionSelector s({600, 300});
    s.LeftDown({600, 300});
    CHECK(s.State() == SelState::Dragging);
    s.MouseMove({800, 500});
    s.LeftUp({1000, 650});
    CHECK(s.State() == SelState::Finished);
    CHECK(s.Selection().Width() == 401 && s.Selection().Height() == 351);
}

static void TestClickThreshold() {
    for (int d = 1; d <= 5; ++d) {
        RegionSelector s({500, 500});
        s.LeftDown({500, 500});
        s.LeftUp({500 + d, 500});
        if (d <= 4)
            CHECK(s.State() == SelState::AnchorPlaced);
        else
            CHECK(s.State() == SelState::Finished);
    }
}

static void TestClickMoveClick() {
    RegionSelector s({502, 302});
    s.LeftDown({502, 302});
    s.LeftUp({502, 302});
    CHECK(s.State() == SelState::AnchorPlaced);
    s.MouseMove({822, 502});
    CHECK(s.Selection().Width() == 321 && s.Selection().Height() == 201);  // "321 x 201"
    s.LeftDown({700, 650});
    CHECK(s.State() == SelState::SecondPress);  // still selecting while held
    s.MouseMove({750, 700});
    CHECK(!s.Done());
    s.LeftUp({750, 700});
    CHECK(s.State() == SelState::Finished);
}

static void TestKeyboardFlow() {
    RegionSelector s({401, 401});
    s.Accept();  // Enter fixes the first corner
    CHECK(s.State() == SelState::AnchorPlaced);
    s.MouseMove({431, 421});  // 30x Right, 20x Down
    CHECK(s.Selection().Width() == 31 && s.Selection().Height() == 21);
    s.Accept();  // Space accepts
    CHECK(s.State() == SelState::Finished);
}

static void TestTabSwap() {
    RegionSelector s({502, 302});
    Pt np;
    CHECK(!s.SwapCorners(&np));  // nothing to swap in Idle
    s.Accept();
    s.MouseMove({829, 505});
    CHECK(s.SwapCorners(&np));
    CHECK(np == (Pt{502, 302}));
    CHECK(s.Anchor() == (Pt{829, 505}));
    s.MouseMove({503, 302});
    CHECK(s.SwapCorners(&np));
    CHECK(np == (Pt{829, 505}));
    CHECK(s.Anchor() == (Pt{503, 302}));
}

static void TestCancel() {
    RegionSelector s({10, 10});
    s.Accept();
    s.MouseMove({100, 100});
    s.Cancel();
    CHECK(s.State() == SelState::Cancelled);
    CHECK(s.Done());
}

int main() {
    TestRectFromCorners();
    TestLabelDirections();
    TestLabelEdges();
    TestPanelCorner();
    TestDragFinishes();
    TestClickThreshold();
    TestClickMoveClick();
    TestKeyboardFlow();
    TestTabSwap();
    TestCancel();
    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
