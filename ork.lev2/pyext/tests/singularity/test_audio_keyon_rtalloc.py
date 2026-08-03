#!/usr/bin/env ork.python
################################################################################
# test_audio_keyon_rtalloc — allocator traffic on the audio thread (RT-safety).
#
# WHAT IT PROVES (needs NO gpu, NO audio hardware, NO display):
#   The synth's per-block audio work allocates NOTHING, and a note-on costs a
#   small, FIXED, budgeted number of allocations rather than one proportional to
#   the patch's dsp grid.
#
#   Measurement is a malloc/calloc/realloc interposer (LD_PRELOAD) whose counter
#   is armed only around the engine call under test, read back through ctypes.
#   The SYNC_NONREALTIME device makes this exact: dev.advanceTime() runs the
#   whole audio-thread control+compute pass inline, so the window contains the
#   RT work and nothing else. Two windows are compared:
#     IDLE  — one pump with no pending note
#     KEYON — one pump that drains a keyOn event (instantiates the layer's
#             controllers + the entire dsp grid, then computes 15 control passes)
#
#   Asserts:
#     (a) IDLE pumps allocate EXACTLY 0 (the compute path is allocation-free;
#         before the dsp traversals became templates it was ~345/pump/voice);
#     (b) the steady-state (median) KEYON pump allocates <= KEYON_BUDGET;
#     (c) no measured pump exceeds KEYON_CEILING — pools grow to their
#         concurrency peak during warmup and must be quiet after it;
#     (d) the child ran the whole schedule and exited 0;
#     (e) the dsp/controller instance recycler took NOTHING from the allocator
#         over the measured window (synth.dspPoolMisses delta == 0) — the same
#         fact from the engine's own counter rather than the interposer.
#
#   KEYON_BUDGET is ZERO as of the controller-pooling slice: a note-on in steady
#   state performs NO allocator call at all. The residual this used to fence,
#   unit by unit (1-layer/3-block/1-controller patch):
#     6 = Layer::getSRC1/getSRC2 built a std::function per modulated param whose
#         capture (a shared_ptr + a nested std::function) could not be inline
#         -> controller_t is a fixed-capacity callable over a raw ControllerInst
#     1 = Layer::keyOn make_shared<ControlBlockInst>
#         -> one persistent ControlBlockInst per Layer, cleared not replaced
#     1 = ControllerData::instantiate (raw new per controller per note)
#         -> ControllerInst::operator new routes to the dsp instance recycler
#     1 = Layer::_controlMap node insert
#         -> a closed ControllerSlot array on the Layer
#   WARMUP covers a ONE-TIME late event (~5 allocations, 4 of them recycler
#   misses, seen at overall note-on ~17 and never again) — hence 32 cycles of
#   warmup before the window opens, and a CEILING that still tolerates it if it
#   ever drifts later. The budget is a fence, not a target: keep it AT the
#   achieved steady state.
#
# SHAPE (mirrors test_audio_wav_render.py): the driver (no --role) is pure stdlib
# and spawns the engine boot as its OWN ork.python subprocess with ORKID_DRM_MODE
# stripped, so a measurement can never depend on / touch inherited display state.
#
# PORTABILITY: the interposer is LD_PRELOAD mechanics (linux). On any other
# platform this SKIPs loudly rather than failing or, worse, passing vacuously —
# the mac seat attests the same behavior with its own tooling.
################################################################################

import os
os.environ["PYTHONUNBUFFERED"] = "1"
import sys
import argparse

# prepend THIS checkout's scripts dir so ork.testing resolves from the same tree.
_ROOT = os.path.abspath(__file__)
for _ in range(5):
  _ROOT = os.path.dirname(_ROOT)
sys.path.insert(0, os.path.join(_ROOT, "obt.project", "scripts"))

SELF = os.path.abspath(__file__)

DT            = 1.0 / 100.0   # 480 frames @48k == 15 whole control passes
WARMUP        = 32            # note cycles run before the allocator is watched
NMEAS         = 16            # measured note cycles
DRAIN         = 12            # pumps after each keyOff (release is 0.05s)
KEYON_BUDGET  = 0             # steady-state allocations per note-on (see header)
KEYON_CEILING = 8             # any pump, including pool growth to a new peak

################################################################################
# the interposer. counts allocator CALLS while armed; free() is counted too so a
# regression that trades an allocation for a deallocation cannot hide.
################################################################################

PROBE_C = r'''
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>

static void* (*real_malloc)(size_t)         = 0;
static void  (*real_free)(void*)            = 0;
static void* (*real_calloc)(size_t, size_t) = 0;
static void* (*real_realloc)(void*, size_t) = 0;

static __thread int tls_inprobe = 0;
static int g_armed = 0;
static unsigned long g_allocs = 0;
static unsigned long g_frees  = 0;

/* dlsym() itself calls calloc before real_calloc is resolved */
static char g_boot[65536];
static size_t g_bootoff = 0;
static int in_boot(void* p) {
  return ((char*)p >= g_boot) && ((char*)p < (g_boot + sizeof(g_boot)));
}
static void* boot_alloc(size_t n) {
  n = (n + 15) & ~(size_t)15;
  if (g_bootoff + n > sizeof(g_boot)) return 0;
  void* r = g_boot + g_bootoff;
  g_bootoff += n;
  return r;
}
static void resolve(void) {
  if (!real_malloc) {
    real_malloc  = dlsym(RTLD_NEXT, "malloc");
    real_free    = dlsym(RTLD_NEXT, "free");
    real_calloc  = dlsym(RTLD_NEXT, "calloc");
    real_realloc = dlsym(RTLD_NEXT, "realloc");
  }
}

void* malloc(size_t n) {
  resolve();
  if (!real_malloc) return boot_alloc(n);
  if (g_armed && !tls_inprobe) g_allocs++;
  return real_malloc(n);
}
void* calloc(size_t a, size_t b) {
  if (!real_calloc) {
    resolve();
    if (!real_calloc) {
      void* p = boot_alloc(a * b);
      if (p) memset(p, 0, a * b);
      return p;
    }
  }
  if (g_armed && !tls_inprobe) g_allocs++;
  return real_calloc(a, b);
}
void* realloc(void* p, size_t n) {
  resolve();
  if (g_armed && !tls_inprobe) g_allocs++;
  return real_realloc(p, n);
}
void free(void* p) {
  if (in_boot(p)) return;
  resolve();
  if (g_armed && !tls_inprobe) g_frees++;
  if (real_free) real_free(p);
}

void orkprobe_arm(void)   { g_allocs = 0; g_frees = 0; g_armed = 1; }
void orkprobe_disarm(void){ g_armed = 0; }
unsigned long orkprobe_allocs(void) { return g_allocs; }
unsigned long orkprobe_frees(void)  { return g_frees; }
'''


################################################################################
# CHILD — one engine boot, audio-only headless, measured note cycles.
################################################################################

def _build_probe_program(S):
  # the wav-render patch: deterministic, asset-free, 2 stages / 3 blocks / 1
  # controller. changing it changes KEYON_BUDGET's accounting.
  bank = S.BankData()
  prog = bank.newProgram("RTALLOC")
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
  ampenv.addSegment("sus", 1.0, 1.0, 0.5)
  ampenv.addSegment("rel", 0.05, 0.0, 0.5)
  dspstg.appendDspBlock("OscilSine", "sine")
  ampblk = ampstg.appendDspBlock("AmpAdaptive", "amp")
  ampblk.paramByName("gain").mods.src1 = ampenv
  ampblk.paramByName("gain").mods.src1scale = 1.0
  return bank, prog


def _role_child():
  import ctypes
  libc = ctypes.CDLL(None)   # the preloaded interposer is in the global namespace
  try:
    arm = libc.orkprobe_arm
    disarm = libc.orkprobe_disarm
    n_allocs = libc.orkprobe_allocs
  except AttributeError:
    print("CHILD_ERROR=probe_not_preloaded", flush=True)
    sys.exit(4)
  n_allocs.restype = ctypes.c_ulong

  import orkengine.core                 # core FIRST (import-order law)
  from orkengine.lev2 import OrkEzApp
  from orkengine.lev2 import singularity as S

  class App(object):
    def __init__(self):
      self.phase = 0
      self.i = 0
      self.idle = []
      self.keyon = []
      self.miss0 = -1
      self.miss1 = -1
      self.ezapp = OrkEzApp.create(
          self,
          name="AudioKeyOnRtAllocTest",
          use_subsystems=['opq', 'core', 'audioO'],   # NO gpu -> no display
          enable_audio_synth=True,
          audio_stream_sync=True,                      # -> StrAudioDevice SYNC
          freerun=True,
      )

    def _cycle(self, watch):
      # one full note cycle: keyOn -> one pump -> keyOff -> drain.
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      note = 60 + (self.i % 12)
      voice = syn.keyOn(note, 127, self.prog, None)
      if watch:
        arm()
      dev.advanceTime(DT)
      if watch:
        disarm()
        self.keyon.append(int(n_allocs()))
      syn.keyOff(voice, note, 127)
      for _ in range(DRAIN):
        dev.advanceTime(DT)

    def onRunLoopIteration(self):
      syn = self.ezapp.audio_synth
      dev = self.ezapp.audio_device
      if syn is None or dev is None:
        return
      ############################################
      if self.phase == 0:   # build the patch; no audio time elapses here
        syn.masterGain = 1.0
        self.bank, self.prog = _build_probe_program(S)
        syn.programbus.uiprogram = self.prog
        dev.advanceTime(DT)
        self.phase = 1
        return
      ############################################
      if self.phase == 1:   # warmup: every pool grows to its peak here
        self._cycle(False)
        self.i += 1
        if self.i >= WARMUP:
          self.i = 0
          self.phase = 2
        return
      ############################################
      if self.phase == 2:   # idle pumps: no note anywhere in the synth
        arm()
        dev.advanceTime(DT)
        disarm()
        self.idle.append(int(n_allocs()))
        self.i += 1
        if self.i >= NMEAS:
          self.i = 0
          self.miss0 = syn.dspPoolMisses
          self.phase = 3
        return
      ############################################
      self._cycle(True)     # phase 3: measured note cycles
      self.i += 1
      if self.i >= NMEAS:
        self.miss1 = syn.dspPoolMisses
        self.ezapp.signalExit()

  app = App()
  app.ezapp.mainThreadLoop(on_iter=app.onRunLoopIteration)
  ok = (len(app.idle) == NMEAS and len(app.keyon) == NMEAS)
  # one token per line — the driver's _grep matches line-start keys.
  print("CHILD_IDLE=%s" % ",".join(str(v) for v in app.idle), flush=True)
  print("CHILD_KEYON=%s" % ",".join(str(v) for v in app.keyon), flush=True)
  print("CHILD_POOLMISSES=%d,%d" % (app.miss0, app.miss1), flush=True)
  sys.exit(0 if ok else 3)


################################################################################
# DRIVER
################################################################################

def _build_probe(tmpdir):
  import subprocess
  csrc = os.path.join(tmpdir, "orkallocprobe.c")
  sopath = os.path.join(tmpdir, "orkallocprobe.so")
  with open(csrc, "w") as f:
    f.write(PROBE_C)
  cc = os.environ.get("CC", "cc")
  p = subprocess.run([cc, "-shared", "-fPIC", "-O1", "-o", sopath, csrc, "-ldl"],
                     capture_output=True, text=True, timeout=180)
  if p.returncode != 0 or not os.path.isfile(sopath):
    return None, (p.stdout or "") + (p.stderr or "")
  return sopath, ""


def _spawn(sopath, timeout=600):
  import subprocess
  env = dict(os.environ)
  env.pop("ORKID_DRM_MODE", None)               # never touch the physical display
  env["ORKID_LOG_ALWAYSFLUSH"] = "1"
  env["LD_PRELOAD"] = sopath
  cmd = ["ork.python", SELF, "--role", "child"]
  p = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout, env=env)
  return p.returncode, (p.stdout or "") + (p.stderr or "")


def _grep_list(out, key):
  for line in out.splitlines():
    if line.startswith(key + "="):
      body = line.split("=", 1)[1].strip()
      if not body:
        return []
      try:
        return [int(v) for v in body.split(",")]
      except ValueError:
        return []
  return []


def _median(v):
  s = sorted(v)
  return s[len(s) // 2]


def _main_driver():
  import subprocess
  import tempfile
  import shutil
  from ork.testing import verdict

  # PLATFORM GUARD: LD_PRELOAD symbol interposition of malloc is linux mechanics.
  #  Say so out loud — a silent pass here would claim RT-safety nobody measured.
  if not sys.platform.startswith("linux"):
    print("TESTVERDICT=SKIP detail=interposer-linux-only platform=%s" % sys.platform, flush=True)
    sys.exit(0)

  tmp = tempfile.mkdtemp(prefix="keyonalloc_")
  try:
    sopath, cerr = _build_probe(tmp)
    if sopath is None:
      print(cerr)
      sys.exit(verdict(False, "could not build the malloc interposer (no working cc?)"))
    try:
      rc, out = _spawn(sopath)
    except subprocess.TimeoutExpired as e:
      print((e.stdout or "") + (e.stderr or ""))
      sys.exit(verdict(False, "child run timed out (possible wedge)"))
  finally:
    shutil.rmtree(tmp, ignore_errors=True)

  print(out)

  idle = _grep_list(out, "CHILD_IDLE")
  keyon = _grep_list(out, "CHILD_KEYON")
  misses = _grep_list(out, "CHILD_POOLMISSES")

  ran_ok = (rc == 0 and len(idle) == NMEAS and len(keyon) == NMEAS)
  idle_ok = ran_ok and (max(idle) == 0)
  keyon_med = _median(keyon) if keyon else -1
  keyon_ok = ran_ok and (0 <= keyon_med <= KEYON_BUDGET)
  ceiling_ok = ran_ok and (max(keyon) <= KEYON_CEILING)
  miss_delta = (misses[1] - misses[0]) if len(misses) == 2 else -1
  miss_ok = (miss_delta == 0)

  passed = (ran_ok and idle_ok and keyon_ok and ceiling_ok and miss_ok)
  detail = ("rc=%d idlemax=%s keyon_median=%s(<=%d) keyon_max=%s(<=%d) "
            "dspPoolMisses=%s delta=%s | "
            "ran=%s idle_zero=%s budget=%s ceiling=%s poolmiss=%s | keyon=%s"
            % (rc, (max(idle) if idle else -1), keyon_med, KEYON_BUDGET,
               (max(keyon) if keyon else -1), KEYON_CEILING,
               misses, miss_delta,
               ran_ok, idle_ok, keyon_ok, ceiling_ok, miss_ok, keyon))
  sys.exit(verdict(passed, detail))


################################################################################

if __name__ == "__main__":
  ap = argparse.ArgumentParser()
  ap.add_argument("--role", default=None)
  args, _ = ap.parse_known_args()

  if args.role == "child":
    _role_child()
  else:
    _main_driver()
