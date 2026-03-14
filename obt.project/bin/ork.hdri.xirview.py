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
import numpy as np

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
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.08, 0.08, 0.1, 1)

    # Full-window base, then split left 20% info / right 80% viewer
    base = lg.makeGrid(width=1, height=1, margin=4,
      uiclass=lev2.ui.Box,
      args=["base", vec4(0.1, 0.1, 0.15, 1)])

    info_item = lg.split(layout=base[0].layout, proportion=0.20,
                         placement=tokens.LEFT,
                         uiclass=lev2.ui.ScrollContainer,
                         args=["info_scroll"])

    # Left panel: scrollable info
    info_scroll = info_item.widget
    info_scroll.scroll_mode = lev2.ui.ScrollMode.Y
    info_scroll.bg_color = vec4(0.12, 0.12, 0.15, 1)

    vpack = lev2.ui.VerticalPack.wfactory(["info_vpack"])
    vpack.margin = 4
    vpack.item_height = 20
    vpack.fill = False
    info_scroll.setChild(vpack)

    col_info = vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["File Info"])
    self.info_text = lev2.ui.TextBox.wfactory(["info_text", vec4(0.12, 0.12, 0.15, 1), ""])
    self.info_text.fixed_height = 500
    self.info_text.setText("Loading...")
    col_info.setChild(self.info_text)

    # Right panel: tabs (replace remaining base cell)
    tabs_item = lg.makeChild(
      uiclass=lev2.ui.TabsWidget,
      args=["xir_tabs", vec3(0.5, 0.5, 0.8)],
    )
    lg.replaceChild(base[0].layout, tabs_item)
    self.tabs = tabs_item.widget

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

    # Build info text
    file_size = os.path.getsize(xir_path)
    if file_size > 1024 * 1024:
      size_str = f"{file_size / (1024*1024):.1f} MB"
    elif file_size > 1024:
      size_str = f"{file_size / 1024:.1f} KB"
    else:
      size_str = f"{file_size} B"

    def _fmt(img):
      bpc = img.bytesPerChannel
      nc = img.numcomponents
      if bpc == 2:
        return f"RGBA16F" if nc == 4 else f"{nc}ch x 16F"
      elif bpc == 1:
        return f"RGBA8" if nc == 4 else f"{nc}ch x 8"
      else:
        return f"{nc}ch x {bpc*8}bit"

    lines = []
    lines.append(f"File: {xir_filename}")
    lines.append(f"Size: {size_str}")
    lines.append(f"Array format: {is_array}")
    lines.append("")
    lines.append(f"Specular levels: {len(self.specular_images)}")
    for i, img in enumerate(self.specular_images):
      r = self.roughness_values[i] if i < len(self.roughness_values) else 0.0
      lines.append(f"  [{i}] {img.width}x{img.height} {_fmt(img)} r={r:.4f}")
    lines.append("")
    lines.append(f"Diffuse mips: {len(self.diffuse_images)}")
    for i, img in enumerate(self.diffuse_images):
      lines.append(f"  [{i}] {img.width}x{img.height} {_fmt(img)}")

    self.info_text.setText("\n".join(lines))

    print("=" * 60)
    print(f"Loading XIR: {xir_path}")
    for l in lines:
      print(f"  {l}")
    print("=" * 60)

    # Create specular tabs from XIR data
    for i, img in enumerate(self.specular_images):
      r = self.roughness_values[i] if i < len(self.roughness_values) else 0.0
      grid = self.tabs.makeChild(
        uiclass=lev2.ui.DynaGrid,
        args=[f"S{i} r={r:.4f}"],
      )
      grid.margin = 4
      imv = grid.makeChild(
        uiclass=lev2.ui.ImageView,
        args=[f"spec_{i}", vec4(0.06, 0.06, 0.08, 1)],
      )
      imv.maintain_aspect_ratio = True
      imv.generate_mipmaps = False
      imv.setImage(img)
      self.spec_imageviews.append(imv)

    # Create diffuse tab and mip ImageViews if diffuse data is present
    if self.diffuse_images:
      diff_grid = self.tabs.makeChild(
        uiclass=lev2.ui.DynaGrid,
        args=["Diffuse"],
      )
      diff_grid.margin = 4
      for i, img in enumerate(self.diffuse_images):
        imv = diff_grid.makeChild(
          uiclass=lev2.ui.ImageView,
          args=[f"diff_{i}", vec4(0.06, 0.06, 0.08, 1)],
        )
        imv.maintain_aspect_ratio = True
        imv.generate_mipmaps = False
        imv.setImage(img)
        self.diff_imageviews.append(imv)

    # Histogram tab: show histograms for first specular level
    histo_grid = self.tabs.makeChild(
      uiclass=lev2.ui.DynaGrid,
      args=["Histogram"],
    )
    histo_grid.margin = 4
    self.histo_imv = histo_grid.makeChild(
      uiclass=lev2.ui.ImageView,
      args=["histo_img", vec4(0.06, 0.06, 0.08, 1)])
    self.histo_imv.maintain_aspect_ratio = True
    self.histo_imv.generate_mipmaps = False

    if self.specular_images:
      self._generate_histogram(self.specular_images[0], "Specular r=0")
    elif self.diffuse_images:
      self._generate_histogram(self.diffuse_images[0], "Diffuse mip 0")

    self.tabs.setActiveTab(0)

  ##############################################

  def _generate_histogram(self, img, title):
    """Render an RGB histogram of an XIR image using matplotlib."""
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt

    w, h = img.width, img.height
    nc, bpc = img.numcomponents, img.bytesPerChannel
    data = bytes(img.data.bytes)

    if bpc == 2:
      from ork.envmap import _half_to_float_array
      raw = _half_to_float_array(np.frombuffer(data, dtype=np.uint16).copy())
    elif bpc == 4:
      raw = np.frombuffer(data, dtype=np.float32).copy()
    else:
      raw = np.frombuffer(data, dtype=np.uint8).astype(np.float32) / 255.0

    pixels = raw.reshape(h, w, nc)

    fig, axes = plt.subplots(2, 1, figsize=(10, 7), facecolor='#111111',
                             gridspec_kw={'height_ratios': [2, 1]})

    ax = axes[0]
    ax.set_facecolor('#1a1a1a')

    pos_vals = raw[raw > 0]
    if len(pos_vals) > 0:
      vmin = max(float(np.min(pos_vals)), 1e-6)
      vmax = float(np.max(pos_vals))
    else:
      vmin, vmax = 1e-6, 1.0

    if vmax / max(vmin, 1e-10) > 100:
      bins = np.logspace(np.log10(vmin), np.log10(vmax), 256)
      ax.set_xscale('log')
    else:
      bins = np.linspace(0, vmax, 256)

    colors = ['#ff4444', '#44ff44', '#4488ff']
    labels = ['R', 'G', 'B']
    for ch in range(min(nc, 3)):
      channel = pixels[:, :, ch].ravel()
      channel = channel[channel > 0]
      ax.hist(channel, bins=bins, alpha=0.6, color=colors[ch],
              label=labels[ch], histtype='stepfilled')

    ax.set_ylabel('Pixel Count', color='#aaa', fontsize=10)
    ax.set_title(f'Histogram: {title}', color='#ddd', fontsize=12, pad=8)
    ax.legend(loc='upper right', fontsize=9)
    ax.tick_params(colors='#888')
    ax.set_yscale('log')
    for spine in ax.spines.values():
      spine.set_color('#333')

    ax2 = axes[1]
    ax2.set_facecolor('#1a1a1a')
    ax2.axis('off')

    stats_lines = []
    for ch in range(min(nc, 3)):
      c = pixels[:, :, ch].ravel()
      cpos = c[c > 0]
      if len(cpos) > 0:
        stats_lines.append(
          f"{labels[ch]}:  min={float(np.min(cpos)):.4f}  "
          f"mean={float(np.mean(c)):.4f}  "
          f"p95={float(np.percentile(c, 95)):.2f}  "
          f"p99={float(np.percentile(c, 99)):.2f}  "
          f"max={float(np.max(c)):.1f}")

    ax2.text(0.05, 0.95, '\n'.join(stats_lines),
             transform=ax2.transAxes, fontsize=9, fontfamily='monospace',
             color='#ccc', verticalalignment='top')

    plt.tight_layout()

    fig.canvas.draw()
    buf = np.frombuffer(fig.canvas.buffer_rgba(), dtype=np.uint8).copy()
    cw, ch_px = fig.canvas.get_width_height()
    buf = buf.reshape(ch_px, cw, 4)
    plt.close(fig)

    histo_img = lev2.Image.createFromBuffer(cw, ch_px, tokens.RGBA8, buf)
    self.histo_imv.setImage(histo_img)

###############################################################################

app = XIRViewer()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()

os._exit(0)
