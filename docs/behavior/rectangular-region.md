# Rectangular Region capture: behaviour specification

Status: **implemented** (v0.1.0) · Code: `src/region_logic.*` (rules), `src/region_overlay.*` (window)
· Tests: `tests/test_region_logic.cpp` · Evidence: [research notes](../research/2026-11-20-rectangular-region-observations.md)

This is the target behaviour for OpenCapture's Rectangular Region capture. It was
derived by black-box observation of the reference application (see
[clean-room policy](../clean-room.md)). All pixel values are at 96 DPI (100 %
scaling) and are scaled by the monitor DPI.

The visuals only need to be close; the UI will be redesigned later. The
**interaction rules** (states, keys, thresholds, placement) are what must match.

## 1. Starting a capture

| Trigger | Notes |
|---|---|
| `Ctrl+PrtSc` global hotkey | Same default as the reference. If another program already owns it, OpenCapture shows a tray warning and the other triggers still work. |
| Tray icon left-click or tray menu → *Capture Rectangular Region* | |
| `OpenCapture.exe --region` | Starts the app if needed, or forwards the request to the running instance. |

When triggered, the whole virtual screen (all monitors) is grabbed **once** and
shown frozen in a borderless, topmost, full-screen window. Everything the user
sees during selection is drawn on top of that frozen image. The captured pixels
come from the frozen image, never from the overlay graphics.

## 2. The mouse pointer

The system pointer is **hidden** for the whole selection. The XOR crosshair (idle)
or the selection outline (selecting) takes its place.

## 3. States

```
          left down                 release, moved >= 5px (either axis)
  Idle ───────────────▶ Dragging ──────────────────────────────────────▶ Finished
   │                       │ release, moved <= 4px
   │ Enter/Space           ▼
   └─────────────────▶ AnchorPlaced ── left down ──▶ SecondPress ── release ──▶ Finished
                           │ Enter/Space                    (rect still follows the mouse)
                           └──────────────────────────────────────────────────▶ Finished

  Any state: Esc, right click, middle click, mouse wheel ──▶ Cancelled
```

* **Idle** – crosshair follows the pointer; no corner fixed yet.
* **Dragging** – first corner fixed where the left button went down.
* **AnchorPlaced** – first corner fixed by a click (or Enter/Space); the second
  corner follows the mouse with no button held ("click, move, click" mode).
* **SecondPress** – the second click is finished on **release**, not on press;
  moving while the button is held keeps updating the rectangle.
* Finishing in Dragging also works with Enter/Space.

Click-vs-drag threshold: a press/release that moved **4 px or less** on both
axes is a click; **5 px or more** on either axis is a drag.

## 4. Keyboard

| Key | Effect |
|---|---|
| Arrow keys | Move the pointer exactly 1 px (no Shift/Ctrl acceleration). Work in every state. |
| Enter / Space | Idle: fix the first corner at the pointer. Otherwise: accept and capture. |
| Tab | Swap the fixed corner and the moving corner: the pointer jumps to the previously fixed corner, which can then be adjusted. Pressing Tab again swaps back. |
| F1 | Hide / show the magnifier panel. |
| Esc | Cancel. |

## 5. Selection geometry

Corners are **inclusive** pixels. Dragging from x = 600 to x = 1000 selects
401 px. A selection where both corners are the same pixel is 1 × 1. The result
is the normalised rectangle (any drag direction works). The pointer is clamped
to the virtual screen.

## 6. Graphics

### 6.1 XOR colour
Crosshair and selection outline are 1 px lines drawn by **XOR with
RGB(0,255,255)**: red is kept, green and blue are inverted (dark backgrounds turn
cyan, light ones turn red).

### 6.2 Crosshair (Idle)
A full-width horizontal line and full-height vertical line through the pointer,
across the whole virtual screen. Because both lines are XORed, the pixel where
they cross is XORed twice and shows its original colour.

### 6.3 Selection outline (other states)
The rectangle's border pixels, each XORed exactly once (corners included). No
crosshair, no dimming of the outside area.

### 6.4 Size label
Shown in every state that has a fixed corner: text `W x H`, e.g. `401 x 351`.

* Body: background RGB(255,255,175), black text, Segoe UI 9 pt.
  Width = text width + 7 px, height = one text line (15 px).
* 1 px grey RGB(150,150,150) drop shadow to the right and below.
* Not shown inside the magnifier's zoom view.

**Placement** (`cursor` = moving corner, `anchor` = fixed corner,
`dx = cursor.x − anchor.x`, `dy = cursor.y − anchor.y`, `w × h` = body size):

* Horizontal side: *right* if `dx > 0`, or `dx == 0 && dy >= 0`; otherwise *left*.
* Vertical side: *down* if `dy > 0`, or `dy == 0 && dx >= 0`; otherwise *up*.

| Side | Position of the body's top-left |
|---|---|
| right | `x = cursor.x + 14` |
| left | `x = cursor.x − 15 − w` |
| down (label on the right) | `y = cursor.y + 15` |
| down (label on the left) | `y = cursor.y + 30` |
| up | `y = cursor.y − 15 − h` |

Screen edges (the label is *flipped*, not clamped):

* right side and `x + w > screen.right` → `x = cursor.x − w`
* left side and `x < screen.left` → `x = cursor.x − 1` (may start 1 px off-screen)
* down and `y + h > screen.bottom` → `y = cursor.y − h`
* up and `y < screen.top` → `y = cursor.y`
* If **both** axes flip (pointer in a screen corner) the label is **hidden**.

### 6.5 Magnifier / info panel

A 244 × 363 px panel in a corner of the monitor containing the pointer (full
monitor area, so it may cover the taskbar).

Layout (panel coordinates):

| Part | Rect | Look |
|---|---|---|
| Outer frame | whole panel | etched: 1 px white then 1 px grey RGB(160,160,160) on every side |
| Zoom view | (2,2) 240 × 200 | 5× nearest-neighbour zoom of 48 × 40 screen px centred on the pointer (pointer pixel at column 24, row 20). Shows the frozen screen **with** XOR graphics, without the size label. Off-screen area is black. |
| Box | (2,202) to (242,361) | raised 2 px bevel (white top/left, grey bottom/right) |
| Info bar | (4,204) 236 × 18 | RGB(200,200,200), bold navy RGB(0,0,128) text, centred: `X , Y = 705 , 455    W x H = 0 x 0` (screen coordinates of the pointer, selection size or 0 x 0) |
| Help | (4,222) 236 × 137 | RGB(51,103,155), white Segoe UI 9 pt, 8 px left margin, 2 px between items. OpenCapture uses its own wording. |

**Corner rule**: the panel starts **bottom-right**. Whenever the pointer is within
**90 px** of the panel (the panel rectangle grown by 90 px, edges inclusive) it
jumps to the opposite corner (bottom-right ↔ top-left). It stays there until the
pointer approaches it again (hysteresis), and it resets to bottom-right for each
new capture.

## 7. After the selection

On finish the overlay closes and the selected pixels go to the output. The
reference's default output opens its editor; OpenCapture currently opens a
simple capture window (view, scroll, `Ctrl+C` copy, `Ctrl+S` save as PNG / JPEG /
BMP, `Esc` close). Other outputs (clipboard, auto-save, …) are on the
[roadmap](../roadmap.md).

## 8. Not yet verified / assumptions

* **Multiple monitors**: only a single 1920 × 1080 monitor was available. OpenCapture
  covers the whole virtual screen, keeps the label inside the virtual screen and
  puts the panel on the monitor under the pointer. To be checked on a multi-monitor
  setup.
* **High DPI**: sizes and offsets are scaled by monitor DPI; the reference was only
  observed at 100 %.
* Whether right/middle click cancels on press or release (OpenCapture: release,
  so the click never reaches the window underneath).
* Whether the F1 panel state is remembered between captures (OpenCapture: no).
