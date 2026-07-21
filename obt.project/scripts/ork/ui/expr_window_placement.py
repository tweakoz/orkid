################################################################################
# ork.ui.expr_window_placement — DETERMINISTIC opening placement for the expression
# editor secondary window (JUL13_DFLOW E2.5 S7, owner refinement 2026-07-19).
#
# The expression editor opens in a real OS secondary window. Its OPENING rect must NOT
# cover the VIEWPORT region (the owner watches the sim react to an applied expression), and
# must be a usable size. This module is the PURE, engine-free geometry: it takes the live
# main-window / viewport / screen rects and returns the window rect + the strategy that won,
# so the choice is unit-testable headlessly and deterministic for a given geometry. The user
# may move/resize afterwards — this only picks the OPENING placement.
#
# Strategy, in preference order (owner-specified):
#   (1) OUTSIDE the main window (flush to its right edge, else its left edge) when the screen
#       has horizontal room — fully clear of the whole main window, so clear of the viewport.
#   (2) OVERLAP the main window only on its NON-viewport side — the largest main-window band
#       (left / right / above / below the viewport) that clears the viewport and fits.
#   (3) DEGENERATE fallback — the smallest usable size, corner-anchored at the screen corner
#       FARTHEST from the viewport centre, then nudged fully clear of the viewport + on-screen.
################################################################################

# minimum usable editor size (px) and the code-editor preferred width (a code editor is
# taller than wide; height prefers the main-window height when placed beside it).
MIN_W = 340
MIN_H = 420
PREF_W = 520

# cascade stagger (px) applied per already-open expression window so concurrent windows do not
# stack EXACTLY on top of each other — a title-bar-ish step, kept within the on-screen region.
CASCADE_STEP = 28


def _intersects(a, b):
    ax, ay, aw, ah = a
    bx, by, bw, bh = b
    return not (ax + aw <= bx or bx + bw <= ax or ay + ah <= by or by + bh <= ay)


def _clampw(v, lo, hi):
    return lo if v < lo else (hi if v > hi else v)


def _apply_cascade(rect, cascade_index, screen_rect, viewport_rect, step=CASCADE_STEP):
    """Stagger `rect` down-and-right by cascade_index title-bar steps so concurrent windows do
    not stack exactly. The staggered rect is clamped ONTO the screen and, if the offset would
    push it back over the viewport, the offending axis reverts to base (staying viewport-clear).
    Returns the (possibly unchanged) rect."""
    if cascade_index <= 0:
        return rect
    x, y, w, h = rect
    sx, sy, sw, sh = screen_rect
    off = cascade_index * step
    nx = _clampw(x + off, sx, sx + sw - w)
    ny = _clampw(y + off, sy, sy + sh - h)
    cand = (nx, ny, w, h)
    if _intersects(cand, viewport_rect):
        cand = (x, ny, w, h)                 # keep the (clear) horizontal placement, stagger only y
        if _intersects(cand, viewport_rect):
            return rect                      # no clear staggered spot: exact stack (rare)
    return cand


def compute_expr_window_rect(main_rect, viewport_rect, screen_rect,
                             min_w=MIN_W, min_h=MIN_H, pref_w=PREF_W, cascade_index=0):
    """Return (rect, strategy) where rect == (x, y, w, h) is the opening placement of the
    expression window and strategy is 'outside' / 'overlap' / 'fallback'. Rects are screen-space.

    GUARANTEE: whenever the screen has ANY min_w x min_h region clear of the viewport, the
    returned rect is that clear + on-screen + min-size (strategy 'outside' or 'overlap'). Only a
    viewport that leaves NO clear min-size region (e.g. one covering the whole screen) forces the
    'fallback' branch, which returns the min-size rect corner-anchored on the screen side AWAY
    from the viewport centre (necessarily overlapping — no clear region exists). Deterministic for
    a given geometry.

    cascade_index > 0 (the count of already-open expression windows) staggers the returned rect
    by that many title-bar steps within the on-screen region, so concurrent windows do not stack
    exactly — kept viewport-clear whenever the base placement was clear."""
    sx, sy, sw, sh = screen_rect
    mx, my, mw, mh = main_rect
    vx, vy, vw, vh = viewport_rect

    # target height: prefer the main-window height, clamp to the screen and the minimum.
    def _fit_h(h):
        return _clampw(h, min_h, max(min_h, sh))

    # ---- (1) OUTSIDE the main window: flush right, else flush left --------------
    right_room = (sx + sw) - (mx + mw)
    left_room = mx - sx
    h1 = _fit_h(mh)
    y1 = _clampw(my, sy, sy + sh - h1)
    if right_room >= min_w:
        w = _clampw(right_room, min_w, pref_w)
        rect = (mx + mw, y1, w, h1)
        if not _intersects(rect, viewport_rect):
            return _apply_cascade(rect, cascade_index, screen_rect, viewport_rect), "outside"
    if left_room >= min_w:
        w = _clampw(left_room, min_w, pref_w)
        rect = (mx + left_room - w, y1, w, h1)      # flush to the main window's left edge
        if not _intersects(rect, viewport_rect):
            return _apply_cascade(rect, cascade_index, screen_rect, viewport_rect), "outside"

    # ---- (2) OVERLAP the main window on its NON-viewport side -------------------
    # the four bands of the main window that clear the viewport; pick the largest that fits.
    bands = [
        (mx, my, vx - mx, mh),                         # left of the viewport
        (vx + vw, my, (mx + mw) - (vx + vw), mh),      # right of the viewport
        (mx, my, mw, vy - my),                         # above the viewport
        (mx, vy + vh, mw, (my + mh) - (vy + vh)),      # below the viewport
    ]
    best = None
    for (bx, by, bw, bh) in bands:
        if bw < min_w or bh < min_h:
            continue
        w = _clampw(bw, min_w, pref_w)
        h = _fit_h(bh)
        y = _clampw(by, sy, sy + sh - h)
        rect = (bx, y, w, h)
        # clamp onto the screen without re-crossing the viewport
        if rect[0] < sx:
            rect = (sx, rect[1], rect[2], rect[3])
        if rect[0] + rect[2] > sx + sw:
            rect = (sx + sw - rect[2], rect[1], rect[2], rect[3])
        if _intersects(rect, viewport_rect):
            continue
        area = rect[2] * rect[3]
        if best is None or area > best[1]:
            best = (rect, area)
    if best is not None:
        return _apply_cascade(best[0], cascade_index, screen_rect, viewport_rect), "overlap"

    # ---- (3) DEGENERATE fallback: min size, farthest screen corner -------------
    w = _clampw(min_w, min_w, sw)
    h = _clampw(min_h, min_h, sh)
    vcx, vcy = vx + vw / 2.0, vy + vh / 2.0
    scx, scy = sx + sw / 2.0, sy + sh / 2.0
    x = sx if vcx >= scx else (sx + sw - w)           # anchor on the side away from viewport
    y = sy if vcy >= scy else (sy + sh - h)
    rect = (x, y, w, h)
    if _intersects(rect, viewport_rect):
        # nudge horizontally clear of the viewport, staying on screen
        if x <= vx:
            x = max(sx, vx - w)
        else:
            x = min(sx + sw - w, vx + vw)
        rect = (x, y, w, h)
    return _apply_cascade(rect, cascade_index, screen_rect, viewport_rect), "fallback"
