#!/usr/bin/env ork.python
###############################################################################
# section_array_bake_instanced.py — O3 per-section texture-ARRAY gate, INSTANCED.
#
# The instanced coverage variant of section_array_bake.py (the flagged gap). Renders N
# instances of the SdfBaked section mesh (make_drawable instances=) with the SectionArray
# capture material, proving:
#   (a) the ptex3d codegen SURVIVES ssbo_instanced=True + wants_capture=True together (all
#       techniques — FWD_SSBO_CUSTOM_INSTANCED forward, FWD_SSBO_CUSTOM_CAPTURE bake, and the
#       instanced FWD_SSBO_CUSTOM_IMPOSTOR that rides the same opt-in — JIT-compile);
#   (b) the forward array sampling DISCRIMINATES sections under instancing (each instance's
#       two sections still sample DIFFERENT baked array layers).
# The bake side stays NON-instanced-VS + instanceCount=1 (the impostor precedent) — no new
# instanced capture VS. Shares the content-addressed cache with the non-instanced gate.
#
#   run:  MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1 ork.python section_array_bake_instanced.py
###############################################################################
import os, sys

os.environ["SECTION_ARRAY_INSTANCED"] = "1"
os.environ.setdefault("SECTION_ARRAY_OUT", "/tmp/section_array_bake_instanced.png")
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import section_array_bake as base   # reads SECTION_ARRAY_INSTANCED at import


if __name__ == "__main__":
  base.main()
