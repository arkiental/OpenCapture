# Research log: Rectangular Region capture (2026-11-20)

Reference: FastStone Capture 11.3 (portable), Windows 11, one 1920 × 1080 monitor
at 100 % scaling. Method: black-box only (see [clean-room policy](../clean-room.md)).
The result is written up as a spec in
[behavior/rectangular-region.md](../behavior/rectangular-region.md); this file
keeps the raw evidence.

## Method

A PowerShell probe (not part of the repo) did the following:

1. Moved the mouse with `SetCursorPos` / `mouse_event` and pressed keys with
   `keybd_event` (Ctrl+PrtSc starts the capture).
2. Took screenshots with `Graphics.CopyFromScreen` (the hidden pointer is not in
   them; the overlay window is).
3. Read pixels and searched for known colours (label yellow, help-panel blue)
   to locate elements, then swept positions to find exact thresholds.
4. Read foreground-window class/style and `GetCursorInfo` to learn about the
   overlay window and pointer.

No screenshots are committed: they show a private desktop and the reference's UI.

## Raw observations

### Overlay window
* Full virtual-screen popup: style `0x96000000` (`WS_POPUP | WS_VISIBLE |
  WS_CLIPSIBLINGS | WS_CLIPCHILDREN`), ex-style `0x00010008` (`WS_EX_TOPMOST |
  WS_EX_CONTROLPARENT`). Not layered: it paints a frozen copy of the screen.
* `GetCursorInfo` returns a NULL cursor handle during selection → pointer hidden.
* The reference's own toolbar disappears from the frozen image.

### XOR colour
Background → line pixel: (28,28,29) → (28,227,226); (25,25,25) → (25,230,230);
(4,3,3) → (3,252,252) (approx.); light grey → (192,63,63). The crosshair
intersection pixel equals the background (double XOR).

### Magnifier panel (cursor at 705,455 → panel bottom-right at 1676,717)
* Frame columns at x = 1676 white, 1677 grey(160) … 1918 white, 1919 grey.
* Zoom area x 1678–1917, y 719–918; crosshair band 5 px wide at x 1798–1802,
  y 819–823 → 5× zoom, pointer pixel at zoom column 24 / row 20.
* Box: y 919–920 white, info bar y 921–938 grey(200), help y 939–1075
  blue(51,103,155), bevel rows 1076–1077 grey, 1078 white, 1079 grey.
* Help lines start at x 1689; line pitch 15 px inside an item, 17 px between items.

### Panel corner thresholds (panel 244 × 363)
| Panel at | Pointer | Result |
|---|---|---|
| bottom-right | (1585,1000) / (1586,1000) | stays / jumps |
| bottom-right | (1800,626) / (1800,627) | stays / jumps |
| top-left | (335,100) / (334,100) | stays / jumps |
| top-left | (100,454) / (100,453) | stays / jumps |

Moving away again does not bring the panel back (hysteresis). A new capture
starts bottom-right.

### Size label (anchor at 1000,500 unless noted; body top-left, body size)
| Pointer | Body | Note |
|---|---|---|
| (1300,700) | (1314,715) 54×15 | down-right |
| (700,300) | (631,270) | up-left |
| (1300,300) | (1314,270) | up-right |
| (700,700) | (631,730) | down-left: +30 |
| (1000,500) | (1014,515) 30×15 | `1 x 1` |
| (1000,600) | (1014,615) | straight down |
| (1100,500) | (1114,515) | straight right |
| (900,500) | (843,470) | straight left → up-left |
| (1000,400) | (943,370) | straight up → up-left |
| (1851,700) / (1852,700) / (1853,700) | x 1865 / 1866 / 1799 | right-edge flip |
| (1300,1049) / (1300,1050) / (1300,1070) | y 1064 / 1065 / 1055 | bottom-edge flip |
| (40,300) / (10,300) / (0,300) | x 39 / 9 / 0 (59 px visible) | left-edge flip, not clamped |
| (700,30) / (700,29) / (700,0) | y 0 / 29 / 0 | top-edge flip |
| (5,5), anchor (1500,700)→(1919,1079) and →(0,0) | hidden | both axes flipped |

Label widths: `1 x 1` 30, `1 x 101` 42, `401 x 351` 54, `991 x 201` 54 → text
width (Segoe UI 9 pt) + 7. Label colours: body (255,255,175), shadow (150,150,150).

### Mouse and keys
* Press, move 1–4 px, release → click mode (overlay stays); 5 px → captured.
* Click mode: second press keeps selecting (rect follows while held), release
  finishes.
* Arrow keys: exactly 1 px per press, also with Shift or Ctrl.
* Tab with anchor (502,302) and pointer (829,505): pointer jumps to (502,302);
  moving it changes that corner; Tab again returns to (829,505).
* Enter at (401,401), 30× Right, 20× Down, Space → 31 × 21 capture.
* Right click during selection → whole capture cancelled.
* F1 → panel disappears.
* Release after a 600,300 → 1000,650 drag → editor opens with a 401 × 351 image.

## Verification of OpenCapture against these numbers

Same probe, same positions, run against OpenCapture v0.1.0:
* Panel frame + zoom: 2 of 49 776 pixels differ.
* Size label body + shadow at (600,300)→(1000,650): 2 of 880 pixels differ
  (the shadow's corner pixels, which show the desktop).
* Crosshair XOR pixels identical; result window shows a 401 × 351 capture.
* Info bar and help text: same layout, approximate fonts / own wording
  (pixel-exact UI is not a goal).
