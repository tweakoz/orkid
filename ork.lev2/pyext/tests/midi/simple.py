#!/usr/bin/env ork.python

################################################################################
# MIDI Tweakables test - control spotlights with MIDI knobs
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys
from orkengine.core import vec3, vec4, quat, mtx4, CrcStringProxy
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight

################################################################################

tokens = CrcStringProxy()
midi = lev2.midi

################################################################################

parser = argparse.ArgumentParser(description='MIDI tweakables test')
parser.add_argument('--device', type=str, default='midi fighter twister',
                    help='MIDI device name (partial match)')
args = parser.parse_args()

################################################################################

class MidiSpotlightApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 grid_variant="_V4",
                                 eye=vec3(0,12,15))
    self.createEzApp(name="MidiTweakablesTest", ssaa=0, fullscreen=False)
    self.transport = None
    self.tweakables = None

  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    SG = SGC.scenegraph

    # Grid params
    SGC.grid_data.modcolor = vec3(0.3)
    SGC.grid_data.intensityA = 0.1
    SGC.grid_data.intensityB = 0.2
    SGC.grid_data.lineWidth = 0.025

    # Setup model
    model = lev2.XgmModel("data://tests/pbr_calib.glb")
    self.drawable_model = model.createDrawable()
    self.modelnode = SG.createDrawableNodeOnLayers(SGC.fwd_layers, "model-node", self.drawable_model)
    self.modelnode.worldTransform.scale = 1
    self.modelnode.worldTransform.translation = vec3(0, 2, 0)

    # Setup cookies
    color_cookies = lev2.TextureArray(w=1024, h=1024, slices=4, fmt=tokens.RGB8, mipmapped=True)
    depth_cookies = lev2.TextureArray(w=1024, h=1024, slices=4, fmt=tokens.Z32F, mipmapped=True)
    color_cookies.needsRadianceCache = False
    cookies = [
      color_cookies.load("src://effect_textures/L0D.png"),
      color_cookies.load("lev2://textures/transponder24.png"),
      color_cookies.load("src://effect_textures/knob2.png"),
      color_cookies.load("src://effect_textures/knob2.png"),
    ]
    ctx.TXI.updateTextureArray(color_cookies)

    # Spotlight configs: (index, frq, default_rgb, default_intensity, fovbase, voffset, vscale, radius)
    light_configs = [
      (0, 0.17, (0.0, 1.0, 0.0), 2750.0, 60.0, 15, 13, 12),
      (1, 0.37, (1.0, 0.0, 0.0), 2500.0, 60.0, 15, 13, 12),
      (2, 0.57, (0.3, 0.3, 0.3), 400.0,  60.0, 15, 13, 12),
      (3, 0.97, (0.0, 0.0, 1.0), 300.0,  70.0,  3,  2,  7),
    ]

    self.spotlights = []
    self.light_tweaks = []  # [(r_twk, g_twk, b_twk, i_twk), ...]

    for i, (idx, frq, rgb, intensity, fovbase, voffset, vscale, radius) in enumerate(light_configs):
      spotlight = StdSpotLight(
        index=idx, SGC=SGC, model=model, frq=frq,
        color=vec3(*rgb) * intensity,
        cookie=cookies[i], depth_cookie=depth_cookies.slice(i),
        fovbase=fovbase, fovamp=20.0, voffset=voffset, vscale=vscale,
        bias=1e-5, dim=2048, radius=radius)
      self.spotlights.append(spotlight)

    SG.lightingmanager.spot_cookies_color = color_cookies
    SG.lightingmanager.spot_cookies_depth = depth_cookies

    # Setup MIDI
    self.transport = midi.DirectTransport()
    if self.transport.open(args.device):
      print(f"Opened MIDI device: {args.device}")
      self.tweakables = midi.TweakableSet(self.transport)
      self._setupTweakables(light_configs)
      self.tweakables.finalize()
    else:
      print(f"Could not open MIDI device: {args.device}")
      print("Available inputs:")
      for name, idx in midi.InputContext().inputs.items():
        print(f"  [{idx}] {name}")

  ##############################################

  def _setupTweakables(self, light_configs):
    T = self.tweakables

    for i, (_, _, rgb, intensity, _, _, _, _) in enumerate(light_configs):
      knob_base = i * 4

      def make_updater(light_idx):
        return lambda t: self._updateLightColor(light_idx)

      r_twk = T.createFloat(name=f"light{i}_r", knobID=knob_base+0, color=midi.COLOR_RED,
                            min=0.0, max=1.0, default=rgb[0], steps=100,
                            on_changed=make_updater(i))
      g_twk = T.createFloat(name=f"light{i}_g", knobID=knob_base+1, color=midi.COLOR_GREEN,
                            min=0.0, max=1.0, default=rgb[1], steps=100,
                            on_changed=make_updater(i))
      b_twk = T.createFloat(name=f"light{i}_b", knobID=knob_base+2, color=midi.COLOR_BLUE,
                            min=0.0, max=1.0, default=rgb[2], steps=100,
                            on_changed=make_updater(i))
      i_twk = T.createFloat(name=f"light{i}_i", knobID=knob_base+3, color=midi.COLOR_YELLOW,
                            min=0.0, max=10000.0, default=intensity, steps=100, shape=2.0,
                            on_changed=make_updater(i))

      self.light_tweaks.append((r_twk, g_twk, b_twk, i_twk))

  ##############################################

  def _updateLightColor(self, light_index):
    r_twk, g_twk, b_twk, i_twk = self.light_tweaks[light_index]
    self.spotlights[light_index].spot_light.data.color = vec3(r_twk.value, g_twk.value, b_twk.value) * i_twk.value

  ##############################################

  def _onUpdate(self, updinfo):
    self.lighttime = updinfo.absolutetime

  ##############################################

  def _onGpuUpdate(self, ctx):
    for spotlight in self.spotlights:
      spotlight.update(self.lighttime)

###############################################################################

MidiSpotlightApp().ezapp.mainThreadLoop()
