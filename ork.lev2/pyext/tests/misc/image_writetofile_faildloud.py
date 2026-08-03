#!/usr/bin/env ork.python
###############################################################################
# image_writetofile_faildloud.py — Image::writeToFile fails LOUD, not silent.
#
# Regression gate for the OIIO silent no-op (image_io_oiio.cpp). Writing to an
# unrecognized / extension-less path used to `return` silently when
# ImageOutput::create() returned null: NO file was produced yet the void
# signature reported success, so callers reported a spurious result. writeToFile
# now returns bool — False (with a LOUD stderr error naming the path + OIIO
# reason) on a bad path, True on a clean write — and callers can act on it.
#
#   run:  ork.python image_writetofile_faildloud.py
###############################################################################
import sys, os, tempfile

import orkengine.core as core       # core before lev2 (import-order invariant)
from orkengine import lev2

img = lev2.Image.createRGBA8FromColor(8, 8, core.vec4(1, 0, 0, 1))

tmp       = tempfile.mkdtemp(prefix="ork_wtf_")
bad_path  = os.path.join(tmp, "no_extension")  # OIIO maps no writer -> loud False
good_path = os.path.join(tmp, "ok.png")        # recognized -> clean True

bad_ret  = img.writeToFile(bad_path)           # expect a LOUD stderr error here
good_ret = img.writeToFile(good_path)

checks = {
    "bad_path_returns_false":  (bad_ret is False),
    "bad_path_writes_nothing": (not os.path.exists(bad_path)),
    "good_path_returns_true":  (good_ret is True),
    "good_path_writes_file":   (os.path.exists(good_path) and os.path.getsize(good_path) > 0),
}

all_ok = all(checks.values())
for name, ok in checks.items():
    print("  [%s] %s" % ("PASS" if ok else "FAIL", name))
print("IMAGE_WRITETOFILE_LOUD_VERDICT=%s" % ("PASS" if all_ok else "FAIL"))
sys.exit(0 if all_ok else 1)
