################################################################################
# ork.testing.app — the proven headless offscreen test lifecycle, as CODE.
#
# This encodes the hard-won folklore every headless lev2 test re-derives by hand:
# the correct init ORDER, the offscreen ezapp construction, an explicit async
# SETTLE step, and a teardown that stops the update side before shutting the engine
# down. Each guard names the bug class it encodes away.
#
# import-order law: orkengine.core is imported FIRST here (before any stdlib that
# might pull hashlib/ssl), then lev2, then ecs — the same discipline every engine
# script follows.
################################################################################

from orkengine import core   # core FIRST (import-order law)
from orkengine import lev2   # noqa: F401  (registers lev2 reflected classes)
from orkengine import ecs

import contextlib
import os
import time

# env var that keeps DRM scanout alive on DRM nodes even under --offscreen. Named
# here (never a hostname) so the guard is greppable. Removing it BEFORE engine init
# is the documented headless requirement on those nodes.
DRM_ENV_VAR = "ORKID_DRM_MODE"


def apply_env_guards(offscreen=True):
  """Apply the headless env guards that MUST precede any lev2 init.

  DRM trap: --offscreen does NOT stop DRM scanout on a DRM node — the compositor
  still scans out unless ORKID_DRM_MODE is removed from the environment BEFORE the
  engine initializes graphics. We pop it for an offscreen run and return what we
  removed so a caller can log/verify. A no-op on non-DRM hosts (the var is absent).
  """
  removed = {}
  if offscreen and DRM_ENV_VAR in os.environ:
    removed[DRM_ENV_VAR] = os.environ.pop(DRM_ENV_VAR)
  return removed


def ensure_parent_dir(path):
  """Create the parent directory of `path` if missing. captureToFile SIGSEGVs when
  its output dir does not exist (#71) — every path the harness is about to write is
  routed through here first."""
  parent = os.path.dirname(os.path.abspath(str(path)))
  if parent and not os.path.isdir(parent):
    os.makedirs(parent, exist_ok=True)
  return path


def preflight_assets(paths):
  """FAIL LOUD, before engine init, on any declared asset path that does not exist
  (#54-class: a missing asset crashes the engine deep in a load, pre-output, with no
  usable diagnostic). Raises FileNotFoundError naming the first missing path. `paths`
  is an explicit list the test declares; empty/None short-circuits."""
  for p in (paths or []):
    if not os.path.exists(str(p)):
      raise FileNotFoundError(
          "ork.testing preflight: declared asset not found: %s "
          "(fail-loud before engine init — see #54-class)" % (p,))
  return True


def settle(timeout=5.0, strict=False, stable_polls=6, poll_s=0.02):
  """Drain in-flight async work before teardown (#63: an early exit while hypermesh
  materialization / a bake / a streaming load is in flight SIGSEGVs — settle-then-exit
  is clean).

  What we drain with the seams that ARE bound to Python:
    * the concurrent (loader) queue: a bounded drain(timeout) + a poll for
      num_pending_operations == 0 held stable across `stable_polls`.
    * asyncWorkPending(): the async-tracker registry the offscreen player settles on.
      NOTE it is not yet bound to Python (C++ only) — detected at runtime so this
      slots in automatically the moment a binding lands. Until then loader-idle is
      the proxy. (Reported as a seam gap.)

  strict=True raises if work is still pending when `timeout` elapses; the default
  is best-effort (return whether it drained)."""
  cq = core.opq_concurrentQueue()
  try:
    cq.drain(float(timeout))
  except Exception:
    pass   # drain is a bounded helper; a backend that lacks it falls to the poll

  async_pending = getattr(core, "asyncWorkPending", None)   # forward-compat probe
  deadline = time.time() + float(timeout)
  stable = 0
  while time.time() < deadline:
    pend = cq.num_pending_operations
    ap = async_pending() if callable(async_pending) else 0
    if pend == 0 and ap == 0:
      stable += 1
      if stable >= stable_polls:
        return True
    else:
      stable = 0
    time.sleep(poll_s)

  drained = (cq.num_pending_operations == 0)
  if strict and not drained:
    raise RuntimeError("ork.testing settle: async work did not drain within %.1fs "
                       "(pending=%d)" % (timeout, cq.num_pending_operations))
  return drained


class _HeadlessHandle(object):
  """The object yielded by headless_app: the bound GPU ctx plus frame-bounded run
  helpers. Kept intentionally small — a compute test drives the ctx directly (the
  test_rtg idiom); a stepped test drives frames through run_frames/run_until."""

  __slots__ = ("ezapp", "ctx", "_frame")

  def __init__(self, ezapp, ctx):
    self.ezapp = ezapp
    self.ctx = ctx
    self._frame = 0

  @property
  def frame(self):
    return self._frame

  def run_frames(self, n):
    """Advance exactly `n` render iterations on the main thread."""
    for _ in range(int(n)):
      self.ezapp.mainThreadIter()
      self._frame += 1
    return self._frame

  def run_until(self, pred, max_frames=600):
    """Advance frames until pred() is truthy or `max_frames` is hit. Returns the
    frame count reached (pred is polled on the main thread between iterations)."""
    for _ in range(int(max_frames)):
      if pred():
        break
      self.ezapp.mainThreadIter()
      self._frame += 1
    return self._frame

  def settle(self, timeout=5.0, strict=False):
    return settle(timeout=timeout, strict=strict)


@contextlib.contextmanager
def headless_app(subsystems=("opq", "core", "gpu", "lev2"),
                 width=256, height=192,
                 offscreen=True, lockstep=False,
                 assets=None, drm_guard=True,
                 settle_timeout=5.0, strict_settle=False):
  """Headless offscreen lev2 app with the proven lifecycle.

  Enter:  env guards (#71 dir / DRM trap) -> asset preflight (#54-class) -> headless
          init (correct order) -> mainThreadBegin -> bindGfxToCurrentThread. Yields a
          _HeadlessHandle whose .ctx is a live GPU context for inline work and whose
          run_frames/run_until step the offscreen render loop.

  Exit:   stop the update side FIRST (signalExit) -> SETTLE async work (#63) ->
          mainThreadEnd -> headless_exit. This is the sequence that avoids the bare-
          exit update-join deadlock (#60): the update loop is signalled to stop and
          the loader is drained before the engine is torn down.

  lockstep=True selects the deterministic virtual-time mode (freerun off) for replay
  gates; the default is realtime freerun (the test_rtg idiom)."""
  if drm_guard:
    apply_env_guards(offscreen=offscreen)
  preflight_assets(assets)

  ezapp = ecs.headless_appinit(
      use_subsystems=list(subsystems),
      offscreen=bool(offscreen),
      width=int(width), height=int(height),
      freerun=(not lockstep))
  ezapp.mainThreadBegin()
  ctx = ezapp.bindGfxToCurrentThread()
  if not ctx:
    raise RuntimeError("ork.testing headless_app: bindGfxToCurrentThread() returned null")

  handle = _HeadlessHandle(ezapp, ctx)
  try:
    yield handle
  finally:
    # #60: stop the update side BEFORE shutdown; #63: settle before teardown.
    try:
      ezapp.signalExit()
    except Exception:
      pass
    try:
      settle(timeout=settle_timeout, strict=strict_settle)
    except Exception:
      if strict_settle:
        raise
    ezapp.mainThreadEnd()
    ecs.headless_exit()
