#include "region_logic.h"

#include <algorithm>

namespace oc {

Rc RectFromCorners(Pt a, Pt b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::max(a.x, b.x) + 1, std::max(a.y, b.y) + 1};
}

static int ScaleDpi(int v, int dpi) { return (v * dpi + 48) / 96; }

LabelMetrics ScaledLabelMetrics(int dpi) {
    LabelMetrics m;
    m.offsetRight = ScaleDpi(m.offsetRight, dpi);
    m.offsetLeft = ScaleDpi(m.offsetLeft, dpi);
    m.offsetDown = ScaleDpi(m.offsetDown, dpi);
    m.offsetDownLeft = ScaleDpi(m.offsetDownLeft, dpi);
    m.offsetUp = ScaleDpi(m.offsetUp, dpi);
    return m;
}

LabelPlacement PlaceSizeLabel(Pt anchor, Pt cursor, int w, int h, const Rc& bounds, const LabelMetrics& m) {
    const int dx = cursor.x - anchor.x;
    const int dy = cursor.y - anchor.y;

    // The label sits diagonally "outside" the moving corner. When the cursor
    // is level with the anchor on one axis, the other axis breaks the tie
    // (observed: moving straight up puts the label up-left, straight left
    // puts it up-left, straight right/down puts it down-right).
    const bool right = dx > 0 || (dx == 0 && dy >= 0);
    const bool down = dy > 0 || (dy == 0 && dx >= 0);

    int x = right ? cursor.x + m.offsetRight : cursor.x - m.offsetLeft - w;
    int y = down ? (right ? cursor.y + m.offsetDown : cursor.y + m.offsetDownLeft) : cursor.y - m.offsetUp - h;

    // If the label would leave the screen it is flipped onto the cursor
    // column/row instead of being clamped.
    bool flippedX = false, flippedY = false;
    if (right && x + w > bounds.right) {
        x = cursor.x - w;
        flippedX = true;
    } else if (!right && x < bounds.left) {
        x = cursor.x - 1;  // not clamped: may start 1px off-screen (measured)
        flippedX = true;
    }
    if (down && y + h > bounds.bottom) {
        y = cursor.y - h;
        flippedY = true;
    } else if (!down && y < bounds.top) {
        y = cursor.y;
        flippedY = true;
    }

    // Flipping on both axes would put the label on top of the cursor; the
    // reference simply hides it in that case (e.g. in a screen corner).
    LabelPlacement out;
    out.visible = !(flippedX && flippedY);
    out.topLeft = {x, y};
    return out;
}

Rc PanelRect(PanelCorner corner, const Rc& mon, int w, int h) {
    if (corner == PanelCorner::TopLeft) return {mon.left, mon.top, mon.left + w, mon.top + h};
    return {mon.right - w, mon.bottom - h, mon.right, mon.bottom};
}

static bool NearPanel(const Rc& panel, Pt p, int margin) {
    const Rc z = panel.Inflated(margin);
    // Inclusive on all four edges (measured: right/bottom edges count too).
    return p.x >= z.left && p.x <= z.right && p.y >= z.top && p.y <= z.bottom;
}

PanelCorner UpdatePanelCorner(PanelCorner current, Pt cursor, const Rc& mon, int w, int h, int margin) {
    if (NearPanel(PanelRect(current, mon, w, h), cursor, margin)) {
        return current == PanelCorner::BottomRight ? PanelCorner::TopLeft : PanelCorner::BottomRight;
    }
    return current;
}

void RegionSelector::MouseMove(Pt p) {
    if (Done()) return;
    cursor_ = p;
}

void RegionSelector::LeftDown(Pt p) {
    cursor_ = p;
    switch (state_) {
        case SelState::Idle:
            anchor_ = p;
            pressAt_ = p;
            state_ = SelState::Dragging;
            break;
        case SelState::AnchorPlaced:
            state_ = SelState::SecondPress;
            break;
        default:
            break;
    }
}

void RegionSelector::LeftUp(Pt p) {
    cursor_ = p;
    switch (state_) {
        case SelState::Dragging: {
            const bool moved = std::abs(p.x - pressAt_.x) >= kDragThreshold ||
                               std::abs(p.y - pressAt_.y) >= kDragThreshold;
            state_ = moved ? SelState::Finished : SelState::AnchorPlaced;
            break;
        }
        case SelState::SecondPress:
            state_ = SelState::Finished;
            break;
        default:
            break;
    }
}

void RegionSelector::Accept() {
    switch (state_) {
        case SelState::Idle:
            anchor_ = cursor_;
            state_ = SelState::AnchorPlaced;
            break;
        case SelState::Dragging:
        case SelState::AnchorPlaced:
        case SelState::SecondPress:
            state_ = SelState::Finished;
            break;
        default:
            break;
    }
}

bool RegionSelector::SwapCorners(Pt* newCursor) {
    if (!HasAnchor() || Done()) return false;
    std::swap(anchor_, cursor_);
    if (newCursor) *newCursor = cursor_;
    return true;
}

void RegionSelector::Cancel() {
    if (state_ != SelState::Finished) state_ = SelState::Cancelled;
}

}  // namespace oc
