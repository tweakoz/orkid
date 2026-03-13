#!/usr/bin/env ork.python
"""
ork.envmap.studio - Professional HDRI environment map generation tool.

Combines source viewing with interactive preprocessing (clamp/gain/gamma),
quick preview baking, and full XIR generation in a single GUI.

Usage:
  ork.envmap.studio.py -i source.exr [-o output.xir]

Controls:
  Display section - Clamp/Gain/Gamma sliders for interactive HDR preview
  Bake section    - Scale, roughness levels, diffuse toggle
  Actions         - Quick Preview (128 samples), Full Bake, Save XIR
"""

import sys, os, time, argparse
import numpy as np
from pathlib import Path
from orkengine import core
from orkengine import lev2
from orkengine.core import vec2, vec3, vec4, CrcStringProxy
from ork.app.application import ComponentizedApplication

tokens = CrcStringProxy()

parser = argparse.ArgumentParser(description="HDRI Environment Map Studio")
parser.add_argument("-i", "--input", type=str, default=None,
                    help="Source environment map (.exr, .hdr, .png, .dds)")
parser.add_argument("-o", "--output", type=str, default=None,
                    help="Output XIR file path")
args = parser.parse_args()

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
  vec3 c = min(src.rgb, vec3(clamp_val));
  c *= gain;
  float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
  c = mix(vec3(lum), c, saturation);
  c = pow(max(c, vec3(0.0)), vec3(inv_gamma));
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

    # Bake parameters
    self.scale_val = 1.0
    self.num_roughness = 10
    self.do_diffuse = False  # matches checkbox default (unchecked)
    self.baking = False

    # UI state
    self.result_imageviews = []
    self.preview_mtl = None
    self.preview_pipeline = None
    self.bake_count = 0

    self.createEzApp(
      name="HDRI Env Map Studio",
      width=1440,
      height=900,
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

    # Split: left 33% controls, right 67% viewer
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

    col_display = vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Display"])
    display_vp = lev2.ui.VerticalPack.wfactory(["display_vp"])
    display_vp.margin = 2
    display_vp.item_height = 28
    display_vp.fill = False
    col_display.setChild(display_vp)

    self.clamp_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Clamp", sli_col, 0.5, 1000.0, 16.0])
    self.clamp_slider.setRange(0.5, 1000.0)
    self.clamp_slider.log_mode = True
    self.clamp_slider.update_on_drag = True
    self.clamp_slider.onValueChanged = lambda w: setattr(self, 'clamp_val', w.value)

    self.gain_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Gain", sli_col, 0.01, 10.0, 1.0])
    self.gain_slider.setRange(0.01, 10.0)
    self.gain_slider.log_mode = True
    self.gain_slider.update_on_drag = True
    self.gain_slider.onValueChanged = lambda w: setattr(self, 'gain_val', w.value)

    self.gamma_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Gamma", sli_col, 0.1, 3.0, 2.2])
    self.gamma_slider.setRange(0.1, 3.0)
    self.gamma_slider.update_on_drag = True
    self.gamma_slider.onValueChanged = lambda w: setattr(self, 'gamma_val', w.value)

    self.sat_slider = display_vp.makeChild(
      uiclass=lev2.ui.FloatSlider,
      args=["Saturation", sli_col, 0.0, 2.0, 1.0])
    self.sat_slider.setRange(0.0, 2.0)
    self.sat_slider.update_on_drag = True
    self.sat_slider.onValueChanged = lambda w: setattr(self, 'saturation_val', w.value)

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
    self.scale_slider.onValueChanged = lambda w: setattr(self, 'scale_val', w.value)

    self.levels_slider = bake_vp.makeChild(
      uiclass=lev2.ui.IntSlider,
      args=["Levels", sli_col, 1, 20, 10])
    self.levels_slider.setRange(1, 20)
    self.levels_slider.onValueChanged = lambda w: setattr(self, 'num_roughness', w.value)

    self.diffuse_chk = bake_vp.makeChild(
      uiclass=lev2.ui.Checkbox,
      args=["Diffuse", vec3(0.3)])
    self.diffuse_chk.onToggled = lambda w: setattr(self, 'do_diffuse', w.toggled)

    # ── Action buttons ────────────────────────────────────────────────

    col_actions = vpack.makeChild(uiclass=lev2.ui.Collapsable, args=["Actions"])
    action_vp = lev2.ui.VerticalPack.wfactory(["action_vp"])
    action_vp.margin = 2
    action_vp.item_height = 32
    action_vp.fill = False
    col_actions.setChild(action_vp)

    self.preview_btn = action_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Quick Preview", vec3(0.25, 0.45, 0.25)])
    self.preview_btn.onPressed = lambda w: self._onQuickPreview()

    self.bake_btn = action_vp.makeChild(
      uiclass=lev2.ui.Button, args=["Full Bake", vec3(0.25, 0.25, 0.45)])
    self.bake_btn.onPressed = lambda w: self._onFullBake()

    ########################################
    # Right cell (67%): viewer tabs
    ########################################

    tabs_item = lg.makeChild(
      uiclass=lev2.ui.TabsWidget,
      args=["viewer_tabs", vec3(0.5, 0.5, 0.8)],
    )
    lg.replaceChild(base[0].layout, tabs_item)
    self.viewer_tabs = tabs_item.widget

    # Source preview tab (always present)
    src_grid = self.viewer_tabs.makeChild(
      uiclass=lev2.ui.DynaGrid, args=["Source"])
    src_grid.margin = 4
    self.source_imv = src_grid.makeChild(
      uiclass=lev2.ui.ImageView,
      args=["src_img", vec4(0.06, 0.06, 0.08, 1)])
    self.source_imv.maintain_aspect_ratio = True
    self.source_imv.generate_mipmaps = False

  ##############################################################################

  def _onGpuInit(self, ctx):
    self.ctx = ctx

    # Compile preview shader
    self.preview_mtl = lev2.FreestyleMaterial()
    self.preview_mtl.gpuInitFromShaderText(ctx, "studio_preview", PREVIEW_SHADER)
    self.preview_mtl.rasterstate.setBlendingMacro(tokens.OFF)
    self.preview_mtl.rasterstate.culltest = tokens.PASS_FRONT

    permu = lev2.FxPipelinePermutation()
    permu.technique = self.preview_mtl.shader.technique("studio_preview")
    self.preview_pipeline = self.preview_mtl.fxcache.findPipeline(permu)
    self.preview_pipeline.sharedMaterial = self.preview_mtl

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
    fmt_name = {1: "L8", 2: "RGBA16F", 4: "RGBA32F"}.get(bpc, f"{bpc}bpc")

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
    print(f"Source: {info}")

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

  ##############################################################################

  def _do_bake(self, quick=False):
    """Run bake and display results."""
    if not self.source_path:
      print("No source loaded")
      return
    if self.baking:
      print("Bake already in progress")
      return

    self.baking = True
    self.bake_count += 1
    from ork.envmap import process_envmap

    if self.output_path:
      out_path = str(Path(self.output_path).resolve())
    else:
      stem = Path(self.source_path).stem
      out_path = f"/tmp/{stem}_studio.xir"

    kwargs = dict(
      clamp=self.clamp_val,
      scale=self.scale_val,
      verbose=True,
    )

    if quick:
      kwargs["roughness_values"] = [0.0, 0.5, 1.0]
      kwargs["skip_diffuse"] = True
      kwargs["specular_samples"] = 128
      label = "Quick Preview"
    else:
      kwargs["num_roughness_levels"] = self.num_roughness
      kwargs["skip_diffuse"] = not self.do_diffuse
      label = "Full Bake"

    print(f"\n{'=' * 60}")
    print(f"{label}: {out_path}")
    print(f"  clamp={self.clamp_val:.1f} scale={self.scale_val:.2f}")
    if quick:
      print(f"  roughness=[0.0, 0.5, 1.0] samples=128")
    else:
      print(f"  levels={self.num_roughness} diffuse={'yes' if self.do_diffuse else 'no'}")
    print("=" * 60)

    start = time.time()
    ok = process_envmap(self.source_path, out_path, self.ctx, self.ezapp, **kwargs)
    elapsed = time.time() - start

    if ok:
      print(f"\n{label} complete ({elapsed:.1f}s): {out_path}")
      self._load_results(out_path)
    else:
      print(f"\n{label} FAILED ({elapsed:.1f}s)")

    self.baking = False

  ##############################################################################

  def _load_results(self, xir_path):
    """Load baked XIR and create result tabs."""
    result = lev2.EnvMapProcessor.readXIR(xir_path)
    spec_imgs = list(result["specular_images"])
    rough_vals = list(result["roughness_values"])
    diff_imgs = list(result["diffuse_images"])

    tag = f"#{self.bake_count}"

    for i, img in enumerate(spec_imgs):
      r = rough_vals[i] if i < len(rough_vals) else 0.0
      grid = self.viewer_tabs.makeChild(
        uiclass=lev2.ui.DynaGrid,
        args=[f"{tag} r={r:.2f}"])
      grid.margin = 4
      imv = grid.makeChild(
        uiclass=lev2.ui.ImageView,
        args=[f"res_s{self.bake_count}_{i}", vec4(0.06, 0.06, 0.08, 1)])
      imv.maintain_aspect_ratio = True
      imv.generate_mipmaps = False
      imv.setImage(img)
      self.result_imageviews.append(imv)

    if diff_imgs:
      diff_grid = self.viewer_tabs.makeChild(
        uiclass=lev2.ui.DynaGrid,
        args=[f"{tag} Diffuse"])
      diff_grid.margin = 4
      for i, img in enumerate(diff_imgs):
        imv = diff_grid.makeChild(
          uiclass=lev2.ui.ImageView,
          args=[f"res_d{self.bake_count}_{i}", vec4(0.06, 0.06, 0.08, 1)])
        imv.maintain_aspect_ratio = True
        imv.generate_mipmaps = False
        imv.setImage(img)
        self.result_imageviews.append(imv)

    # Switch to first result tab
    self.viewer_tabs.setActiveTab(1)

  ##############################################################################

  def _onQuickPreview(self):
    self._do_bake(quick=True)

  def _onFullBake(self):
    self._do_bake(quick=False)

###############################################################################

app = EnvMapStudio()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
os._exit(0)
