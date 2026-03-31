"""
ork.fsmviz — Generic FSM/HFSM Visualization Library
=====================================================

Builds on orkengine.core.fsm and PrimCanvas to visualize any
hierarchical finite state machine with layout hints.

Features:
  - Resolution-independent layout (scales with window)
  - Interactive drag (nodes and groups)
  - Cubic bezier arrow routing with distributed ports
  - Automatic port distribution on shared edges

Usage:
    from ork.fsmviz import FsmVizBuilder, FsmVisualizer

    b = FsmVizBuilder()
    root = b.state(None, "ROOT", hidden=True)
    idle = b.state(root, "IDLE", pos=(0.3, 0.5), color=my_color)
    run  = b.state(root, "RUNNING", pos=(0.7, 0.5), color=my_color)
    b.transition(idle, "start", run, color=green, src_side="right")

    app = FsmVisualizer(b, title="My FSM")
    app.createEzApp(name="My FSM", width=1440, height=820)
    app.ezapp.mainThreadLoop(on_iter=lambda: {})
"""

import math
import hashlib
import json
import os
import inspect
from collections import defaultdict
from pathlib import Path

from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2
from orkengine.core import fsm
from ork.app.application import ComponentizedApplication

_tokens = CrcStringProxy()

###############################################################################
# Layout Hints
###############################################################################

class StateHint:
    """Layout hint for an FSM state.

    Args:
        pos:          (x, y) normalized 0..1 — leaf nodes only
        color:        vec4 fill color (leaf nodes)
        group_color:  vec4 fill color (parent/group states)
        border_color: vec4 border color (groups)
        hooks:        str describing callbacks, e.g. "onEnter: stage  onExit: unstage"
        hook_color:   vec4 for hook annotation text
        hidden:       if True, state and its subtree are excluded from the diagram
    """
    def __init__(self, *,
                 pos=None,
                 color=None,
                 group_color=None,
                 border_color=None,
                 hooks=None,
                 hook_color=None,
                 hidden=False):
        self.pos = pos
        self.color = color
        self.group_color = group_color
        self.border_color = border_color
        self.hooks = hooks
        self.hook_color = hook_color
        self.hidden = hidden


class TransitionHint:
    """Layout hint for an FSM transition arrow.

    Args:
        color:          vec4 arrow color
        src_side:       (legacy, ignored) kept for API compat
        dst_side:       (legacy, ignored) kept for API compat
        label_offset:   (dx, dy) in reference coords for label positioning
        src_port_angle: float radians, manual port angle on src node (None=auto)
        dst_port_angle: float radians, manual port angle on dst node (None=auto)
    """
    def __init__(self, *,
                 color=None,
                 src_side="auto",
                 dst_side="auto",
                 label_offset=(0, 0),
                 src_port_angle=None,
                 dst_port_angle=None):
        self.color = color
        self.src_side = src_side
        self.dst_side = dst_side
        self.label_offset = label_offset
        self.src_port_angle = src_port_angle
        self.dst_port_angle = dst_port_angle


###############################################################################
# FsmVizBuilder
###############################################################################

class FsmVizBuilder:
    """Wraps fsm.FsmData and collects layout hints alongside state/transition creation."""

    def __init__(self):
        self.data = fsm.FsmData()
        self.state_hints = {}
        self.transitions = []
        self._states = {}
        # Capture caller's script path at creation time for config hashing
        self._caller_file = "unknown"
        for frame_info in inspect.stack():
            f = frame_info.filename
            if f != __file__ and not f.startswith("<"):
                self._caller_file = os.path.abspath(f)
                break

    def state(self, parent, name, **kwargs):
        """Create a state with optional layout hints (see StateHint)."""
        s = self.data.createState(parent, name)
        self._states[name] = s
        self.state_hints[name] = StateHint(**kwargs)
        return s

    def transition(self, src, event, dst, **kwargs):
        """Add a transition with optional layout hints (see TransitionHint)."""
        self.data.addTransition(src, event, dst)
        self.transitions.append((src, event, dst, TransitionHint(**kwargs)))

    def find(self, name):
        return self._states[name]

    @property
    def all_states(self):
        return [(s, self.state_hints.get(s.name, StateHint())) for s in self.data.states]

    def children_of(self, parent_state):
        return [s for s in self.data.states if s.parent is parent_state]

    def leaf_descendants(self, state):
        result = []
        for c in self.children_of(state):
            if self.is_leaf(c):
                result.append(c)
            else:
                result.extend(self.leaf_descendants(c))
        return result

    def is_leaf(self, state):
        return len(self.children_of(state)) == 0

    def is_group(self, state):
        return not self.is_leaf(state)

    # ── Layout persistence ──

    def config_path(self, fsm_name):
        """Compute a unique config file path for this FSM based on the
        creating script's path and the FSM name."""
        import obt.path
        key = f"{self._caller_file}:{fsm_name}"
        h = hashlib.sha256(key.encode()).hexdigest()[:16]
        config_dir = Path(obt.path.temp()) / "fsmviz"
        config_dir.mkdir(parents=True, exist_ok=True)
        return config_dir / f"{h}.fsm.json"

    def save_layout(self, fsm_name):
        """Save current node positions and port angles to config file."""
        data = {"nodes": {}, "ports": []}
        # Node positions
        for name, hint in self.state_hints.items():
            if hint.pos:
                data["nodes"][name] = list(hint.pos)
        # Port angles
        for src_state, event, dst_state, thint in self.transitions:
            entry = {
                "src": src_state.name,
                "event": event,
                "dst": dst_state.name,
            }
            if thint.src_port_angle is not None:
                entry["src_angle"] = thint.src_port_angle
            if thint.dst_port_angle is not None:
                entry["dst_angle"] = thint.dst_port_angle
            data["ports"].append(entry)

        path = self.config_path(fsm_name)
        with open(path, 'w') as f:
            json.dump(data, f, indent=2)

    def load_layout(self, fsm_name):
        """Load saved node positions and port angles. Returns True if loaded."""
        path = self.config_path(fsm_name)
        if not path.exists():
            return False
        try:
            with open(path) as f:
                data = json.load(f)
        except (json.JSONDecodeError, IOError):
            return False

        # Restore node positions
        for name, pos in data.get("nodes", {}).items():
            if name in self.state_hints:
                self.state_hints[name].pos = tuple(pos)

        # Restore port angles
        for entry in data.get("ports", []):
            sn = entry.get("src")
            ev = entry.get("event")
            dn = entry.get("dst")
            for src_state, event, dst_state, thint in self.transitions:
                if src_state.name == sn and event == ev and dst_state.name == dn:
                    if "src_angle" in entry:
                        thint.src_port_angle = entry["src_angle"]
                    if "dst_angle" in entry:
                        thint.dst_port_angle = entry["dst_angle"]
                    break
        return True


###############################################################################
# Layout Engine
###############################################################################

class Rect:
    __slots__ = ('x', 'y', 'w', 'h')
    def __init__(self, x=0, y=0, w=0, h=0):
        self.x, self.y, self.w, self.h = x, y, w, h
    @property
    def cx(self): return self.x + self.w / 2
    @property
    def cy(self): return self.y + self.h / 2
    @property
    def right(self): return self.x + self.w
    @property
    def bottom(self): return self.y + self.h
    def contains(self, x, y):
        return self.x <= x <= self.right and self.y <= y <= self.bottom

    def perimeter_point(self, angle):
        """Point on the rectangle perimeter at the given angle (radians) from center.
        angle=0 is right, pi/2 is down (screen coords Y-down)."""
        hw = self.w / 2.0
        hh = self.h / 2.0
        if hw < 0.5 or hh < 0.5:
            return (self.cx, self.cy)
        dx = math.cos(angle)
        dy = math.sin(angle)
        # Ray from center: find t where it hits the rect boundary
        if abs(dx) < 1e-10:
            t = hh / abs(dy)
        elif abs(dy) < 1e-10:
            t = hw / abs(dx)
        else:
            t = min(hw / abs(dx), hh / abs(dy))
        return (self.cx + dx * t, self.cy + dy * t)


# Reference design size
REF_W, REF_H = 1440.0, 820.0

# Layout constants (in reference coords)
LEAF_W, LEAF_H = 150, 38
GROUP_PADDING  = 28
GROUP_HEADER_H = 28

# Default colors
DEFAULT_LEAF_COLOR  = vec4(0.30, 0.35, 0.45, 1.0)
DEFAULT_GROUP_COLOR = vec4(0.12, 0.12, 0.18, 0.85)
DEFAULT_BORDER      = vec4(0.25, 0.25, 0.35, 0.5)
DEFAULT_HOOK_COLOR  = vec4(0.55, 0.65, 0.60, 0.9)
DEFAULT_ARROW_COLOR = vec4(0.50, 0.50, 0.60, 0.9)

COL_TEXT       = vec4(0.88, 0.88, 0.92, 1.0)
COL_TEXT_GROUP = vec4(0.60, 0.60, 0.70, 1.0)
COL_TEXT_TITLE = vec4(0.75, 0.75, 0.85, 1.0)
COL_TEXT_SUB   = vec4(0.50, 0.50, 0.60, 1.0)
COL_BG         = vec4(0.06, 0.06, 0.09, 1.0)

# Bezier settings
BEZIER_SEGMENTS = 24
BEZIER_TANGENT_SCALE = 0.4  # control point distance as fraction of endpoint distance


class FsmLayout:
    """Computes positioned rects and bezier arrow paths for all states."""

    def __init__(self, builder, cw, ch):
        self.builder = builder
        self.cw, self.ch = cw, ch
        self.sx = cw / REF_W
        self.sy = ch / REF_H

        self.leaf_rects  = []   # (name, Rect, color)
        self.group_rects = []   # (name, Rect, group_color, border_color, hooks, hook_color)
        self.arrow_data  = []   # (bezier_points, label, color, label_pos, crossings, src_port, dst_port, arrow_idx)
                                # bezier_points = list of (x,y) in screen coords
                                # crossings = list of (x,y) where curve crosses group borders
                                # src_port/dst_port = (x,y) port positions for hit testing

        self._state_rects = {}
        self._compute()

    def _compute(self):
        b = self.builder
        sx, sy = self.sx, self.sy

        # Pass 1: place leaf nodes
        for s, hint in b.all_states:
            if b.is_leaf(s) and not hint.hidden and hint.pos:
                px, py = hint.pos
                w, h = LEAF_W * sx, LEAF_H * sy
                r = Rect(px * self.cw - w/2, py * self.ch - h/2, w, h)
                self._state_rects[s.name] = r
                self.leaf_rects.append((s.name, r, hint.color or DEFAULT_LEAF_COLOR))

        # Pass 2: group rects bottom-up
        def depth(state):
            d, p = 0, state.parent
            while p is not None:
                d += 1; p = p.parent
            return d

        all_groups = [(s, h) for s, h in b.all_states if b.is_group(s) and not h.hidden]
        all_groups.sort(key=lambda sh: -depth(sh[0]))

        for s, hint in all_groups:
            child_rects = [self._state_rects[c.name]
                           for c in b.children_of(s) if c.name in self._state_rects]
            if not child_rects:
                continue
            xmin = min(r.x for r in child_rects)
            xmax = max(r.right for r in child_rects)
            ymin = min(r.y for r in child_rects)
            ymax = max(r.bottom for r in child_rects)
            gp = GROUP_PADDING * sx
            hh = GROUP_HEADER_H * sy
            gr = Rect(xmin - gp, ymin - hh, (xmax - xmin) + gp*2, (ymax - ymin) + gp + hh)
            self._state_rects[s.name] = gr
            self.group_rects.append((
                s.name, gr,
                hint.group_color or DEFAULT_GROUP_COLOR,
                hint.border_color or DEFAULT_BORDER,
                hint.hooks or "",
                hint.hook_color or DEFAULT_HOOK_COLOR,
            ))

        self.group_rects.reverse()

        # Pass 3: arrows with port distribution
        self._compute_arrows(b)

    def _compute_arrows(self, b):
        sx, sy = self.sx, self.sy

        # Collect transitions — store on self for hit testing
        transitions = []
        for src_state, event, dst_state, thint in b.transitions:
            sn, dn = src_state.name, dst_state.name
            if sn not in self._state_rects or dn not in self._state_rects:
                continue
            transitions.append((sn, dn, event, thint))
        self._transitions = transitions

        # For each node, compute ideal angle = angle from node center to the
        # closest point on the OTHER node's perimeter (not its center).
        # Manual port angles (from TransitionHint) override auto computation.
        node_ports = defaultdict(list)  # node_name -> [(arrow_idx, role, ideal_angle, is_manual)]
        for i, (sn, dn, ev, th) in enumerate(transitions):
            sr, dr = self._state_rects[sn], self._state_rects[dn]

            if th.src_port_angle is not None:
                src_angle = th.src_port_angle
                src_manual = True
            else:
                cp_on_dst = _closest_point_on_rect(dr, sr.cx, sr.cy)
                src_angle = math.atan2(cp_on_dst[1] - sr.cy, cp_on_dst[0] - sr.cx)
                src_manual = False

            if th.dst_port_angle is not None:
                dst_angle = th.dst_port_angle
                dst_manual = True
            else:
                cp_on_src = _closest_point_on_rect(sr, dr.cx, dr.cy)
                dst_angle = math.atan2(cp_on_src[1] - dr.cy, cp_on_src[0] - dr.cx)
                dst_manual = False

            node_ports[sn].append((i, 'src', src_angle, src_manual))
            node_ports[dn].append((i, 'dst', dst_angle, dst_manual))

        # Spread ports on each node with enforced minimum angular separation.
        # Manual (already-baked) ports are fixed — only fresh auto ports get pushed.
        MIN_ANGLE = math.radians(18)
        port_angles = {}  # (arrow_idx, role) -> final_angle

        for node_name, entries in node_ports.items():
            entries.sort(key=lambda e: e[2])
            ideals = [e[2] for e in entries]
            fixed = [e[3] for e in entries]
            spread = _spread_angles(ideals, MIN_ANGLE, fixed)
            for j, (idx, role, _, _) in enumerate(entries):
                port_angles[(idx, role)] = spread[j]

        # Bake all computed angles back into hints — auto becomes manual.
        # From here on, all ports are fixed unless the user drags them.
        for i, (sn, dn, event, thint) in enumerate(transitions):
            src_a = port_angles.get((i, 'src'))
            dst_a = port_angles.get((i, 'dst'))
            if src_a is not None:
                thint.src_port_angle = src_a
            if dst_a is not None:
                thint.dst_port_angle = dst_a

        # Build bezier arrow data
        for i, (sn, dn, event, thint) in enumerate(transitions):
            sr = self._state_rects[sn]
            dr = self._state_rects[dn]

            src_angle = port_angles.get((i, 'src'), 0.0)
            dst_angle = port_angles.get((i, 'dst'), math.pi)

            sp = sr.perimeter_point(src_angle)
            dp = dr.perimeter_point(dst_angle)

            # Control points extend outward along the port angle
            dist = math.sqrt((dp[0]-sp[0])**2 + (dp[1]-sp[1])**2)
            tangent_len = max(40 * sx, dist * BEZIER_TANGENT_SCALE)

            cp1 = (sp[0] + math.cos(src_angle) * tangent_len,
                   sp[1] + math.sin(src_angle) * tangent_len)
            cp2 = (dp[0] + math.cos(dst_angle) * tangent_len,
                   dp[1] + math.sin(dst_angle) * tangent_len)

            # Tessellate
            points = []
            for seg in range(BEZIER_SEGMENTS + 1):
                t = seg / BEZIER_SEGMENTS
                x, y = _cubic_bezier(sp, cp1, cp2, dp, t)
                points.append((x, y))

            # Label at curve midpoint
            lx, ly = _cubic_bezier(sp, cp1, cp2, dp, 0.5)
            lox = thint.label_offset[0] * sx
            loy = thint.label_offset[1] * sy
            label_pos = (lx + lox, ly + loy)

            color = thint.color or DEFAULT_ARROW_COLOR
            crossings = self._find_group_crossings(points, sn, dn)
            self.arrow_data.append((points, event, color, label_pos, crossings, sp, dp, i))

    def _find_group_crossings(self, points, src_name, dst_name):
        """Find points where a polyline crosses group rect boundaries.
        For each group rect, only the FIRST intersection along the curve
        is recorded. Direction is determined geometrically: if the point
        just AFTER the crossing is inside the rect, we're entering.
        Returns list of (x, y, is_entering) tuples."""
        crossings = []

        for group_name, rect, *_ in self.group_rects:
            edges = [
                ((rect.x, rect.y),       (rect.right, rect.y)),
                ((rect.x, rect.bottom),   (rect.right, rect.bottom)),
                ((rect.x, rect.y),        (rect.x, rect.bottom)),
                ((rect.right, rect.y),     (rect.right, rect.bottom)),
            ]
            first_hit = None
            first_seg = len(points)
            first_t = 1.0
            for i in range(len(points) - 1):
                ax, ay = points[i]
                bx, by = points[i + 1]
                for (ex0, ey0), (ex1, ey1) in edges:
                    pt = _segment_intersect(ax, ay, bx, by, ex0, ey0, ex1, ey1)
                    if pt:
                        dx, dy = bx - ax, by - ay
                        seg_len_sq = dx*dx + dy*dy
                        t = ((pt[0]-ax)*dx + (pt[1]-ay)*dy) / seg_len_sq if seg_len_sq > 0.001 else 0.0
                        if i < first_seg or (i == first_seg and t < first_t):
                            first_seg = i
                            first_t = t
                            first_hit = pt
            if first_hit:
                # Curve tangent at crossing = flow direction (source → dest)
                ax, ay = points[first_seg]
                bx, by = points[min(first_seg + 1, len(points) - 1)]
                tangent_x = bx - ax
                tangent_y = by - ay
                crossings.append((first_hit[0], first_hit[1], tangent_x, tangent_y, group_name))
        return crossings

###############################################################################
# Geometry helpers
###############################################################################

def _closest_point_on_rect(rect, px, py):
    """Closest point on rect's perimeter to point (px, py)."""
    # Clamp to rect interior first
    cx = max(rect.x, min(px, rect.right))
    cy = max(rect.y, min(py, rect.bottom))
    # If point is inside rect, find nearest edge
    if rect.x < px < rect.right and rect.y < py < rect.bottom:
        dists = [
            (px - rect.x,      (rect.x, py)),       # left
            (rect.right - px,   (rect.right, py)),   # right
            (py - rect.y,       (px, rect.y)),       # top
            (rect.bottom - py,  (px, rect.bottom)),  # bottom
        ]
        return min(dists, key=lambda d: d[0])[1]
    return (cx, cy)


###############################################################################
# Angular port spreading
###############################################################################

def _normalize_angle(a):
    """Normalize angle to [-pi, pi)."""
    while a >= math.pi:
        a -= 2 * math.pi
    while a < -math.pi:
        a += 2 * math.pi
    return a


def _angular_dist(a, b):
    """Signed angular distance from a to b, in [-pi, pi)."""
    return _normalize_angle(b - a)


def _spread_angles(ideals, min_sep, fixed=None):
    """Spread angles so adjacent ports maintain min_sep radians of separation.
    Handles wraparound (-pi to pi). Input must be sorted.
    fixed: optional list of bools — True means that port's angle is locked.
    Returns list of spread angles."""
    n = len(ideals)
    if n <= 1:
        return list(ideals)

    if fixed is None:
        fixed = [False] * n

    angles = list(ideals)

    # Iterative relaxation — only move non-fixed ports
    for _iteration in range(30):
        moved = False
        for i in range(n - 1):
            gap = _angular_dist(angles[i], angles[i + 1])
            if gap < min_sep:
                push = (min_sep - gap)
                if fixed[i] and fixed[i + 1]:
                    continue  # both fixed, can't move
                elif fixed[i]:
                    angles[i + 1] = _normalize_angle(angles[i + 1] + push)
                elif fixed[i + 1]:
                    angles[i] = _normalize_angle(angles[i] - push)
                else:
                    angles[i] = _normalize_angle(angles[i] - push / 2.0)
                    angles[i + 1] = _normalize_angle(angles[i + 1] + push / 2.0)
                moved = True

        # Wraparound gap
        wrap_gap = _angular_dist(angles[-1], angles[0]) + 2 * math.pi
        if wrap_gap < min_sep:
            push = (min_sep - wrap_gap)
            if fixed[-1] and fixed[0]:
                pass
            elif fixed[-1]:
                angles[0] = _normalize_angle(angles[0] + push)
            elif fixed[0]:
                angles[-1] = _normalize_angle(angles[-1] - push)
            else:
                angles[-1] = _normalize_angle(angles[-1] - push / 2.0)
                angles[0] = _normalize_angle(angles[0] + push / 2.0)
                moved = True

        if not moved:
            break

    return angles


def _cubic_bezier(p0, p1, p2, p3, t):
    """Evaluate cubic bezier at parameter t."""
    u = 1.0 - t
    uu, tt = u * u, t * t
    uuu, ttt = uu * u, tt * t
    x = uuu*p0[0] + 3*uu*t*p1[0] + 3*u*tt*p2[0] + ttt*p3[0]
    y = uuu*p0[1] + 3*uu*t*p1[1] + 3*u*tt*p2[1] + ttt*p3[1]
    return (x, y)


###############################################################################
# Segment intersection
###############################################################################

def _segment_intersect(ax, ay, bx, by, cx, cy, dx, dy):
    """Intersect line segment AB with segment CD.
    Returns (x, y) or None."""
    denom = (bx - ax) * (dy - cy) - (by - ay) * (dx - cx)
    if abs(denom) < 1e-10:
        return None
    t = ((cx - ax) * (dy - cy) - (cy - ay) * (dx - cx)) / denom
    u = ((cx - ax) * (by - ay) - (cy - ay) * (bx - ax)) / denom
    if 0.0 < t < 1.0 and 0.0 <= u <= 1.0:
        return (ax + t * (bx - ax), ay + t * (by - ay))
    return None


###############################################################################
# Arrow geometry — thick bezier with arrowhead
###############################################################################

def _make_bezier_arrow_verts(points, color, thickness, ch):
    """Tessellated thick bezier curve with arrowhead.
    points: list of (x,y) in screen coords (Y-down).
    Returns TriList vertices in PrimCanvas coords (Y-up)."""
    if len(points) < 2:
        return []

    verts = []
    def v(x, y):
        vd = lev2.ui.VertexData()
        vd.setPosition(x, y)
        vd.setColor(color)
        verts.append(vd)

    # Shorten the last segment to make room for arrowhead
    arrow_len = 14
    arrow_w = thickness * 3.5

    # Work in PrimCanvas coords (flip Y)
    pts = [(x, ch - y) for x, y in points]

    # Find direction of the last segment for arrowhead
    last_dx = pts[-1][0] - pts[-2][0]
    last_dy = pts[-1][1] - pts[-2][1]
    last_len = math.sqrt(last_dx**2 + last_dy**2)
    if last_len < 0.001:
        return []
    lux, luy = last_dx / last_len, last_dy / last_len

    # Arrowhead tip is at pts[-1], base is arrow_len back
    tip = pts[-1]
    arrow_base = (tip[0] - lux * arrow_len, tip[1] - luy * arrow_len)

    # Replace last point with arrow_base for the line portion
    line_pts = list(pts[:-1]) + [arrow_base]

    # Build thick line as triangle strip pairs
    for i in range(len(line_pts) - 1):
        ax, ay = line_pts[i]
        bx, by = line_pts[i + 1]

        dx, dy = bx - ax, by - ay
        seg_len = math.sqrt(dx*dx + dy*dy)
        if seg_len < 0.001:
            continue
        nx, ny = -dy / seg_len * thickness, dx / seg_len * thickness

        # Two triangles per segment
        v(ax + nx, ay + ny)
        v(ax - nx, ay - ny)
        v(bx + nx, by + ny)

        v(bx + nx, by + ny)
        v(ax - nx, ay - ny)
        v(bx - nx, by - ny)

    # Arrowhead triangle
    apx, apy = -luy * arrow_w, lux * arrow_w
    v(arrow_base[0] + apx, arrow_base[1] + apy)
    v(arrow_base[0] - apx, arrow_base[1] - apy)
    v(tip[0], tip[1])

    return verts


def _make_thin_line(x0, y0, x1, y1, color, thickness, ch):
    """Simple thick line segment without arrowhead. Screen coords Y-down."""
    y0_q, y1_q = ch - y0, ch - y1
    dx, dy = x1 - x0, y1_q - y0_q
    length = math.sqrt(dx*dx + dy*dy)
    if length < 1:
        return []
    ux, uy = dx/length, dy/length
    px, py = -uy * thickness, ux * thickness
    verts = []
    def v(x, y):
        vd = lev2.ui.VertexData()
        vd.setPosition(x, y)
        vd.setColor(color)
        verts.append(vd)
    v(x0+px, y0_q+py); v(x0-px, y0_q-py); v(x1+px, y1_q+py)
    v(x1+px, y1_q+py); v(x0-px, y0_q-py); v(x1-px, y1_q-py)
    return verts


def _add_circle_verts(prim, cx, cy, radius, color, segments=10):
    """Add a filled circle as a triangle fan to an existing TriListPrimitive.
    Coordinates in PrimCanvas space (Y-up)."""
    def v(x, y):
        vd = lev2.ui.VertexData()
        vd.setPosition(x, y)
        vd.setColor(color)
        prim.addVertex(vd)

    step = 2.0 * math.pi / segments
    for i in range(segments):
        a0 = i * step
        a1 = (i + 1) * step
        v(cx, cy)
        v(cx + math.cos(a0) * radius, cy + math.sin(a0) * radius)
        v(cx + math.cos(a1) * radius, cy + math.sin(a1) * radius)


###############################################################################
# SVG arrow icons for hook direction indicators
###############################################################################

# Rightward-pointing flow arrow (rotated at render time to match curve tangent)
_SVG_FLOW_ARROW = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M20 12 L6 4 L6 20 Z" fill="#e0e0e0" stroke="#ffffff" stroke-width="0.5"/>
</svg>'''

# Green downward arrow (enter/bringup — for hook labels)
_SVG_ARROW_ENTER = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 20 L4 6 L20 6 Z" fill="#70c080" stroke="#90e0a0" stroke-width="0.5"/>
</svg>'''

# Red/orange upward arrow (exit/teardown — for hook labels)
_SVG_ARROW_EXIT = '''<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24">
  <path d="M12 4 L4 18 L20 18 Z" fill="#c07060" stroke="#e09080" stroke-width="0.5"/>
</svg>'''


###############################################################################
# Visualizer Application
###############################################################################

class _TabState:
    """Per-tab state for a single FSM in the visualizer."""
    def __init__(self, name, builder, subtitle="", footer_lines=None):
        self.name = name
        self.builder = builder
        self.subtitle = subtitle
        self.footer_lines = footer_lines or []
        self.last_layout = None
        self.dirty = True

        # Drag state — nodes
        self.dragging = False
        self.drag_leaf_names = []
        self.drag_last_mx = 0
        self.drag_last_my = 0

        # Drag state — ports
        self.dragging_port = False
        self.drag_port_arrow_idx = -1
        self.drag_port_role = ""
        self.drag_port_node_name = ""

        # Load saved layout
        if builder.load_layout(name):
            print(f"[fsmviz] Loaded layout for '{name}' from {builder.config_path(name)}")


class _CanvasTab:
    """Per-tab rendering state — each tab has its own PrimCanvas."""
    def __init__(self, tab_state, canvas):
        self.state = tab_state
        self.canvas = canvas
        self.pip = None
        self.pip_vtx = None
        self.pip_tex = None
        self.font = None
        self.last_w = 0
        self.last_h = 0
        self.lyr_groups = None
        self.lyr_nodes = None
        self.lyr_arrows = None
        self.lyr_icons = None
        self.lyr_text = None
        self.lyr_labels = None
        self.tex_enter = None
        self.tex_exit = None
        self.tex_flow_arrow = None
        self.gpu_initialized = False


class FsmVisualizer(ComponentizedApplication):
    """PrimCanvas application that renders FSMs from FsmVizBuilders in tabs.

    Resolution-independent — rebuilds layout on window resize.
    Interactive — nodes and groups can be dragged to reposition.
    Supports multiple FSMs in tabs via TabsWidget.

    Args:
        tabs: list of dicts with keys:
              'name' (str), 'builder' (FsmVizBuilder),
              'subtitle' (str, optional), 'footer_lines' (list, optional)
        OR for single-FSM backwards compat:
        builder: FsmVizBuilder, title: str, subtitle: str, footer_lines: list
    """

    def __init__(self, builder=None, title="", subtitle="", footer_lines=None, tabs=None):
        super().__init__(profiler_channels=[])

        # Build tab state list
        if tabs:
            self._tab_states = [_TabState(t['name'], t['builder'],
                                          t.get('subtitle', ''),
                                          t.get('footer_lines', []))
                                for t in tabs]
        else:
            self._tab_states = [_TabState(title or "untitled", builder, subtitle, footer_lines)]

        self._canvas_tabs = []  # populated in _onUiInit
        self._tabs_widget = None

    def _onUiInit(self):
        lg_group = self.ezapp.topLayoutGroup
        lg_group.clearColorStd = COL_BG
        root_layout = lg_group.layout

        # Create TabsWidget
        tabs_layout = lg_group.makeChild(uiclass=lev2.ui.TabsWidget, args=["fsm_tabs"])
        self._tabs_widget = tabs_layout.widget
        self._tabs_widget.tabbar_background_color = vec4(0.08, 0.08, 0.12, 1.0)
        self._tabs_widget.content_background = COL_BG
        tabs_layout.layout.top.anchorTo(root_layout.top)
        tabs_layout.layout.left.anchorTo(root_layout.left)
        tabs_layout.layout.bottom.anchorTo(root_layout.bottom)
        tabs_layout.layout.right.anchorTo(root_layout.right)

        # Create a PrimCanvas per tab
        for ts in self._tab_states:
            canvas = self._tabs_widget.makeChild(
                uiclass=lev2.ui.PrimCanvas, args=[ts.name])
            canvas.bg_color = COL_BG
            canvas.draw_background = True
            canvas.supersample = 3
            ct = _CanvasTab(ts, canvas)
            self._canvas_tabs.append(ct)

    def _onGpuInit(self, ctx):
        # Shared textures
        tex_enter = self._svg_to_texture(ctx, _SVG_ARROW_ENTER, 24)
        tex_exit  = self._svg_to_texture(ctx, _SVG_ARROW_EXIT, 24)
        tex_flow  = self._svg_to_texture(ctx, _SVG_FLOW_ARROW, 24)

        # Initialize each canvas tab
        for ct in self._canvas_tabs:
            ct.canvas.gpuInit(ctx)
            ct.pip = ct.canvas.pipelineSolid
            ct.pip_vtx = ct.canvas.pipelineVtxSolid
            ct.pip_tex = ct.canvas.pipelineTextured
            ct.font = lev2.FontManager.fontForId("i14")
            ct.lyr_groups = ct.canvas.createLayer("groups")
            ct.lyr_nodes  = ct.canvas.createLayer("nodes")
            ct.lyr_arrows = ct.canvas.createLayer("arrows")
            ct.lyr_icons  = ct.canvas.createLayer("icons")
            ct.lyr_text   = ct.canvas.createLayer("text")
            ct.lyr_labels = ct.canvas.createLayer("labels")
            ct.tex_enter = tex_enter
            ct.tex_exit = tex_exit
            ct.tex_flow_arrow = tex_flow
            ct.gpu_initialized = True
            ct.canvas.onPreRender = lambda _ct=ct: self._on_pre_render_tab(_ct)
            ct.canvas.onUiEvent = lambda ev, _ct=ct: self._on_ui_event_tab(ev, _ct)

    def _draw_rotated_icon(self, layer, texture, cx, cy, size, angle, ch, pip_tex):
        """Draw a rotated textured quad icon centered at screen coords (Y-down).
        angle: radians, 0=right, pi/2=down in screen space."""
        qp = lev2.ui.QuadPrimitive(pipeline=pip_tex, texture=texture)
        qd = lev2.ui.QuadData()
        qd.setPosition(cx - size/2, ch - cy - size/2)
        qd.setSize(size, size)
        qd.setColor(vec4(1, 1, 1, 1))
        qd.setUV(0, 1, 1, 0)
        qd.setRotation(-angle)  # negate for Y-up canvas
        qp.addQuad(qd)
        layer.addPrimitive(qp)

    def _svg_to_texture(self, ctx, svg_str, size):
        img = lev2.Image.fromSvgStringSquare(svg_str, size)
        tex = lev2.Texture("icon_%d" % id(svg_str))
        ctx.TXI.updateTexture(tex, img, False)
        return tex

    # ── App-level key handling ──

    def _onUiEvent(self, ev):
        code = ev.code
        if code == _tokens.KEY_DOWN.hashed and ev.super and ev.keycode == ord('E'):
            self._open_svg_export_dialog()
            return lev2.ui.HandlerResult()
        return lev2.ui.HandlerResult()

    # ── Per-tab input handling (via canvas.onUiEvent callback) ──

    def _on_ui_event_tab(self, ev, ct):
        ts = ct.state
        code = ev.code
        # Convert root coords to canvas-local coords
        mx, my = ct.canvas.rootToLocal(ev.x, ev.y)

        if code == _tokens.PUSH.hashed:
            self._start_drag(mx, my, ct)
        elif code == _tokens.RELEASE.hashed:
            was_dragging = ts.dragging or ts.dragging_port
            ts.dragging = False
            ts.dragging_port = False
            ts.drag_leaf_names = []
            if was_dragging:
                ts.builder.save_layout(ts.name)
        elif code == _tokens.DRAG.hashed:
            if ts.dragging_port:
                self._update_port_drag(mx, my, ct)
            elif ts.dragging:
                self._update_drag(mx, my, ct)

        return lev2.ui.HandlerResult()

    def _start_drag(self, mx, my, ct):
        ts = ct.state
        b = ts.builder
        cw, ch = ct.last_w, ct.last_h
        if cw < 2 or ch < 2:
            return

        PORT_HIT_RADIUS = max(8.0, 12.0 * cw / REF_W)
        if ts.last_layout:
            layout_transitions = ts.last_layout._transitions
            for points, label, color, lp, crossings, sp, dp, aidx in ts.last_layout.arrow_data:
                sn, dn, ev, thint = layout_transitions[aidx]
                dist_src = math.sqrt((mx - sp[0])**2 + (my - sp[1])**2)
                if dist_src < PORT_HIT_RADIUS:
                    ts.dragging_port = True
                    ts.drag_port_arrow_idx = aidx
                    ts.drag_port_role = "src"
                    ts.drag_port_node_name = sn
                    return
                dist_dst = math.sqrt((mx - dp[0])**2 + (my - dp[1])**2)
                if dist_dst < PORT_HIT_RADIUS:
                    ts.dragging_port = True
                    ts.drag_port_arrow_idx = aidx
                    ts.drag_port_role = "dst"
                    ts.drag_port_node_name = dn
                    return

        layout = FsmLayout(b, cw, ch)

        for name, rect, _color in layout.leaf_rects:
            if rect.contains(mx, my):
                ts.dragging = True
                ts.drag_leaf_names = [name]
                ts.drag_last_mx = mx
                ts.drag_last_my = my
                return

        for name, rect, *_ in reversed(layout.group_rects):
            header_bottom = rect.y + GROUP_HEADER_H * (ch / REF_H)
            if rect.x <= mx <= rect.right and rect.y <= my <= header_bottom:
                state = b.find(name)
                leaves = b.leaf_descendants(state)
                leaf_names = [l.name for l in leaves
                              if l.name in b.state_hints and b.state_hints[l.name].pos]
                if leaf_names:
                    ts.dragging = True
                    ts.drag_leaf_names = leaf_names
                    ts.drag_last_mx = mx
                    ts.drag_last_my = my
                    return

    def _update_drag(self, mx, my, ct):
        ts = ct.state
        cw, ch = ct.last_w, ct.last_h
        if cw < 2 or ch < 2:
            return
        dx_norm = (mx - ts.drag_last_mx) / cw
        dy_norm = (my - ts.drag_last_my) / ch
        for name in ts.drag_leaf_names:
            hint = ts.builder.state_hints.get(name)
            if hint and hint.pos:
                px, py = hint.pos
                hint.pos = (px + dx_norm, py + dy_norm)
        ts.drag_last_mx = mx
        ts.drag_last_my = my
        ts.dirty = True

    def _update_port_drag(self, mx, my, ct):
        ts = ct.state
        cw, ch = ct.last_w, ct.last_h
        if cw < 2 or ch < 2:
            return

        node_name = ts.drag_port_node_name
        if node_name not in ts.last_layout._state_rects:
            return

        rect = ts.last_layout._state_rects[node_name]
        angle = math.atan2(my - rect.cy, mx - rect.cx)

        if ts.last_layout and ts.drag_port_arrow_idx < len(ts.last_layout._transitions):
            sn, dn, ev, _ = ts.last_layout._transitions[ts.drag_port_arrow_idx]
            for src_state, event, dst_state, thint in ts.builder.transitions:
                if src_state.name == sn and dst_state.name == dn and event == ev:
                    if ts.drag_port_role == "src":
                        thint.src_port_angle = angle
                    else:
                        thint.dst_port_angle = angle
                    break

        ts.dirty = True

    # ── Rendering (per-tab) ──

    def _on_pre_render_tab(self, ct):
        ts = ct.state
        cw, ch = ct.canvas.width, ct.canvas.height
        if cw < 2 or ch < 2:
            return
        if cw != ct.last_w or ch != ct.last_h:
            ct.last_w, ct.last_h = cw, ch
            ts.dirty = True
        if not ts.dirty:
            return
        ts.dirty = False
        self._rebuild_tab(ct)

    def _rebuild_tab(self, ct):
        ts = ct.state
        cw, ch = ct.last_w, ct.last_h
        pip, pip_vtx, font = ct.pip, ct.pip_vtx, ct.font
        sx, sy = cw / REF_W, ch / REF_H

        ct.lyr_groups.clear()
        ct.lyr_nodes.clear()
        ct.lyr_arrows.clear()
        ct.lyr_icons.clear()
        ct.lyr_text.clear()
        ct.lyr_labels.clear()

        layout = FsmLayout(ts.builder, cw, ch)
        ts.last_layout = layout

        def quad(layer, x, y, w, h, color, radius=0):
            qp = lev2.ui.QuadPrimitive(pipeline=pip)
            qd = lev2.ui.QuadData()
            qd.setPosition(x, ch - y - h)
            qd.setSize(w, h)
            qd.setColor(color)
            if radius > 0:
                qd.setCornerRadius(radius)
            qp.addQuad(qd)
            layer.addPrimitive(qp)

        def text(layer, s, x, y, color):
            tp = lev2.ui.TextPrimitive(font=font, color=color)
            tp.addItem(s, vec2(x, y))
            layer.addPrimitive(tp)

        # Title
        if ts.name:
            text(ct.lyr_text, ts.name,
                 cw/2 - len(ts.name)*4, 30*sy, COL_TEXT_TITLE)
        if ts.subtitle:
            text(ct.lyr_text, ts.subtitle,
                 cw/2 - len(ts.subtitle)*4, 52*sy, COL_TEXT_SUB)

        # Groups — draw boxes and borders; labels placed after crossings are known
        group_hooks = {}
        group_rects_by_name = {}
        for name, rect, gc, bc, hooks, hc in layout.group_rects:
            bw = max(1.0, 1.5 * sx)
            r = 10 * sx
            quad(ct.lyr_groups, rect.x - bw, rect.y - bw, rect.w + bw*2, rect.h + bw*2, bc, radius=r + bw)
            quad(ct.lyr_groups, rect.x, rect.y, rect.w, rect.h, gc, radius=r)
            group_rects_by_name[name] = rect
            if hooks and "onExit:" in hooks:
                parts = hooks.split("onExit:")
                enter_act = parts[0].replace("onEnter:", "").strip()
                exit_act = parts[1].strip()
                group_hooks[name] = (enter_act, exit_act, hc)

        # Leaf nodes
        for name, rect, color in layout.leaf_rects:
            quad(ct.lyr_nodes, rect.x, rect.y, rect.w, rect.h, color, radius=6*sx)
            text(ct.lyr_text, name, rect.cx - len(name)*4, rect.cy - 6, COL_TEXT)

        # Shared label constants
        CHAR_W = 7
        LABEL_H = 16
        LABEL_PAD = 4
        LABEL_TEXT_Y_NUDGE = 1  # vertical adjustment for text centering in label boxes
        COL_LABEL_BG = vec4(0.04, 0.04, 0.07, 0.60)

        # Arrows (bezier) + group boundary crossing dots
        arrow_prim = lev2.ui.TriListPrimitive(pipeline=pip_vtx)
        thickness = max(1.2, 1.8 * sx)
        dot_radius = max(3.0, 4.5 * sx)
        dot_segments = 12

        # Collect all action labels and crossing positions per group
        action_labels = []  # (anchor_x, anchor_y, label_text, color, tangent_x, tangent_y, arrow_idx, gname)
        event_label_rects = []  # placed event label rects for overlap checking
        group_crossings = defaultdict(list)  # group_name -> [(x, y), ...]

        for points, label, color, label_pos, crossings, sp, dp, aidx in layout.arrow_data:
            verts = _make_bezier_arrow_verts(points, color, thickness, ch)
            for vt in verts:
                arrow_prim.addVertex(vt)
            # Event label — slide along curve to avoid crossings and other labels
            elw = len(label) * CHAR_W + LABEL_PAD * 2
            elh = LABEL_H + LABEL_PAD

            # Collect obstacle rects: crossing icons + already-placed event labels
            obstacles = []
            for cx_c, cy_c, _, _, _ in crossings:
                obs_sz = dot_radius * 3
                obstacles.append((cx_c - obs_sz, cy_c - obs_sz, obs_sz*2, obs_sz*2))

            # Try positions along the curve, starting from midpoint, alternating outward
            n_pts = len(points)
            best_idx = n_pts // 2
            found = False
            for offset in range(n_pts // 2):
                for idx in ([n_pts//2 + offset, n_pts//2 - offset] if offset > 0 else [n_pts//2]):
                    if idx < 1 or idx >= n_pts - 1:
                        continue
                    px, py = points[idx]
                    ex, ey = px - elw/2, py - elh/2
                    # Check overlap with obstacles
                    overlaps = False
                    for ox, oy, ow, oh in obstacles:
                        if (ex < ox + ow and ex + elw > ox and
                            ey < oy + oh and ey + elh > oy):
                            overlaps = True
                            break
                    # Check overlap with already placed event labels
                    if not overlaps:
                        for plx, ply, plw, plh in event_label_rects:
                            if (ex < plx + plw and ex + elw > plx and
                                ey < ply + plh and ey + elh > ply):
                                overlaps = True
                                break
                    if not overlaps:
                        best_idx = idx
                        found = True
                        break
                if found:
                    break

            mid_x, mid_y = points[best_idx]
            elx = mid_x - elw / 2
            ely = mid_y - elh / 2
            event_label_rects.append((elx, ely, elw, elh))
            quad(ct.lyr_labels, elx, ely, elw, elh, COL_LABEL_BG, radius=3*sx)
            text(ct.lyr_labels, label, elx + LABEL_PAD, ely + LABEL_PAD/2 + LABEL_TEXT_Y_NUDGE, color)

            # Draw flow-direction arrows at group boundary crossings
            # and collect action labels
            for crossing_x, crossing_y, tx, ty, gname in crossings:
                icon_sz = dot_radius * 3
                angle = math.atan2(ty, tx)
                self._draw_rotated_icon(ct.lyr_icons, ct.tex_flow_arrow,
                                        crossing_x, crossing_y, icon_sz, angle, ch, ct.pip_tex)
                group_crossings[gname].append((crossing_x, crossing_y))

                # Determine action label from group hooks + flow direction
                if gname in group_hooks:
                    enter_act, exit_act, hc = group_hooks[gname]
                    inward_nx = ts.last_layout._state_rects[gname].cx - crossing_x
                    inward_ny = ts.last_layout._state_rects[gname].cy - crossing_y
                    dot = tx * inward_nx + ty * inward_ny
                    act_text = enter_act if dot > 0 else exit_act
                    action_labels.append((crossing_x, crossing_y, act_text, hc, tx, ty, aidx, gname))

        ct.lyr_arrows.addPrimitive(arrow_prim)

        # Place group name labels in the corner furthest from average crossing position
        LABEL_MARGIN = 6
        for gname, rect in group_rects_by_name.items():
            pts = group_crossings.get(gname, [])
            if pts:
                avg_x = sum(p[0] for p in pts) / len(pts)
                avg_y = sum(p[1] for p in pts) / len(pts)
            else:
                # No crossings — default to bottom-right so label goes top-left
                avg_x = rect.right
                avg_y = rect.bottom

            # Four corners
            corners = [
                (rect.x + LABEL_MARGIN,                    rect.y + LABEL_MARGIN),      # top-left
                (rect.right - len(gname)*7 - LABEL_MARGIN, rect.y + LABEL_MARGIN),      # top-right
                (rect.x + LABEL_MARGIN,                    rect.bottom - 16 - LABEL_MARGIN),  # bottom-left
                (rect.right - len(gname)*7 - LABEL_MARGIN, rect.bottom - 16 - LABEL_MARGIN),  # bottom-right
            ]
            # Pick corner furthest from average crossing position
            best_corner = corners[0]
            best_dist = -1
            for cx, cy in corners:
                d = (cx - avg_x)**2 + (cy - avg_y)**2
                if d > best_dist:
                    best_dist = d
                    best_corner = (cx, cy)

            text(ct.lyr_text, gname, best_corner[0], best_corner[1], COL_TEXT_GROUP)

        # Layout action labels.
        # Pass 1: Determine outer side per curve from curvature (cross product
        #         of start→mid and mid→end vectors). This is global to the curve.
        # Pass 2: Place each label at MAX_TETHER on the outer side, perpendicular
        #         to curve tangent at the crossing. Resolve overlaps.
        MAX_TETHER = 90.0 * sx

        # Pass 1: compute outer-side normal per curve (arrow_idx -> (nx, ny))
        curve_outer = {}  # arrow_idx -> (outer_nx, outer_ny) unit normal
        for points, label, color, label_pos, crossings, sp, dp, aidx in layout.arrow_data:
            if len(points) < 3:
                continue
            # Use start, mid, end points to determine curvature direction
            p0 = points[0]
            pm = points[len(points) // 2]
            p1 = points[-1]
            # Vectors: start→mid and start→end
            v1x, v1y = pm[0] - p0[0], pm[1] - p0[1]
            v2x, v2y = p1[0] - p0[0], p1[1] - p0[1]
            # Cross product (z-component): positive = curves left, negative = curves right
            cross = v1x * v2y - v1y * v2x
            # Chord direction (start → end)
            cx_d, cy_d = p1[0] - p0[0], p1[1] - p0[1]
            chord_len = math.sqrt(cx_d*cx_d + cy_d*cy_d)
            if chord_len > 0.001:
                cx_d, cy_d = cx_d / chord_len, cy_d / chord_len
            # Outer normal = perpendicular to chord, on convex side
            # cross > 0: curve bows left, outer is right (-cy_d, cx_d)
            # cross < 0: curve bows right, outer is left (cy_d, -cx_d)
            if cross > 0:
                curve_outer[aidx] = (cy_d, -cx_d)   # right side of chord
            else:
                curve_outer[aidx] = (-cy_d, cx_d)    # left side of chord

        # Pass 2: place labels on outer side, perpendicular to tangent
        label_rects = []
        for ax, ay, ltxt, lcol, tanx, tany, aidx, gname in action_labels:
            lw = len(ltxt) * CHAR_W + LABEL_PAD * 2
            lh = LABEL_H + LABEL_PAD

            # Get curve's outer normal
            outer = curve_outer.get(aidx, (0, -1))
            onx, ony = outer

            # Perpendicular to tangent at crossing, on outer side
            tan_len = math.sqrt(tanx*tanx + tany*tany)
            if tan_len > 0.001:
                tux, tuy = tanx / tan_len, tany / tan_len
                # Two perpendicular options
                perp_ax, perp_ay = -tuy, tux
                perp_bx, perp_by = tuy, -tux
                # Pick the one that aligns with curve's outer normal
                dot_a = perp_ax * onx + perp_ay * ony
                dot_b = perp_bx * onx + perp_by * ony
                if dot_a >= dot_b:
                    nx, ny = perp_ax, perp_ay
                else:
                    nx, ny = perp_bx, perp_by
            else:
                nx, ny = onx, ony

            # Place label at MAX_TETHER on outer side
            lbl_cx = ax + nx * MAX_TETHER
            lbl_cy = ay + ny * MAX_TETHER
            lx = lbl_cx - lw / 2
            ly = lbl_cy - lh / 2
            label_rects.append([lx, ly, lw, lh, ax, ay, ltxt, lcol])

        # Pass 3: resolve overlapping label rects by pushing apart
        for _iteration in range(30):
            moved = False
            for i in range(len(label_rects)):
                for j in range(i + 1, len(label_rects)):
                    ri, rj = label_rects[i], label_rects[j]
                    # Check rect overlap
                    if (ri[0] < rj[0] + rj[2] and ri[0] + ri[2] > rj[0] and
                        ri[1] < rj[1] + rj[3] and ri[1] + ri[3] > rj[1]):
                        # Push apart: compute center-to-center vector
                        ci_x, ci_y = ri[0] + ri[2]/2, ri[1] + ri[3]/2
                        cj_x, cj_y = rj[0] + rj[2]/2, rj[1] + rj[3]/2
                        dx = cj_x - ci_x
                        dy = cj_y - ci_y
                        d = math.sqrt(dx*dx + dy*dy)
                        if d < 1:
                            dx, dy, d = 1, 0, 1
                        push = 4.0
                        ri[0] -= (dx/d) * push
                        ri[1] -= (dy/d) * push
                        rj[0] += (dx/d) * push
                        rj[1] += (dy/d) * push
                        moved = True
            if not moved:
                break

        # Render action labels with dark background + connecting line
        line_prim = lev2.ui.TriListPrimitive(pipeline=pip_vtx)
        line_thickness = max(0.8, 1.0 * sx)

        for lx, ly, lw, lh, ax, ay, ltxt, lcol in label_rects:
            # Dark background box
            quad(ct.lyr_labels, lx, ly, lw, lh, COL_LABEL_BG, radius=3*sx)
            text(ct.lyr_labels, ltxt, lx + LABEL_PAD, ly + LABEL_PAD/2 + LABEL_TEXT_Y_NUDGE, lcol)
            # Thin connecting line — start past the direction indicator icon
            lcx, lcy = lx + lw/2, ly + lh/2
            dx_l, dy_l = lcx - ax, lcy - ay
            dist_l = math.sqrt(dx_l*dx_l + dy_l*dy_l)
            icon_clear = dot_radius * 1.5
            if dist_l > icon_clear:
                ux_l, uy_l = dx_l / dist_l, dy_l / dist_l
                lsx = ax + ux_l * icon_clear
                lsy = ay + uy_l * icon_clear
                line_verts = _make_thin_line(lsx, lsy, lcx, lcy, lcol, line_thickness, ch)
            else:
                line_verts = []
            for v in line_verts:
                line_prim.addVertex(v)

        ct.lyr_icons.addPrimitive(line_prim)

        # Legend with colored dot indicators
        legend_dot_prim = lev2.ui.TriListPrimitive(pipeline=pip_vtx)
        legend_dot_r = max(3.0, 4.5 * sx)
        lx, ly = 20 * sx, ch - 140 * sy
        text(ct.lyr_text, "Transitions:", lx, ly, COL_TEXT_SUB)
        for i, (src, ev, dst, thint) in enumerate(ts.builder.transitions):
            col = thint.color or DEFAULT_ARROW_COLOR
            row_y = ly + (18 + i * 18) * sy
            _add_circle_verts(legend_dot_prim,
                              lx + legend_dot_r,
                              ch - row_y - 5,
                              legend_dot_r, col, 12)
            line = f"{ev:12s} {src.name} -> {dst.name}"
            text(ct.lyr_text, line, lx + legend_dot_r * 3 + 4, row_y, col)
        ct.lyr_arrows.addPrimitive(legend_dot_prim)

        # Footer
        if ts.footer_lines:
            fx = cw - 420 * sx
            fy = ch - (30 + len(ts.footer_lines) * 16) * sy
            for i, (s, col) in enumerate(ts.footer_lines):
                text(ct.lyr_text, s, fx, fy + i*16*sy, col)

        ct.canvas.markDirty()

    def _open_svg_export_dialog(self):
        """Open a file browser in a secondary window for SVG export."""
        from ork.ui.filesystem_browser import FilesystemBrowser
        home = os.path.expanduser("~")
        popup = self.ezapp.createSecondaryWindow(
            width=800, height=600, x=200, y=150,
            title="Export SVG", decorated=True, resizable=True, floating=True)
        uic = popup.ui_context
        root = lev2.ui.LayoutGroup.create("popup_lg")
        root.setRect(0, 0, popup.width, popup.height)
        uic.top = root
        root.margin = 4

        browser_item = root.makeChild(
            uiclass=FilesystemBrowser,
            args=["browser", home, ".svg", vec3(0.1, 0.1, 0.1), "save"],
            fill=True)
        browser = browser_item.widget.uservars.filesystem_browser

        def on_export(path):
            path_str = str(path)
            active_idx = self._tabs_widget.getActiveTab()
            if active_idx < len(self._canvas_tabs):
                self._canvas_tabs[active_idx].canvas.exportSvg(path_str)
                os.system(f"open {path_str}")
            popup.requestClose()

        browser.onActivate = on_export
        browser.onCancel = lambda: popup.requestClose()

    def _onUpdate(self, updinfo):
        pass
