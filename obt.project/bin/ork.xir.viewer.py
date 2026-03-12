#!/usr/bin/env ork.python

################################################################################
# XIR Viewer - Displays pre-filtered environment map slices from XIR files
# One tab per specular roughness level, one tab for diffuse mip chain
#
# Usage: ork.xir.viewer.py -i /path/to/file.xir
#
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import sys, os, argparse

from orkengine import core
from orkengine import lev2
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from ork.app.application import ComponentizedApplication

tokens = CrcStringProxy()

################################################################################

parser = argparse.ArgumentParser(description="XIR Viewer - Display pre-filtered environment map slices")
parser.add_argument("-i", "--input", type=str, required=True,
                    help="Path to XIR file to view")
args = parser.parse_args()

xir_path = args.input
if not os.path.exists(xir_path):
  print(f"ERROR: File not found: {xir_path}")
  sys.exit(1)

xir_filename = os.path.basename(xir_path)

################################################################################

class XIRViewer(ComponentizedApplication):

  def __init__(self):
    super().__init__(profiler_channels=[])

    self.specular_images = []
    self.diffuse_images = []
    self.roughness_values = []
    self.spec_imageviews = []
    self.diff_imageviews = []

    self.createEzApp(
      name=f"XIR Viewer - {xir_filename}",
      width=1280,
      height=800,
      fullscreen=False,
      enable_audio=False,
      enable_audio_output=False,
      enable_audio_synth=False,
      enable_freerun_ups=True,
      enable_freerun_fps=True,
      use_subsystems=["opq", "core", "gpu", "lev2"],
    )
    self.ezapp.uicontext.debug_event_routing = False

  ##############################################

  def _onUiInit(self):
    lg_group = self.ezapp.topLayoutGroup
    lg_group.margin = 4
    lg_group.clearColorStd = vec4(0.08, 0.08, 0.1, 1)

    tabs_item = lg_group.makeChild(
      uiclass=lev2.ui.TabsWidget,
      args=["xir_tabs", vec3(0.5, 0.5, 0.8)],
    )
    tabs_item.layout.fill(lg_group.layout)
    self.tabs = tabs_item.widget

    ############################################
    # One tab per specular roughness level
    # Each tab is a DynaGrid showing all mips for that roughness
    # (currently 1 mip per roughness in XIR format)
    ############################################

    for i in range(10):
      r = (i / 9.0) ** 0.5
      grid = self.tabs.makeChild(
        uiclass=lev2.ui.DynaGrid,
        args=[f"S{i} r={r:.2f}"],
      )
      grid.margin = 4
      imv = grid.makeChild(
        uiclass=lev2.ui.ImageView,
        args=[f"spec_{i}", vec4(0.06, 0.06, 0.08, 1)],
      )
      imv.maintain_aspect_ratio = True
      imv.generate_mipmaps = False
      self.spec_imageviews.append(imv)

    ############################################
    # Diffuse tab: DynaGrid showing all mip levels
    ############################################

    self.diff_grid = self.tabs.makeChild(
      uiclass=lev2.ui.DynaGrid,
      args=["Diffuse"],
    )
    self.diff_grid.margin = 4

    # Pre-create up to 16 ImageView slots for diffuse mips
    for i in range(16):
      imv = self.diff_grid.makeChild(
        uiclass=lev2.ui.ImageView,
        args=[f"diff_{i}", vec4(0.06, 0.06, 0.08, 1)],
      )
      imv.maintain_aspect_ratio = True
      imv.generate_mipmaps = False
      self.diff_imageviews.append(imv)

    self.tabs.setActiveTab(0)

  ##############################################

  def _onGpuInit(self, ctx):
    print("=" * 60)
    print(f"Loading XIR: {xir_path}")
    print("=" * 60)

    result = lev2.EnvMapProcessor.readXIR(xir_path)

    self.specular_images = list(result["specular_images"])
    self.roughness_values = list(result["roughness_values"])
    self.diffuse_images = list(result["diffuse_images"])
    is_array = result["is_array_format"]

    print(f"  Array format: {is_array}")
    print(f"  Specular levels: {len(self.specular_images)}")
    for i, img in enumerate(self.specular_images):
      r = self.roughness_values[i] if i < len(self.roughness_values) else 0.0
      print(f"    [{i}] {img.width}x{img.height}  nc={img.numcomponents}  bpc={img.bytesPerChannel}  r={r:.4f}")

    print(f"  Diffuse mips: {len(self.diffuse_images)}")
    for i, img in enumerate(self.diffuse_images):
      print(f"    [{i}] {img.width}x{img.height}  nc={img.numcomponents}  bpc={img.bytesPerChannel}")
    print("=" * 60)

    # Populate specular tabs
    for i, img in enumerate(self.specular_images):
      if i < len(self.spec_imageviews):
        self.spec_imageviews[i].setImage(img)

    # Populate diffuse mips
    for i, img in enumerate(self.diffuse_images):
      if i < len(self.diff_imageviews):
        self.diff_imageviews[i].setImage(img)

###############################################################################

app = XIRViewer()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()

os._exit(0)
