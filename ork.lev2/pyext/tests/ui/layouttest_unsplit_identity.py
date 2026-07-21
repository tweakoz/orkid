#!/usr/bin/env ork.python
################################################################################
# S0.5 gate: split -> unsplit identity.
#
#  A layout that is split and then unsplit must return to a guide-graph that is
#  structurally identical (layoutSignature) to the never-split baseline, and
#  validateTree() must stay clean through every structural mutation.
#
#  Headless / pure-logic — no GPU or window.
################################################################################
import sys
from orkengine import core   # core before lev2
from orkengine import lev2

tokens = core.CrcStringProxy()

################################################################################

def build_root(name):
  root = lev2.ui.LayoutGroup.wfactory([name])
  root.setRect(0, 0, 1000, 1000)
  return root

def make_fill_child(root, name):
  return root.makeChild(uiclass=lev2.ui.LayoutGroup, args=[name], fill=True)

################################################################################

def main():
  ####################################
  # baseline: a root with one fill child, never split
  ####################################
  base = build_root("base")
  make_fill_child(base, "leaf")
  assert base.validateTree(), "baseline validateTree() failed"
  sig_baseline = base.layoutSignature()
  print("baseline sig :", sig_baseline)

  ####################################
  # split, then unsplit back to the survivor
  ####################################
  root = build_root("root")
  target = make_fill_child(root, "leaf")
  assert root.validateTree(), "post-makeChild validateTree() failed"

  split_item = root.split(
      layout=target.layout,
      proportion=0.4,
      placement=tokens.LEFT,
      uiclass=lev2.ui.LayoutGroup,
      args=["new_side"])
  assert root.validateTree(), "post-split validateTree() failed"
  sig_split = root.layoutSignature()
  print("split sig    :", sig_split)
  assert sig_split != sig_baseline, "split did not change the guide graph"

  # collapse the split, keeping the original target as survivor
  root.unsplit(target.layout)
  assert root.validateTree(), "post-unsplit validateTree() failed"
  sig_after = root.layoutSignature()
  print("unsplit sig  :", sig_after)

  assert sig_after == sig_baseline, (
      "guide-graph mismatch after unsplit:\n"
      f"  baseline = {sig_baseline}\n"
      f"  after    = {sig_after}")

  print("=== S0.5 split->unsplit identity gate PASSED ===", flush=True)
  sys.exit(0)

main()
