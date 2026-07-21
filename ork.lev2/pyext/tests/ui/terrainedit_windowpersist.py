#!/usr/bin/env ork.python
################################################################################
# W6 gate: TerrainEditor multi-window arrangement persistence — REAL editor, offscreen.
#
#  A secondary DOCK window open at exit is persisted (schema v2 'windows' list) and
#  RECREATED on the next editor boot, hosting its member panels. Subprocess-per-mode,
#  MULTI-LAUNCH within a mode (persist -> relaunch), ork.testing VERDICT-BEFORE-TEARDOWN.
#
#    --mode a : PERSIST + RELAUNCH-DETERMINISM (the heart of the slice).
#               (1) fresh boot -> tear the propsheet out into a managed secondary via the
#                   glue's real tear-out pump -> _saveSession -> the on-disk state file
#                   grows a 'windows' entry (schema assert, driver-side).
#               (2) RELAUNCH (x2, fresh subprocess each): boot reads the state, RECREATES
#                   the secondary hosting the propsheet; dual assert — propsheet renders
#                   (lit) in the recreated secondary, and is STRUCTURALLY absent from main.
#               (3) the two relaunch secondary captures are BYTE-EQUAL (determinism oracle).
#    --mode b : RESET. From the persisted-multi-window state, the glue reset entry closes
#               the secondary (return-on-close) then clears the persisted 'windows' list;
#               a relaunch boots SINGLE-window.
#    --mode c : COMPAT. A v1 state file (no 'windows' list) loads exactly as today — single
#               window, identical layout signature, and a re-save is BYTE-IDENTICAL to the
#               v1 input (no 'windows' key injected).
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys, argparse, subprocess

ROOT = os.path.abspath(__file__)
for _ in range(5):
    ROOT = os.path.dirname(ROOT)
sys.path.insert(0, os.path.join(ROOT, "obt.project", "scripts"))

from orkengine.core import vec4, CrcStringProxy
from orkengine import lev2
from ork.testing import verdict, read_verdict, PASS, PASS_WITH_TEARDOWN_BUG

TERR_KW = dict(extent_m=512.0, dsl_kwargs={"amplitude": 60.0}, preview_dim=256, chunk=128)

W, H       = 480, 360               # main (editor) window
TEAR_XY    = (W + 120, 0)           # where the persist run drops the torn-out secondary

SETTLE     = 70    # frames for the terrain scene to bake + settle before gate ops
RECREATE_WAIT = 400  # frames to wait for boot-time recreation to realize the secondary
POST       = 34    # frames to let a (re)build render + its text surfaces settle before capture

TMP = os.path.join(os.environ.get("TMPDIR", "/tmp"), "terredit_winpersist")

LIT_THRESH      = 24     # per-channel brightness that counts a pixel as "lit"
SEC_CONTENT_MIN = 12000  # lit pixels proving the recreated property sheet renders content


################################################################################
# async framebuffer capture (is_ready-polled; the W1/W2 idiom)
################################################################################

class Capturer:
    def __init__(self):
        self._buf = None; self._fut = None
        self._inflight = False; self._armed = False
        self.result = None

    def arm(self):
        self._armed = True; self._inflight = False
        self._buf = None; self._fut = None; self.result = None

    @property
    def ready(self):
        return self.result is not None

    def tick(self, ctx):
        if not self._armed or self.result is not None:
            return
        if not self._inflight:
            self._buf = lev2.CaptureBuffer()
            self._fut = ctx.FBI.captureAsFormat(ctx.FBI.main_RTG.buffer(0), self._buf, "RGBA8")
            self._inflight = True
            return
        if self._fut is not None and not bool(self._fut.is_ready):
            return
        import numpy
        arr = numpy.array(self._buf, dtype=numpy.uint8).reshape(self._buf.height, self._buf.width, 4)
        self.result = arr[..., :3].copy()
        self._armed = False


def _save_png(arr, path):
    if arr is None:
        return
    from PIL import Image
    os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
    Image.fromarray(arr).save(path)

def _lit_count(arr):
    import numpy
    if arr is None:
        return -1
    return int((arr.max(axis=2) > LIT_THRESH).sum())


################################################################################
# gate app — the REAL TerrainEditor, subclassed to drive a per-role script from the
# main/GPU-thread post-frame hook (where dock mutations are render-safe).
################################################################################

from ork.editor.terrainedit import TerrainEditor


class Gate(TerrainEditor):

    def __init__(self, role, tag, out, **editor_kw):
        self.gate_role = role
        self.gate_tag  = tag
        self.gate_out  = out
        self._tokens   = CrcStringProxy()
        super().__init__("voronoi", **TERR_KW, offscreen=True, **editor_kw)

        self._gframe = 0
        self._gphase = 0
        self._gt0    = 0
        self._sec_win  = None
        self._sec_dock = None
        self._sec_cap  = Capturer()
        self._main_cap = Capturer()
        self._names_snap = {}
        self._verdict_emitted = False
        self.rc = 0

    @property
    def mgr(self):
        return self._dock_glue.mgr

    ############################################################

    def onAppExit(self):
        # the gate controls saving explicitly (at a known point); never save on exit so a
        # capture/probe run can NEVER clobber the on-disk slot between two relaunches.
        self._persist_layout = False
        super().onAppExit()

    ############################################################

    def onGpuPostFrame(self, ctx):
        super().onGpuPostFrame(ctx)         # editor capture + glue.pump (tear-out + W6 recreate)
        self._main_cap.tick(ctx)
        if self._sec_win is not None:
            try:
                self._sec_win.markDirty()   # keep the secondary re-rendering for its capturer
            except Exception:
                pass
        self._gframe += 1
        try:
            self._gate_tick(ctx)
        except Exception as e:
            import traceback
            traceback.print_exc()
            self._emit(False, f"gate exception: {e}")

    ############################################################

    @staticmethod
    def _names(dock):
        return sorted(p.name for p in dock.allPanels())

    def _secondary(self):
        for reg in self.mgr._windows.values():
            if not reg.is_main:
                return reg
        return None

    def _wire_secondary(self, reg):
        self._sec_win  = reg.window
        self._sec_dock = reg.dock
        reg.window.onGpuPostFrame = lambda c: self._sec_cap.tick(c)

    def _populate_propsheet_in(self, dock):
        # bind the Terrain Parameters model so the sheet has visible rows to render (a fresh
        # recreate carries the node-property model, empty until selection).
        self.params_model.refresh()
        for p in dock.allPanels():
            if p.name == "propsheet":
                p.child.model = self.params_model
                p.child.rebuild()
                return True
        return False

    ############################################################

    def _emit(self, ok, detail):
        if self._verdict_emitted:
            return
        self.rc = verdict(ok, detail)
        self._verdict_emitted = True
        # disarm teardown re-entry: the manager wired win.onClosed on every attached secondary.
        for reg in list(self.mgr._windows.values()):
            if reg.is_main:
                continue
            try:
                reg.window.onClosed = None
            except Exception:
                pass
        self.ezapp.signalExit()

    ############################################################
    # per-role script (main/GPU thread; dock mutations are render-sequential here)
    ############################################################

    def _gate_tick(self, ctx):
        if self._verdict_emitted:
            return
        r = self.gate_role
        if   r == "persist":      self._tick_persist(ctx)
        elif r == "capture":      self._tick_capture(ctx)
        elif r == "reset":        self._tick_reset(ctx)
        elif r == "single_probe": self._tick_single_probe(ctx)
        elif r == "dump_default": self._tick_dump_default(ctx)
        if self._gframe > 2000:
            self._emit(False, "TIMEOUT")

    ############################################################
    # persist: fresh boot -> tear propsheet out (glue pump) -> populate -> save -> assert file
    ############################################################

    def _tick_persist(self, ctx):
        ph = self._gphase
        if ph == 0:
            if self._gframe >= SETTLE:
                sx, sy = TEAR_XY
                # route through the REAL managed tear-out path (glue pump realizes it next frame)
                self.mgr._on_coord_tearout("propsheet", "main", sx, sy)
                self._gphase = 1
                self._gt0 = self._gframe
        elif ph == 1:
            reg = self._secondary()
            if reg is not None and "propsheet" in self._names(reg.dock):
                self._wire_secondary(reg)
                self._populate_propsheet_in(reg.dock)
                self._gphase = 2
                self._gt0 = self._gframe
            elif self._gframe >= self._gt0 + RECREATE_WAIT:
                self._emit(False, "tear-out never realized the secondary")
        elif ph == 2:
            if self._gframe >= self._gt0 + POST:
                self._saveSession()                       # THE save trigger (writes schema v2)
                self._assert_persist()

    def _assert_persist(self):
        import json
        path = self._sessionLayoutPath()
        with open(path) as f:
            doc = json.load(f)
        wins = doc.get("windows")
        has = isinstance(wins, list) and len(wins) >= 1
        entry = wins[0] if has else {}
        key_ok    = has and isinstance(entry.get("window_key"), str) and entry["window_key"]
        layout_ok = has and isinstance(entry.get("layout"), dict) and "propsheet" in _entry_ids(entry)
        print(f"[persist] windows={wins} key_ok={bool(key_ok)} layout_ok={bool(layout_ok)}", flush=True)
        print(f"WINDOWS_COUNT={len(wins) if has else 0}", flush=True)
        print(f"WINDOWS_KEY={entry.get('window_key') if has else ''}", flush=True)
        self._emit(bool(has and key_ok and layout_ok),
                   f"has_windows={has} key_ok={bool(key_ok)} propsheet_in_layout={bool(layout_ok)}")

    ############################################################
    # capture: boot recreates the secondary -> populate -> capture sec (lit) + assert structural
    ############################################################

    def _tick_capture(self, ctx):
        ph = self._gphase
        if ph == 0:
            reg = self._secondary()
            if reg is not None and "propsheet" in self._names(reg.dock):
                self._wire_secondary(reg)
                self._populate_propsheet_in(reg.dock)
                self._gphase = 1
                self._gt0 = self._gframe
            elif self._gframe >= RECREATE_WAIT:
                self._emit(False, "boot never recreated the secondary hosting the propsheet")
        elif ph == 1:
            if self._gframe >= self._gt0 + POST:
                self._sec_cap.arm(); self._main_cap.arm()
                self._gphase = 2
        elif ph == 2:
            if self._sec_cap.ready and self._main_cap.ready:
                sec = self._sec_cap.result
                _save_png(sec, self.gate_out)
                _save_png(self._main_cap.result,
                          os.path.join(TMP, f"{self.gate_tag}_main.png"))
                sec_names  = self._names(self._sec_dock)
                main_names = self._names(self.dock)
                lit = _lit_count(sec)
                in_sec      = "propsheet" in sec_names
                not_in_main = "propsheet" not in main_names
                content     = lit >= SEC_CONTENT_MIN
                print(f"[capture:{self.gate_tag}] sec_names={sec_names} main_names={main_names} "
                      f"sec_lit={lit} in_sec={in_sec} not_in_main={not_in_main} content={content}",
                      flush=True)
                print(f"CAP_OK={bool(in_sec and not_in_main and content)}", flush=True)
                self._emit(bool(in_sec and not_in_main and content),
                           f"in_sec={in_sec} not_in_main={not_in_main} content={content}")

    ############################################################
    # reset: boot recreates the secondary -> glue reset entry -> single window -> save (cleared)
    ############################################################

    def _tick_reset(self, ctx):
        # request_reset() sets the flag; the editor's own _onGpuUpdate drives pump_reset every
        # frame (the real Shift+L path) — close every secondary (return-on-close) then restore main.
        ph = self._gphase
        if ph == 0:
            if self.secondary_count() >= 1 and self._gframe >= SETTLE:
                self._dock_glue.request_reset()           # THE glue reset entry (Shift+L path)
                self._gphase = 1
                self._gt0 = self._gframe
            elif self._gframe >= RECREATE_WAIT:
                self._emit(False, "boot never recreated the secondary to reset")
        elif ph == 1:
            if self.secondary_count() == 0 and self._gframe >= self._gt0 + POST:
                self._saveSession()
                self._assert_reset()

    def secondary_count(self):
        return sum(1 for r in self.mgr._windows.values() if not r.is_main)

    def _assert_reset(self):
        import json
        path = self._sessionLayoutPath()
        with open(path) as f:
            doc = json.load(f)
        wins = doc.get("windows", None)
        cleared = not wins
        print(f"[reset] windows_after_reset={wins} secondary_count={self.secondary_count()}", flush=True)
        print(f"POST_RESET_WINDOWS={0 if cleared else len(wins)}", flush=True)
        self._emit(bool(cleared and self.secondary_count() == 0),
                   f"windows_cleared={cleared} single_window={self.secondary_count() == 0}")

    ############################################################
    # single_probe: settle, report single-window structural state (reset-relaunch + compat)
    ############################################################

    def _tick_single_probe(self, ctx):
        if self._gframe >= SETTLE:
            from ork.ui.dock_layout import save_layout, to_json
            print(f"SECCOUNT={self.secondary_count()}", flush=True)
            print(f"NAMES={self._names(self.dock)}", flush=True)
            print(f"SIG={self.dock.layoutSignature()}", flush=True)
            print(f"RESAVE={to_json(save_layout(self.dock))}", flush=True)
            self._emit(True, f"secondary_count={self.secondary_count()}")

    ############################################################
    # dump_default: force the default layout, emit its v1 json + signature (compat seed)
    ############################################################

    def _tick_dump_default(self, ctx):
        if self._gframe >= SETTLE:
            from ork.ui.dock_layout import save_layout, to_json
            print(f"V1JSON={to_json(save_layout(self.dock))}", flush=True)
            print(f"SIG={self.dock.layoutSignature()}", flush=True)
            self._emit(True, "default dumped")


def _entry_ids(entry):
    from ork.ui.dock_layout import panel_ids
    try:
        return panel_ids(entry.get("layout", {}))
    except Exception:
        return []


################################################################################
# child entry
################################################################################

def run_child(args):
    editor_kw = {}
    if args.role == "dump_default":
        editor_kw["reset_layout"] = True   # ignore any saved session -> canonical default layout
    gate = Gate(args.role, args.tag, args.out, **editor_kw)
    gate.ezapp.mainThreadLoop()            # teardown runs here, AFTER the verdict line
    sys.exit(gate.rc)


################################################################################
# driver
################################################################################

def _slot():
    from ork.ui.app_state import dock_layout_path
    return dock_layout_path("terrainedit", entry_path=os.path.abspath(__file__))

def _clean_slot():
    p = _slot()
    if os.path.exists(p):
        os.remove(p)

def _spawn(role, tag="x", out="", extra_env=None, timeout=360):
    env = dict(os.environ)
    if extra_env:
        env.update(extra_env)
    cmd = ["ork.python", os.path.abspath(__file__), "--role", role, "--tag", tag, "--out", out]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
    sys.stdout.write(p.stdout); sys.stderr.write(p.stderr)
    return p.returncode, (p.stdout or "") + (p.stderr or "")

def _pass(rc, out):
    return read_verdict(out, rc) in (PASS, PASS_WITH_TEARDOWN_BUG)

def _val(out, key):
    for line in out.splitlines():
        if line.startswith(key + "="):
            return line[len(key) + 1:]
    return None

def _pixels_equal(a, b):
    import numpy
    from PIL import Image
    ia = numpy.asarray(Image.open(a).convert("RGB"), dtype=numpy.int16)
    ib = numpy.asarray(Image.open(b).convert("RGB"), dtype=numpy.int16)
    if ia.shape != ib.shape:
        return False, -1
    return bool((ia == ib).all()), int(numpy.abs(ia - ib).max())


################################################################################

def mode_a():
    problems = []
    _clean_slot()

    # (1) PERSIST — tear the propsheet out into a managed secondary and save the arrangement
    rc, out = _spawn("persist", "persist")
    if not _pass(rc, out):
        problems.append(f"(a.persist) verdict={read_verdict(out, rc)}")
    # driver-side schema assert on the on-disk state file
    import json
    slot = _slot()
    try:
        with open(slot) as f:
            doc = json.load(f)
        wins = doc.get("windows")
        if not (isinstance(wins, list) and len(wins) == 1):
            problems.append(f"(a) state file missing 'windows' entry: {doc.get('windows')}")
        else:
            e = wins[0]
            if not e.get("window_key"):
                problems.append("(a) windows entry missing window_key")
            if "propsheet" not in _entry_ids(e):
                problems.append("(a) windows entry layout does not host the propsheet")
    except Exception as ex:
        problems.append(f"(a) could not read state file: {ex}")

    # (2) RELAUNCH x2 — recreate the secondary; assert content + determinism byte-equality
    r1 = os.path.join(TMP, "relaunch_r1_sec.png")
    r2 = os.path.join(TMP, "relaunch_r2_sec.png")
    rc1, out1 = _spawn("capture", "r1", r1)
    if not _pass(rc1, out1):
        problems.append(f"(a.relaunch1) verdict={read_verdict(out1, rc1)}")
    rc2, out2 = _spawn("capture", "r2", r2)
    if not _pass(rc2, out2):
        problems.append(f"(a.relaunch2) verdict={read_verdict(out2, rc2)}")

    if _val(out1, "CAP_OK") != "True":
        problems.append("(a) relaunch1 did not host+render the propsheet in the recreated secondary")
    if _val(out2, "CAP_OK") != "True":
        problems.append("(a) relaunch2 did not host+render the propsheet in the recreated secondary")

    if os.path.exists(r1) and os.path.exists(r2):
        eq, mx = _pixels_equal(r1, r2)
        print(f"[a] relaunch determinism: byte_equal={eq} maxdiff={mx}", flush=True)
        if not eq:
            problems.append(f"(a) relaunch secondary captures NOT byte-equal (maxdiff={mx})")
    else:
        problems.append("(a) missing relaunch capture PNG(s)")

    _clean_slot()
    return problems

def mode_b():
    problems = []
    _clean_slot()
    rc, out = _spawn("persist", "persist")
    if not _pass(rc, out):
        problems.append(f"(b.persist) verdict={read_verdict(out, rc)}")

    # RESET from the persisted-multi state: recreate then close -> clear 'windows'
    rcr, outr = _spawn("reset", "reset")
    if not _pass(rcr, outr):
        problems.append(f"(b.reset) verdict={read_verdict(outr, rcr)}")
    if _val(outr, "POST_RESET_WINDOWS") != "0":
        problems.append(f"(b) reset did not clear the persisted windows list "
                        f"(POST_RESET_WINDOWS={_val(outr, 'POST_RESET_WINDOWS')})")

    # relaunch after reset -> single window
    rcp, outp = _spawn("single_probe", "resetprobe")
    if not _pass(rcp, outp):
        problems.append(f"(b.relaunch) verdict={read_verdict(outp, rcp)}")
    if _val(outp, "SECCOUNT") != "0":
        problems.append(f"(b) reset session did NOT boot single-window (SECCOUNT={_val(outp, 'SECCOUNT')})")

    _clean_slot()
    return problems

def mode_c():
    problems = []
    _clean_slot()

    # seed a canonical v1 (no 'windows') state file from the editor's default layout
    rcd, outd = _spawn("dump_default", "dump")
    if not _pass(rcd, outd):
        problems.append(f"(c.dump) verdict={read_verdict(outd, rcd)}")
    v1 = _val(outd, "V1JSON")
    default_sig = _val(outd, "SIG")
    if not v1 or '"windows"' in v1:
        problems.append(f"(c) default dump is not a clean v1 doc: {v1}")
    else:
        with open(_slot(), "w") as f:
            f.write(v1)

    # boot on the v1 file -> single window, identical signature, byte-identical re-save
    rcp, outp = _spawn("single_probe", "compat")
    if not _pass(rcp, outp):
        problems.append(f"(c.probe) verdict={read_verdict(outp, rcp)}")
    if _val(outp, "SECCOUNT") != "0":
        problems.append(f"(c) v1 file wrongly recreated a secondary (SECCOUNT={_val(outp, 'SECCOUNT')})")
    if default_sig and _val(outp, "SIG") != default_sig:
        problems.append(f"(c) v1 layout not restored: SIG={_val(outp, 'SIG')} != {default_sig}")
    if v1 and _val(outp, "RESAVE") != v1:
        problems.append("(c) v1 re-save NOT byte-identical (a 'windows' key was injected)")

    _clean_slot()
    return problems


def driver(mode):
    os.makedirs(TMP, exist_ok=True)
    fn = {"a": mode_a, "b": mode_b, "c": mode_c}[mode]
    problems = fn()
    if problems:
        print(f"=== TerrainEditor window-persist gate (mode {mode}) FAILED ===", flush=True)
        for p in problems:
            print("  - " + p, flush=True)
        sys.exit(1)
    print(f"=== TerrainEditor window-persist gate (mode {mode}) PASSED ===", flush=True)
    sys.exit(0)


################################################################################

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", choices=["a", "b", "c"], default=None)
    ap.add_argument("--role",
                    choices=["persist", "capture", "reset", "single_probe", "dump_default"],
                    default=None)
    ap.add_argument("--tag", default="x")
    ap.add_argument("--out", default="")
    args = ap.parse_args()
    if args.role:
        run_child(args)
    elif args.mode:
        driver(args.mode)
    else:
        # default: run all three modes in sequence (one process orchestrating subprocesses)
        for m in ("a", "b", "c"):
            r = subprocess.run(["ork.python", os.path.abspath(__file__), "--mode", m])
            if r.returncode != 0:
                sys.exit(r.returncode)
        sys.exit(0)


if __name__ == "__main__":
    main()
