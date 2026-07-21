#!/usr/bin/env ork.python
###############################################################################
# Terrain DISPLAY IN-PLACE fast-path gate (#88 v2). Drives the REAL dflow editor terrain
# chain for xxx3 through the CANVAS display-flag path (NodeEditor._set_display) over the
# owner's interior-revisit sequence, in ONE session:
#
#     default -> flow3d_0 -> lpf_3 -> flow3d_0   (the last leg is an interior REVISIT)
#
#   leg0 flow3d_0 : terra(default) -> interior CROSSES the material boundary  -> FULL SWAP
#   leg1 lpf_3    : first visit (product not yet current this session)        -> FULL SWAP
#   leg2 flow3d_0 : interior->interior REVISIT, product current on disk       -> IN-PLACE
#
# Asserts the v2 contract on the revisit leg:
#   (a) NO rebuild_count bump  AND  a plane_swap_count bump  == the full swap was SKIPPED and
#       the held drawable's height plane was morphed in place (scenegraph/sim/camera kept);
#   (b) leg2 renders BYTE-IDENTICAL to leg0 (same flow3d_0 surface, same camera) — the in-
#       place plane push must reproduce a fresh full-swap of the same product exactly.
# The two full-swap legs must each bump rebuild_count (and NOT plane_swap_count).
#
# Offscreen (flagtest) with a PRIVATE assetcache sink so it never collides with a live
# editor's <assetcache>/terrain bakes (the warm SHARED dflowcache at <staging> is untouched).
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, zlib, tempfile

from orkengine import core   # core before lev2
from orkengine import lev2   # noqa
from ork.editor.dflowedit import DflowEditor
from orkengine.core import Path as _Path

# (node id, expected route): the last flow3d_0 is the interior REVISIT (in-place fast path).
LEGS = [("flow3d_0", "swap"), ("lpf_3", "swap"), ("flow3d_0", "inplace")]
_PRIV = os.path.join(tempfile.gettempdir(), "tered_inplace_ac")


def _crc(b):
    return "%08x" % (zlib.crc32(b) & 0xFFFFFFFF) if b is not None else "<none>"


class InplaceGate(DflowEditor):
    def __init__(self, source):
        self._legs = []              # per leg: {"crc","rc_delta","psc_delta","route"}
        self.result_ok = False       # machine verdict (read by main for the exit code)
        self._settle = 150
        self._after_swap = 180       # frames post-trigger before capture (generous settle)
        super().__init__([source], flagtest=True)

    def _onGpuInit(self, ctx):
        os.makedirs(_PRIV, exist_ok=True)
        _Path.setPathExpander("assetcache", _Path(_PRIV))   # private sink (no owner collision)
        super()._onGpuInit(ctx)

    def _flagtestTick(self):
        host = self._viewport_host
        if host is None:
            print("INPLACE_RESULT=NA (no viewport host)", flush=True)
            self._selftest_done = True; self.ezapp.signalExit(); return
        st = self._st
        if st is None:
            st = self._st = {"stage": "settle", "f": 0, "leg": 0}
        st["f"] += 1; f = st["f"]; stage = st["stage"]

        if stage == "settle":
            if f >= self._settle:
                st["stage"] = "issue"
        elif stage == "issue":
            nid, route = LEGS[st["leg"]]
            st["rc_before"] = host.rebuild_count
            st["psc_before"] = host.plane_swap_count
            st["switch_f"] = f
            self.node_editor._set_display(nid)      # CANVAS display-flag path
            st["stage"] = "wait"
        elif stage == "wait":
            _, route = LEGS[st["leg"]]
            swapped = host.rebuild_count > st["rc_before"]
            rebound = host.plane_swap_count > st["psc_before"]
            # each route waits on ITS expected trigger, but accept the other so a WRONG route
            # is still captured + recorded (the per-leg assertion then fails loudly).
            triggered = rebound if route == "inplace" else swapped
            if (triggered or swapped or rebound) and (f - st["switch_f"]) >= self._after_swap:
                self._cap_request(); st["stage"] = "cap"
            elif (f - st["switch_f"]) >= 1200:      # never triggered -> capture the stuck state
                self._cap_request(); st["stage"] = "cap"
        elif stage == "cap":
            if not self._cap_pending and self._cap_result is not None:
                rgb = self._cap_result.get("rgb")
                nid, route = LEGS[st["leg"]]
                self._legs.append({
                    "nid": nid, "route": route,
                    "crc": _crc(rgb.tobytes() if rgb is not None else None),
                    "rc_delta": host.rebuild_count - st["rc_before"],
                    "psc_delta": host.plane_swap_count - st["psc_before"],
                })
                st["leg"] += 1
                st["stage"] = "done" if st["leg"] >= len(LEGS) else "issue"
        elif stage == "done":
            self._finish()
            return
        if f > 12000 and not self._selftest_done:
            print("INPLACE_TIMEOUT", flush=True); self._finish()

    def _finish(self):
        legs = self._legs
        ok = False
        if len(legs) == len(LEGS):
            l0, l1, l2 = legs
            for lg in legs:
                print(f"[inplace] leg {lg['nid']:<10} route={lg['route']:<7} crc={lg['crc']} "
                      f"rc_delta={lg['rc_delta']} psc_delta={lg['psc_delta']}", flush=True)
            # the two full-swap legs bumped rebuild_count (and NOT plane_swap_count).
            swaps_ok = (l0["rc_delta"] >= 1 and l0["psc_delta"] == 0
                        and l1["rc_delta"] >= 1 and l1["psc_delta"] == 0)
            # the REVISIT leg took the in-place fast path: NO full swap, a plane rebind.
            inplace_ok = (l2["rc_delta"] == 0 and l2["psc_delta"] == 1)
            # and rendered byte-identical to the earlier flow3d_0 full-swap leg.
            identical = (l2["crc"] == l0["crc"]) and (l2["crc"] != "<none>")
            ok = swaps_ok and inplace_ok and identical
            print(f"[inplace] full-swap legs {swaps_ok}  revisit in-place {inplace_ok}  "
                  f"leg2==leg0 byte-identical {identical} -> {ok}", flush=True)
        else:
            print(f"[inplace] INCOMPLETE legs={legs}", flush=True)
        self.result_ok = ok
        print(f"INPLACE_RESULT={'PASS' if ok else 'FAIL'}", flush=True)
        self._selftest_done = True
        self.ezapp.signalExit()


def main():
    app = InplaceGate("xxx3")
    app.ezapp.mainThreadLoop()
    app.ezapp.shutdown()
    sys.exit(0 if app.result_ok else 1)


if __name__ == "__main__":
    main()
