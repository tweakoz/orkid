#!/usr/bin/env ork.python
################################################################################
# S0.5 gate: validateTree() clean after a cross-group move of a LIVE widget via
#  plain Group::addChild auto-reparent (through a Group* base pointer).
#
#  This is THE proof of the removeChild-virtuality fix: Group::addChild calls
#  w->parent()->removeChild(w). Before the fix that resolved to the non-virtual
#  Group::removeChild, leaving a STALE anchor::Layout node in the source group
#  (validateTree finds it). After the fix, the LayoutGroup override also drops
#  the child's layout node + prunes orphan guides, so the source stays clean.
#
#  Headless / pure-logic — no GPU or window.
################################################################################
import sys
from orkengine import core   # core before lev2
from orkengine import lev2

################################################################################

def main():
  A = lev2.ui.LayoutGroup.wfactory(["groupA"])
  A.setRect(0, 0, 800, 600)
  B = lev2.ui.LayoutGroup.wfactory(["groupB"])
  B.setRect(0, 0, 800, 600)

  item = A.makeChild(uiclass=lev2.ui.LayoutGroup, args=["mover"], fill=True)
  mover = item.widget

  assert A.validateTree(), "A invalid before move"
  assert A.numChildren() == 1, f"A should have 1 child, has {A.numChildren()}"
  assert A.layoutSignature() == "L{cg=[],ch=[L{cg=[],ch=[]};]}", A.layoutSignature()

  ####################################
  # move mover from A to B via plain addChild (auto-reparent)
  ####################################
  B.addChild(mover)

  assert B.numChildren() == 1, f"B should own the widget now, has {B.numChildren()}"
  assert A.numChildren() == 0, f"A still has {A.numChildren()} widget children after move"

  # THE proof: A must have NO stranded layout node for the moved widget.
  assert A.validateTree(), \
      "A retains a STALE anchor::Layout after cross-group move (removeChild virtuality bug)"
  assert A.layoutSignature() == "L{cg=[],ch=[]}", \
      f"A layout tree was not pruned on reparent: {A.layoutSignature()}"

  print("=== S0.5 cross-group reparent validateTree gate PASSED ===", flush=True)
  sys.exit(0)

main()
