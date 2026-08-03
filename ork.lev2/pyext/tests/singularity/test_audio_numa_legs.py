#!/usr/bin/env ork.python
################################################################################
# test_audio_numa_legs — the NUMA-balancing immunity, on a real device.
#
# WHAT IT PROVES (needs a HOST AUDIO DEVICE; no gpu, no display):
#   The three legs of audio_numa.h actually fire on the deadline-bearing path,
#   and while they are up the kernel's automatic NUMA balancer does not migrate
#   pages out from under the running audio thread.
#
#   The failure this fences is not theoretical: with kernel.numa_balancing on,
#   the scanner marks anonymous pages PROT_NONE from the process's OWN threads,
#   and the next touch migrates the folio SYNCHRONOUSLY in the faulting context.
#   When that context is the audio callback inside a dsp sample loop, the
#   callback stalls for milliseconds and the device underruns. The fix is
#   unprivileged and therefore silent when it half-works — hence a gate that
#   reads the legs' own report lines rather than trusting the code is linked in.
#
#   The STREAM and NULL devices are EXCLUDED from the fix by design (they are
#   pumped by their consumer and carry no deadline), so this gate cannot be run
#   headless-without-hardware: it SKIPs loudly where no host device opens.
#
#   Asserts, over a bounded MUTED run (master gain 0, sustained voices held so
#   the dsp path keeps touching the pools):
#     (a) leg 2 reported an outcome — "AUDIO-MLOCK: locked resident" OR the
#         "AUDIO-MLOCK: DENIED" line. BOTH are correct: RLIMIT_MEMLOCK is a
#         per-login grant this process may simply not have, and the degrade is
#         designed to be loud and non-fatal. The gate records WHICH;
#     (b) leg 1 reported "AUDIO-NUMA: bound<singularity-pools>" with every span
#         accepted and a nonzero byte count (a bind of nothing is not a bind),
#         and no "bind ... INCOMPLETE" line followed it;
#     (c) leg 3 reported "AUDIO-NUMA: thread<...> pinned to node<N>";
#     (d) the run really drove a host callback (audioDiagCounters callbacks > 0)
#         — otherwise (a)-(c) could be satisfied by a device that never ran;
#     (e) numa_pages_migrated moved by ~nothing across the run's STEADY STATE:
#         the balancer neither migrated system-wide (/proc/vmstat) nor charged
#         this process's own tasks (/proc/self/task/*/sched) more than a noise
#         budget. See MIGRATION BUDGET below;
#     (f) THE KILL SWITCH: a second, shorter run with ORKID_AUDIO_NO_NUMAFIX=1
#         prints the "AUDIO-NUMA: DISABLED" line and prints NEITHER the bound
#         line NOR the pinned line. Without this arm, (a)-(c) would still pass
#         against a build that ignored the A/B switch, and every A/B measurement
#         the audio lane took would be meaningless.
#
# MIGRATION BUDGET
#   /proc/vmstat numa_pages_migrated is SYSTEM-WIDE, so an absolute constant
#   would encode this box's idle noise. The driver therefore CALIBRATES: it
#   samples the counter over an idle interval immediately before the run, and
#   the budget is 4x that measured rate scaled to the window, floored at
#   MIG_FLOOR pages so a perfectly quiet box does not produce a zero-tolerance
#   assert. The fence is coarse ON PURPOSE — it does not need to resolve tens of
#   pages, because the regression it catches walks the bound pools, and those
#   are ~2GiB (over 500,000 pages) on this path. Note mbind(MPOL_MF_MOVE)'s own
#   relocation during leg 1 does NOT land in this counter (it is charged to
#   pgmigrate_success); numa_pages_migrated counts balancer migrations only.
#   The per-process view is the same budget applied to the sum over this
#   process's tasks — tighter in principle (no cross-talk), but the process
#   also owns plenty of NON-audio memory that stays in the scan set, so it gets
#   a budget rather than a zero.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import re
import sys
import glob
import time
import argparse

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

BASELINE_S = 6.0    # idle calibration interval (driver, before any boot)
SETTLE_S   = 3.0    # after the first callback: pool bind + first-touch settle
WINDOW_S   = 8.0    # the measured steady-state window
OFF_S      = 4.0    # kill-switch arm: only long enough to reach a callback
BOOT_S     = 25.0   # cap on the wait for the first host callback
MIG_FLOOR  = 256    # pages (1MiB) — floor under the calibrated budget
MIG_SLACK  = 4.0    # multiple of the measured idle rate

HELD_NOTES = 8      # sustained voices held across the whole window

_BOUND_RE = re.compile(
    r"AUDIO-NUMA: bound<singularity-pools> node<(-?\d+)> spans<(\d+)/(\d+)> bytes<([0-9.]+)MiB>")
_PIN_RE = re.compile(r"AUDIO-NUMA: thread<[^>]*> pinned to node<(-?\d+)>")

MLOCK_OK     = "AUDIO-MLOCK: locked resident"
MLOCK_DENIED = "AUDIO-MLOCK: DENIED"
NUMA_DISABLED = "AUDIO-NUMA: DISABLED"
BIND_INCOMPLETE = "INCOMPLETE"


################################################################################
# counters
################################################################################

def _vmstat_migrated():
  with open("/proc/vmstat", "r") as f:
    for line in f:
      if line.startswith("numa_pages_migrated "):
        return int(line.split()[1])
  return -1


def _self_task_migrated():
  # per-TASK counter (task->numa_pages_migrated); summed over the process so a
  # migration charged to any thread of this engine is visible.
  total = 0
  for p in glob.glob("/proc/self/task/*/sched"):
    try:
      with open(p, "r") as f:
        for line in f:
          if line.startswith("numa_pages_migrated"):
            total += int(line.split(":")[1].strip())
            break
    except (IOError, OSError, ValueError):
      pass   # a thread can exit between the glob and the open
  return total


################################################################################
# CHILD — one engine boot on the REAL device, muted, bounded.
################################################################################

def _build_probe_program(S):
  bank = S.BankData()
  prog = bank.newProgram("NUMALEGS")
  lyr = prog.newLayer()
  dspstg = lyr.appendStage("DSP")
  ampstg = lyr.appendStage("AMP")
  dspstg.ioconfig.inputs = [0, 1]
  dspstg.ioconfig.outputs = [0, 1]
  ampstg.ioconfig.inputs = [0]
  ampstg.ioconfig.outputs = [0, 1]
  pch = dspstg.appendDspBlock("Pitch", "pitch")
  lyr.pitchBlock = pch
  lyr.panmode = 0
  lyr.pan = 7
  ampenv = lyr.appendController("RateLevelEnv", "AMPENV")
  ampenv.ampenv = True
  ampenv.bipolar = False
  ampenv.sustainSegment = 1
  ampenv.addSegment("atk", 0.01, 1.0, 0.5)
  ampenv.addSegment("sus", 600.0, 1.0, 0.5)
  ampenv.addSegment("rel", 0.05, 0.0, 0.5)
  dspstg.appendDspBlock("OscilSine", "sine")
  ampblk = ampstg.appendDspBlock("AmpAdaptive", "amp")
  ampblk.paramByName("gain").mods.src1 = ampenv
  ampblk.paramByName("gain").mods.src1scale = 1.0
  return bank, prog


def _role_child(run_seconds, measure):
  import orkengine.core                 # core FIRST (import-order law)
  from orkengine import lev2
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  class App(object):
    def __init__(self):
      self.state = 0
      self.t_mark = None
      self.vm0 = -1
      self.vm1 = -1
      self.tm0 = -1
      self.tm1 = -1
      self.window = -1.0
      self.callbacks = 0
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioNumaLegsTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          freerun=True,                                # -> real host device
      )

    def iterate(self):
      syn = self.ezapp.audio_synth
      if syn is None:
        return
      now = time.monotonic()
      ############################################
      if self.state == 0:   # MUTE FIRST, then hold voices; nothing is audible.
        syn.masterGain = 0.0
        self.bank, self.prog = _build_probe_program(S)
        syn.programbus.uiprogram = self.prog
        for i in range(HELD_NOTES):
          syn.keyOn(48 + 3 * i, 100, self.prog, None)
        self.t_mark = now
        self.state = 1
        return
      ############################################
      time.sleep(0.004)
      cb = int(lev2.audioDiagCounters().get("callbacks", 0))
      self.callbacks = cb
      ############################################
      if self.state == 1:   # wait for the host callback to actually be running
        if cb > 0:
          self.t_mark = now
          self.state = 2
        elif (now - self.t_mark) > BOOT_S:
          self.state = 9   # no device ever ran -> the driver SKIPs
          self.ezapp.signalExit()
        return
      ############################################
      if self.state == 2:   # settle: pool bind + first-touch of the dsp grids
        if (now - self.t_mark) >= SETTLE_S:
          self.vm0 = _vmstat_migrated()
          self.tm0 = _self_task_migrated()
          self.t_mark = now
          self.state = 3
        return
      ############################################
      if self.state == 3:   # the measured steady-state window
        if (now - self.t_mark) >= run_seconds:
          self.vm1 = _vmstat_migrated()
          self.tm1 = _self_task_migrated()
          self.window = now - self.t_mark
          syn.masterGain = 0.0
          self.ezapp.signalExit()
        return

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.iterate)

  print("CHILD_CALLBACKS=%d" % app.callbacks, flush=True)
  if measure:
    print("CHILD_VMSTAT=%d,%d" % (app.vm0, app.vm1), flush=True)
    print("CHILD_TASKMIG=%d,%d" % (app.tm0, app.tm1), flush=True)
    print("CHILD_WINDOW=%.3f" % app.window, flush=True)
  sys.exit(0)


################################################################################
# DRIVER
################################################################################

def _spawn(role_env, seconds, measure, timeout=180):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env.pop("ORKID_AUDIO_IOCLASS", None)          # the REAL device is the point
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  env.update(role_env)
  cmd = ["ork.python", SELF, "--role", "child", "--seconds", str(seconds)]
  if measure:
    cmd.append("--measure")
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      return line.split("=", 1)[1].strip()
  return None


def _pair(out, key):
  raw = _grep(out, key)
  if not raw:
    return (-1, -1)
  parts = raw.split(",")
  if len(parts) != 2:
    return (-1, -1)
  try:
    return (int(parts[0]), int(parts[1]))
  except ValueError:
    return (-1, -1)


def _main_driver():
  import subprocess
  from ork.testing import verdict

  # PLATFORM GUARD: legs 1 and 3 are linux mempolicy/affinity mechanics. Say so
  #  out loud rather than passing vacuously where there is nothing to prove.
  if not sys.platform.startswith("linux"):
    print("TESTVERDICT=SKIP detail=numa-legs-linux-only platform=%s" % sys.platform, flush=True)
    sys.exit(0)

  # calibrate the system's idle migration rate FIRST — before any engine boot
  #  perturbs it. this is what makes the budget honest on a shared box.
  base0 = _vmstat_migrated()
  time.sleep(BASELINE_S)
  base1 = _vmstat_migrated()
  idle_rate = max(0.0, (base1 - base0) / BASELINE_S)
  budget = max(MIG_FLOOR, int(idle_rate * WINDOW_S * MIG_SLACK))

  try:
    rc_on, out_on = _spawn({}, WINDOW_S, True)
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "numafix run timed out (possible wedge)"))
  print(out_on)

  callbacks = int(_grep(out_on, "CHILD_CALLBACKS") or 0)
  if rc_on == 0 and callbacks == 0:
    # no host device opened here (headless CI seat, device busy) — the legs are
    #  DISABLED BY DESIGN on the consumer-pumped paths, so there is nothing to
    #  measure. Loud skip, never a green tick.
    print("TESTVERDICT=SKIP detail=no-host-audio-callback (real device required)", flush=True)
    sys.exit(0)

  bound = _BOUND_RE.search(out_on)
  pin = _PIN_RE.search(out_on)
  mlock_locked = (MLOCK_OK in out_on)
  mlock_denied = (MLOCK_DENIED in out_on)
  mlock_outcome = "locked" if mlock_locked else ("denied" if mlock_denied else "SILENT")

  span_bound = int(bound.group(2)) if bound else -1
  span_total = int(bound.group(3)) if bound else -1
  mib = float(bound.group(4)) if bound else -1.0
  home_node = int(bound.group(1)) if bound else -99
  pin_node = int(pin.group(1)) if pin else -99

  leg2_ok = (mlock_locked or mlock_denied)
  leg1_ok = (bound is not None and span_total > 0 and span_bound == span_total
             and mib > 0.0 and (BIND_INCOMPLETE not in out_on))
  leg3_ok = (pin is not None and pin_node >= 0 and pin_node == home_node)

  vm0, vm1 = _pair(out_on, "CHILD_VMSTAT")
  tm0, tm1 = _pair(out_on, "CHILD_TASKMIG")
  window = float(_grep(out_on, "CHILD_WINDOW") or -1.0)
  vm_delta = (vm1 - vm0) if (vm0 >= 0 and vm1 >= 0) else -1
  tm_delta = (tm1 - tm0) if (tm0 >= 0 and tm1 >= 0) else -1
  sampled_ok = (vm_delta >= 0 and tm_delta >= 0 and window >= WINDOW_S * 0.9)
  mig_ok = sampled_ok and (vm_delta <= budget) and (tm_delta <= budget)

  ############################################
  # the A/B switch: without this arm the assertions above prove nothing about
  #  whether the switch is honored, and every A/B measurement taken with it is
  #  suspect.
  ############################################
  try:
    rc_off, out_off = _spawn({"ORKID_AUDIO_NO_NUMAFIX": "1"}, OFF_S, False)
  except subprocess.TimeoutExpired as e:
    print((e.stdout or "") + (e.stderr or ""))
    sys.exit(verdict(False, "kill-switch run timed out (possible wedge)"))
  print(out_off)

  off_disabled = (NUMA_DISABLED in out_off)
  off_nobind = (_BOUND_RE.search(out_off) is None)
  off_nopin = (_PIN_RE.search(out_off) is None)
  killswitch_ok = (rc_off == 0 and off_disabled and off_nobind and off_nopin)

  passed = (rc_on == 0 and leg2_ok and leg1_ok and leg3_ok and callbacks > 0
            and mig_ok and killswitch_ok)
  detail = ("rc=%d,%d callbacks=%d mlock=%s bind=node%d spans=%d/%d bytes=%.1fMiB "
            "pin=node%d window=%.1fs idle_rate=%.1fpg/s budget=%dpg "
            "vmstat_delta=%d taskmig_delta=%d | "
            "leg1=%s leg2=%s leg3=%s migration=%s killswitch=%s"
            "(disabled=%s nobind=%s nopin=%s)"
            % (rc_on, rc_off, callbacks, mlock_outcome, home_node, span_bound,
               span_total, mib, pin_node, window, idle_rate, budget,
               vm_delta, tm_delta,
               leg1_ok, leg2_ok, leg3_ok, mig_ok, killswitch_ok,
               off_disabled, off_nobind, off_nopin))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  ap.add_argument("--seconds", type=float, default=WINDOW_S)
  ap.add_argument("--measure", action="store_true")
  args, _ = ap.parse_known_args()

  if args.role == "child":
    _role_child(args.seconds, args.measure)
  else:
    _main_driver()
