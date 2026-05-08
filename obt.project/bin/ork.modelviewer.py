#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys, os, glob
from obt import path

################################################################################

thisdir = path.directoryOfInvokingModule()

sys.path.append(str(thisdir/".."/".."/"ork.lev2"/"examples"/"python")) # add parent dir to path

print("WTF")
print(sys.argv)
################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-g", '--showgrid', action="store_true", help='show grid' )
parser.add_argument('--showskeleton', action="store_true", help='show skeleton' )
parser.add_argument("-f", '--forceregen', action="store_true", help='force asset regeneration' )
parser.add_argument("-m", "--model", type=str, required=False, default="data://tests/pbr1/pbr1", help='asset to load')
parser.add_argument("-i", "--lightintensity", type=float, default=1.0, help='light intensity')
parser.add_argument("-s", "--specularintensity", type=float, default=1.0, help='specular intensity')
parser.add_argument("-a", "--ambientintensity", type=float, default=0.0, help='diffuse intensity')
parser.add_argument("-d", "--diffuseintensity", type=float, default=1.0, help='diffuse intensity')
parser.add_argument("-D", "--camdist", type=float, default=0.0, help='camera distance')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-o", "--overrideshader", type=str, default="", help='override shader')
parser.add_argument("-c", "--overridecolor", type=str, default="", help='override color (vec3)')
parser.add_argument("-z", "--disablezeroareapolycheck", action="store_true", help='disable zero area poly check')
parser.add_argument("-x", "--encrypt", action="store_true", help='encrpyt model')
parser.add_argument("-t", "--ssaa", type=int, default=2, help='ssaa')
parser.add_argument("-u", "--ssao", type=int, default=0, help='SSAO samples')
parser.add_argument("-L", "--lightmap", type=str, default="", help='set active lightmap')
parser.add_argument('-r', '--rendermodel', type=str, default='forward', help='rendering model (deferred,forward)')
parser.add_argument('-S', '--spotlight', type=float, nargs='?', const=0.2, default=None,
                    help='attach animated spotlight + cookie at INTENSITY (default 1.0); '
                         'omit the flag entirely to disable')
parser.add_argument('-E', '--exposure', type=float, default=None, help='enable ACES tonemapper with given exposure')
parser.add_argument('--list', action="store_true", help='list available model short names')

################################################################################

args = vars(parser.parse_args())

################################################################################
# Build model shortname map from filesystem
################################################################################

def build_model_shortname_map():
  """Scan misc_gltf_samples directory and build shortname -> path map"""
  from pathlib import Path
  shortname_to_path = {}

  # Get the tests directory
  workspace_dir = os.environ.get("ORKID_WORKSPACE_DIR", "")
  if not workspace_dir:
    return shortname_to_path

  gltf_dir = Path(workspace_dir) / "ork.data" / "tests" / "misc_gltf_samples"
  if not gltf_dir.exists():
    return shortname_to_path

  # Scan recursively for .glb files
  for glb_file in gltf_dir.rglob("*.glb"):
    shortname = glb_file.stem  # filename without extension
    # Construct data:// path
    relative_path = glb_file.relative_to(Path(workspace_dir) / "ork.data")
    data_path = f"data://{relative_path}"
    shortname_to_path[shortname] = data_path

  return shortname_to_path

shortname_map = build_model_shortname_map()
# Sorted list of shortnames — used by the M key in the viewport to cycle.
shortname_list = sorted(shortname_map.keys())

# Handle --list option
if args["list"]:
  from pathlib import Path

  # Group models by subdirectory
  dir_groups = {}
  for shortname, fullpath in shortname_map.items():
    # Extract subdirectory from path like "data://tests/misc_gltf_samples/characters/goblin1.glb"
    parts = fullpath.split("/")
    if len(parts) >= 5:  # data://tests/misc_gltf_samples/SUBDIR/model.glb
      subdir = parts[4]  # Get the subdirectory (characters, furnishings, etc.)
    else:
      subdir = "misc"

    # Skip biped2 directory (has extremely long animation names)
    if subdir == "biped2":
      continue

    if subdir not in dir_groups:
      dir_groups[subdir] = []
    dir_groups[subdir].append(shortname)

  # Print grouped and formatted
  print("\nAvailable models:")
  print("=" * 80)

  for subdir in sorted(dir_groups.keys()):
    models = sorted(dir_groups[subdir])

    # Truncate very long names and calculate dynamic column width
    max_name_len = 30  # Hard cap for display
    truncated_models = []
    for m in models:
      if len(m) > max_name_len:
        truncated_models.append(m[:max_name_len-2] + "..")
      else:
        truncated_models.append(m)

    # Calculate column width for this group (max name length + 2 for spacing)
    col_width = min(max(len(m) for m in truncated_models) + 2, max_name_len + 2)

    # Determine number of columns based on 80 char width
    cols = max(1, (80 - 4) // col_width)  # 4 for indent

    print(f"{subdir}:")

    # Print models in grid
    for i in range(0, len(truncated_models), cols):
      row = truncated_models[i:i+cols]
      line = "  " + "".join(f"{m:<{col_width}}" for m in row)
      print(line)

  print("=" * 80)
  print(f"Total: {len(shortname_map)} models | Usage: ork.modelviewer.py -m <shortname>")
  sys.exit(0)

################################################################################
# Parse arguments and resolve model shortnames
################################################################################

showgrid = args["showgrid"]
modelpath = args["model"]

# Resolve shortname to full path if it's a shortname
if modelpath in shortname_map:
  print(f"Resolved shortname '{modelpath}' -> {shortname_map[modelpath]}")
  modelpath = shortname_map[modelpath]

lightintens = args["lightintensity"]
specuintens = args["specularintensity"]
diffuintens = args["diffuseintensity"]
ambiuintens = args["ambientintensity"]
camdist = args["camdist"]
envmap = args["envmap"]
oshader = args["overrideshader"]
ocolor = args["overridecolor"]
ssaa = args["ssaa"]
ssao = args["ssao"]
lightmap = args["lightmap"]
rendermodel = args["rendermodel"]
spotlight_intensity = args["spotlight"]   # None if --spotlight not given, else float
use_spotlight       = (spotlight_intensity is not None)

if args["forceregen"]:
  os.environ["ORKID_LEV2_FORCE_MODEL_REGEN"] = "1"

if args["showskeleton"]:
  os.environ["ORKID_LEV2_SHOW_SKELETON"] = "1"

if args["disablezeroareapolycheck"]:
  os.environ["ORKID_LEV2_MESHUTIL_DISABLE_ZEROAREACHECK"] = "1"

if args["encrypt"]:
  os.environ["ORKID_ASSET_ENCRYPT_MODE"] = "1"

#os.environ["ORKID_LOGFILE_meshutil.assimp"] = os.environ["OBT_STAGE"]+"/tempdir/assimp.log"

################################################################################

# make sure env vars are set before importing the engine...

from orkengine.core import *
from orkengine.lev2 import *
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StdSpotLight

def trace_imports(frame, event, arg):
    if event == "import":
        module_name = arg
        print(f"Importing module: {module_name}")
    return trace_imports

sys.settrace(trace_imports)
from lev2utils.cameras import setupUiCameraX
from lev2utils.shaders import *
from lev2utils.primitives import createGridData

################################################################################

#assert(False)

class SceneGraphApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.materials = set()
    self.modelinsts=[]
    self.ssaamode = False
    if ssao>0:
      self.ssaamode = True
    self.curbrdfi = 0
    self.brdfset = [("GGX",tokens.GGX),("VELVET",tokens.GGXVELVET),("GGXRIM",tokens.GGXRIM),("BLINN",tokens.BLINN),("PHONG",tokens.PHONG)]
    # 0.8 inserted into satset for the default — between 0.75 and 1.0.
    self.satset = [0.0, 0.1, 0.2, 0.5, 0.75, 0.8, 1.0, 1.25, 1.5, 1.75, 2.0]
    self.gamset = [0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4]
    # Default to 0.8 for both saturation and gamma.
    self.cursati = self.satset.index(0.8)   # = 5
    self.curgami = self.gamset.index(0.8)   # = 0

    # Spotlight cycling — L cycles brightness scale, Shift-L cycles color.
    # BASE magnitude is the spotlight's pre-scale intensity (≈ matches the
    # shaderballs default of vec3(1000,800,500)*10 in raw magnitude).
    self.spot_base   = 10000.0
    self.spot_levels = [0.0, 0.1, 0.2, 0.5, 0.75, 1.0, 2.0, 4.0]
    self.spot_colors = [
      ("white",  vec3(1.00, 1.00, 1.00)),
      ("warm",   vec3(1.00, 0.85, 0.65)),  # incandescent / sun
      ("cold",   vec3(0.70, 0.85, 1.00)),  # cool-white LED
      ("R",      vec3(1.00, 0.00, 0.00)),
      ("G",      vec3(0.00, 1.00, 0.00)),
      ("B",      vec3(0.00, 0.00, 1.00)),
      ("M",      vec3(1.00, 0.00, 1.00)),
      ("C",      vec3(0.00, 1.00, 1.00)),
    ]
    # Initial level: snap --spotlight INTENSITY to the closest preset.
    _initial_level = spotlight_intensity 
    self.spot_level_idx = 2
    self.spot_color_idx = 1  # warm by default — matches shaderballs visual

    # Environment map switching — discover from <staging>/assetcache/envmaps2/.
    # Same pattern as shaderballs.py: filesystem-driven so we cycle exactly
    # the .xir files actually present, not a hardcoded registry list whose
    # entries may not all be deployed in this staging.
    stage_dir   = os.environ.get("OBT_STAGE", "")
    envmap_glob = os.path.join(stage_dir, "assetcache", "envmaps2", "*.xir")
    envmap_files = sorted(glob.glob(envmap_glob))
    self.envmap_names = [os.path.splitext(os.path.basename(f))[0] for f in envmap_files]
    self.envmap_paths = [f"<assetcache>/envmaps2/{n}.xir" for n in self.envmap_names]
    self.skybox_names = self.envmap_paths   # alias kept for back-compat with existing HUD/handler refs
    self.skybox_cache = dict()
    self.skybox_index = -1

    self.createEzApp(ssaa=ssaa, fullscreen=True,fullscreen_mode="windowed")

  ##############################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    self.sgviewport_item = lg.makeChild(
      uiclass=ui.SceneGraphViewport,
      args=["PrimarySG"],
      fill=True
    )

  ##############################################

  def _onGpuInit(self,ctx):

    sceneparams = VarMap()
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = float(lightintens)
    sceneparams.SpecularIntensity = float(specuintens)
    sceneparams.DiffuseIntensity = float(diffuintens)
    sceneparams.AmbientLight = vec3(ambiuintens)
    sceneparams.DepthFogDistance = float(1e5)
    sceneparams.SkyboxTexPathStr = "<assetcache>/envmaps2/cold4k.xir"
    sceneparams.ssaa = ssaa

    envmap = args["envmap"]
    if envmap != "":
      if "<" in envmap:
        envmap = path.Path(envmap).expanded
        print(f"Expanded envmap path: {envmap}")
      sceneparams.SkyboxTexPathStr = str(envmap)

    #rendermodel = "DeferredPBR"
    global rendermodel
    if rendermodel == "deferred":
      rendermodel = "DeferredPBR"
    elif rendermodel == "forward":
      rendermodel="ForwardPBR"

    sceneparams.preset = rendermodel

    ###################################
    # post fx nodes
    ###################################
    acesNode = PostFxNodeACES()
    acesNode.exposure = args["exposure"] if args["exposure"] is not None else 0.0
    acesNode.gpuInit(ctx,8,8)
    acesNode.addToSceneVars(sceneparams,"PostFxChain")
    self.aces_node = acesNode
    self.exposure_values = [0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 5.0]
    cur_exp = acesNode.exposure
    self.cur_exposure_idx = 0
    for i, v in enumerate(self.exposure_values):
      if abs(v - cur_exp) < 0.01:
        self.cur_exposure_idx = i

    postNode = PostFxNodeHSVG()
    postNode.hue = 0.0
    postNode.saturation = self.satset[self.cursati]   # default 0.8
    postNode.value = 1.0
    postNode.gamma = self.gamset[self.curgami]        # default 0.8
    postNode.gpuInit(ctx,8,8)
    postNode.addToSceneVars(sceneparams,"PostFxChain")
    self.post_node = postNode

    self.scene = scenegraph.Scene(sceneparams)
    self.layer_donly = self.scene.createLayer("depth_prepass")
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.fwd_layers = [self.layer_fwd,self.layer_donly]
    self.pbr_common = self.scene.pbr_common
    self.pbr_common.useFloatColorBuffer = True

    ######################

    # Find the loaded model's position in shortname_list so the M key
    # picks up cycling from where we are. Match either by shortname or
    # by full data:// path (modelpath could be either).
    self.model_index = -1
    for _i, _name in enumerate(shortname_list):
      if _name == modelpath or shortname_map.get(_name) == modelpath:
        self.model_index = _i
        break

    self.model = XgmModel(modelpath)
    self.sgnode = self.model.createNode("node",self.layer_fwd)
    self.pbr_common = self.scene.pbr_common
    self.model.debugRenderingModel = tokens.NONE
    self.model.debugPassID = tokens.ALL # PROBE MAIN
    self.model.debugSubPassID = tokens.ALL # tokens.FORWARD_PBR

    ######################
    # override shader ?
    ######################

    if lightmap != "":
      self.modelinst = self.sgnode.user.pyext_retain_modelinst
      for m in self.model.meshes:
        for s in m.submeshes:
          mtl = s.material
          mtl.setActiveLightMapA(lightmap,vec3(1))

    elif oshader != "":
      self.modelinst = self.sgnode.user.pyext_retain_modelinst
      mesh = self.model.meshes[0]
      orig_submesh = mesh.submeshes[0]


      subinst = self.modelinst.submeshinsts[0]
      mtl_cloned = orig_submesh.material.clone()
      mtl_cloned.baseColor = vec4(1,1,1,1)

      if ocolor != "":
        ocolor_eval = eval(ocolor)
        mtl_cloned.baseColor = vec4(ocolor_eval,1)


      mtl_cloned.metallicFactor = 0
      mtl_cloned.roughnessFactor = 1
      mtl_cloned.texColor = Texture.load("src://effect_textures/white.dds")
      mtl_cloned.texNormal = Texture.load("src://effect_textures/default_normal.dds")
      mtl_cloned.texMtlRuf = Texture.load("src://effect_textures/white.dds")

      if oshader=="topo":
        #meshutil_submesh = ???
        #self.barysub_isect = meshutil_submesh.barycentricUVs()
        #self.prim = meshutil.RigidPrimitive(self.barysub_isect,ctx)
        mtl_cloned.metallicFactor = 0
        mtl_cloned.roughnessFactor = 1
        mtl_cloned.shaderpath = "orkshader://deferred_ovr_topo.glfx"
      elif oshader=="mirror":
        mtl_cloned.metallicFactor = 1
        mtl_cloned.roughnessFactor = .01
      elif oshader=="shinyplastic":
        mtl_cloned.metallicFactor = 0
        mtl_cloned.roughnessFactor = 0
      elif oshader=="roughplastic":
        mtl_cloned.metallicFactor = 0
        mtl_cloned.roughnessFactor = 1
      elif oshader=="roughmetal":
        mtl_cloned.metallicFactor = 1
        mtl_cloned.roughnessFactor = .75

      mtl_cloned.gpuInit(ctx)
      subinst.overrideMaterial(mtl_cloned)

    ######################
    # camera
    ######################

    self.cameralut = CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(
      cameralut=self.cameralut,
      camname="Camera0"
    )

    center = self.model.boundingCenter
    radius = self.model.boundingRadius*2.5

    if camdist!=0.0:
      radius = camdist

    self.uicam.lookAt( center-vec3(0,0,radius),
                       center,
                       vec3(0,1,0) )

    #self.uicam.base_zmoveamt = radius*0.01

    self.camera.copyFrom( self.uicam.cameradata )

    ######################
    # viewport setup
    ######################

    sgviewport = self.sgviewport_item.widget
    sgviewport.cameraName = "Camera0"
    sgviewport.scenegraph = self.scene
    sgviewport.forkDB()
    sgviewport.evhandler = lambda e: self._onViewportEvent(e)
    sgviewport.ignoreEvents = False

    ###################################

    if showgrid:
      self.grid_data = createGridData()
      if rendermodel == "ForwardPBR":
        self.grid_data.shader_suffix = "_V3"
      self.grid_node = self.layer_fwd.createGridNode("grid",self.grid_data)
      self.grid_node.sortkey = 1

    ###################################
    # --spotlight: attach the same animated spotlight + cookie that
    # shaderballs.py sets up. Useful for verifying spot/PBR/specular-
    # reflection behavior on real models. Cookie textures must be
    # assigned to the lightingmanager BEFORE its gpuInit, hence the
    # spotlight setup goes here, not in _onUpdate.
    ###################################
    if use_spotlight:
      from types import SimpleNamespace
      lmgr = self.scene.lightingmanager
      sgc_shim = SimpleNamespace(scenegraph=self.scene, layer_fwd=self.layer_fwd)
      spotlight_marker = XgmModel("data://tests/pbr_calib.glb")

      COOKIE_DIM = 2048
      color_cookies = TextureArray(w=COOKIE_DIM, h=COOKIE_DIM, slices=4,
                                   fmt=tokens.RGB8, mipmapped=True)
      depth_cookies = TextureArray(w=COOKIE_DIM, h=COOKIE_DIM, slices=4,
                                   fmt=tokens.Z32F, mipmapped=True)
      color_cookies.needsRadianceCache = False
      cookie1 = color_cookies.load("src://effect_textures/knob2.png")
      ctx.TXI.updateTextureArray(color_cookies)
      depth_cookie1 = depth_cookies.slice(0)

      # Initial color is whatever the L / Shift-L cycle resolves to right
      # now (defaults: brightness 1.0 unless overridden by --spotlight,
      # color "warm"). Cycles can re-set at runtime via L / Shift-L.
      _initial_color = (self.spot_colors[self.spot_color_idx][1]
                        * self.spot_base
                        * self.spot_levels[self.spot_level_idx])
      self.spotlight1 = StdSpotLight(
        index=0,
        SGC=sgc_shim,
        model=spotlight_marker,
        frq=0.17,
        color=_initial_color,
        cookie=cookie1,
        depth_cookie=depth_cookie1,
        dim=COOKIE_DIM,
        radius=24,
        voffset=10,
        fovbase=45)

      lmgr.spot_cookies_color = color_cookies
      lmgr.spot_cookies_depth = depth_cookies

    self.scene.lightingmanager.gpuInit(ctx)

    # HUD keybinding legend (scale font with SSAA since it renders in pre-resolve RT)
    hud_fonts = {0: "i24", 1: "i36", 2: "i48", 3: "i48", 4: "i48"}
    hud_scale = max(ssaa, 1)
    self._hud_drawable = StringDrawableData()
    self._hud_drawable.pos2D = vec2(10 * hud_scale, 20 * hud_scale)
    self._hud_drawable.color = vec4(0, 0, 0, 1)
    self._hud_drawable.font = hud_fonts.get(ssaa, "i48")
    self._hud_node = self.layer_fwd.createDrawableNodeFromData("hud_keys", self._hud_drawable)
    self._hud_node.sortkey = 2000
    self._update_hud()

  ##############################################

  def _update_hud(self):
    brdf = self.brdfset[self.curbrdfi][0]
    sat = self.satset[self.cursati]
    gam = self.gamset[self.curgami]
    exp = self.exposure_values[self.cur_exposure_idx]
    ssao_str = "ON" if self.ssaamode else "OFF"
    sky_idx = self.skybox_index
    sky_str = (self.envmap_names[sky_idx]
               if sky_idx >= 0 and sky_idx < len(self.envmap_names)
               else "default")
    exp_str = f"{exp:.2f}" if exp > 0 else "OFF"
    model_str = (shortname_list[self.model_index]
                 if 0 <= self.model_index < len(shortname_list)
                 else "(custom)")
    spot_lvl  = self.spot_levels[self.spot_level_idx]
    spot_clr  = self.spot_colors[self.spot_color_idx][0]
    self._hud_drawable.text = (
      f"[A] SSAO: {ssao_str}\n"
      f"[B] BRDF: {brdf}\n"
      f"[E] Envmap: {sky_str}\n"
      f"[M] Model: {model_str}\n"
      f"[L] Spot brightness: {spot_lvl:g}    [Shift-L] Color: {spot_clr}\n"
      f"[S] Saturation: {sat:.1f}\n"
      f"[G] Gamma: {gam:.1f}\n"
      f"[T] ACES Exposure: {exp_str}\n"
      f"[R] Reset All"
    )

  ##############################################

  def _apply_spot_color(self):
    """Push the current (level, color) cycle state to the spot's
    DynamicSpotLight color. No-op if --spotlight wasn't enabled."""
    if not hasattr(self, "spotlight1"):
      return
    level     = self.spot_levels[self.spot_level_idx]
    color_unit = self.spot_colors[self.spot_color_idx][1]
    self.spotlight1.spot_light.data.color = color_unit * self.spot_base * level

  ##############################################

  def _swap_model_to_index(self, idx):
    """Detach the current model node from layer_fwd, load the model at
    `idx` in shortname_list, and create a fresh sgnode. Wraps if idx is
    out of range. No-op if shortname_list is empty."""
    if not shortname_list:
      print("M: no models in shortname_list (build_model_shortname_map empty)")
      return
    self.model_index = idx % len(shortname_list)
    new_short = shortname_list[self.model_index]
    new_path  = shortname_map[new_short]
    if hasattr(self, "sgnode") and self.sgnode is not None:
      self.layer_fwd.removeDrawableNode(self.sgnode)
    self.model  = XgmModel(new_path)
    self.sgnode = self.model.createNode("node", self.layer_fwd)
    self.model.debugRenderingModel = tokens.NONE
    self.model.debugPassID    = tokens.ALL
    self.model.debugSubPassID = tokens.ALL
    print(f"MODEL [{self.model_index+1}/{len(shortname_list)}] {new_short} -> {new_path}")

  ##############################################

  def _onViewportEvent(self,uievent):
    res = ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      ######################
      if uievent.keycode == ord("A"):
        self.ssaamode = not self.ssaamode
      ######################
      elif uievent.keycode == ord("B"):
        self.curbrdfi = (self.curbrdfi + 1) % len(self.brdfset)
        self.pbr_common.setBRDF(self.brdfset[self.curbrdfi][1])
      ######################
      elif uievent.keycode == ord("E"):
        # Cycle envmaps discovered from <staging>/assetcache/envmaps2/.
        # Path expansion (<assetcache>/...) is handled in the C++
        # requestRadianceMapsAsync (pbr_common.cpp:_expandIfNeeded), so we
        # can pass the placeholder-bearing path directly.
        if len(self.envmap_paths) == 0:
          print("E: no envmaps found in <staging>/assetcache/envmaps2/")
        else:
          self.skybox_index = (self.skybox_index + 1) % len(self.envmap_paths)
          path_str = self.envmap_paths[self.skybox_index]
          if path_str in self.skybox_cache:
            skybox = self.skybox_cache[path_str]
          else:
            skybox = PbrCommon.requestRadianceMapsAsync(path_str)
            self.skybox_cache[path_str] = skybox
          self.pbr_common.RadianceMaps = skybox
          print("ENVMAP", self.envmap_names[self.skybox_index])
      ######################
      elif uievent.keycode == ord("M"):
        # Cycle to the next model in the auto-discovered shortname list.
        self._swap_model_to_index(self.model_index + 1)
      ######################
      elif uievent.keycode == ord("L"):
        # L: cycle spotlight brightness preset.
        # Shift-L: cycle spotlight color preset.
        if uievent.shift:
          self.spot_color_idx = (self.spot_color_idx + 1) % len(self.spot_colors)
          print("SPOT COLOR", self.spot_colors[self.spot_color_idx][0])
        else:
          self.spot_level_idx = (self.spot_level_idx + 1) % len(self.spot_levels)
          print("SPOT BRIGHTNESS", self.spot_levels[self.spot_level_idx])
        self._apply_spot_color()
      ######################
      elif uievent.keycode == ord("S"):
        self.cursati = (self.cursati + 1) % len(self.satset)
        self.post_node.saturation = self.satset[self.cursati]
      ######################
      elif uievent.keycode == ord("T"):
        self.cur_exposure_idx = (self.cur_exposure_idx + 1) % len(self.exposure_values)
        self.aces_node.exposure = self.exposure_values[self.cur_exposure_idx]
      ######################
      elif uievent.keycode == ord("G"):
        self.curgami = (self.curgami + 1) % len(self.gamset)
        self.post_node.gamma = self.gamset[self.curgami]
      ######################
      elif uievent.keycode == ord("R"):
        self.ssaamode = False
        self.curbrdfi = 0
        self.pbr_common.setBRDF(self.brdfset[0][1])
        self.cursati = self.satset.index(0.8)   # default 0.8
        self.post_node.saturation = self.satset[self.cursati]
        self.curgami = self.gamset.index(0.8)   # default 0.8
        self.post_node.gamma = self.gamset[self.curgami]
        self.cur_exposure_idx = 0  # OFF
        self.aces_node.exposure = 0.0
      ######################
      self._update_hud()
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom( self.uicam.cameradata )
    return res

  ################################################

  def _onUpdate(self,updinfo):

    if self.ssaamode:
      self.pbr_common.ssaoNumSamples = ssao
    else:
      self.pbr_common.ssaoNumSamples = 0
    if hasattr(self, "spotlight1"):
      self.spotlight1.update(updinfo.absolutetime)
    self.camera.copyFrom(self.uicam.cameradata)
    self.scene.updateScene(self.cameralut)
    self.sgviewport_item.widget.setDirty()

###############################################################################

print("XXXX")
app = SceneGraphApp()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
