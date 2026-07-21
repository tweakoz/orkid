#!/usr/bin/env ork.python
###############################################################################
# image-load self-defend gate (#46). A 0-byte / truncated image datablock used
# to walk cursor math straight into getItem<>'s OrkAssert, whose -O2 force-
# segfault (banner often unflushed) produced a MISLEADING backtrace far from the
# real cause (a 0-byte icon PNG once masqueraded as a PBR env-map crash). The fix
# refuses LOUDLY with a named, catchable error BEFORE any cursor read.
#
# This test feeds Image.createFromFile a 0-byte, a 3-byte, a bare-signature (open-
# fail) PNG, and an unrecognized-magic buffer, and asserts each raises a catchable
# error (RuntimeError) carrying path/size — NOT a crash. A regression here segfaults
# the interpreter (no PASSED line, rc != 0), which is exactly the failure we guard.
###############################################################################
import os; os.environ["PYTHONUNBUFFERED"] = "1"
import sys, tempfile
from orkengine import core   # core MUST be imported before lev2
from orkengine import lev2


def _expect_raise(label, path, must_contain):
  try:
    lev2.Image.createFromFile(path)
  except Exception as e:  # pybind maps std::runtime_error -> RuntimeError (catchable)
    msg = str(e)
    ok = all(tok in msg for tok in must_contain)
    print(f"[{label}] raised: {msg}", flush=True)
    if not ok:
      print(f"[{label}] FAIL — message missing one of {must_contain}", flush=True)
    return ok
  print(f"[{label}] FAIL — no error raised (silent accept of bad image)", flush=True)
  return False


def main():
  tmp = tempfile.mkdtemp(prefix="imgguard_")

  cases = []

  p0 = os.path.join(tmp, "zero.png")
  open(p0, "wb").close()  # 0 bytes
  cases.append(("0-byte", p0, [p0, "size<0>"]))

  p3 = os.path.join(tmp, "three.png")
  with open(p3, "wb") as f:
    f.write(b"\x89PN")  # 3 bytes (truncated, < 4 magic bytes)
  cases.append(("3-byte", p3, [p3, "size<3>"]))

  # valid PNG signature only (8 bytes) — passes the magic sniff but OIIO open fails
  psig = os.path.join(tmp, "sigonly.png")
  with open(psig, "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n")
  cases.append(("png-sig-only", psig, ["initFromInMemoryFile"]))

  # 4 bytes of unrecognized magic
  pbad = os.path.join(tmp, "bad.bin")
  with open(pbad, "wb") as f:
    f.write(b"XZXZ")
  cases.append(("unknown-magic", pbad, ["unrecognized image format"]))

  results = [_expect_raise(*c) for c in cases]
  passed = all(results)
  n_ok = sum(1 for r in results if r)
  print(f"=== image load guard gate {'PASSED' if passed else 'FAILED'} "
        f"({n_ok}/{len(results)} cases) ===", flush=True)
  sys.exit(0 if passed else 1)


if __name__ == "__main__":
  main()
