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
    print(f"lg_group type: {type(lg_group)}")
    print(f"lg_group.margin before: {lg_group.margin}")
    lg_group.margin = 5
    print(f"lg_group.margin after: {lg_group.margin}")

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
    #self.create_polish_flag(tabs)
    #self.create_dutch_flag(tabs)
    #self.create_austrian_flag(tabs)

    print("All flag tabs created!")

  def create_french_flag(self, tabs):
    """French flag: Blue | White | Red (vertical stripes)"""
    print("Creating French flag tab...")

    # Create tab with a LayoutGroup - makeChild returns the widget directly
    tab_group = tabs.makeChild(uiclass=lev2.ui.LayoutGroup, args=["France"])
    print(f"tab_group type: {type(tab_group)}")
    tab_group.margin = 3  # Set margin for the tab group
    print(f"tab_group.margin after setting: {tab_group.margin}")

    # Start with 1x1 grid (single box)
    base = tab_group.makeGrid(width=1, height=1, margin=3,
                              uiclass=lev2.ui.Box,
                              args=["base", vec4(0, 0, 1, 1)])  # Blue

    # Split RIGHT to add white stripe (1/3 of total) - margin inherited from tab_group
    white = tab_group.split(layout=base[0].layout, proportion=0.333,
                            placement=tokens.RIGHT,
                            uiclass=lev2.ui.Box,
                            args=["white", vec4(1, 1, 1, 1)])

    # Split the white stripe RIGHT to add red stripe (1/2 of remaining = 1/3 of total)
    red = tab_group.split(layout=white.layout, proportion=0.5,
                          placement=tokens.RIGHT,
                          uiclass=lev2.ui.Box,
                          args=["red", vec4(1, 0, 0, 1)])

  def create_german_flag(self, tabs):
    """German flag: Black / Red / Yellow (horizontal stripes)"""
    print("Creating German flag tab...")

    tab_group = tabs.makeChild(uiclass=lev2.ui.LayoutGroup, args=["Germany"])

    # Start with black stripe
    base = tab_group.makeGrid(width=1, height=1, margin=0,
                               uiclass=lev2.ui.Box,
                               args=["black", vec4(0, 0, 0, 1)])

    # Split BOTTOM to add red stripe (1/3 of total)
    red = tab_group.split(layout=base[0].layout, proportion=0.333,
                          placement=tokens.BOTTOM, margin=0,
                          uiclass=lev2.ui.Box,
                          args=["red", vec4(1, 0, 0, 1)])

    # Split the red stripe BOTTOM to add yellow stripe (1/2 of remaining = 1/3 of total)
    yellow = tab_group.split(layout=red.layout, proportion=0.5,
                             placement=tokens.BOTTOM, margin=0,
                             uiclass=lev2.ui.Box,
                             args=["yellow", vec4(1, 0.8, 0, 1)])

  def create_polish_flag(self, tabs):
    """Polish flag: White / Red (horizontal stripes)"""
    print("Creating Polish flag tab...")

    tab_group = tabs.makeChild(uiclass=lev2.ui.LayoutGroup, args=["Poland"])

    # Start with white stripe
    base = tab_group.makeGrid(width=1, height=1, margin=0,
                               uiclass=lev2.ui.Box,
                               args=["white", vec4(1, 1, 1, 1)])

    # Split BOTTOM to add red stripe
    red = tab_group.split(layout=base[0].layout, proportion=0.5,
                          placement=tokens.BOTTOM, margin=0,
                          uiclass=lev2.ui.Box,
                          args=["red", vec4(0.86, 0.12, 0.20, 1)])

  def create_dutch_flag(self, tabs):
    """Dutch flag: Red / White / Blue (horizontal stripes)"""
    print("Creating Dutch flag tab...")

    tab_group = tabs.makeChild(uiclass=lev2.ui.LayoutGroup, args=["Netherlands"])

    # Start with red stripe
    base = tab_group.makeGrid(width=1, height=1, margin=0,
                               uiclass=lev2.ui.Box,
                               args=["red", vec4(0.68, 0.11, 0.18, 1)])

    # Split BOTTOM to add white stripe (1/3 of total)
    white = tab_group.split(layout=base[0].layout, proportion=0.333,
                            placement=tokens.BOTTOM, margin=0,
                            uiclass=lev2.ui.Box,
                            args=["white", vec4(1, 1, 1, 1)])

    # Split the white stripe BOTTOM to add blue stripe (1/2 of remaining = 1/3 of total)
    blue = tab_group.split(layout=white.layout, proportion=0.5,
                           placement=tokens.BOTTOM, margin=0,
                           uiclass=lev2.ui.Box,
                           args=["blue", vec4(0.13, 0.29, 0.58, 1)])

  def create_austrian_flag(self, tabs):
    """Austrian flag: Red / White / Red (horizontal stripes)"""
    print("Creating Austrian flag tab...")

    tab_group = tabs.makeChild(uiclass=lev2.ui.LayoutGroup, args=["Austria"])

    # Start with red stripe
    base = tab_group.makeGrid(width=1, height=1, margin=0,
                               uiclass=lev2.ui.Box,
                               args=["red-top", vec4(0.93, 0.16, 0.22, 1)])

    # Split BOTTOM to add white stripe (1/3 of total)
    white = tab_group.split(layout=base[0].layout, proportion=0.333,
                            placement=tokens.BOTTOM, margin=0,
                            uiclass=lev2.ui.Box,
                            args=["white", vec4(1, 1, 1, 1)])

    # Split the white stripe BOTTOM to add red stripe (1/2 of remaining = 1/3 of total)
    red = tab_group.split(layout=white.layout, proportion=0.5,
                          placement=tokens.BOTTOM, margin=0,
                          uiclass=lev2.ui.Box,
                          args=["red-bottom", vec4(0.93, 0.16, 0.22, 1)])

  def onGpuInit(self, ctx):
    pass

  def onUpdate(self, updinfo):
    pass

################################################################################

SplitLayoutApp().ezapp.mainThreadLoop()
