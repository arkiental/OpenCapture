// OpenCapture - pure (Win32-free) logic for the Rectangular Region capture.
//
// Everything in this file is deterministic geometry / state handling so it can
// be unit tested without a desktop. The numbers come from black-box
// observation of the reference application; see
// docs/behavior/rectangular-region.md for the measurements behind each rule.
#pragma once

#include <cstdlib>

namespace oc {

struct Pt {
    int x = 0;
    int y = 0;
    friend bool operator==(Pt a, Pt b) { return a.x == b.x && a.y == b.y; }
};

// Half-open rectangle: [left, right) x [top, bottom).
struct Rc {
    int left = 0, top = 0, right = 0, bottom = 0;

    int Width() const { return right - left; }
    int Height() const { return bottom - top; }
    bool Empty() const { return right <= left || bottom <= top; }
    bool Contains(Pt p) const { return p.x >= left && p.x < right && p.y >= top && p.y < bottom; }
    Rc Inflated(int d) const { return {left - d, top - d, right + d, bottom + d}; }
    friend bool operator==(const Rc& a, const Rc& b) {
        return a.left == b.left && a.top == b.top && a.right == b.right && a.bottom == b.bottom;
    }
};

// Selection between two *inclusive* corner pixels. Dragging from x=600 to
// x=1000 selects 401 pixels, exactly like the reference.
Rc RectFromCorners(Pt a, Pt b);

// ---------------------------------------------------------------------------
// Size label ("401 x 351") that follows the moving corner.
// ---------------------------------------------------------------------------
struct LabelMetrics {
    int offsetRight = 14;     // gap right of the cursor when the label is on the right
    int offsetLeft = 15;      // gap left of the cursor when the label is on the left
    int offsetDown = 15;      // gap below the cursor (label on the right)
    int offsetDownLeft = 30;  // gap below the cursor (label on the left)
    int offsetUp = 15;        // gap above the cursor
};

LabelMetrics ScaledLabelMetrics(int dpi);

struct LabelPlacement {
    bool visible = false;
    Pt topLeft;  // top-left of the label body (the 1px drop shadow is extra)
};

// `bodyW`/`bodyH` is the label body size without its shadow, `bounds` the
// area the label must stay inside (the whole overlay / virtual screen).
LabelPlacement PlaceSizeLabel(Pt anchor, Pt cursor, int bodyW, int bodyH, const Rc& bounds,
                              const LabelMetrics& m = LabelMetrics{});

// ---------------------------------------------------------------------------
// Magnifier / info panel that sits in a monitor corner and dodges the cursor.
// ---------------------------------------------------------------------------
enum class PanelCorner { BottomRight, TopLeft };

Rc PanelRect(PanelCorner corner, const Rc& monitor, int panelW, int panelH);

// The panel jumps to the opposite corner as soon as the cursor comes within
// `margin` pixels of it (inclusive on every edge, as measured). It stays in the
// new corner until the cursor approaches it again (hysteresis).
PanelCorner UpdatePanelCorner(PanelCorner current, Pt cursor, const Rc& monitor, int panelW,
                              int panelH, int margin);

// ---------------------------------------------------------------------------
// Selection state machine (mouse + keyboard), independent of any window.
// ---------------------------------------------------------------------------
enum class SelState {
    Idle,          // crosshair, no corner fixed yet
    Dragging,      // left button held after fixing the first corner
    AnchorPlaced,  // first corner fixed by a click / Enter, waiting for the second
    SecondPress,   // left button held for the second corner (finishes on release)
    Finished,
    Cancelled,
};

class RegionSelector {
public:
    // A press-move-release shorter than this (in both axes) is treated as a
    // click that only fixes the first corner. Measured: 4px -> click, 5px -> drag.
    static constexpr int kDragThreshold = 5;

    explicit RegionSelector(Pt start = {}) : cursor_(start) {}

    void MouseMove(Pt p);
    void LeftDown(Pt p);
    void LeftUp(Pt p);
    void Accept();  // Enter / Space
    // Tab: swap the fixed and the moving corner. Returns true (and the new
    // cursor position in *newCursor) when the swap happened.
    bool SwapCorners(Pt* newCursor);
    void Cancel();  // Esc, right/middle click, mouse wheel

    SelState State() const { return state_; }
    bool HasAnchor() const {
        return state_ == SelState::Dragging || state_ == SelState::AnchorPlaced ||
               state_ == SelState::SecondPress || state_ == SelState::Finished;
    }
    bool Done() const { return state_ == SelState::Finished || state_ == SelState::Cancelled; }
    Pt Anchor() const { return anchor_; }
    Pt Cursor() const { return cursor_; }
    // Current selection (valid when HasAnchor()).
    Rc Selection() const { return RectFromCorners(anchor_, cursor_); }

private:
    SelState state_ = SelState::Idle;
    Pt anchor_;
    Pt cursor_;
    Pt pressAt_;
};

}  // namespace oc
