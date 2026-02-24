#!/usr/bin/env ork.python

################################################################################
# Overlay Dropdown Menu Test
# Demonstrates the overlay system and hierarchical dropdown menus
################################################################################

import signal
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.frame_profiler import FrameProfilerComponent

tokens = CrcStringProxy()

################################################################################

SHADER_CHOICES = [
    "/PBR/Standard",
    "/PBR/Metallic",
    "/PBR/ClearCoat",
    "/PBR/Subsurface",
    "/Unlit/Flat",
    "/Unlit/Emissive",
    "/Unlit/Wireframe",
    "/Toon/CelShaded",
    "/Toon/Outline",
    "/Toon/Hatching",
    "/Special/Glass",
    "/Special/Water",
    "/Special/Hologram",
]

EFFECT_CHOICES = [
    "/Particles/Fire",
    "/Particles/Smoke",
    "/Particles/Sparks",
    "/PostProcess/Bloom",
    "/PostProcess/DOF",
    "/PostProcess/MotionBlur",
    "/PostProcess/ColorGrading/LUT",
    "/PostProcess/ColorGrading/Manual",
    "/Volumetric/Fog",
    "/Volumetric/Clouds",
]

FLAT_CHOICES = [
    "/Red",
    "/Green",
    "/Blue",
    "/Yellow",
    "/Cyan",
    "/Magenta",
]

################################################################################

class OverlayDropdownTest(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.profiler = self.addComponent("profiler", FrameProfilerComponent,
                                      gpu_filter=["*", "-fwd:total"])
    self.createEzApp(width=1280, height=720)

  ##############################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.clearColorStd = vec4(0.12, 0.12, 0.15, 1)

    # Create a scroll container filling the layout group
    sc = lg.makeChild(uiclass=lev2.ui.ScrollContainer, args=["scroll1"])
    sc.layout.fill(lg.layout)
    self.scroll = sc.widget
    self.scroll.scroll_mode = lev2.ui.ScrollMode.Y
    self.scroll.bg_color = vec4(0.12, 0.12, 0.15, 1)

    vpack = lev2.ui.VerticalPack.wfactory(["root_vpack"])
    vpack.margin = 8
    vpack.item_height = 40
    vpack.fill = False
    self.scroll.setChild(vpack)

    # Selection label (using LabelBox so we can update the text)
    self.sel_label = vpack.makeChild(
        uiclass=lev2.ui.LabelBox, args=["sel_label", vec4(0.2, 0.2, 0.25, 1), "Selection: (none)"])

    # Shader dropdown button
    self.btn_shader = vpack.makeChild(
        uiclass=lev2.ui.Button, args=["Choose Shader...", vec3(0.3, 0.4, 0.5)])

    # Effect dropdown button
    self.btn_effect = vpack.makeChild(
        uiclass=lev2.ui.Button, args=["Choose Effect...", vec3(0.4, 0.3, 0.5)])

    # Flat dropdown button
    self.btn_flat = vpack.makeChild(
        uiclass=lev2.ui.Button, args=["Choose Color (flat)...", vec3(0.3, 0.5, 0.4)])

    # Instructions label
    vpack.makeChild(
        uiclass=lev2.ui.LabelBox, args=["info_label", vec4(0.15, 0.15, 0.2, 1), "Click a button to open dropdown. ESC to dismiss."])

  ##############################################

  def _onGpuInit(self, ctx):
    # Set up button callbacks (after GPU init so widgets are ready)
    self.btn_shader.onPressed = lambda w: self._show_dropdown(w, SHADER_CHOICES)
    self.btn_effect.onPressed = lambda w: self._show_dropdown(w, EFFECT_CHOICES)
    self.btn_flat.onPressed = lambda w: self._show_dropdown(w, FLAT_CHOICES)

  ##############################################

  def _show_dropdown(self, widget, choices):
    # Get button position in root coords for overlay placement
    rx, ry = widget.localToRoot(0, widget.height)
    lev2.ui.DropdownMenu.show(
        context=self.ezapp.uicontext,
        paths=choices,
        x=rx, y=ry,
        on_selected=lambda val: self._on_selected(val)
    )

  def _on_selected(self, value):
    print(f"Selected: {value}")
    self.sel_label.text = f"Selection: {value}"

###############################################################################

app = OverlayDropdownTest()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
