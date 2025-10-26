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

  def _createSlider(self, parent, label, color, min_val, max_val, default_val, callback):
    """Helper to create a slider with callback"""
    slider = parent.makeChild(uiclass=lev2.ui.FloatSlider, args=[label, color, min_val, max_val, default_val])
    slider.update_on_drag = True
    slider.onValueChanged = callback
    return slider

  def _createModeCombo(self, parent, label, color, mode_attr):
    """Helper to create a mode selection combobox"""
    combo = parent.makeChild(uiclass=lev2.ui.ComboBox, args=[label, color, 0, 100, 0])
    combo.setItems(["anim", "light", "dark", "user"])
    setattr(self, mode_attr, 0)  # 0=anim, 1=light, 2=dark, 3=user

    def on_changed(w):
      setattr(self, mode_attr, w.selected_index)
    combo.onSelectionChanged = on_changed
    return combo

  def _createThemedTab(self, parent, name, color, theme):
    """Helper to create a themed tab box"""
    tab = parent.makeChild(uiclass=lev2.ui.EvTestBox, args=[name, color])
    tab.theme = theme
    return tab

  #########################################################

  def __init__(self):
    super().__init__()

    # Opacity values
    self.sg_opacity = 0.85
    self.ui_opacity = 0.9

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
    sg_overlay_style.text_color = vec4(1.0, 1.0, 1.0, 1.0)
    sg_overlay_style.corner_radius = 16
    sg_overlay_style.border_width = 2
    sg_overlay_style.blend_mode = tokens.ALPHA
    self.custom_db.registerStyle(tokens.sg_overlay, sg_overlay_style)
    self.sg_overlay_style = sg_overlay_style

    # Theme for UI panel tab boxes (controlled by sliders)
    ui_tab_style = lev2.ui.Style()
    ui_tab_style.bg_color = vec4(0.5, 0.2, 0.3, 0.9)
    ui_tab_style.border_color = vec4(0.8, 0.5, 0.6, 1.0)
    ui_tab_style.text_color = vec4(1.0, 1.0, 1.0, 1.0)
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
    vpack.margin = 1
    vpack.item_height = 24
    vpack.fill = True

    # Radius sliders - SG on left, UI on right
    hpack_radius = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["radius_sliders"])
    hpack_radius.margin = 2
    hpack_radius.uniform = True

    self._createSlider(hpack_radius, "SG Radius", vec3(0.3, 0.3, 0.5), 0.0, 64.0, 16.0,
                       lambda w: setattr(self.sg_overlay_style, 'corner_radius', int(w.value)))

    self._createSlider(hpack_radius, "UI Radius", vec3(0.5, 0.3, 0.3), 0.0, 64.0, 16.0,
                       lambda w: setattr(self.ui_tab_style, 'corner_radius', int(w.value)))

    # Border sliders - SG on left, UI on right
    hpack_border = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["border_sliders"])
    hpack_border.margin = 2
    hpack_border.uniform = True

    self._createSlider(hpack_border, "SG Border", vec3(0.3, 0.3, 0.5), 0.0, 10.0, 2.0,
                       lambda w: setattr(self.sg_overlay_style, 'border_width', int(w.value)))

    self._createSlider(hpack_border, "UI Border", vec3(0.5, 0.3, 0.3), 0.0, 10.0, 2.0,
                       lambda w: setattr(self.ui_tab_style, 'border_width', int(w.value)))

    # Opacity sliders - SG on left, UI on right
    hpack_opacity = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["opacity_sliders"])
    hpack_opacity.margin = 2
    hpack_opacity.uniform = True

    self._createSlider(hpack_opacity, "SG Opacity", vec3(0.3, 0.3, 0.5), 0.0, 1.0, 0.85,
                       lambda w: setattr(self, 'sg_opacity', w.value))

    self._createSlider(hpack_opacity, "UI Opacity", vec3(0.5, 0.3, 0.3), 0.0, 1.0, 0.9,
                       lambda w: setattr(self, 'ui_opacity', w.value))

    # ColorEdit widgets - SG on left, UI on right
    hpack_coloredit = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["color_edits"])
    hpack_coloredit.margin = 2
    hpack_coloredit.uniform = True
    hpack_coloredit.fixed_height = 128

    self.sg_coloredit = hpack_coloredit.makeChild(uiclass=lev2.ui.ColorEdit, args=["SG Color", vec4(0.2, 0.3, 0.4, 0.85)])
    self.ui_coloredit = hpack_coloredit.makeChild(uiclass=lev2.ui.ColorEdit, args=["UI Color", vec4(0.5, 0.2, 0.3, 0.9)])

    # Add comboboxes for theme mode selection
    hpack_modes = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["modes"])
    hpack_modes.margin = 2
    hpack_modes.uniform = True

    self._createModeCombo(hpack_modes, "SG Mode", vec3(0.3, 0.3, 0.5), "sg_mode")
    self._createModeCombo(hpack_modes, "UI Mode", vec3(0.5, 0.3, 0.3), "ui_mode")

    # Add tabs widget with themed boxes
    tabs = vpack.makeChild(uiclass=lev2.ui.TabsWidget, args=["tabs", vec3(0.3, 0.3, 0.5)])

    self._createThemedTab(tabs, "Tab1", vec4(0.6, 0, 0, 1), tokens.ui_tab)
    self._createThemedTab(tabs, "Tab2", vec4(0, 0.6, 0, 1), tokens.ui_tab)
    self._createThemedTab(tabs, "Tab3", vec4(0.5, 0.5, 0, 1), tokens.ui_tab)

  ##############################################

  def _applyModeToStyle(self, style, mode, abstime, speed, opacity, coloredit):
    """Apply color mode to a style (mode: 0=anim, 1=light, 2=dark, 3=user)"""
    if mode == 0:  # anim
      t = (math.sin(abstime * speed) + 1.0) * 0.5
      style.bg_color = vec4(0.2 + t * 0.3, 0.3 + t * 0.2, 0.4, opacity)
      style.border_color = vec4(0.5 + t * 0.4, 0.6 + t * 0.3, 0.8, opacity)
      style.text_color = vec4(1.0, 1.0, 1.0, opacity)
    elif mode == 1:  # light
      style.bg_color = vec4(0.8, 0.8, 0.8, opacity)
      style.border_color = vec4(0.0, 0.0, 0.0, opacity)
      style.text_color = vec4(0.0, 0.0, 0.0, opacity)  # Black text on light background
    elif mode == 2:  # dark
      style.bg_color = vec4(0.2, 0.2, 0.2, opacity)
      style.border_color = vec4(1.0, 1.0, 0.0, opacity)
      style.text_color = vec4(1.0, 1.0, 0.0, opacity)  # Yellow text on dark background
    elif mode == 3:  # user
      user_color = coloredit.currentColor
      style.bg_color = vec4(user_color.x, user_color.y, user_color.z, opacity)
      style.border_color = vec4(user_color.x * 1.5, user_color.y * 1.5, user_color.z * 1.5, opacity)
      style.text_color = vec4(1.0, 1.0, 1.0, opacity)

  def onGpuUpdate(self, ctx):
    super().onGpuUpdate(ctx)

    abstime = self.absolutetime

    # Update styles based on their modes, opacity, and user colors
    self._applyModeToStyle(self.sg_overlay_style, self.sg_mode, abstime, 0.5, self.sg_opacity, self.sg_coloredit)
    self._applyModeToStyle(self.ui_tab_style, self.ui_mode, abstime, 0.7, self.ui_opacity, self.ui_coloredit)

  ##############################################

  def onUiEvent(self, uievent):
    return lev2.ui.HandlerResult()

###############################################################################

ThemesTestApp().ezapp.mainThreadLoop()
