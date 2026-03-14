#!/usr/bin/env ork.python
"""
ork.envmap.studio - Professional HDRI environment map generation tool.

Combines source viewing with interactive preprocessing (clamp/gain/gamma/saturation),
quick preview baking, and full XIR generation in a single GUI.

Uses compute shaders for non-blocking progressive filtering.

Usage:
  ork.envmap.studio.py [-i source.exr] [-o output.xir]

Controls:
  Display section - Clamp/Gain/Gamma/Saturation sliders for interactive HDR preview
  Bake section    - Scale, roughness levels, diffuse toggle
  Actions         - Quick Preview (128 samples), Full Bake, Save XIR
"""

import sys, os, time, argparse, json
import numpy as np
from pathlib import Path
from obt import template
from orkengine import core
from orkengine import lev2
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from ork.app.application import ComponentizedApplication
from ork.envmap import ROUGHNESS_POWER

tokens = CrcStringProxy()

parser = argparse.ArgumentParser(description="HDRI Environment Map Studio")
parser.add_argument("-i", "--input", type=str, default=None,
                    help="Source environment map (.exr, .hdr, .png, .dds)")
parser.add_argument("-o", "--output", type=str, default=None,
                    help="Output XIR file path")
args = parser.parse_args()

###############################################################################
# Paths
###############################################################################

_BIN_DIR = Path(os.path.abspath(__file__)).parent
_SHADER_PATH = _BIN_DIR.parent / "scripts" / "ork" / "_envmap_shader.fxv2"

HEADER_SIZE = 3  # vec4s at start of accum SSBO

###############################################################################
# Preview shader - tone maps HDR source with clamp/gain/gamma
###############################################################################

PREVIEW_SHADER = """
uniform_set ub_preview {
  mat4 mvp;
  float clamp_val;
  float gain;
  float inv_gamma;
  float saturation;
  float tonemap_aces;
  float aces_exposure;
}
sampler_set ss_preview (descriptor_set 0) {
  sampler2D ColorMap;
}
vertex_interface iface_vprev : ub_preview {
  inputs {
    vec4 position : POSITION;
    vec4 uv0 : TEXCOORD0;
  }
  outputs {
    vec4 frg_uv0;
  }
}
fragment_interface iface_fprev
  : iface_vprev
  : ub_preview
  : ss_preview {
  outputs {
    layout(location = 0) vec4 out_color;
  }
}
vertex_shader vs_prev : iface_vprev {
  gl_Position = mvp * position;
  frg_uv0 = uv0;
}
fragment_shader fs_prev : iface_fprev {
  vec4 src = texture(ColorMap, frg_uv0.xy);
  vec3 c = src.rgb;
  c *= gain;
  float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
  c = mix(vec3(lum), c, saturation);
  if (tonemap_aces > 0.5) {
    c *= aces_exposure;
    float a = 2.51;
    float b = 0.03;
    float ca = 2.43;
    float d = 0.59;
    float e = 0.14;
    c = clamp((c * (a * c + b)) / (c * (ca * c + d) + e), 0.0, 1.0);
  }
  c = pow(max(c, vec3(0.0)), vec3(inv_gamma));
  if (clamp_val > 0.0) c = min(c, vec3(clamp_val));
  out_color = vec4(c, 1.0);
}
state_block sb_prev : default {
  CullTest = OFF;
}
technique studio_preview {
  fxconfig = fxcfg_default;
  vf_pass = { vs_prev, fs_prev, sb_prev }
}
"""

###############################################################################
# Per-source-image settings persistence
###############################################################################

SETTINGS_PATH = Path.home() / ".obt-global" / "envstudio.json"

def _load_all_settings():
  if SETTINGS_PATH.exists():
    try:
      return json.loads(SETTINGS_PATH.read_text())
    except (json.JSONDecodeError, OSError):
      pass
  return {}

def _save_all_settings(data):
  SETTINGS_PATH.parent.mkdir(parents=True, exist_ok=True)
  SETTINGS_PATH.write_text(json.dumps(data, indent=2))

def _load_settings_for(source_path):
  key = str(Path(source_path).resolve())
  return _load_all_settings().get(key, {})

def _save_settings_for(source_path, settings):
  key = str(Path(source_path).resolve())
  data = _load_all_settings()
  data[key] = settings
  _save_all_settings(data)

###############################################################################

class EnvMapStudio(ComponentizedApplication):

  def __init__(self):
    super().__init__(profiler_channels=[])
    self.source_path = args.input
    self.output_path = args.output
    self.source_img = None

    # Display parameters (shader uniforms)
    self.clamp_val = 16.0
    self.gain_val = 1.0
    self.gamma_val = 2.2
    self.saturation_val = 1.0
    self.tonemap_aces = True
    self.aces_exposure = 1.0

    # Preview parameters
    self.pv_scale_val = 0.5
    self.pv_num_roughness = 3
    self.pv_roughness_curve = ROUGHNESS_POWER
    self.pv_num_samples = 128
    self.pv_do_diffuse = True

    # Bake parameters
    self.scale_val = 1.0
    self.num_roughness = 10
    self.roughness_curve = ROUGHNESS_POWER
    self.num_samples = 8192
    self.do_diffuse = True

    # UI state
    self._result_tabs = []  # persistent (grid, imv, dummy_img) per specular level
    self._diffuse_grid = None  # single DynaGrid for all diffuse mips
    self._diffuse_imvs = []    # (imv, dummy_img) per diffuse mip
    self.preview_mtl = None
    self.preview_pipeline = None
    self.bake_count = 0
    self._save_dialog_pending = False
    self._open_dialog_pending = False
    self._save_win = None
    self._open_win = None
    self._save_xir_path = None
    self._open_hdri_path = None
    self._last_saved_xir = None  # path of most recently saved XIR

    # Compute shader state
    self._compute_mtl = None
    self._cs_clear = None
    self._cs_spec_equi = None
    self._cs_spec_std = None
    self._cs_diff_equi = None
    self._cs_diff_std = None
    self._pv_shader_text = None

    # Preview state machine
    # States: idle -> preprocess -> compile -> create_levels -> filtering
    self._pv_state = "idle"
    self._pv_quick = False
    self._pv_levels = []      # list of dicts per roughness/diffuse level
    self._pv_src_w = 0
    self._pv_src_h = 0
    self._pv_start_time = 0
    self._pv_src_flat = None       # preprocessed source as flat float32
    self._pv_level_specs = []      # pending level specifications
    self._pv_level_idx = 0         # index into _pv_level_specs during creation

    self.createEzApp(
      name="HDRI Env Map Studio",
      width=1440,
      height=960,
      fullscreen=False,
      enable_audio=False,
      enable_audio_output=False,
      enable_audio_synth=False,
      enable_freerun_ups=True,
      enable_freerun_fps=True,
      use_subsystems=["opq", "core", "gpu", "lev2"],
    )
    self.ezapp.uicontext.debug_event_routing = False

  ##############################################################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    lg.margin = 4
    lg.clearColorStd = vec4(0.08, 0.08, 0.1, 1)

    # Full-window base for viewer (will be split to add controls on left)
    base = lg.makeGrid(width=1, height=1, margin=4,
      uiclass=lev2.ui.Box,
      args=["base", vec4(0.1, 0.1, 0.15, 1)])

    # Split: left 25% controls, right 75% viewer
    ctrl_item = lg.split(layout=base[0].layout, proportion=0.25,
                         placement=tokens.LEFT,
                         uiclass=lev2.ui.ScrollContainer,
                         args=["ctrl_scroll"])

    ########################################
    # Left cell: scroll container with controls
    ########################################

    ctrl_scroll = ctrl_item.widget
    ctrl_scroll.scroll_mode = lev2.ui.ScrollMode.Y
    ctrl_scroll.bg_color = vec4(0.12, 0.12, 0.15, 1)

    vpack = lev2.ui.VerticalPack.wfactory(["ctrl_vpack"])
    vpack.margin = 4
    vpack.item_height = 28
    vpack.fill = False
    ctrl_scroll.setChild(vpack)

    sli_col = vec3(0.3, 0.4, 0.5)

    # ── Source info ───────────────────────────────────────────────────

    col_info = vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Source Info"])
    self.info_text = lev2.ui.TextBox.wfactory(["info_text", vec4(0, 0, 0, 1), "info"])
    self.info_text.fixed_height = 140
    self.info_text.setText("No source loaded")
    col_info.setChild(self.info_text)

    # ── Display controls ──────────────────────────────────────────────

    col_display = vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Preprocessing"])
    display_vp = lev2.ui.VerticalPack.wfactory(["display_vp"])
    display_vp.margin = 2
    display_vp.item_height = 28
    display_vp.fill = False
    col_display.setChild(display_vp)

    self.gain_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Gain", sli_col, 0.01, 10.0, 1.0])
    self.gain_slider.setRange(0.01, 10.0)
    self.gain_slider.log_mode = True
    self.gain_slider.update_on_drag = True
    self.gain_slider.onValueChanged = lambda w: self._set_and_save('gain_val', w.value)

    self.sat_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Saturation", sli_col, 0.0, 2.0, 1.0])
    self.sat_slider.setRange(0.0, 2.0)
    self.sat_slider.update_on_drag = True
    self.sat_slider.onValueChanged = lambda w: self._set_and_save('saturation_val', w.value)

    self.aces_checkbox = display_vp.makeChild(
      uiclass=lev2.ui.Checkbox,
      args=["ACES Tonemap", sli_col])
    self.aces_checkbox.toggled = self.tonemap_aces
    def _on_aces_toggle(chk=self.aces_checkbox):
      self.tonemap_aces = chk.toggled
      self._save_settings()
    self.aces_checkbox.onToggled = _on_aces_toggle

    self.aces_exposure_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["ACES Exposure", sli_col, 0.01, 10.0, 1.0])
    self.aces_exposure_slider.setRange(0.01, 10.0)
    self.aces_exposure_slider.log_mode = True
    self.aces_exposure_slider.update_on_drag = True
    self.aces_exposure_slider.onValueChanged = lambda w: self._set_and_save('aces_exposure', w.value)

    self.gamma_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Gamma", sli_col, 0.1, 3.0, 2.2])
    self.gamma_slider.setRange(0.1, 3.0)
    self.gamma_slider.update_on_drag = True
    self.gamma_slider.onValueChanged = lambda w: self._set_and_save('gamma_val', w.value)

    self.clamp_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Clamp", sli_col, 0.5, 1000.0, 16.0])
    self.clamp_slider.setRange(0.0, 1000.0)
    self.clamp_slider.log_mode = True
    self.clamp_slider.update_on_drag = True
    self.clamp_slider.onValueChanged = lambda w: self._set_and_save('clamp_val', w.value)

    # ── Preview controls ──────────────────────────────────────────────

    col_preview = vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Preview Settings"])
    preview_vp = lev2.ui.VerticalPack.wfactory(["preview_vp"])
    preview_vp.margin = 2
    preview_vp.item_height = 28
    preview_vp.fill = False
    col_preview.setChild(preview_vp)

    self.pv_scale_slider = preview_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Scale", sli_col, 0.05, 1.0, 0.5])
    self.pv_scale_slider.setRange(0.05, 1.0)
    self.pv_scale_slider.onValueChanged = lambda w: self._set_and_save('pv_scale_val', w.value)

    self.pv_levels_slider = preview_vp.makeChild(
      uiclass=lev2.ui.IntSlider,
      args=["RoughnessLevels", sli_col, 1, 20, 3])
    self.pv_levels_slider.setRange(1, 20)
    self.pv_levels_slider.onValueChanged = lambda w: self._set_and_save('pv_num_roughness', w.value)

    self.pv_curve_slider = preview_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["RoughnessCurve", sli_col, 0.1, 3.0, ROUGHNESS_POWER])
    self.pv_curve_slider.setRange(0.1, 3.0)
    self.pv_curve_slider.onValueChanged = lambda w: self._set_and_save('pv_roughness_curve', w.value)

    self.pv_samples_slider = preview_vp.makeChild(
      uiclass=lev2.ui.IntSlider,
      args=["Samples", sli_col, 16, 16384, 128])
    self.pv_samples_slider.setRange(16, 16384)
    self.pv_samples_slider.onValueChanged = lambda w: self._set_and_save('pv_num_samples', w.value)

    self.preview_btn = preview_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Preview", vec3(0.25, 0.45, 0.25)])
    self.preview_btn.onPressed = lambda w: self._onQuickPreview()

    self.pv_stop_btn = preview_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Stop", vec3(0.5, 0.25, 0.25)])
    self.pv_stop_btn.onPressed = lambda w: self._onStop()

    # ── Bake controls ─────────────────────────────────────────────────

    col_bake = vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Bake Settings"])
    bake_vp = lev2.ui.VerticalPack.wfactory(["bake_vp"])
    bake_vp.margin = 2
    bake_vp.item_height = 28
    bake_vp.fill = False
    col_bake.setChild(bake_vp)

    self.scale_slider = bake_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Scale", sli_col, 0.1, 1.0, 1.0])
    self.scale_slider.setRange(0.1, 1.0)
    self.scale_slider.onValueChanged = lambda w: self._set_and_save('scale_val', w.value)

    self.levels_slider = bake_vp.makeChild(
      uiclass=lev2.ui.IntSlider,
      args=["RoughnessLevels", sli_col, 1, 20, 10])
    self.levels_slider.setRange(1, 20)
    self.levels_slider.onValueChanged = lambda w: self._set_and_save('num_roughness', w.value)

    self.curve_slider = bake_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["RoughnessCurve", sli_col, 0.1, 3.0, ROUGHNESS_POWER])
    self.curve_slider.setRange(0.1, 3.0)
    self.curve_slider.onValueChanged = lambda w: self._set_and_save('roughness_curve', w.value)

    self.samples_slider = bake_vp.makeChild(
      uiclass=lev2.ui.IntSlider,
      args=["Samples", sli_col, 16, 32768, 8192])
    self.samples_slider.setRange(16, 32768)
    self.samples_slider.onValueChanged = lambda w: self._set_and_save('num_samples', w.value)

    self.bake_btn = bake_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Full Bake", vec3(0.25, 0.25, 0.45)])
    self.bake_btn.onPressed = lambda w: self._onFullBake()

    self.bake_stop_btn = bake_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Stop", vec3(0.5, 0.25, 0.25)])
    self.bake_stop_btn.onPressed = lambda w: self._onStop()

    # ── Actions ────────────────────────────────────────────────────────

    col_actions = vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Actions"])
    actions_vp = lev2.ui.VerticalPack.wfactory(["actions_vp"])
    actions_vp.margin = 2
    actions_vp.item_height = 28
    actions_vp.fill = False
    col_actions.setChild(actions_vp)

    self.open_btn = actions_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Open HDRI", vec3(0.3, 0.35, 0.45)])
    self.open_btn.onPressed = lambda w: self._onOpenHDRI()

    self.save_btn = actions_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Save XIR", vec3(0.35, 0.3, 0.45)])
    self.save_btn.onPressed = lambda w: self._onSaveXIR()

    self.preview_mdl_btn = actions_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Preview Model", vec3(0.3, 0.35, 0.3)])
    self.preview_mdl_btn.onPressed = lambda w: self._onPreviewModel()

    ########################################
    # Right cell (75%): viewer tabs
    ########################################

    tabs_item = lg.makeChild(
      uiclass=lev2.ui.TabsWidget,
      args=["viewer_tabs", vec3(0.5, 0.5, 0.8)],
    )
    lg.replaceChild(base[0].layout, tabs_item)
    self.viewer_tabs = tabs_item.widget
    self.viewer_tabs.sort_tabs = False  # preserve insertion order

    # Source preview tab (always present)
    src_grid = self.viewer_tabs.makeChild(
      uiclass=lev2.ui.DynaGrid, args=["Source"])
    src_grid.margin = 4
    self.source_imv = src_grid.makeChild(
      uiclass=lev2.ui.ImageView,
      args=["src_img", vec4(0.06, 0.06, 0.08, 1)])
    self.source_imv.maintain_aspect_ratio = True
    self.source_imv.generate_mipmaps = False

    # Histogram tab
    histo_grid = self.viewer_tabs.makeChild(
      uiclass=lev2.ui.DynaGrid, args=["Histogram"])
    histo_grid.margin = 4
    self.histo_imv = histo_grid.makeChild(
      uiclass=lev2.ui.ImageView,
      args=["histo_img", vec4(0.06, 0.06, 0.08, 1)])
    self.histo_imv.maintain_aspect_ratio = True
    self.histo_imv.generate_mipmaps = False

  ##############################################################################

  def _onGpuInit(self, ctx):
    self.ctx = ctx

    # Compile preview shader (for source tab)
    self.preview_mtl = lev2.FreestyleMaterial()
    self.preview_mtl.gpuInitFromShaderText(ctx, "studio_preview", PREVIEW_SHADER)
    self.preview_mtl.rasterstate.setBlendingMacro(tokens.OFF)
    self.preview_mtl.rasterstate.culltest = tokens.PASS_FRONT

    permu = lev2.FxPipelinePermutation()
    permu.technique = self.preview_mtl.shader.technique("studio_preview")
    self.preview_pipeline = self.preview_mtl.fxcache.findPipeline(permu)
    self.preview_pipeline.sharedMaterial = self.preview_mtl

    # Compute shader compiled lazily in _pv_setup (needs image dimensions for template params)

    # Load source if provided via CLI
    if self.source_path:
      self._load_source(ctx)

  ##############################################################################

  def _load_source(self, ctx):
    """Load source image and setup interactive preview."""
    path = str(Path(self.source_path).resolve())
    img = lev2.Image.createFromFile(path)
    if not img or img.width == 0:
      print(f"ERROR: Could not load {path}")
      return

    self.source_img = img
    w, h = img.width, img.height
    nc, bpc = img.numcomponents, img.bytesPerChannel
    bpc_name = {1: "8bit", 2: "16F", 4: "32F"}.get(bpc, f"{bpc}bpc")
    fmt_name = f"{nc}ch {bpc_name}"

    # Analyze dynamic range
    data = bytes(img.data.bytes)
    if bpc == 2:
      from ork.envmap import _half_to_float_array
      floats = _half_to_float_array(np.frombuffer(data, dtype=np.uint16).copy())
    elif bpc == 4:
      floats = np.frombuffer(data, dtype=np.float32).copy()
    else:
      floats = np.frombuffer(data, dtype=np.uint8).astype(np.float32) / 255.0

    pos = floats[floats > 0]
    fmax = float(np.max(pos)) if len(pos) > 0 else 0
    fmin = float(np.min(pos)) if len(pos) > 0 else 1e-6
    dr = 20.0 * np.log10(fmax / fmin) if fmin > 0 and fmax > 0 else 0
    fmean = float(np.mean(floats))
    p99 = float(np.percentile(floats, 99))

    info = (f"{os.path.basename(path)}\n"
            f"{w} x {h}  {fmt_name}  nc={nc}\n"
            f"min: {fmin:.4f}\n"
            f"mean: {fmean:.4f}\n"
            f"p99: {p99:.4f}\n"
            f"max: {fmax:.1f}\n"
            f"dynamic range: {dr:.1f} dB")
    self.info_text.setText(info)
    #print(f"Source: {info}")

    # Display in source tab
    self.source_imv.setImage(img)

    # Apply preview shader with interactive uniforms
    self.source_imv.pipeline = self.preview_pipeline
    mtl = self.preview_mtl
    self.preview_pipeline.bindParam(mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    self.preview_pipeline.bindParam(mtl.param("ColorMap"), self.source_imv.texture)
    self.preview_pipeline.bindParam(mtl.param("clamp_val"), lambda: self.clamp_val)
    self.preview_pipeline.bindParam(mtl.param("gain"), lambda: self.gain_val)
    self.preview_pipeline.bindParam(mtl.param("inv_gamma"), lambda: 1.0 / max(self.gamma_val, 0.01))
    self.preview_pipeline.bindParam(mtl.param("saturation"), lambda: self.saturation_val)
    self.preview_pipeline.bindParam(mtl.param("tonemap_aces"), lambda: 1.0 if self.tonemap_aces else 0.0)
    self.preview_pipeline.bindParam(mtl.param("aces_exposure"), lambda: self.aces_exposure)

    # Restore saved settings for this source
    self._restore_settings()

    # Generate histogram of raw source
    self._generate_histogram(img)

  ##############################################################################

  def _generate_histogram(self, img):
    """Render an RGB histogram of the raw source image using matplotlib."""
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

    # --- Top: log-scale histogram of HDR values ---
    ax = axes[0]
    ax.set_facecolor('#1a1a1a')

    # Use log-spaced bins for HDR data
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
    ax.set_title('Source HDR Histogram (raw, before preprocessing)',
                 color='#ddd', fontsize=12, pad=8)
    ax.legend(loc='upper right', fontsize=9)
    ax.tick_params(colors='#888')
    ax.set_yscale('log')
    for spine in ax.spines.values():
      spine.set_color('#333')

    # --- Bottom: per-channel stats ---
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

    # Channel ratios at bright pixels (why is the sun orange?)
    if nc >= 3:
      brightness = np.max(pixels[:, :, :3], axis=2)
      p99_thresh = float(np.percentile(brightness, 99))
      bright_mask = brightness > p99_thresh
      if np.any(bright_mask):
        br = pixels[bright_mask, 0]
        bg = pixels[bright_mask, 1]
        bb = pixels[bright_mask, 2]
        stats_lines.append("")
        stats_lines.append(
          f"Top 1% brightest pixels (>{p99_thresh:.1f}):  "
          f"R={float(np.mean(br)):.1f}  "
          f"G={float(np.mean(bg)):.1f}  "
          f"B={float(np.mean(bb)):.1f}  "
          f"ratio R:G:B = "
          f"{float(np.mean(br)/max(np.mean(bg),1e-6)):.2f} : "
          f"1.00 : "
          f"{float(np.mean(bb)/max(np.mean(bg),1e-6)):.2f}")

    ax2.text(0.05, 0.95, '\n'.join(stats_lines),
             transform=ax2.transAxes, fontsize=9, fontfamily='monospace',
             color='#ccc', verticalalignment='top')

    plt.tight_layout()

    # Render to numpy array
    fig.canvas.draw()
    buf = np.frombuffer(fig.canvas.buffer_rgba(), dtype=np.uint8).copy()
    cw, ch_px = fig.canvas.get_width_height()
    buf = buf.reshape(ch_px, cw, 4)
    plt.close(fig)

    # Create orkid Image from RGBA8 buffer
    histo_img = lev2.Image.createFromBuffer(cw, ch_px, tokens.RGBA8, buf)
    self.histo_imv.setImage(histo_img)

  ##############################################################################

  def _set_and_save(self, attr, value):
    """Update attribute and persist settings."""
    setattr(self, attr, value)
    self._save_settings()

  def _save_settings(self):
    """Persist current settings for the loaded source image."""
    if not self.source_path:
      return
    _save_settings_for(self.source_path, {
      "clamp": self.clamp_val,
      "gain": self.gain_val,
      "gamma": self.gamma_val,
      "saturation": self.saturation_val,
      "tonemap_aces": self.tonemap_aces,
      "aces_exposure": self.aces_exposure,
      "scale": self.scale_val,
      "num_roughness": self.num_roughness,
      "roughness_curve": self.roughness_curve,
      "num_samples": self.num_samples,
      "pv_scale": self.pv_scale_val,
      "pv_num_roughness": self.pv_num_roughness,
      "pv_roughness_curve": self.pv_roughness_curve,
      "pv_num_samples": self.pv_num_samples,
    })

  def _restore_settings(self):
    """Restore saved settings for the loaded source image."""
    if not self.source_path:
      return
    s = _load_settings_for(self.source_path)
    if not s:
      return
    #print(f"  Restoring settings from {SETTINGS_PATH}")
    self.clamp_val = s.get("clamp", self.clamp_val)
    self.gain_val = s.get("gain", self.gain_val)
    self.gamma_val = s.get("gamma", self.gamma_val)
    self.saturation_val = s.get("saturation", self.saturation_val)
    self.tonemap_aces = s.get("tonemap_aces", self.tonemap_aces)
    self.aces_exposure = s.get("aces_exposure", self.aces_exposure)
    self.scale_val = s.get("scale", self.scale_val)
    self.num_roughness = s.get("num_roughness", self.num_roughness)
    self.roughness_curve = s.get("roughness_curve", self.roughness_curve)
    self.num_samples = s.get("num_samples", self.num_samples)
    self.pv_scale_val = s.get("pv_scale", self.pv_scale_val)
    self.pv_num_roughness = s.get("pv_num_roughness", self.pv_num_roughness)
    self.pv_roughness_curve = s.get("pv_roughness_curve", self.pv_roughness_curve)
    self.pv_num_samples = s.get("pv_num_samples", self.pv_num_samples)
    # Update slider widgets to match
    self.clamp_slider.value = self.clamp_val
    self.gain_slider.value = self.gain_val
    self.gamma_slider.value = self.gamma_val
    self.sat_slider.value = self.saturation_val
    self.aces_checkbox.toggled = self.tonemap_aces
    self.aces_exposure_slider.value = self.aces_exposure
    self.scale_slider.value = self.scale_val
    self.levels_slider.value = self.num_roughness
    self.curve_slider.value = self.roughness_curve
    self.samples_slider.value = self.num_samples
    self.pv_scale_slider.value = self.pv_scale_val
    self.pv_levels_slider.value = self.pv_num_roughness
    self.pv_curve_slider.value = self.pv_roughness_curve
    self.pv_samples_slider.value = self.pv_num_samples

  ##############################################################################
  # Preprocessing: CPU-side clamp+gain+saturation+gamma matching the shader
  ##############################################################################

  def _preprocess_source(self):
    """Apply clamp/gain/saturation/gamma to source image on CPU.
    Returns a new Image with the transforms baked in."""
    from ork.envmap import _half_to_float_array, _float_to_half_array

    img = self.source_img
    w, h = img.width, img.height
    nc, bpc = img.numcomponents, img.bytesPerChannel
    data = bytes(img.data.bytes)

    # Decode to float32 array [H, W, nc] then expand to RGBA
    if bpc == 2:
      raw = np.frombuffer(data, dtype=np.uint16).copy()
      decoded = _half_to_float_array(raw).reshape(h, w, nc)
    elif bpc == 4:
      decoded = np.frombuffer(data, dtype=np.float32).copy().reshape(h, w, nc)
    else:
      decoded = np.frombuffer(data, dtype=np.uint8).astype(np.float32).reshape(h, w, nc) / 255.0

    # Expand to RGBA
    floats = np.ones((h, w, 4), dtype=np.float32)
    if nc == 1:
      floats[:, :, 0] = decoded[:, :, 0]
      floats[:, :, 1] = decoded[:, :, 0]
      floats[:, :, 2] = decoded[:, :, 0]
    elif nc == 2:
      floats[:, :, 0] = decoded[:, :, 0]
      floats[:, :, 1] = decoded[:, :, 0]
      floats[:, :, 2] = decoded[:, :, 0]
      floats[:, :, 3] = decoded[:, :, 1]
    elif nc == 3:
      floats[:, :, :3] = decoded
    else:
      floats[:, :, :] = decoded[:, :, :4]

    # Apply transforms matching the preview shader exactly
    rgb = floats[:, :, :3].copy()
    # 1. Gain
    rgb *= self.gain_val
    # 2. Saturation
    lum = 0.2126 * rgb[:,:,0] + 0.7152 * rgb[:,:,1] + 0.0722 * rgb[:,:,2]
    for c in range(3):
      rgb[:,:,c] = lum + (rgb[:,:,c] - lum) * self.saturation_val
    # 3. ACES filmic tonemap (Narkowicz 2015 fit)
    if self.tonemap_aces:
      rgb *= self.aces_exposure
      a, b, ca, d, e = 2.51, 0.03, 2.43, 0.59, 0.14
      rgb = np.clip((rgb * (a * rgb + b)) / (rgb * (ca * rgb + d) + e), 0.0, 1.0)
    # 4. Gamma
    np.maximum(rgb, 0.0, out=rgb)
    inv_gamma = 1.0 / max(self.gamma_val, 0.01)
    np.power(rgb, inv_gamma, out=rgb)
    # 5. Clamp (0 = disabled)
    if self.clamp_val > 0.0:
      np.minimum(rgb, self.clamp_val, out=rgb)

    floats[:, :, :3] = rgb

    # Encode back to Image (always RGBA)
    result = lev2.Image()
    if bpc == 2:
      result.initWithFormat(w, h, tokens.RGBA16F)
      mv = result.data.mutable_bytes
      encoded = _float_to_half_array(floats.reshape(-1))
      np.frombuffer(mv, dtype=np.uint16)[:] = encoded
    elif bpc == 4:
      result.initWithFormat(w, h, tokens.RGBA32F)
      mv = result.data.mutable_bytes
      np.frombuffer(mv, dtype=np.float32)[:] = floats.reshape(-1)
    else:
      result.initWithFormat(w, h, tokens.RGBA8)
      mv = result.data.mutable_bytes
      np.frombuffer(mv, dtype=np.uint8)[:] = np.clip(floats * 255, 0, 255).astype(np.uint8).reshape(-1)

    #print(f"  Preprocessed: clamp={self.clamp_val:.1f} gain={self.gain_val:.2f} "
    #      f"sat={self.saturation_val:.2f} gamma={self.gamma_val:.2f}")
    return result

  ##############################################################################
  # Compute-based progressive preview
  ##############################################################################

  def _upload_to_ssbo(self, FXI, data, ssbo, byte_offset):
    """Upload numpy float32 array to SSBO via direct memcpy."""
    flat = np.ascontiguousarray(data.reshape(-1), dtype=np.float32)
    FXI.copyDataIntoShaderStorageBuffer(flat, ssbo, byte_offset)

  def _make_display_pipeline(self, ctx, accum_ssbo, img_w, img_h):
    """Create a display material+pipeline bound to one accum SSBO."""
    mtl = lev2.FreestyleMaterial()
    mtl.gpuInitFromShaderText(ctx, "envmap_display", self._pv_shader_text)
    permu = lev2.FxPipelinePermutation()
    permu.technique = mtl.shader.technique("tek_display")
    pipeline = mtl.fxcache.findPipeline(permu)
    pipeline.sharedMaterial = mtl
    pipeline.bindParam(mtl.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    pipeline.bindParam(mtl.param("img_width"), float(img_w))
    pipeline.bindParam(mtl.param("img_height"), float(img_h))
    pipeline.bindStorage(mtl.storage("sif_envmap"), accum_ssbo)
    return mtl, pipeline

  ##############################################################################
  # Staged setup: each stage runs in one frame for responsiveness
  ##############################################################################

  def _pv_do_preprocess(self, ctx):
    """Stage 1: CPU preprocess source image."""
    from ork.envmap import _half_to_float_array

    self.info_text.setText("Preprocessing source...")
    print("  Stage: preprocess")

    preprocessed = self._preprocess_source()
    fw, fh = preprocessed.width, preprocessed.height

    scale = self.pv_scale_val if self._pv_quick else self.scale_val
    if scale != 1.0:
      new_w = max(1, int(fw * scale))
      new_h = max(1, int(fh * scale))
      print(f"  Scaling {fw}x{fh} -> {new_w}x{new_h}")
      preprocessed = preprocessed.resized(new_w, new_h)
      fw, fh = new_w, new_h

    self._pv_src_w = fw
    self._pv_src_h = fh

    # Convert to float32 RGBA
    data = bytes(preprocessed.data.bytes)
    bpc = preprocessed.bytesPerChannel
    nc = preprocessed.numcomponents
    if bpc == 2:
      raw = np.frombuffer(data, dtype=np.uint16).copy()
      floats = _half_to_float_array(raw).reshape(fh, fw, nc)
    elif bpc == 4:
      floats = np.frombuffer(data, dtype=np.float32).copy().reshape(fh, fw, nc)
    else:
      floats = np.frombuffer(data, dtype=np.uint8).astype(np.float32).reshape(fh, fw, nc) / 255.0

    if nc < 4:
      padded = np.ones((fh, fw, 4), dtype=np.float32)
      padded[:, :, :nc] = floats
      floats = padded

    self._pv_src_flat = floats.reshape(-1).astype(np.float32)

  def _pv_do_compile(self, ctx):
    """Stage 2: Compile compute/display shader."""
    fw, fh = self._pv_src_w, self._pv_src_h
    self.info_text.setText(f"Compiling shader...\n{fw}x{fh}")
    print("  Stage: compile shader")

    src_pixels = fw * fh
    accum_offset = HEADER_SIZE + src_pixels
    total_size = accum_offset + src_pixels

    raw_shader = _SHADER_PATH.read_text()
    self._pv_shader_text = template.template_string(raw_shader, {
      "SRC_PIXELS": str(src_pixels),
      "ACCUM_OFFSET": str(accum_offset),
      "TOTAL_SIZE": str(total_size),
    })

    self._compute_mtl = lev2.FreestyleMaterial()
    self._compute_mtl.gpuInitFromShaderText(ctx, "envmap_compute", self._pv_shader_text)

    self._cs_clear = self._compute_mtl.computeShader("cs_clear")
    self._cs_spec_equi = self._compute_mtl.computeShader("cs_filter_specular_equi")
    self._cs_spec_std = self._compute_mtl.computeShader("cs_filter_specular_std")
    self._cs_diff_equi = self._compute_mtl.computeShader("cs_filter_diffuse_equi")
    self._cs_diff_std = self._compute_mtl.computeShader("cs_filter_diffuse_std")

    print(f"  DIAG: cs_clear={self._cs_clear} cs_spec_equi={self._cs_spec_equi}")
    if not self._cs_clear or not self._cs_spec_equi:
      raise RuntimeError("Compute shader compilation failed - handles are None")

    # Store computed sizes for level creation
    self._pv_src_pixels = src_pixels
    self._pv_accum_offset = accum_offset
    self._pv_total_size = total_size

  def _pv_do_plan_levels(self, ctx):
    """Stage 3: Plan all levels and create level specs."""
    fw, fh = self._pv_src_w, self._pv_src_h
    self.info_text.setText(f"Planning levels...\n{fw}x{fh}")
    print("  Stage: plan levels")

    if self._pv_quick:
      n = max(1, self.pv_num_roughness)
      denom = max(1, n - 1)
      curve = self.pv_roughness_curve
      roughness_values = [(i / denom) ** curve for i in range(n)]
      total_samples = self.pv_num_samples
      do_diffuse = True
    else:
      denom = max(1, self.num_roughness - 1)
      curve = self.roughness_curve
      roughness_values = [(i / denom) ** curve for i in range(self.num_roughness)]
      total_samples = self.num_samples
      do_diffuse = True

    self.bake_count += 1
    self._pv_tag = f"#{self.bake_count}"
    self._pv_total_samples = total_samples

    cs_spec = self._cs_spec_equi
    cs_diff = self._cs_diff_equi

    # Build level specs: diffuse first (tab after Source), then specular
    self._pv_level_specs = []

    if do_diffuse:
      dw, dh = fw, fh
      mip = 0
      while dw >= 4 and dh >= 4:
        self._pv_level_specs.append({
          'type': 'diffuse',
          'roughness': 1.0,
          'img_w': dw, 'img_h': dh,
          'num_pixels': dw * dh,
          'cs': cs_diff,
          'label': f"diff mip{mip} {dw}x{dh}",
          'mip': mip,
          'imv_name': f"res_d{self.bake_count}_{mip}",
        })
        dw >>= 1
        dh >>= 1
        mip += 1

    for idx, roughness in enumerate(roughness_values):
      self._pv_level_specs.append({
        'type': 'specular',
        'roughness': roughness,
        'img_w': fw, 'img_h': fh,
        'num_pixels': fw * fh,
        'cs': cs_spec,
        'label': f"spec r={roughness:.2f}",
        'tab_name': f"S{idx}: r={roughness:.2f}",
        'imv_name': f"res_s{self.bake_count}_{idx}",
      })

    self._pv_levels = []
    self._pv_level_idx = 0

    label = "Quick Preview" if self._pv_quick else "Full Bake"
    nlev = len(self._pv_level_specs)
    #print(f"\n{'=' * 60}")
    #print(f"{label}: {fw}x{fh}  {nlev} levels  samples={total_samples}")
    #print("=" * 60)

  def _pv_do_create_level(self, ctx):
    """Stage 4 (repeated): Create one SSBO + tab/imageview per call."""
    idx = self._pv_level_idx
    spec = self._pv_level_specs[idx]
    nlev = len(self._pv_level_specs)
    fw, fh = self._pv_src_w, self._pv_src_h

    self.info_text.setText(
      f"Creating level {idx+1}/{nlev}\n"
      f"{spec['label']}\n"
      f"Uploading source data...")
   #print(f"  Creating level {idx+1}/{nlev}: {spec['label']}")

    FXI = ctx.FXI
    ssbo_bytes = self._pv_total_size * 16
    src_data_offset = HEADER_SIZE * 16

    t0 = time.time()
    ssbo = FXI.createShaderStorageBufferWithLength(ssbo_bytes)

    # Write header
    iw, ih = spec['img_w'], spec['img_h']
    FXI.copyDataIntoShaderStorageBuffer(
      [float(iw), float(ih), float(fw), float(fh)], ssbo, 0)
    FXI.copyDataIntoShaderStorageBuffer(
      [float(spec['roughness']), 0.0, 0.0, float(self._pv_total_samples)], ssbo, 16)
    FXI.copyDataIntoShaderStorageBuffer(
      [0.0, 0.0, 0.0, 0.0], ssbo, 32)

    # Upload source data
    self._upload_to_ssbo(FXI, self._pv_src_flat, ssbo, src_data_offset)
    dt = time.time() - t0
    #print(f"    SSBO upload: {dt:.2f}s ({len(self._pv_src_flat)} floats)")

    is_diffuse = spec['type'] == 'diffuse'

    if is_diffuse:
      # All diffuse mips share a single "Diffuse" tab
      mip_idx = spec['mip']
      if mip_idx < len(self._diffuse_imvs):
        imv, dummy = self._diffuse_imvs[mip_idx]
      else:
        if self._diffuse_grid is None:
          self._diffuse_grid = self.viewer_tabs.makeChild(
            uiclass=lev2.ui.DynaGrid, args=["Diffuse"])
          self._diffuse_grid.margin = 4
        imv = self._diffuse_grid.makeChild(
          uiclass=lev2.ui.ImageView,
          args=[spec['imv_name'], vec4(0.06, 0.06, 0.08, 1)])
        imv.maintain_aspect_ratio = True
        imv.generate_mipmaps = False
        dummy = lev2.Image()
        dummy.initWithFormat(iw, ih, tokens.RGBA8)
        imv.setImage(dummy)
        self._diffuse_imvs.append((imv, dummy))
    else:
      # Specular levels get individual tabs
      num_diffuse = sum(1 for s in self._pv_level_specs if s['type'] == 'diffuse')
      spec_idx = idx - num_diffuse
      if spec_idx < len(self._result_tabs):
        grid, imv, dummy = self._result_tabs[spec_idx]
      else:
        grid = self.viewer_tabs.makeChild(
          uiclass=lev2.ui.DynaGrid,
          args=[spec['tab_name']])
        grid.margin = 4
        imv = grid.makeChild(
          uiclass=lev2.ui.ImageView,
          args=[spec['imv_name'], vec4(0.06, 0.06, 0.08, 1)])
        imv.maintain_aspect_ratio = True
        imv.generate_mipmaps = False
        dummy = lev2.Image()
        dummy.initWithFormat(iw, ih, tokens.RGBA8)
        imv.setImage(dummy)
        self._result_tabs.append((grid, imv, dummy))

    disp_mtl, disp_pipeline = self._make_display_pipeline(ctx, ssbo, iw, ih)
    imv.pipeline = disp_pipeline

    self._pv_levels.append({
      'ssbo': ssbo,
      'imv': imv,
      'disp_mtl': disp_mtl,
      'type': spec['type'],
      'roughness': spec['roughness'],
      'img_w': spec['img_w'],
      'img_h': spec['img_h'],
      'sample_offset': 0,
      'total_samples': self._pv_total_samples,
      'num_pixels': spec['num_pixels'],
      'done': False,
      'cs': spec['cs'],
      'label': spec['label'],
    })

    self._pv_level_idx += 1

    # Switch to first result tab on first level
    if idx == 0:
      self.viewer_tabs.setActiveTab(1)

  def _pv_filter_frame(self, ctx):
    """Dispatch one batch of compute samples for all levels (round-robin).
    All levels refine simultaneously so the user sees progressive results on every tab."""
    nlev = len(self._pv_levels)

    # Check if all done
    all_done = all(lv['done'] for lv in self._pv_levels)
    if all_done:
      elapsed = time.time() - self._pv_start_time
      #print(f"\nBake complete ({elapsed:.1f}s)")
      self._pv_state = "idle"
      self.info_text.setText(
        f"Bake complete\n"
        f"{nlev} levels\n"
        f"Time: {elapsed:.1f}s")
      return

    FXI = ctx.FXI
    CI = ctx.CI
    spd = 4 if self._pv_quick else 16

    levels_active = 0
    for level in self._pv_levels:
      if level['done']:
        continue

      remaining = level['total_samples'] - level['sample_offset']
      batch = min(spd, remaining)

      if batch <= 0:
        level['done'] = True
        elapsed = time.time() - self._pv_start_time
        #print(f"\n  {level['label']} done ({elapsed:.1f}s)")
        continue

      # Update header[1] with current batch params
      FXI.copyDataIntoShaderStorageBuffer(
        [level['roughness'], float(batch), float(level['sample_offset']), float(level['total_samples'])],
        level['ssbo'], 16)

      num_workgroups = (level['num_pixels'] + 63) // 64
      cs = level['cs']
      CI.beginDispatchPhase()
      CI.bindStorageBuffer(cs, 0, level['ssbo'])
      CI.dispatch(cs, num_workgroups, 1, 1)
      CI.endDispatchPhase()

      level['sample_offset'] += batch
      levels_active += 1

    # Progress summary
    total_done = sum(lv['sample_offset'] for lv in self._pv_levels)
    total_all = sum(lv['total_samples'] for lv in self._pv_levels)
    pct = total_done * 100 // max(1, total_all)
    elapsed = time.time() - self._pv_start_time
    done_count = sum(1 for lv in self._pv_levels if lv['done'])
    self.info_text.setText(
      f"Baking: {done_count}/{nlev} levels done\n"
      f"Progress: {pct}%\n"
      f"Elapsed: {elapsed:.1f}s")

  ##############################################################################

  def _onGpuUpdate(self, ctx):
    if self._open_dialog_pending:
      self._openOpenDialog(ctx)

    if self._open_hdri_path:
      self._doOpenHDRI(ctx)

    if self._save_dialog_pending:
      self._openSaveDialog(ctx)

    if self._save_xir_path:
      self._doSaveXIR(ctx)

    if self._pv_state == "idle":
      return

    try:
      if self._pv_state == "preprocess":
        self._pv_do_preprocess(ctx)
        self._pv_state = "compile"
        return

      if self._pv_state == "compile":
        self._pv_do_compile(ctx)
        self._pv_state = "plan_levels"
        return

      if self._pv_state == "plan_levels":
        self._pv_do_plan_levels(ctx)
        self._pv_state = "create_levels"
        return

      if self._pv_state == "create_levels":
        if self._pv_level_idx < len(self._pv_level_specs):
          self._pv_do_create_level(ctx)
        else:
          # All levels created, start filtering
          self._pv_src_flat = None  # free memory
          self._pv_start_time = time.time()
          self._pv_state = "filtering"
          nlev = len(self._pv_levels)
          #print(f"  Setup complete: {nlev} levels")
        return

      if self._pv_state == "filtering":
        self._pv_filter_frame(ctx)

    except Exception as e:
      import traceback
      traceback.print_exc()
      self._pv_state = "idle"
      self.info_text.setText(f"ERROR:\n{e}")

  ##############################################################################

  def _onQuickPreview(self):
    if self._pv_state != "idle":
      #print("Preview already in progress")
      return
    if not self.source_img:
      #print("No source loaded")
      return
    self._pv_quick = True
    self._pv_state = "preprocess"

  def _onFullBake(self):
    if self._pv_state != "idle":
      #print("Bake already in progress")
      return
    if not self.source_img:
      #print("No source loaded")
      return
    self._pv_quick = False
    self._pv_state = "preprocess"

  def _onStop(self):
    if self._pv_state == "idle":
      return
    elapsed = time.time() - self._pv_start_time if self._pv_start_time else 0
    self._pv_state = "idle"
    self._pv_src_flat = None
    nlev = len(self._pv_levels)
    done = sum(1 for lv in self._pv_levels if lv['done'])
    #print(f"\nStopped ({elapsed:.1f}s, {done}/{nlev} levels complete)")
    self.info_text.setText(
      f"Stopped\n"
      f"{done}/{nlev} levels done\n"
      f"Time: {elapsed:.1f}s")

  def _onOpenHDRI(self):
    if self._pv_state != "idle":
      #print("Wait for bake to finish (or stop it first)")
      return
    self._open_dialog_pending = True

  def _openOpenDialog(self, ctx):
    """Open a secondary window with a FilesystemBrowser to select an HDRI."""
    from ork.ui.filesystem_browser import FilesystemBrowser

    self._open_dialog_pending = False

    initial_dir = str(Path(self.source_path).resolve().parent) if self.source_path else str(Path.home())

    self._open_win = self.ezapp.createSecondaryWindow(
      width=700, height=500,
      x=200, y=200,
      title="Open HDRI",
      decorated=True,
      resizable=True,
      floating=True,
    )

    uic = self._open_win.ui_context
    win_w = self._open_win.width
    win_h = self._open_win.height

    root = lev2.ui.LayoutGroup.create("open_lg")
    root.setRect(0, 0, win_w, win_h)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["open_browser", initial_dir, "", vec3(0.1, 0.1, 0.1), "load"],
      fill=True,
    )
    browser = browser_item.widget.uservars.filesystem_browser

    def on_open(path):
      #print(f"Open HDRI: {path}")
      self._open_hdri_path = str(Path(path).resolve())
      self._open_win.requestClose()
      self._open_win = None

    def on_cancel():
      self._open_win.requestClose()
      self._open_win = None

    browser.onActivate = on_open
    browser.onCancel = on_cancel

  def _doOpenHDRI(self, ctx):
    """Load the selected HDRI file."""
    path = self._open_hdri_path
    self._open_hdri_path = None
    self.source_path = path
    self._load_source(ctx)

  def _onPreviewModel(self):
    if not self._last_saved_xir:
      self.info_text.setText("No XIR saved yet.\nSave XIR first.")
      return
    import subprocess
    cmd = ["ork.modelviewer.py", "-e", self._last_saved_xir, "-m", "lion"]
    print(f"Launching: {' '.join(cmd)}")
    subprocess.Popen(cmd)

  def _onSaveXIR(self):
    if self._pv_state != "idle":
      #print("Wait for bake to finish (or stop it first)")
      return
    if not self._pv_levels:
      self.info_text.setText("No bake results to save.\nRun Preview or Full Bake first.")
      return
    # If -o was passed, save directly without dialog
    if self.output_path:
      self._save_xir_path = str(Path(self.output_path).resolve())
      return
    # Schedule file dialog creation for GPU thread
    self._save_dialog_pending = True

  def _openSaveDialog(self, ctx):
    """Open a secondary window with a FilesystemBrowser in save mode."""
    from ork.ui.filesystem_browser import FilesystemBrowser

    self._save_dialog_pending = False

    # Default directory and filename
    if self.source_path:
      initial_dir = str(Path(self.source_path).resolve().parent)
      default_name = Path(self.source_path).stem + ".xir"
    else:
      initial_dir = str(Path.home())
      default_name = "output.xir"

    self._save_win = self.ezapp.createSecondaryWindow(
      width=700, height=500,
      x=200, y=200,
      title="Save XIR",
      decorated=True,
      resizable=True,
      floating=True,
    )

    uic = self._save_win.ui_context
    win_w = self._save_win.width
    win_h = self._save_win.height

    root = lev2.ui.LayoutGroup.create("save_lg")
    root.setRect(0, 0, win_w, win_h)
    uic.top = root
    root.margin = 4

    browser_item = root.makeChild(
      uiclass=FilesystemBrowser,
      args=["save_browser", initial_dir, "*.xir", vec3(0.1, 0.1, 0.1), "save"],
      fill=True,
    )
    browser = browser_item.widget.uservars.filesystem_browser
    browser.filename_edit.text = default_name

    def on_save(path):
      #print(f"Save XIR to: {path}")
      self._save_xir_path = str(Path(path).resolve())
      self._save_win.requestClose()
      self._save_win = None

    def on_cancel():
      self._save_win.requestClose()
      self._save_win = None

    browser.onActivate = on_save
    browser.onCancel = on_cancel

  def _doSaveXIR(self, ctx):
    """Read back SSBO accum data, normalize, create Images, write XIR."""
    from ork.envmap import _float_to_half_array

    dest = self._save_xir_path
    self._save_xir_path = None

    self.info_text.setText(f"Saving XIR...\n{os.path.basename(dest)}")
    #print(f"Saving XIR to {dest}")

    FXI = ctx.FXI
    ext = os.path.splitext(self.source_path)[1].lower()
    is_hdr = ext in (".exr", ".hdr")

    specular_images = []
    specular_roughness = []
    diffuse_images = []

    for level in self._pv_levels:
      iw, ih = level['img_w'], level['img_h']
      num_pixels = level['num_pixels']
      accum_offset = self._pv_accum_offset

      # Read accum region from SSBO
      accum_byte_offset = accum_offset * 16
      accum_byte_length = num_pixels * 16
      mapping = FXI.mapStorageBuffer(level['ssbo'], accum_byte_offset, accum_byte_length, tokens.READ_ONLY)
      raw = np.frombuffer(mapping.data, dtype=np.float32).copy().reshape(num_pixels, 4)
      FXI.unmapStorageBuffer(mapping)

      # Normalize: rgb / weight
      w = raw[:, 3:4]
      mask = w > 0
      rgb = np.where(mask, raw[:, :3] / w, 0.0).astype(np.float32)

      # Create Image
      img = lev2.Image()
      if is_hdr:
        img.initWithFormat(iw, ih, tokens.RGBA16F)
        mv = img.data.mutable_bytes
        # Pack as RGBA16F (rgb + alpha=1.0)
        rgba = np.ones((num_pixels, 4), dtype=np.float32)
        rgba[:, :3] = rgb
        halfs = _float_to_half_array(rgba.reshape(-1))
        np.frombuffer(mv, dtype=np.uint16)[:] = halfs
      else:
        img.initWithFormat(iw, ih, tokens.RGBA8)
        mv = img.data.mutable_bytes
        rgba = np.ones((num_pixels, 4), dtype=np.float32)
        rgba[:, :3] = rgb
        np.frombuffer(mv, dtype=np.uint8)[:] = np.clip(rgba * 255, 0, 255).astype(np.uint8).reshape(-1)

      if level['type'] == 'specular':
        specular_images.append(img)
        specular_roughness.append(level['roughness'])
      else:
        diffuse_images.append(img)

      #print(f"  {level['label']}: {iw}x{ih} readback ok")

    os.makedirs(os.path.dirname(os.path.abspath(dest)), exist_ok=True)
    ok = lev2.EnvMapProcessor.writeXIR(specular_images, specular_roughness, diffuse_images, dest)

    if ok:
      self._last_saved_xir = dest
      self.info_text.setText(f"Saved XIR\n{os.path.basename(dest)}\n"
                             f"{len(specular_images)} specular, {len(diffuse_images)} diffuse")
    else:
      #print(f"  ERROR: failed to write {dest}")
      self.info_text.setText(f"ERROR saving XIR\n{os.path.basename(dest)}")

###############################################################################

app = EnvMapStudio()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
os._exit(0)
