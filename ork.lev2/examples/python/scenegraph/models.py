#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
################################################################################

import math, random, argparse, sys, time
from ork import path as ork_path
from orkengine.core import vec2, vec3, vec4, quat, mtx4, VarMap, CrcStringProxy, Path as asset_path, lev2_pyexdir
from orkengine import lev2
from orkengine.lev2 import PostFxNodeHSVG
from ork.app.application import ComponentizedApplication
from ork.app.frame_profiler import FrameProfilerComponent

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCameraX

tokens = CrcStringProxy()

################################################################################
# tweak sys path
################################################################################

sys.path.append(str(ork_path.py_examples)) # add parent dir to path
from scenegraph._sg_boilerplate import SpinningModelInst, TurntableModelInst

################################################################################
# command line args
################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--seed', type=int, default=57, help='random seed' )

args = vars(parser.parse_args())
seed = args["seed"]
random.seed(seed)

################################################################################

class SceneGraphApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.profiler = self.addComponent("profiler", FrameProfilerComponent)
    self.materials = set()
    self.modelinsts = []

    self.skybox_names = [
      "ork_envmaps|pillars4k",
      "ork_envmaps|cold4k",
      "ork_envmaps|ocean4k",
      "ork_envmaps|arena4k",
      "ork_envmaps|club4k",
      "ork_envmaps|desert4k",
      "ork_envmaps|canyon4k",
      "ork_envmaps|crossroads4k",
      "ork_envmaps|futcity4k",
      "ork_envmaps|ethereal4k",
      "ork_envmaps|tozenv_nebula",
      "ork_envmaps|tozenv_hellscape",
      "ork_envmaps|blender_studio",
      "ork_envmaps|blender_interior",
      "ork_envmaps|blender_courtyard",
      "ork_envmaps|blender_city",
      "ork_envmaps|blender_sunrise",
      "ork_envmaps|blender_sunset",
      "ork_envmaps|blender_night",
      "ork_envmaps|blender_forest",
    ]
    self.skybox_cache = dict()
    self.skybox_index = -1

    self.createEzApp(fullscreen=True, ssaa=0)

  ##############################################

  def _onUiInit(self):
    lg = self.ezapp.topLayoutGroup
    self.sgviewport_item = lg.makeChild(
      uiclass=lev2.ui.SceneGraphViewport,
      args=["PrimarySG"],
      fill=True
    )

  ##############################################

  def _onGpuInit(self, ctx):

    ####################################
    # scene
    ####################################

    sceneparams = VarMap()
    sceneparams.preset = "ForwardPBR"
    sceneparams.SkyboxIntensity = 1.0
    sceneparams.SpecularIntensity = 1.0
    sceneparams.DiffuseIntensity = 1.0
    sceneparams.AmbientLight = vec3(0.0)
    sceneparams.DepthFogDistance = float(1e6)
    sceneparams.UseFloatBuffer = True
    sceneparams.SkyboxTexPathStr = "nebula"

    postNode = PostFxNodeHSVG()
    postNode.hue = 0.0
    postNode.saturation = 0.85
    postNode.value = 1.0
    postNode.gamma = 1.2
    postNode.gpuInit(ctx, 8, 8)
    postNode.addToSceneVars(sceneparams, "PostFxChain")
    self.post_node = postNode

    self.scene = lev2.scenegraph.Scene(sceneparams)
    self.layer_fwd = self.scene.createLayer("std_forward")
    self.pbr_common = self.scene.pbr_common

    ####################################
    # camera
    ####################################

    self.cameralut = lev2.CameraDataLut()
    self.camera, self.uicam = setupUiCameraX(
      cameralut=self.cameralut,
      camname="Camera0"
    )
    self.uicam.lookAt(vec3(0, 3.5, -3.5), vec3(0, 0, 0), vec3(0, 1, 0))

    ####################################
    # attach to viewport
    ####################################

    sgviewport = self.sgviewport_item.widget
    sgviewport.cameraName = "Camera0"
    sgviewport.scenegraph = self.scene
    sgviewport.forkDB()
    sgviewport.evhandler = lambda e: self._onViewportEvent(e)
    sgviewport.ignoreEvents = False

    ####################################
    # model assets
    ####################################

    TESTS = asset_path("data://tests")
    MISC_GLTF = TESTS/"misc_gltf_samples"
    BASEOBJS = asset_path("src://environ/objects")
    ART = MISC_GLTF/"art_and_sculpture"
    CHARS = MISC_GLTF/"characters"
    VEHI = MISC_GLTF/"vehicles"
    PLANTS = MISC_GLTF/"plants"

    models = [
      CHARS/"scarlett.glb",
      CHARS/"knight1.glb",
      CHARS/"titan.glb",
      CHARS/"enchantress1.glb",
      CHARS/"goblin1.glb",
      ART/"warhorn.glb",
      ART/"lion.glb",
      ART/"bear.glb",
      ART/"omask.glb",
      ART/"obox.glb",
      ART/"sitter.glb",
      ART/"fracvase.glb",
      ART/"dragon_helm.glb",
      ART/"orchid1.glb",
      ART/"gothic_teapot.glb",
      VEHI/"car.glb",
    ]
    models2 = [
      PLANTS/"plant1.glb",
      PLANTS/"plant2.glb",
      PLANTS/"plant3.glb",
      PLANTS/"plant4.glb",
      PLANTS/"plant5.glb",
    ]
    models3 = [MISC_GLTF/"DamagedHelmet.glb"]

    ####################################
    # load models
    ####################################

    models = [lev2.XgmModel(str(m)) for m in models]
    models2 = [lev2.XgmModel(str(m)) for m in models2]
    models3 = [lev2.XgmModel(str(m)) for m in models3]

    ####################################
    # create scenegraph nodes
    ####################################

    fi = 0.0
    for i in range(len(models)):
      model = models[i % len(models)]
      minst = TurntableModelInst(model, self.layer_fwd, i, fi, range=3.5)
      self.modelinsts.append(minst)
      fi += (1.0 / len(models)) * math.pi * 2.0

    fi = 0.0
    for i in range(len(models2)):
      model = models2[i % len(models2)]
      minst = TurntableModelInst(model, self.layer_fwd, i, fi, range=1.5)
      self.modelinsts.append(minst)
      fi += (1.0 / len(models2)) * math.pi * 2.0

    fi = 0.0
    for i in range(len(models3)):
      model = models3[i % len(models3)]
      minst = TurntableModelInst(model, self.layer_fwd, i, fi, range=0)
      self.modelinsts.append(minst)
      fi += (1.0 / len(models3)) * math.pi * 2.0

    self.scene.lightingmanager.gpuInit(ctx)

  ##############################################

  def _onViewportEvent(self, uievent):
    res = lev2.ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      if uievent.keycode == ord("S"):
        self.skybox_index = (self.skybox_index + 1) % len(self.skybox_names)
        skybox_name = self.skybox_names[self.skybox_index]
        if skybox_name in self.skybox_cache:
          skybox = self.skybox_cache[skybox_name]
        else:
          skybox = lev2.PbrCommon.requestRadianceMapsAsync(skybox_name)
          self.skybox_cache[skybox_name] = skybox
        self.pbr_common.RadianceMaps = skybox
        return res
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.uicam.updateMatrices()
      self.camera.copyFrom(self.uicam.cameradata)
    return res

  ##############################################

  def _onUpdate(self, updinfo):
    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)
    self.camera.copyFrom(self.uicam.cameradata)
    self.scene.updateScene(self.cameralut)
    self.sgviewport_item.widget.setDirty()

###############################################################################

app = SceneGraphApp()
app.ezapp.mainThreadLoop()
app.ezapp.shutdown()
