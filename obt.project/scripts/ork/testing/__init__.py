################################################################################
# ork.testing — orkid's reusable offscreen / capture test harness.
#
# The hard-won headless test-lifecycle folklore, as CODE every new test reuses. Each
# guard encodes away a specific filed failure class:
#
#   headless_app  (app.py)      correct init ORDER + offscreen ezapp + async SETTLE +
#                               a teardown that stops the update side before shutdown
#                               (#60 bare-exit deadlock, #63 settle-then-exit).
#   capture_app   (capture.py)  proven offscreen capture: output-dir creation (#71),
#                               asset preflight fail-loud (#54-class), capture->settle
#                               ->readback ordering before teardown, ssaa opt-in (#70),
#                               DRM/env guards before engine init (DRM scanout trap).
#   verdict       (verdict.py)  VERDICT-BEFORE-TEARDOWN protocol (#57): the pass/fail
#                               line is emitted+flushed BEFORE teardown; read_verdict
#                               reconciles it with the rc (PASS_WITH_TEARDOWN_BUG).
#   watchdog      (watchdog.py) deadline watchdog that SAMPLES the wedged process
#                               (faulthandler + macOS `sample`) THEN hard-exits with a
#                               distinct rc — sample-before-kill, automated.
#
# verdict + watchdog are pure stdlib and imported eagerly (a gate-runner classifies
# logs / arms a deadline WITHOUT booting the engine). app + capture pull the engine in,
# so they are imported LAZILY on first attribute access — importing ork.testing does not
# force a lev2 init, and when it does happen app.py honors the core-before-lev2 law.
################################################################################

from ork.testing.verdict import (
    verdict, read_verdict,
    PASS, FAIL, PASS_WITH_TEARDOWN_BUG, NO_VERDICT, CRASH_NO_VERDICT,
    VERDICT_PREFIX,
)
from ork.testing.watchdog import Watchdog, armed, WATCHDOG_RC

__all__ = [
    "headless_app", "capture_app",
    "apply_env_guards", "preflight_assets", "ensure_parent_dir", "settle",
    "verdict", "read_verdict",
    "PASS", "FAIL", "PASS_WITH_TEARDOWN_BUG", "NO_VERDICT", "CRASH_NO_VERDICT",
    "VERDICT_PREFIX",
    "Watchdog", "armed", "WATCHDOG_RC",
]


def __getattr__(name):
  # lazy engine-backed exports (PEP 562): keep `import ork.testing` engine-free.
  if name in ("headless_app", "apply_env_guards", "preflight_assets",
              "ensure_parent_dir", "settle"):
    from ork.testing import app as _app
    return getattr(_app, name)
  if name == "capture_app":
    from ork.testing.capture import capture_app as _capture_app
    return _capture_app
  raise AttributeError("module %r has no attribute %r" % (__name__, name))
