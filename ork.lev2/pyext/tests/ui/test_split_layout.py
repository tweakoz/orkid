#!/usr/bin/env ork.python

################################################################################
# Test split layout functionality using makeGrid as reference
################################################################################

import sys
from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2

tokens = CrcStringProxy()

################################################################################

class SplitLayoutApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = lev2.OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    self.ezapp.topWidget.enableUiDraw()

    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 5

    # First, test that makeGrid works (sanity check)
    print("Testing makeGrid (should work)...")
    grid_items = lg_group.makeGrid(
      width=2,
      height=1,
      margin=5,
      uiclass=lev2.ui.Box,
      args=["grid", vec4(0.3, 0.3, 0.8, 1)]
    )
    print(f"Created {len(grid_items)} grid items")
    for i, item in enumerate(grid_items):
      print(f"  Item {i}: widget={item.widget}, layout={item.layout}")

    # Now split the left box vertically - ONE API CALL
    # Split at 50%, create GREEN box in BOTTOM half
    # The existing blue box becomes the TOP half
    print("\nTesting splitVertical on left box...")

    new_item = lg_group.splitVertical(
      layout=grid_items[0].layout,      # Split this existing layout
      proportion=0.5,                    # Split at 50% from top
      half=tokens.BOTTOM,                # Create new widget in bottom half (token)
      margin=5,
      uiclass=lev2.ui.Box,
      args=["bottom-green", vec4(0, 1, 0, 1)]  # Green box in bottom
    )
    print(f"Created new bottom item: widget={new_item.widget}, layout={new_item.layout}")
    print("  (Original blue box should now be in top half)")

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updinfo):
    pass

################################################################################

SplitLayoutApp().ezapp.mainThreadLoop()
