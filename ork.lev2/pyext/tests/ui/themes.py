#!/usr/bin/env ork.python

################################################################################
# Theme System Test - Comprehensive demonstration of UI theming with SDF rendering
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import argparse, time, os, math, sys, signal

from obt import host, path as obt_path
from ork import path as ork_path

from ork.app.application import ComponentizedApplication
from ork.app.loggerui import LoggerUIComponent
from ork.app.testlib.multiscene1 import MultiScene1Component
from _themes_overlay import OverlayComponent
from ork.ui.color_picker import ColorPicker

from orkengine.core import vec2, vec3, vec4, CrcStringProxy
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

  def _createBlendCombo(self, parent, label, color, blend_attr):
    """Helper to create a blend selection combobox"""
    combo = parent.makeChild(uiclass=lev2.ui.ComboBox, args=[label, color, 0, 100, 0])
    combo.setItems(["OFF", "ALPHA", "PREMA", "ADDITIVE", "SUBTRACTIVE", "ALPHA_ADDITIVE", "ALPHA_SUBTRACTIVE","INVERSE_SUBTRACTIVE","ALPHA_MODULATE","SRC_MINUS_DST","DST_MINUS_SRC"])
    setattr(self, blend_attr, "ALPHA") 
    combo.selected_index = 1

    def on_changed(w):
      setattr(self, blend_attr, w.selectedItem())
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

    self.addComponent("loggerui", LoggerUIComponent, filter_regex=[".*"]) 

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

    ############################################
    # Configure EzApp creation args
    ############################################

    self.ezapp_args = {
      'width': 1600,
      'height': 900,
      'enable_freerun_ups': True,
      'enable_freerun_fps': True
    }

    ############################################
    # Create EzApp and initialize
    ############################################

    self.createEzApp(name="UiTestThemes")
    self.ezapp.uicontext.debug_event_routing = False
 
  ##############################################

  def _onGpuInit(self,ctx):
    
    self.uvmap = lev2.Image.createFromFile(ork_path.effect_textures/"uvmap_A.png")
    self.knob1 = lev2.Image.createFromFile(ork_path.effect_textures/"knob1.png")
    self.knob2 = lev2.Image.createFromFile(ork_path.effect_textures/"knob2.png")
   
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

    ########################################
    # Set custom theme on UI context
    ########################################

    custom_theme = lev2.ui.ThemeEngine(self.custom_db)
    self.uicontext.theme_engine = custom_theme

    ########################################
    # Create widget pack (independent of multiscene)
    ########################################

    lg_group = self.ezapp.topLayoutGroup

    ########################################
    # Create vertical pack widget and replace top-left grid cell
    ########################################

    pk1 = lg_group.makeChild(uiclass=lev2.ui.VerticalPack, args=["widget_pack"])
    grid0 = self.multiscene.griditems[0]
    lg_group.replaceChild(grid0.layout, pk1)
    vpack = pk1.widget
    vpack.margin = 1
    vpack.item_height = 24
    vpack.fill = True

    ########################################
    # Radius sliders - SG on left, UI on right
    ########################################

    hpack_radius = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["radius_sliders"])
    hpack_radius.margin = 2
    hpack_radius.uniform = True

    self._createSlider(hpack_radius, "SG Radius", vec3(0.3, 0.3, 0.5), 0.0, 64.0, 16.0,
                       lambda w: setattr(self.sg_overlay_style, 'corner_radius', int(w.value)))

    self._createSlider(hpack_radius, "UI Radius", vec3(0.5, 0.3, 0.3), 0.0, 64.0, 16.0,
                       lambda w: setattr(self.ui_tab_style, 'corner_radius', int(w.value)))

    ########################################
    # Border sliders - SG on left, UI on right
    ########################################

    hpack_border = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["border_sliders"])
    hpack_border.margin = 2
    hpack_border.uniform = True

    self._createSlider(hpack_border, "SG Border", vec3(0.3, 0.3, 0.5), 0.0, 10.0, 2.0,
                       lambda w: setattr(self.sg_overlay_style, 'border_width', int(w.value)))

    self._createSlider(hpack_border, "UI Border", vec3(0.5, 0.3, 0.3), 0.0, 10.0, 2.0,
                       lambda w: setattr(self.ui_tab_style, 'border_width', int(w.value)))

    ########################################
    # Opacity sliders - SG on left, UI on right
    ########################################

    hpack_opacity = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["opacity_sliders"])
    hpack_opacity.margin = 2
    hpack_opacity.uniform = True

    self._createSlider(hpack_opacity, "SG Opacity", vec3(0.3, 0.3, 0.5), 0.0, 1.0, 0.85,
                       lambda w: setattr(self, 'sg_opacity', w.value))

    self._createSlider(hpack_opacity, "UI Opacity", vec3(0.5, 0.3, 0.3), 0.0, 1.0, 0.9,
                       lambda w: setattr(self, 'ui_opacity', w.value))

    ########################################
    # Add comboboxes for theme mode selection
    ########################################

    hpack_modes = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["modes"])
    hpack_modes.margin = 2
    hpack_modes.uniform = True
    hpack_modes.draw_background = False

    self._createModeCombo(hpack_modes, "SG Theme", vec3(0.3, 0.3, 0.5), "sg_mode")
    self._createModeCombo(hpack_modes, "UI Theme", vec3(0.5, 0.3, 0.3), "ui_mode")

    ########################################
    # Add comboboxes for Blend Mode selection
    ########################################

    hpack_blend = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["modes"])
    hpack_blend.margin = 2
    hpack_blend.uniform = True
    hpack_blend.draw_background = False

    self._createBlendCombo(hpack_blend, "SG Blend", vec3(0.3, 0.3, 0.5), "sg_blend")
    self._createBlendCombo(hpack_blend, "UI Blend", vec3(0.5, 0.3, 0.3), "ui_blend")

    ########################################
    # ColorPicker widgets - SG and UI (using makeChild, side-by-side)
    ########################################

    hpack_pickers = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["pickers"])
    hpack_pickers.margin = 2
    hpack_pickers.uniform = True
    hpack_pickers.fixed_height = 160
    hpack_pickers.bg_color = vec4(0,0,0, 1.0)

    sg_picker_container = hpack_pickers.makeChild(uiclass=ColorPicker, args=["SG_Picker", vec3(0.3, 0.3, 0.5),vec4(0.2, 0.3, 0.4, 0.85)])
    ui_picker_container = hpack_pickers.makeChild(uiclass=ColorPicker, args=["UI_Picker", vec3(0.5, 0.3, 0.3),vec4(0.5, 0.2, 0.3, 0.9)])

    ########################################
    # Get picker instances from container's uservars (widget.makeChild returns widget directly, not layout item)
    ########################################

    self.sg_picker = sg_picker_container.uservars.color_picker
    self.ui_picker = ui_picker_container.uservars.color_picker

    ########################################
    # Add button images
    ########################################

    self.button_hpack = vpack.makeChild(uiclass=lev2.ui.HorizontalPack, args=["button_images"])
    self.button_hpack.margin = 2
    self.button_hpack.uniform = True
    self.button_hpack.fixed_height = 48
    self.button_hpack.bg_color = vec4(0,0,0,1)
    
    button_names = ["close", "maximize", "minimize", "restore"]

    knob1i = self.knob1.inverted
    knob1i = knob1i.dualThresholded(0.2, 0.0, 1.0, 1.0,0x07) # RGB threshold
    knobi1 = knob1i.gammaed(0.01)
    knobi1 = knobi1.contrasted(2, 0.5)
    #knob1i = knobi1.rotated90cw
    self.movie1 = lev2.MoviePlaybackContext()
    movie1_path = ork_path.assetcache/"movies"/"bunny.mp4"
    self.movie1.init(movie1_path)
    provider1 = self.movie1.image_provider
    self.movie1.play()
    self.movie2 = lev2.MoviePlaybackContext()
    movie2_path = ork_path.assetcache/"movies"/"wipeout.mp4"
    self.movie2.init(movie2_path)
    provider2 = self.movie2.image_provider
    self.movie2.play()

    for btn_name in button_names:
      btn = self.button_hpack.makeChild(uiclass=lev2.ui.ImageButton, args=[f"btn_{btn_name}", btn_name])
      #btn.margin = 2

      match btn_name:
        case "close":
          btn.bgcolor = vec4(0, 0, 0, 1.0)
          uvmap2 = self.uvmap.rotated90cw
          uvmap3 = self.uvmap.rotated90ccw
          btn.inactive_image        = self.uvmap
          btn.active_released_image = uvmap2
          btn.active_pressed_image  = uvmap3
          btn.preserve_aspect_ratio = True
        case "restore":
          btn.bgcolor = vec4(0, 0, 0, 1.0)
          btn.inactive_image        = provider1
          btn.active_released_image = provider1
          btn.active_pressed_image  = provider2
          btn.preserve_aspect_ratio = True
        case "minimize":
          btn.inactive_image        = knob1i
          btn.active_released_image = knob1i
          btn.active_pressed_image  = knob1i
          btn.bgcolor = vec4(0.5, 0.3, 0.3, 1.0)
          btn.inactive_blend_mode = tokens.DST_MINUS_SRC
          btn.active_released_blend_mode = tokens.SUBTRACTIVE
          btn.active_pressed_blend_mode = tokens.ADDITIVE
        case "maximize":
          btn.inactive_image        = self.knob2
          btn.active_released_image = self.knob2
          btn.active_pressed_image  = self.knob2
          btn.bgcolor = vec4(0.3, 0.3, 0.5, 1.0)
          btn.inactive_blend_mode = tokens.ALPHA_ADDITIVE
          btn.active_released_blend_mode = tokens.ADDITIVE
          btn.active_pressed_blend_mode = tokens.SUBTRACTIVE    

    ########################################
    # Add tabs widget with themed boxes
    ########################################

    tabs = vpack.makeChild(uiclass=lev2.ui.TabsWidget, args=["tabs", vec3(0.3, 0.3, 0.5)])

    self._createThemedTab(tabs, "Tab1", vec4(0.6, 0, 0, 1), tokens.ui_tab)

    ########################################
    # Tab2 - ImageRenderer SVG synthesis test
    ########################################

    # Create synthesized image using ImageRenderer
    img_width = 512
    img_height = 512
    renderer = lev2.ImageRenderer(img_width, img_height)

    # Clear to dark background
    renderer.clear(vec4(0.15, 0.15, 0.2, 1.0))

    # Create brushes and pens
    red_brush = lev2.ImageBrush(vec4(1.0, 0.2, 0.2, 1.0))
    blue_brush = lev2.ImageBrush(vec4(0.2, 0.4, 1.0, 1.0))
    yellow_brush = lev2.ImageBrush(vec4(1.0, 0.9, 0.2, 1.0))

    white_pen = lev2.ImagePen(vec4(1.0, 1.0, 1.0, 1.0), 3.0)
    cyan_pen = lev2.ImagePen(vec4(0.2, 1.0, 1.0, 1.0), 2.0)

    # Draw some shapes
    renderer.fillCircle(vec2(256, 256), 180, blue_brush)
    renderer.strokeCircle(vec2(256, 256), 180, white_pen)

    renderer.fillBox(vec2(150, 150), vec2(80, 80), red_brush, 10)
    renderer.strokeBox(vec2(150, 150), vec2(80, 80), white_pen, 10)

    renderer.fillBox(vec2(362, 150), vec2(80, 80), yellow_brush, 10)
    renderer.strokeBox(vec2(362, 150), vec2(80, 80), white_pen, 10)

    # Draw some lines
    import math
    renderer.strokeLine(vec2(100, 400), vec2(412, 400), cyan_pen)
    renderer.strokeLine(vec2(256, 300), vec2(256, 450), cyan_pen)

    # Create ImageView to display the rendered image
    img_view = tabs.makeChild(uiclass=lev2.ui.ImageView, args=["RenderedImage",vec4(0)])
    img_view.generate_mipmaps = True
    img_view.image = renderer.color_buffer
    img_view.maintain_aspect_ratio = True
    #img_view.theme = tokens.ui_tab

    ########################################
    # Tab3 - AlignmentGroup test
    ########################################

    alignment_group = tabs.makeChild(uiclass=lev2.ui.AlignmentGroup, args=["Tab3"])
    alignment_group.alignment = tokens.CENTER
    alignment_group.width_proportional = 1.0
    alignment_group.height_proportional = 1.0
    alignment_group.min_width_pixels = 90
    alignment_group.max_width_pixels = 180*3
    alignment_group.min_height_pixels = 32
    alignment_group.max_height_pixels = 128
    alignment_group.bg_color = vec4(0.1, 0.1, 0.2, 1.0)
    alignment_group.draw_background = False
    alignment_group.margin = 2

    evtestbox = alignment_group.makeChild(uiclass=lev2.ui.EvTestBox, args=["TestBox", vec4(0.5, 0.5, 0, 1)])
    evtestbox.theme = tokens.ui_tab

    edit_panel = self.multiscene.panels[1]
    edit_panel.autocam = False
    edit_panel.griditem.widget.evhandler = lambda x: self.onCameraUiEvent(x)

  ##############################################

  def _applyModeToStyle(self, style, mode, blendmode, abstime, speed, opacity, picker):
    """Apply color mode to a style (mode: 0=anim, 1=light, 2=dark, 3=user)"""
    if mode == 0:  # anim
      t = (math.sin(abstime * speed) + 1.0) * 0.5
      style.bg_color = vec4(0.2 + t * 0.3, 0.3 + t * 0.2, 0.4, opacity)
      style.border_color = vec4(0.5 + t * 0.4, 0.6 + t * 0.3, 0.8, opacity)
      style.text_color = vec4(1.0, 1.0, 1.0, opacity)
      style.blend_mode = getattr(tokens, blendmode)
    elif mode == 1:  # light
      style.bg_color = vec4(0.8, 0.8, 0.8, opacity)
      style.border_color = vec4(0.0, 0.0, 0.0, opacity)
      style.text_color = vec4(0.0, 0.0, 0.0, opacity)  # Black text on light background
      style.blend_mode = getattr(tokens, blendmode)
    elif mode == 2:  # dark
      style.bg_color = vec4(0.2, 0.2, 0.2, opacity)
      style.border_color = vec4(1.0, 1.0, 0.0, opacity)
      style.text_color = vec4(1.0, 1.0, 0.0, opacity)  # Yellow text on dark background
      style.blend_mode = getattr(tokens, blendmode)
    elif mode == 3:  # user
      user_color = picker.current_color
      style.bg_color = vec4(user_color.x, user_color.y, user_color.z, opacity)
      style.border_color = vec4(user_color.x * 1.5, user_color.y * 1.5, user_color.z * 1.5, opacity)
      style.text_color = vec4(1.0, 1.0, 1.0, opacity)
      style.blend_mode = getattr(tokens, blendmode)

  def _onGpuUpdate(self, ctx):

    abstime = self.absolutetime

    # Update styles based on their modes, opacity, and user colors
    self._applyModeToStyle(self.sg_overlay_style, self.sg_mode, self.sg_blend, abstime, 0.5, self.sg_opacity, self.sg_picker)
    self._applyModeToStyle(self.ui_tab_style, self.ui_mode, self.ui_blend, abstime, 0.7, self.ui_opacity, self.ui_picker)

  ##############################################

  def onCameraUiEvent(self, uievent):
    panel = self.multiscene.panels[1]
    uicam = panel.uicam
    handled = uicam.uiEventHandler(uievent)
    if handled:
      uicam.updateMatrices()
      panel.camera.copyFrom( uicam.cameradata )
    return lev2.ui.HandlerResult()

###############################################################################

ThemesTestApp().ezapp.mainThreadLoop()
