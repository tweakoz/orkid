#!/usr/bin/env ork.python

################################################################################
# Theme System Test - Comprehensive demonstration of UI theming with SDF rendering
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import argparse, time, os, math, sys, signal

from obt import host, path

from ork.app.application import ComponentizedApplication
from ork.app.testlib.multiscene1 import MultiScene1Component
from _themes_overlay import OverlayComponent

from orkengine.core import vec3, vec4, CrcStringProxy
from orkengine import lev2

l2exdir = (lev2.lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir)
from lev2utils.cameras import *

tokens = CrcStringProxy()

################################################################################

parser = argparse.ArgumentParser()
args = parser.parse_args()

################################################################################

class ThemesTestApp(ComponentizedApplication):

  #########################################################

  def __init__(self):
    super().__init__()

    ########################################
    # multiscene component (4 SG viewports)
    ########################################

    self.multiscene = self.addComponent("multiscene1",
                                         MultiScene1Component,
                                         use_8k_textures = False )

    ########################################
    # overlay components (Blender-style edge panels for each SG viewport)
    ########################################

    self.overlay1 = self.addComponent("overlay1", OverlayComponent, grid_index=1, phase_offset=0.0)
    self.overlay2 = self.addComponent("overlay2", OverlayComponent, grid_index=2, phase_offset=2.0)
    self.overlay3 = self.addComponent("overlay3", OverlayComponent, grid_index=3, phase_offset=4.0)

    ########################################
    # create application
    ########################################

    self.ezapp = lev2.OrkEzApp.create(
        self,
        enable_lockstep_ups = False,
        enable_lockstep_fps = False,
        enable_freerun_ups = True,
        enable_freerun_fps = True,
        enable_audio_synth=False,
        enable_graphics=True,
        freerun=True,
        target_ups = 400,
        target_fps = 120,
        width = 1600,
        height = 900,
        fullscreen = False,
        msaa_samples=1)

    self.ezapp.topWidget.enableUiDraw()

  ##############################################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)

    self.uicontext = self.ezapp.uicontext

    ########################################
    # Setup theme databases
    ########################################

    # Create base theme database
    self.base_db = lev2.ui.createDefaultStyleDatabase()

    # Create child database for custom styles
    self.custom_db = lev2.ui.StyleDatabase.createChild(self.base_db)

    # Create two shared themes

    # Theme for scenegraph viewport overlays (controlled by sliders)
    sg_overlay_style = lev2.ui.Style()
    sg_overlay_style.bg_color = vec4(0.2, 0.3, 0.4, 0.85)
    sg_overlay_style.border_color = vec4(0.6, 0.7, 0.8, 1.0)
    sg_overlay_style.corner_radius = 16
    sg_overlay_style.border_width = 2
    sg_overlay_style.blend_mode = tokens.ALPHA
    self.custom_db.registerStyle(tokens.sg_overlay, sg_overlay_style)
    self.sg_overlay_style = sg_overlay_style

    # Theme for UI panel tab boxes (controlled by sliders)
    ui_tab_style = lev2.ui.Style()
    ui_tab_style.bg_color = vec4(0.5, 0.2, 0.3, 0.9)
    ui_tab_style.border_color = vec4(0.8, 0.5, 0.6, 1.0)
    ui_tab_style.corner_radius = 16
    ui_tab_style.border_width = 2
    ui_tab_style.blend_mode = tokens.ALPHA
    self.custom_db.registerStyle(tokens.ui_tab, ui_tab_style)
    self.ui_tab_style = ui_tab_style

    # Set custom theme on UI context
    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

    ########################################
    # Create widget pack (independent of multiscene)
    ########################################

    lg_group = self.ezapp.topLayoutGroup

    # Create vertical pack widget and replace top-left grid cell
    pk1 = lg_group.makeChild(uiclass=lev2.ui.VerticalPack, args=["widget_pack"])
    grid0 = self.multiscene.griditems[0]
    lg_group.replaceChild(grid0.layout, pk1)
    vpack = pk1.widget
    vpack.margin = 4
    vpack.item_height = 32
    vpack.fill = True

    # Radius sliders - SG on left, UI on right
    hpack_radius = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["radius_sliders"])
    hpack_radius.margin = 2
    hpack_radius.uniform = True

    sg_radius_slider = hpack_radius.makeChild(uiclass=lev2.ui.FloatSlider, args=["SG Radius", vec3(0.3, 0.3, 0.5), 0.0, 64.0, 16.0])
    sg_radius_slider.update_on_drag = True

    def on_sg_radius(w):
      self.sg_overlay_style.corner_radius = int(w.value)
    sg_radius_slider.onValueChanged = on_sg_radius

    ui_radius_slider = hpack_radius.makeChild(uiclass=lev2.ui.FloatSlider, args=["UI Radius", vec3(0.5, 0.3, 0.3), 0.0, 64.0, 16.0])
    ui_radius_slider.update_on_drag = True

    def on_ui_radius(w):
      self.ui_tab_style.corner_radius = int(w.value)
    ui_radius_slider.onValueChanged = on_ui_radius

    # Border sliders - SG on left, UI on right
    hpack_border = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["border_sliders"])
    hpack_border.margin = 2
    hpack_border.uniform = True

    sg_border_slider = hpack_border.makeChild(uiclass=lev2.ui.FloatSlider, args=["SG Border", vec3(0.3, 0.3, 0.5), 0.0, 10.0, 2.0])
    sg_border_slider.update_on_drag = True

    def on_sg_border(w):
      self.sg_overlay_style.border_width = int(w.value)
    sg_border_slider.onValueChanged = on_sg_border

    ui_border_slider = hpack_border.makeChild(uiclass=lev2.ui.FloatSlider, args=["UI Border", vec3(0.5, 0.3, 0.3), 0.0, 10.0, 2.0])
    ui_border_slider.update_on_drag = True

    def on_ui_border(w):
      self.ui_tab_style.border_width = int(w.value)
    ui_border_slider.onValueChanged = on_ui_border

    # Add comboboxes for theme mode selection
    hpack_modes = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["modes"])
    hpack_modes.margin = 2
    hpack_modes.uniform = True

    # SG overlay mode selector
    sg_mode_combo = hpack_modes.makeChild(uiclass=lev2.ui.ComboBox, args=["SG Mode", vec3(0.3, 0.3, 0.5), 0, 100, 0])
    sg_mode_combo.setItems(["anim", "light", "dark"])
    self.sg_mode = "anim"  # Default mode

    def on_sg_mode_changed(w):
      self.sg_mode = w.selectedItem
    sg_mode_combo.onSelectionChanged = on_sg_mode_changed

    # UI tab mode selector
    ui_mode_combo = hpack_modes.makeChild(uiclass=lev2.ui.ComboBox, args=["UI Mode", vec3(0.5, 0.3, 0.3), 0, 100, 0])
    ui_mode_combo.setItems(["anim", "light", "dark"])
    self.ui_mode = "anim"  # Default mode

    def on_ui_mode_changed(w):
      self.ui_mode = w.selectedItem
    ui_mode_combo.onSelectionChanged = on_ui_mode_changed

    # Add tabs widget
    tabs = vpack.makeChild(uiclass=lev2.ui.TabsWidget, args=["tabs", vec3(0.3, 0.3, 0.5)])

    # Add themed test boxes to tabs (all use ui_tab theme)
    tab1_box = tabs.makeChild(uiclass=lev2.ui.EvTestBox, args=["Tab1", vec4(0.6, 0, 0, 1)])
    tab1_box.theme = tokens.ui_tab

    tab2_box = tabs.makeChild(uiclass=lev2.ui.EvTestBox, args=["Tab2", vec4(0, 0.6, 0, 1)])
    tab2_box.theme = tokens.ui_tab

    tab3_box = tabs.makeChild(uiclass=lev2.ui.EvTestBox, args=["Tab3", vec4(0.5, 0.5, 0, 1)])
    tab3_box.theme = tokens.ui_tab

  ##############################################

  def onGpuUpdate(self, ctx):
    super().onGpuUpdate(ctx)

    abstime = self.absolutetime

    # Update SG overlay style based on mode
    if self.sg_mode == "anim":
      # Animated colors
      t = (math.sin(abstime * 0.5) + 1.0) * 0.5
      self.sg_overlay_style.bg_color = vec4(0.2 + t * 0.3, 0.3 + t * 0.2, 0.4, 0.85)
      self.sg_overlay_style.border_color = vec4(0.5 + t * 0.4, 0.6 + t * 0.3, 0.8, 1.0)
    elif self.sg_mode == "light":
      # Black on light grey
      self.sg_overlay_style.bg_color = vec4(0.8, 0.8, 0.8, 0.9)
      self.sg_overlay_style.border_color = vec4(0.0, 0.0, 0.0, 1.0)
    elif self.sg_mode == "dark":
      # Yellow on dark grey
      self.sg_overlay_style.bg_color = vec4(0.2, 0.2, 0.2, 0.9)
      self.sg_overlay_style.border_color = vec4(1.0, 1.0, 0.0, 1.0)

    # Update UI tab style based on mode
    if self.ui_mode == "anim":
      # Animated colors
      t = (math.sin(abstime * 0.7) + 1.0) * 0.5
      self.ui_tab_style.bg_color = vec4(0.5 + t * 0.3, 0.2 + t * 0.2, 0.3, 0.9)
      self.ui_tab_style.border_color = vec4(0.8 + t * 0.2, 0.5 + t * 0.3, 0.6, 1.0)
    elif self.ui_mode == "light":
      # Black on light grey
      self.ui_tab_style.bg_color = vec4(0.8, 0.8, 0.8, 0.9)
      self.ui_tab_style.border_color = vec4(0.0, 0.0, 0.0, 1.0)
    elif self.ui_mode == "dark":
      # Yellow on dark grey
      self.ui_tab_style.bg_color = vec4(0.2, 0.2, 0.2, 0.9)
      self.ui_tab_style.border_color = vec4(1.0, 1.0, 0.0, 1.0)

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

ThemesTestApp().ezapp.mainThreadLoop()
