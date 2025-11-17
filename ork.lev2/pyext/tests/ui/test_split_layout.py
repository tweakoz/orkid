#!/usr/bin/env ork.python

################################################################################
# Test split layout functionality - create flags using split()
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
    lg_group.clearColorGuide = vec4(1, 1, 0, 1)

    # Create tabs widget at the top
    print("Creating tabs widget...")
    tabs_item = lg_group.makeChild(
      uiclass=lev2.ui.TabsWidget,
      args=["flags-tabs", vec3(0.3, 0.3, 0.5)]
    )
    tabs = tabs_item.widget
    tabs_item.layout.fill(lg_group.layout)

    # Add multiple flag tabs
    self.create_french_flag(tabs)
    self.create_german_flag(tabs)
    self.create_polish_flag(tabs)
    self.create_dutch_flag(tabs)
    self.create_austrian_flag(tabs)
    self.create_sevenway(tabs)
    
    tabs.setActiveTab(0)

    print("All flag tabs created!")

  def _create_striped_flag(self, tabs, name, stripes):
    """Helper to create a flag with colored stripes.

    Args:
      tabs: TabsWidget to add the flag to
      name: Name of the flag tab
      stripes: List of (placement, color) tuples where:
        - placement: tokens.RIGHT, tokens.BOTTOM, etc.
        - color: vec4(r, g, b, a)
    """
    tab_group = tabs.makeChild(uiclass=lev2.ui.LayoutGroup, args=[name])
    tab_group.margin = 3

    # Start with first stripe as 1x1 grid
    base = tab_group.makeGrid(width=1, height=1,
                              uiclass=lev2.ui.Box,
                              args=[f"stripe-0", stripes[0][1]])

    # Add remaining stripes
    prev = base[0]
    for i, (placement, color) in enumerate(stripes[1:], 1):
      # For equal stripes: first split at 1/n, then each subsequent at 1/2
      proportion = 1.0 / (len(stripes) - i + 1) if i == 1 else 0.5
      prev = tab_group.split(layout=prev.layout, proportion=proportion,
                            placement=placement,
                            uiclass=lev2.ui.Box,
                            args=[f"stripe-{i}", color])

  def create_french_flag(self, tabs):
    """French flag: Blue | White | Red (vertical stripes)"""
    print("Creating French flag tab...")
    self._create_striped_flag(tabs, "France", [
      (None, vec4(0, 0, 1, 1)),        # Blue (base)
      (tokens.RIGHT, vec4(1, 1, 1, 1)), # White
      (tokens.RIGHT, vec4(1, 0, 0, 1))  # Red
    ])

  def create_german_flag(self, tabs):
    """German flag: Black / Red / Yellow (horizontal stripes)"""
    print("Creating German flag tab...")
    self._create_striped_flag(tabs, "Germany", [
      (None, vec4(0, 0, 0, 1)),          # Black (base)
      (tokens.BOTTOM, vec4(1, 0, 0, 1)),  # Red
      (tokens.BOTTOM, vec4(1, 0.8, 0, 1)) # Yellow
    ])

  def create_polish_flag(self, tabs):
    """Polish flag: White / Red (horizontal stripes)"""
    print("Creating Polish flag tab...")
    self._create_striped_flag(tabs, "Poland", [
      (None, vec4(1, 1, 1, 1)),               # White (base)
      (tokens.BOTTOM, vec4(0.86, 0.12, 0.20, 1)) # Red
    ])

  def create_dutch_flag(self, tabs):
    """Dutch flag: Red / White / Blue (horizontal stripes)"""
    print("Creating Dutch flag tab...")
    self._create_striped_flag(tabs, "Netherlands", [
      (None, vec4(0.68, 0.11, 0.18, 1)),  # Red (base)
      (tokens.BOTTOM, vec4(1, 1, 1, 1)),   # White
      (tokens.BOTTOM, vec4(0.13, 0.29, 0.58, 1)) # Blue
    ])

  def create_austrian_flag(self, tabs):
    """Austrian flag: Red / White / Red (horizontal stripes)"""
    print("Creating Austrian flag tab...")
    self._create_striped_flag(tabs, "Austria", [
      (None, vec4(0.93, 0.16, 0.22, 1)),  # Red (base)
      (tokens.BOTTOM, vec4(1, 1, 1, 1)),   # White
      (tokens.BOTTOM, vec4(0.93, 0.16, 0.22, 1)) # Red
    ])

  def create_sevenway(self, tabs):
    print("Misc Split Test tab...")

    tab_group = tabs.makeChild(uiclass=lev2.ui.LayoutGroup, args=["7-WAY"])
    tab_group.margin = 3
    tab_group.clearColorStd = vec4(0, 0, 0.1, 1)

    red = vec4(1,0,0, 1)    
    blk = vec4(0,0,0, 1)
    blu = vec4(0,0,1, 1)   
    mag = vec4(1,0,1, 1)   
    cyn = vec4(0,1,1, 1)   

    top_left = tab_group.makeGrid(width=1, height=1,
                                  uiclass=lev2.ui.Box,
                                  args=["stripe-0", mag])

    middle = tab_group.split(layout=top_left[0].layout, proportion=0.1,
                             placement=tokens.BOTTOM,
                             uiclass=lev2.ui.Box,
                             args=[f"stripe-x", blk])


    bottom_left = tab_group.split(layout=middle.layout, proportion=0.9,
                             placement=tokens.BOTTOM,
                             uiclass=lev2.ui.Box,
                             args=[f"stripe-y", cyn])

    top_right = tab_group.split(layout=top_left[0].layout, proportion=0.333,
                             placement=tokens.RIGHT,
                             uiclass=lev2.ui.Box,
                             args=[f"stripe-z1", mag])
    
    top_middle = tab_group.split(layout=top_right.layout, proportion=0.5,
                             placement=tokens.LEFT,
                             uiclass=lev2.ui.Box,
                             args=[f"stripe-z2", red])

    bottom_middle = tab_group.split(layout=bottom_left.layout, proportion=0.333,
                             placement=tokens.RIGHT,
                             uiclass=lev2.ui.Box,
                             args=[f"stripe-z1", blu])
    
    bottom_right = tab_group.split(layout=bottom_middle.layout, proportion=0.5,
                                   placement=tokens.RIGHT,
                                   uiclass=lev2.ui.Box,
                                   args=[f"stripe-z2", cyn])


  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updinfo):
    pass

################################################################################

SplitLayoutApp().ezapp.mainThreadLoop()
