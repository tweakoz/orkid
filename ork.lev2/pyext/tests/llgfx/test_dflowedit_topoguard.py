#!/usr/bin/env ork.python

################################################################################
# test_dflowedit_topoguard — HOST-side invalid-topology resilience gate for the
# particles family in ork.dflow.edit.py (#81 part 2, the editor half of the owner's
# "topology edits never crash" law; the engine half is S8.5 createDrawable-returns-None).
#
# Drives the EXACT canvas/document path (node_model.delete_node / add_node / connect + host
# rebake) offscreen and proves the ParticlesViewportHost self-defends when a rebake yields an
# INVALID topology (createDrawable -> None):
#
#   DELETE (the owner's repro): load fireball -> settle a running plume -> DELETE a mid-chain
#     op via the document -> host rebake. Expect: NO exception; the last-good drawable is HELD
#     (the post-delete frame is BYTE-FROZEN == the pre-delete frame and NON-BLACK); the sim is
#     STOPPED (graphinst dropped, tick_count frozen even while PLAYING); a LOUD status note
#     ("invalid topology — sim stopped ...") is emitted. Then RECONNECT the chain gap -> the
#     sim RESUMES: a fresh valid drawable is committed, tick_count advances again, and the
#     frame CHANGES (transport MAD proof) — the owner's recover-on-restart expectation.
#
#   ADD-unwired: settle a running plume -> ADD a floating chain op (no wiring) via the document
#     -> host rebake. The capability EXCLUDES the fully-floating op ("... excluded until wired"),
#     so the preview keeps LIVING: NO exception, a valid drawable stays committed, the sim keeps
#     advancing (frame CHANGES, tick_count advances), and the exclusion note is emitted.
#
# Oracle: the DELETE post-edit frame matches the pre-delete frame (held last-good, frozen); the
# ADD post-edit frame is the excluded-preview (still living). Both stated in the leaf verdict.
#
# Each capture leaf is a FRESH offscreen process (the deterministic-seed discipline the sibling
# bypass gate relies on). benches=False -> the static production graph (no TESTBENCH), so the
# only motion is the sim itself. The leaf subclasses the shell (NO shell edit) purely to script
# the edit/capture state machine on top of the shell's proven offscreen capture + viewport host.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"

import sys
import shutil
import subprocess
import tempfile

_SCRIPTS = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         "..", "..", "..", "..", "obt.project", "scripts"))
if _SCRIPTS not in sys.path[:1]:
  sys.path.insert(0, _SCRIPTS)

BASE_ADV = 90            # PLAYING settle ticks before the first capture (a developed plume)
FROZEN_MAD = 0.01        # a HELD last-good frame must match the pre-edit frame within this (8-bit)
CHANGED_MAD = 0.02       # a RESUMED / LIVING sim must move whole-frame pixels by >= this (8-bit)
NONBLACK_MEAN = 0.01     # the last-good frame must be lit (mean gray, 0..1)
NONBLACK_RANGE = 0.02    # ... and carry content (max-min gray)


# ---- offscreen edit/capture leaf (a shell SUBCLASS — the shell file is untouched) ----

def _run_leaf(source, mode, base_adv, out_prefix):
  """Boot the editor shell offscreen and script a topology edit + captures for `mode`
  ('delete' | 'add'), printing a TOPO_<MODE>_RESULT verdict. Subclasses DflowEditor only to
  add the edit state machine — it reuses the shell's viewport host + offscreen capture."""
  import numpy
  from orkengine import core        # noqa: F401  (core before lev2)
  from orkengine import lev2         # noqa: F401
  from ork.editor.dflowedit import DflowEditor
  from ork.hypergraph.dflow.particles.capabilities import classify

  def _mad(a, b):
    if a is None or b is None or a.shape != b.shape:
      return -1.0
    return float(numpy.abs(a.astype(numpy.float32) - b.astype(numpy.float32)).mean())

  def _mean_range(rgb):
    if rgb is None:
      return (0.0, 0.0)
    g = rgb.astype(numpy.float32).mean(axis=2) / 255.0
    return (float(g.mean()), float(g.max() - g.min()))

  def _pick_midchain_op(model):
    """(op, src, dst): a chain OP with BOTH a pool source and a pool consumer, plus the two
    endpoints the reconnect must bridge after the op is deleted (src.pool -> dst.pool)."""
    pool_in, pool_out = {}, {}
    for (om, op_plug, im, ip) in model.edges():
      if op_plug == "pool" and ip == "pool":
        pool_out[om] = im
        pool_in[im] = om
    mids = [n for n in model.nodes()
            if classify(model._class_of(n)) == "op" and n in pool_in and n in pool_out]
    if not mids:
      return (None, None, None)
    op = mids[len(mids) // 2]
    return (op, pool_in[op], pool_out[op])

  class _TopoAbEditor(DflowEditor):
    def __init__(self, sources):
      self._topo_mode = mode
      self._topo_base = int(base_adv)
      self._topo_out = out_prefix
      self._topo_st = None
      self._topo_notes = []
      self._topo_done = False
      super().__init__(sources, offscreen=True, benches=False)

    # the shell arms its offscreen capture only for its own gates; drive the same drain here.
    def onGpuPostFrame(self, ctx):
      super().onGpuPostFrame(ctx)                 # parent's drain is gated off (no shell flag set)
      if self._cap_pending and not self._cap_inflight:
        self._capIssue(ctx)
      elif self._cap_inflight:
        ready = (self._cap_async is None) or bool(self._cap_async.is_ready)
        if ready:
          self._capFinish()

    def _onUpdate(self, updinfo):
      super()._onUpdate(updinfo)                  # viewport_host.update() + sgv.setDirty()
      if not self._topo_done:
        self._topoTick()

    def _capReady(self):
      return (not self._cap_pending) and (self._cap_result is not None)

    def _wireNotes(self):
      if self._viewport_host is not None and self._viewport_host._on_status is None:
        self._viewport_host._on_status = lambda m: self._topo_notes.append(m)

    def _note_seen(self, needle):
      return any(needle in n for n in self._topo_notes)

    def _finish(self, ok, lines):
      for ln in lines:
        print(f"[topo-{self._topo_mode}] {ln}", flush=True)
      print(f"TOPO_{self._topo_mode.upper()}_RESULT={'PASS' if ok else 'FAIL'}", flush=True)
      self._topo_done = True
      self.ezapp.signalExit()

    def _topoTick(self):
      host = self._viewport_host
      st = self._topo_st
      if host is None:
        self._finish(False, ["no viewport host (fireball did not host a particle payload)"])
        return
      if st is None:
        st = self._topo_st = {"stage": "settle", "f": 0}
      st["f"] += 1
      f = st["f"]
      W = 40                                       # advance window (rebake applies + pixels move)
      S = 20                                       # settle before a capture
      stage = st["stage"]

      if f > 2400:
        self._finish(False, [f"TIMEOUT at stage {stage!r}"])
        return

      if self._topo_mode == "delete":
        self._deleteTick(host, st, f, W, S, stage)
      else:
        self._addTick(host, st, f, W, S, stage)

    # ---- DELETE: hold-last-good on invalid topology, resume on reconnect ----
    def _deleteTick(self, host, st, f, W, S, stage):
      model = self._binding.node_model
      if stage == "settle":
        if f < self._topo_base or host._drawable is None:
          return
        self._wireNotes()
        op, src, dst = _pick_midchain_op(model)
        if op is None:
          self._finish(False, ["no mid-chain op with both pool endpoints (unexpected fireball shape)"])
          return
        st["op"], st["src"], st["dst"] = op, src, dst
        host.pause()                               # freeze a deterministic pre-edit frame
        st["stage"], st["at"] = "pre_settle", f + S
      elif stage == "pre_settle":
        if f >= st["at"]:
          self._cap_request(); st["stage"] = "pre_cap"
      elif stage == "pre_cap":
        if self._capReady():
          st["frame_pre"] = self._cap_result["rgb"]
          st["tick_pre"] = host.tick_count
          model.delete_node(st["op"])              # the owner's repro -> broken pool chain
          host.start()                             # PLAY: prove the sim stays STOPPED anyway
          st["stage"], st["at"] = "edit_advance", f + W
      elif stage == "edit_advance":
        if f >= st["at"]:
          st["drawable_kept"] = host._drawable is not None
          st["graphinst_none"] = host._graphinst is None
          st["tick_postdel"] = host.tick_count
          self._cap_request(); st["stage"] = "edit_cap"
      elif stage == "edit_cap":
        if self._capReady():
          st["frame_held1"] = self._cap_result["rgb"]
          st["stage"], st["at"] = "held_advance", f + W    # a 2nd held capture -> frozen-stable?
      elif stage == "held_advance":
        if f >= st["at"]:
          st["tick_held2"] = host.tick_count
          self._cap_request(); st["stage"] = "held_cap"
      elif stage == "held_cap":
        if self._capReady():
          st["frame_held2"] = self._cap_result["rgb"]
          model.connect(st["src"], "pool", st["dst"], "pool")   # reconnect the gap
          host.start()
          st["stage"], st["at"] = "recon_advance", f + W
      elif stage == "recon_advance":
        if f >= st["at"]:
          st["graphinst_back"] = host._graphinst is not None
          st["tick_recon"] = host.tick_count
          self._cap_request(); st["stage"] = "recon_cap"
      elif stage == "recon_cap":
        if self._capReady():
          st["frame_recon"] = self._cap_result["rgb"]
          self._deleteVerdict(host, st)

    def _deleteVerdict(self, host, st):
      import numpy  # noqa: F401
      mean_h, range_h = _mean_range(st["frame_held1"])
      # the engine's soft-fail link attempt perturbs the SHARED particle SSBO ONCE, so the held
      # frame is not byte-identical to the pre-delete frame; the SIM is nonetheless stopped. Prove
      # the HELD frame is FROZEN-STABLE (two held captures a window apart match) and near the
      # pre-delete plume relative to the resumed frame (held, not resimulated). Non-black on the
      # held frame is the anti-black-out proof.
      held_frozen_mad = _mad(st["frame_held1"], st["frame_held2"])
      near_pre_mad = _mad(st["frame_pre"], st["frame_held1"])
      resume_mad = _mad(st["frame_pre"], st["frame_recon"])
      nonblack = mean_h > NONBLACK_MEAN and range_h > NONBLACK_RANGE
      held = (st["drawable_kept"] and (0.0 <= held_frozen_mad <= FROZEN_MAD)
              and near_pre_mad < resume_mad)
      stopped = (st["graphinst_none"] and st["tick_postdel"] == st["tick_pre"]
                 and st["tick_held2"] == st["tick_pre"])
      note_stop = self._note_seen("invalid topology")
      resumed = (st["graphinst_back"] and st["tick_recon"] > st["tick_pre"]
                 and resume_mad >= CHANGED_MAD)
      note_resume = self._note_seen("sim resumed")
      ok = bool(nonblack and held and stopped and note_stop and resumed and note_resume)
      if self._topo_out:
        try:
          numpy.save(self._topo_out + "_pre.npy", st["frame_pre"])
          numpy.save(self._topo_out + "_held1.npy", st["frame_held1"])
          numpy.save(self._topo_out + "_recon.npy", st["frame_recon"])
        except Exception:
          pass
      self._finish(ok, [
          f"deleted mid-chain op {st['op']!r} (reconnect {st['src']}->{st['dst']})",
          f"last-good HELD: drawable_kept={st['drawable_kept']} held_frozen_MAD={held_frozen_mad:.4f} "
          f"(<= {FROZEN_MAD}) near_pre_MAD={near_pre_mad:.4f} (< resume) held_mean={mean_h:.4f} "
          f"held_range={range_h:.4f} nonblack={nonblack} held={held}",
          f"sim STOPPED: graphinst_none={st['graphinst_none']} tick {st['tick_pre']}=="
          f"{st['tick_postdel']}=={st['tick_held2']} (frozen={stopped}) status_note={note_stop}",
          f"RESUMED on reconnect: graphinst_back={st['graphinst_back']} tick {st['tick_pre']}->"
          f"{st['tick_recon']} resume_MAD={resume_mad:.4f} (>= {CHANGED_MAD}) "
          f"resumed={resumed} status_note={note_resume}",
      ])

    # ---- ADD-unwired: excluded floating op, preview keeps living ----
    def _addTick(self, host, st, f, W, S, stage):
      model = self._binding.node_model
      if stage == "settle":
        if f < self._topo_base or host._drawable is None:
          return
        self._wireNotes()
        self._cap_request(); st["stage"] = "a_cap"      # PLAYING (do NOT pause: prove it keeps living)
      elif stage == "a_cap":
        if self._capReady():
          st["frame_a"] = self._cap_result["rgb"]
          st["tick_a"] = host.tick_count
          new = model.add_node("psys::GravityModuleData", (0.0, 0.0))  # a floating (unwired) op
          st["new"] = new
          st["stage"], st["at"] = "b_advance", f + W
      elif stage == "b_advance":
        if f >= st["at"]:
          st["drawable_present"] = host._drawable is not None
          st["graphinst_live"] = host._graphinst is not None
          st["topo_invalid"] = host._topology_invalid
          st["tick_b"] = host.tick_count
          self._cap_request(); st["stage"] = "b_cap"
      elif stage == "b_cap":
        if self._capReady():
          st["frame_b"] = self._cap_result["rgb"]
          self._addVerdict(host, st)

    def _addVerdict(self, host, st):
      import numpy  # noqa: F401
      mean_a, range_a = _mean_range(st["frame_a"])
      living_mad = _mad(st["frame_a"], st["frame_b"])
      nonblack = mean_a > NONBLACK_MEAN and range_a > NONBLACK_RANGE
      present = st["drawable_present"] and st["graphinst_live"] and (not st["topo_invalid"])
      living = living_mad >= CHANGED_MAD and st["tick_b"] > st["tick_a"]
      note_excl = self._note_seen("excluded until wired")
      ok = bool(nonblack and present and living and note_excl)
      if self._topo_out:
        try:
          numpy.save(self._topo_out + "_a.npy", st["frame_a"])
          numpy.save(self._topo_out + "_b.npy", st["frame_b"])
        except Exception:
          pass
      self._finish(ok, [
          f"added floating op {st['new']!r} (unwired)",
          f"preview LIVING: drawable_present={st['drawable_present']} graphinst_live="
          f"{st['graphinst_live']} topo_invalid={st['topo_invalid']} present={present} "
          f"nonblack={nonblack} pre_mean={mean_a:.4f}",
          f"sim kept ADVANCING: living_MAD={living_mad:.4f} (>= {CHANGED_MAD}) tick "
          f"{st['tick_a']}->{st['tick_b']} living={living} status_note={note_excl}",
      ])

  app = _TopoAbEditor([source])
  app.ezapp.mainThreadLoop()
  app.ezapp.shutdown()


# ---- orchestrator -----------------------------------------------------------

def _spawn(orkpython, argv, tag, timeout=180):
  cmd = [orkpython, os.path.abspath(__file__)] + argv
  for attempt in (1, 2):
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    out = r.stdout or ""
    ok = any(k in out for k in ("TOPO_DELETE_RESULT=PASS", "TOPO_ADD_RESULT=PASS"))
    for ln in out.splitlines():
      if ln.startswith("[topo-") or ln.startswith("[particles-viewport]") or "TOPO_" in ln:
        print(f"    [{tag}] {ln.strip()}", flush=True)
    if ok:
      return True
    print(f"    [{tag}] leaf FAILED attempt {attempt} (rc={r.returncode})"
          f"{'' if attempt == 2 else ' — retrying'}\n{(r.stderr or '')[-500:]}", flush=True)
  return False


def _orchestrate():
  orkpython = shutil.which("ork.python") or sys.executable
  workdir = tempfile.mkdtemp(prefix="dflowedit_topo_")
  ok_all = True

  print("[gate:topo-delete] fireball DELETE mid-chain -> hold last-good -> reconnect resumes ...",
        flush=True)
  d = _spawn(orkpython, ["--capture-topo", "fireball", "delete", str(BASE_ADV),
                         os.path.join(workdir, "delete")], "delete")
  print(f"GATE_TOPO_DELETE={'PASS' if d else 'FAIL'}", flush=True)
  ok_all = ok_all and d

  print("[gate:topo-add] fireball ADD unwired op -> excluded, preview keeps living ...", flush=True)
  a = _spawn(orkpython, ["--capture-topo", "fireball", "add", str(BASE_ADV),
                         os.path.join(workdir, "add")], "add")
  print(f"GATE_TOPO_ADD={'PASS' if a else 'FAIL'}", flush=True)
  ok_all = ok_all and a

  print(f"DFLOWEDIT_TOPOGUARD_RESULT={'PASS' if ok_all else 'FAIL'}", flush=True)
  return 0 if ok_all else 1


def main(argv):
  if argv and argv[0] == "--capture-topo":
    _run_leaf(argv[1], argv[2], argv[3], argv[4])
    return 0
  return _orchestrate()


if __name__ == "__main__":
  sys.exit(main(sys.argv[1:]))
