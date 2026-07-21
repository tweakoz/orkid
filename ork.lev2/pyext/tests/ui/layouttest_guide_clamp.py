#!/usr/bin/env ork.python
################################################################################
# S0.5 gate: programmatic guide clamping.
#
#  The 32px anti-crossing that guards splitter drags must ALSO apply when a
#  guide's proportion is SET programmatically. Setting a crossing proportion
#  must clamp (never silently corrupt into a crossed layout).
#
#  Headless / pure-logic — no GPU or window.
################################################################################
import sys
from orkengine import core   # core before lev2
from orkengine import lev2

################################################################################

def main():
  root = lev2.ui.LayoutGroup.wfactory(["clamp_root"])
  root.setRect(0, 0, 1000, 1000)
  layout = root.layout

  gA = layout.proportionalVerticalGuide(0.30)
  gB = layout.proportionalVerticalGuide(0.70)
  assert abs(gA.proportion - 0.30) < 1e-4, gA.proportion
  assert abs(gB.proportion - 0.70) < 1e-4, gB.proportion

  ####################################
  # push gB across gA (down to 0.1) — must clamp to just-right of gA
  ####################################
  gB.proportion = 0.10
  print("gB after crossing-set:", gB.proportion)

  assert gB.proportion > gA.proportion, \
      f"guide crossed its neighbor (gB={gB.proportion} <= gA={gA.proportion}) — clamp failed"

  expected = 0.30 + 32.0 / 1000.0
  assert abs(gB.proportion - expected) < 1e-3, \
      f"gB not clamped to the 32px minimum gap: got {gB.proportion}, expected ~{expected}"

  print("=== S0.5 programmatic guide clamp gate PASSED ===", flush=True)
  sys.exit(0)

main()
