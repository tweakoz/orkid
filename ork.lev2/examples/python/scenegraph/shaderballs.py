#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################

import math, random, argparse, sys, signal, colorsys, os, glob
from orkengine.core import vec2, vec3, vec4, quat, mtx4
from orkengine.core import dfrustum, dvec4, fmtx4_to_dmtx4
from orkengine.core import lev2_pyexdir, Transform
from orkengine.core import CrcStringProxy, thisdir, VarMap
from orkengine import lev2
from ork.app.application import ComponentizedApplication
from ork.app.std_scenegraph import StandardSceneGraphComponent, StdSpotLight
from ork.app.loggerui import LoggerUIComponent

tokens = CrcStringProxy()

################################################################################

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph
from lev2utils.lighting import MySpotLight, MyCookie

SSAO_NUM_SAMPLES = 16


################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-i", "--intensity", type=float, default=1.5, help='envmap intensity')
parser.add_argument("-t", "--ssaa", type=int, default=2, help='ssaa (super-sample anti-aliasing factor)')


################################################################################

args = vars(parser.parse_args())
envmap = args["envmap"]
inten = args["intensity"]
ssaa = args["ssaa"]

################################################################################

class NODE(object):

  def __init__(self,model,app, index):

    super().__init__()
    SGC = app.SGC
    SG = SGC.scenegraph
    self.model = model
    self.drawable_model = model.createDrawable()
    self.modelinst = self.drawable_model.modelinst
    self.sgnode = SG.createDrawableNodeOnLayers(SGC.fwd_layers,"model-node-%d"%index,self.drawable_model)
    self.sgnode.worldTransform.scale = 1
    self.sgnode.worldTransform.translation = vec3(0)
    #self.sgnode = model.createNode("node%d"%index,layer)

################################################################################

class SceneGraphApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.materials = set()
    self.nodes=[]
    self.ssaomode = False

    ###############################################
    # Discover envmaps from <staging>/assetcache/envmaps2/. Only this
    # directory is consulted (per request); the asset registry's
    # ork_envmaps|... entries are intentionally not used here.
    ###############################################
    stage_dir = os.environ.get("OBT_STAGE", "")
    envmap_glob = os.path.join(stage_dir, "assetcache", "envmaps2", "*.xir")
    envmap_files = sorted(glob.glob(envmap_glob))
    self.envmap_names = [os.path.splitext(os.path.basename(f))[0] for f in envmap_files]
    # asset-style paths (resolved by orkid's asset system at load time)
    self.envmap_paths = [f"<assetcache>/envmaps2/{n}.xir" for n in self.envmap_names]
    self.envmap_cache = dict()
    # initial: --envmap arg takes precedence, otherwise blender_studio if present
    initial_envmap = "blender_studio"
    self.envmap_index = -1
    for i, n in enumerate(self.envmap_names):
      if n == initial_envmap:
        self.envmap_index = i
        break
    initial_skybox = envmap if envmap != "" else (
      self.envmap_paths[self.envmap_index] if self.envmap_index >= 0
      else "<assetcache>/envmaps2/blender_studio.xir")

    ###############################################
    # ColorPicker / cycling state for the keyboard handler.
    ###############################################
    self.color_seed = 12  # initial seed; spacebar increments + re-rolls
    self.brdfset = [("GGX",      tokens.GGX),
                    ("VELVET",   tokens.GGXVELVET),
                    ("GGXRIM",   tokens.GGXRIM),
                    ("BLINN",    tokens.BLINN),
                    ("PHONG",    tokens.PHONG)]
    self.satset  = [0.0, 0.1, 0.2, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0]
    self.gamset  = [0.8, 1.0, 1.2, 1.4, 1.6, 1.8, 2.0, 2.4]
    self.expset  = [0.0, 0.25, 0.5, 0.75, 1.0, 1.5, 2.0, 3.0, 5.0]
    self.brdf_idx = 0
    self.sat_idx  = 5  # 1.0
    self.gam_idx  = 1  # 1.0
    self.exp_idx  = 0  # OFF

    ###############################################
    # Post-fx chain: ACES exposure + HSVG (hue/sat/value/gamma).
    # SGC accepts `post_nodes=[]` and handles gpuInit + addToSceneVars
    # for them automatically (see ork/app/std_scenegraph.py).
    ###############################################
    self.aces_node = lev2.PostFxNodeACES()
    self.aces_node.exposure = self.expset[self.exp_idx]
    self.post_node = lev2.PostFxNodeHSVG()
    self.post_node.hue        = 0.0
    self.post_node.saturation = self.satset[self.sat_idx]
    self.post_node.value      = 1.0
    self.post_node.gamma      = self.gamset[self.gam_idx]

    params_dict = {
      "ssaa": ssaa,                # consumed by scenegraph.cpp:432 (compositor RT sizing)
      "SkyboxIntensity": float(inten),
      "SpecularIntensity": float(1),
      "DiffuseIntensity": float(1),
      "AmbientLight": vec3(0),
      "DepthFogDistance": float(10000),
      "SSAONumSamples": int(SSAO_NUM_SAMPLES),
      "SSAONumSteps": 2,
      "SSAOBias": 0.005,
      "SSAORadius": 0.05, # 2 inches
      "SSAOWeight": 0.25,
      "SSAOPower": 0.125,
      "SSAOFeedback": 1.0/16.0,
      "SkyboxTexPathStr": initial_skybox
    }
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 enable_ui_camera=True,
                                 eye=vec3(0,20,20),
                                 sg_params=params_dict,
                                 post_nodes=[self.aces_node, self.post_node])
    self.createEzApp(name="ShaderBalls", ssaa=ssaa,
                      use_subsystems=['opq', 'core', 'gpu', 'lev2'])

  ##############################################

  def _onGpuInit(self,ctx):
    SGC = self.SGC
    SG = SGC.scenegraph

    SGC.grid_data.extent = 10

    PBRC = SG.pbr_common
    PBRC.useFloatColorBuffer = True
    PBRC.useDepthPrepass = True
    PBRC.dppZBias = 1.0e-4

    ###################################

    model = lev2.XgmModel("data://tests/pbr_calib.glb")



    random.seed(12)
    white = lev2.Image.createFromFile("src://effect_textures/white_64.dds")
    normal = lev2.Image.createFromFile("src://effect_textures/default_normal.dds")
    for mesh in model.meshes:
      for submesh in mesh.submeshes:
        copy = submesh.material.clone()
        copy.assignImages(
          ctx,
          color = white,
          normal = normal,
          mtlruf = white,
          doConform=True
        )
        submesh.material = copy

    # Build the 81 balls. Material is cloned per-ball, metallic/roughness
    # set from grid position, baseColor set in regenColors() so it can be
    # re-rolled with the spacebar. Note: `self.nodes += [node]` was
    # previously OUTSIDE this loop and only retained the last ball — fixed
    # so all 81 are indexable for regenColors.
    for i in range(81):
      node = NODE(model,self,i)

      x = (i % 9)
      z = int(i/9)

      ######################
      # set transform
      ######################

      node.sgnode.worldTransform.translation = vec3((x-4)*2,1,(z-4)*2)

      ######################
      # override material for submeshinst (metallic/roughness frozen,
      # color filled by regenColors)
      ######################

      subinst = node.modelinst.submeshinsts[0]
      mtl_cloned = subinst.material.clone()
      mtl_cloned.metallicFactor = float(x/8.0)
      mtl_cloned.roughnessFactor = float(z/8.0)
      subinst.overrideMaterial(mtl_cloned)

      self.nodes += [node]

    self.regenColors()

    lmgr = SG.lightingmanager
    COOKIE_DIM = 2048
    color_cookies = lev2.TextureArray(w=COOKIE_DIM,h=COOKIE_DIM,slices=4,fmt=tokens.RGB8,mipmapped=True)
    depth_cookies = lev2.TextureArray(w=COOKIE_DIM,h=COOKIE_DIM,slices=4,fmt=tokens.Z32F,mipmapped=True)
    color_cookies.needsRadianceCache = False

    cookie1 = color_cookies.load("src://effect_textures/knob2.png")
    ctx.TXI.updateTextureArray(color_cookies)
    depth_cookie1 = depth_cookies.slice(0)

    self.spotlight1 = StdSpotLight( index=0,
                                    SGC=SGC,
                                    model=model,
                                    frq=0.17,
                                    color=vec3(1000,800,500)*10.0,
                                    cookie=cookie1,
                                    depth_cookie=depth_cookie1, 
                                    dim=COOKIE_DIM,
                                    radius=24,
                                    voffset=10,
                                    fovbase=45)

    lmgr.spot_cookies_color = color_cookies
    lmgr.spot_cookies_depth = depth_cookies

    lmgr.gpuInit(ctx)

    ###############################################
    # HUD legend at top-left. Modeled on ork.modelviewer.py — font scales
    # with SSAA factor since the HUD renders into the pre-resolve RT.
    ###############################################
    hud_fonts = {0: "i16", 1: "i24", 2: "i32", 3: "i40", 4: "i48"}
    hud_scale = max(ssaa, 1)
    self._hud_drawable = lev2.StringDrawableData()
    self._hud_drawable.pos2D = vec2(10 * hud_scale, 20 * hud_scale)
    self._hud_drawable.color = vec4(0, 0, 0, 1)
    self._hud_drawable.font = hud_fonts.get(ssaa, "i48")
    self._hud_node = self.SGC.layer_fwd.createDrawableNodeFromData("hud_keys", self._hud_drawable)
    self._hud_node.sortkey = 2000
    self._update_hud()

  ################################################

  def _update_hud(self):
    brdf = self.brdfset[self.brdf_idx][0]
    sat  = self.satset[self.sat_idx]
    gam  = self.gamset[self.gam_idx]
    exp  = self.expset[self.exp_idx]
    ssao_str = "ON" if self.ssaomode else "OFF"
    sky_str  = (self.envmap_names[self.envmap_index]
                if self.envmap_index >= 0 and len(self.envmap_names) > 0
                else "default")
    exp_str  = f"{exp:.2f}" if exp > 0 else "OFF"
    self._hud_drawable.text = (
      f"[A] SSAO: {ssao_str}\n"
      f"[B] BRDF: {brdf}\n"
      f"[E] Envmap: {sky_str}\n"
      f"[S] Saturation: {sat:.1f}\n"
      f"[G] Gamma: {gam:.1f}\n"
      f"[T] ACES Exposure: {exp_str}\n"
      f"[R] Reset All\n"
      f"[SPACE] Re-roll Colors"
    )

  ################################################

  def regenColors(self):
    """Re-roll baseColor (HSV) for every ball. metallic/roughness untouched.
    Spacebar invokes this with an incrementing seed for deterministic shuffles."""
    random.seed(self.color_seed)
    self.color_seed += 1
    for node in self.nodes:
      subinst = node.modelinst.submeshinsts[0]
      h = random.uniform(0,6)
      s = random.uniform(0,0.7)
      v = random.uniform(0.1,1)
      rgb = colorsys.hsv_to_rgb(h,s,v)
      subinst.material.baseColor = vec4(rgb[0], rgb[1], rgb[2], 1)

  ################################################

  def onUiEvent(self,uievent):
    pbrc = self.SGC.pbr_common
    res = lev2.ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      ##############################
      if uievent.keycode == ord(" "):                # SPACE: re-roll colors
        self.regenColors()
      ##############################
      elif uievent.keycode == ord("A"):              # A: toggle SSAO
        self.ssaomode = not self.ssaomode
        print("SSAO MODE", self.ssaomode)
      ##############################
      elif uievent.keycode == ord("B"):              # B: cycle BRDF
        self.brdf_idx = (self.brdf_idx + 1) % len(self.brdfset)
        pbrc.setBRDF(self.brdfset[self.brdf_idx][1])
        print("BRDF", self.brdfset[self.brdf_idx][0])
      ##############################
      elif uievent.keycode == ord("E"):              # E: cycle envmap (assetcache/envmaps2 only)
        if len(self.envmap_paths) == 0:
          print("E: no envmaps found in <staging>/assetcache/envmaps2/")
        else:
          self.envmap_index = (self.envmap_index + 1) % len(self.envmap_paths)
          path_str = self.envmap_paths[self.envmap_index]
          if path_str in self.envmap_cache:
            skybox = self.envmap_cache[path_str]
          else:
            skybox = lev2.PbrCommon.requestRadianceMapsAsync(path_str)
            self.envmap_cache[path_str] = skybox
          pbrc.RadianceMaps = skybox
          print("ENVMAP", self.envmap_names[self.envmap_index])
      ##############################
      elif uievent.keycode == ord("S"):              # S: cycle Saturation
        self.sat_idx = (self.sat_idx + 1) % len(self.satset)
        self.post_node.saturation = self.satset[self.sat_idx]
        print("SATURATION", self.satset[self.sat_idx])
      ##############################
      elif uievent.keycode == ord("T"):              # T: cycle ACES exposure
        self.exp_idx = (self.exp_idx + 1) % len(self.expset)
        self.aces_node.exposure = self.expset[self.exp_idx]
        exp = self.expset[self.exp_idx]
        print("ACES EXPOSURE", f"{exp:.2f}" if exp > 0 else "OFF")
      ##############################
      elif uievent.keycode == ord("G"):              # G: cycle Gamma
        self.gam_idx = (self.gam_idx + 1) % len(self.gamset)
        self.post_node.gamma = self.gamset[self.gam_idx]
        print("GAMMA", self.gamset[self.gam_idx])
      ##############################
      elif uievent.keycode == ord("R"):              # R: reset all
        self.ssaomode = False
        self.brdf_idx = 0
        pbrc.setBRDF(self.brdfset[0][1])
        self.sat_idx = 5  # 1.0
        self.post_node.saturation = self.satset[self.sat_idx]
        self.gam_idx = 1  # 1.0
        self.post_node.gamma = self.gamset[self.gam_idx]
        self.exp_idx = 0  # OFF
        self.aces_node.exposure = self.expset[self.exp_idx]
        print("RESET")
      ##############################
      elif uievent.keycode == ord("-"):              # - : roughness power down
        pbrc.roughnessPower *= 0.95
        print("ROUGHNESS POWER",pbrc.roughnessPower)
      ##############################
      elif uievent.keycode == ord("="):              # = : roughness power up
        pbrc.roughnessPower *= 1.05
        print("ROUGHNESS POWER",pbrc.roughnessPower)
      ##############################
      # Refresh the HUD after any state-changing key.
      self._update_hud()
    return res

  ################################################

  def _onGpuUpdate(self,ctx):
    if hasattr(self,"spotlight1"):
      self.spotlight1.update(self.lighttime)

  ################################################

  def _onUpdate(self,updinfo):
    pbrc = self.SGC.pbr_common
    if self.ssaomode == True:
      pbrc.ssaoNumSamples = SSAO_NUM_SAMPLES
    else:
      pbrc.ssaoNumSamples = 0
    self.lighttime = updinfo.absolutetime

###############################################################################

app = SceneGraphApp()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
