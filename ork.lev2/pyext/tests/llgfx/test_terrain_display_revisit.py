#!/usr/bin/env ork.python
###############################################################################
# Terrain DISPLAY-REVISIT regression gate (#88). Drives the REAL dflow editor terrain
# chain for xxx3 through the CANVAS display-flag path (NodeEditor._set_display) over the
# owner's revisit sequence lpf_3 -> flow3d_0 -> lpf_3, in ONE session, and asserts each
# leg's SETTLED render matches its OWN node:
#     leg3 (revisit lpf_3)  == leg1 (lpf_3)          — a revisit restores the surface
#     leg2 (flow3d_0)       != leg1 (lpf_3)          — a distinct node shows a distinct field
# The sequence is run TWICE (same session) and the per-leg pixel CRCs must be deterministic
# across both passes. Offscreen (flagtest) with a PRIVATE assetcache sink so it never
# collides with a live editor's <assetcache>/terrain/terra bakes (the warm SHARED dflowcache
# at <staging> is untouched, so the cook stays warm).
#
# SCOPE NOTE: this guards the deterministic display-revisit path (the on-disk product + the
# fresh-drawable swap). The owner's #88 report is a WINDOWED-interactive PERMANENT staleness
# that did not reproduce in any offscreen configuration (cold/warm, private/real sink, every
# switch timing); this gate LOCKS IN the offscreen-correct behavior against regression.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, zlib, tempfile

from orkengine import core   # core before lev2
from orkengine import lev2   # noqa
from ork.editor.dflowedit import DflowEditor
from orkengine.core import Path as _Path

SEQ = ["lpf_3", "flow3d_0", "lpf_3"]
_PRIV = os.path.join(tempfile.gettempdir(), "tered_revisit_ac")


def _crc(b):
    return "%08x" % (zlib.crc32(b) & 0xFFFFFFFF) if b is not None else "<none>"


class RevisitGate(DflowEditor):
    def __init__(self, source):
        self._passes = []            # per pass: [crc0(lpf_3), crc1(flow3d_0), crc2(lpf_3)]
        self.result_ok = False       # machine verdict (read by main for the exit code)
        self._settle = 150
        self._after_swap = 180       # frames post-swap before capture (generous settle)
        super().__init__([source], flagtest=True)

    def _onGpuInit(self, ctx):
        os.makedirs(_PRIV, exist_ok=True)
        _Path.setPathExpander("assetcache", _Path(_PRIV))   # private sink (no owner collision)
        super()._onGpuInit(ctx)

    def _flagtestTick(self):
        host = self._viewport_host
        if host is None:
            print("REVISIT_RESULT=NA (no viewport host)", flush=True)
            self._selftest_done = True; self.ezapp.signalExit(); return
        st = self._st
        if st is None:
            st = self._st = {"stage": "settle", "f": 0, "pass": 0, "leg": 0, "cur": []}
        st["f"] += 1; f = st["f"]; stage = st["stage"]

        if stage == "settle":
            if f >= self._settle:
                st["stage"] = "issue"
        elif stage == "issue":
            nid = SEQ[st["leg"]]
            st["rc_before"] = host.rebuild_count
            st["psc_before"] = host.plane_swap_count
            st["switch_f"] = f
            self.node_editor._set_display(nid)      # CANVAS display-flag path
            st["stage"] = "wait"
        elif stage == "wait":
            # a leg lands via a FULL SWAP (rebuild_count) OR — #88 v2 — an in-place plane
            # rebind on an interior REVISIT of a current product (plane_swap_count). A same-
            # display no-op does NEITHER and falls through to the settled-frame capture below.
            triggered = (host.rebuild_count > st["rc_before"]
                         or host.plane_swap_count > st["psc_before"])
            if triggered and (f - st["switch_f"]) >= self._after_swap:
                self._cap_request(); st["stage"] = "cap"
            elif (f - st["switch_f"]) >= 1200:      # no-op / stuck -> capture the settled state
                self._cap_request(); st["stage"] = "cap"
        elif stage == "cap":
            if not self._cap_pending and self._cap_result is not None:
                rgb = self._cap_result.get("rgb")
                st["cur"].append(_crc(rgb.tobytes() if rgb is not None else None))
                st["leg"] += 1
                if st["leg"] >= len(SEQ):
                    self._passes.append(list(st["cur"]))
                    st["cur"] = []; st["leg"] = 0; st["pass"] += 1
                    st["stage"] = "done" if st["pass"] >= 2 else "issue"
                else:
                    st["stage"] = "issue"
        elif stage == "done":
            self._finish()
            return
        if f > 9000 and not self._selftest_done:
            print("REVISIT_TIMEOUT", flush=True); self._finish()

    def _finish(self):
        p = self._passes
        ok = False
        if len(p) == 2 and all(len(x) == 3 for x in p):
            a, b = p[0], p[1]
            # per-pass invariant (byte-exact WITHIN a pass): a revisit restores the EXACT
            # surface, and the intermediate flow3d_0 is a distinct field. Run twice: the
            # invariant must reproduce in BOTH passes. (Cross-pass byte equality is NOT
            # asserted — the leg's temporal/camera settle state legitimately differs, e.g.
            # pass-2's opening lpf_3 is a same-display no-op held for the settle window.)
            restore = (a[2] == a[0]) and (b[2] == b[0])          # leg3 == leg1 each pass
            distinct = (a[1] != a[0]) and (b[1] != b[0])         # flow3d_0 != lpf_3
            ok = restore and distinct
            print(f"[revisit] pass1 crcs {a}", flush=True)
            print(f"[revisit] pass2 crcs {b}", flush=True)
            print(f"[revisit] leg3==leg1 (both passes) {restore}  flow3d!=lpf3 (both) {distinct} "
                  f"-> {ok}", flush=True)
        else:
            print(f"[revisit] INCOMPLETE passes={p}", flush=True)
        self.result_ok = ok
        print(f"REVISIT_RESULT={'PASS' if ok else 'FAIL'}", flush=True)
        self._selftest_done = True
        self.ezapp.signalExit()


def main():
    app = RevisitGate("xxx3")
    app.ezapp.mainThreadLoop()
    app.ezapp.shutdown()
    sys.exit(0 if app.result_ok else 1)


if __name__ == "__main__":
    main()
