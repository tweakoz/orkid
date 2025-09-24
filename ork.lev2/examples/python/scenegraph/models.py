#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
################################################################################

import math, random, argparse, sys, threading
from ork import path as ork_path
from orkengine.core import vec2, vec3, vec4, quat, mtx4, CrcStringProxy, Path as asset_path
from orkengine import lev2

tokens = CrcStringProxy()
################################################################################
# tweak sys path
################################################################################

sys.path.append(str(ork_path.py_examples)) # add parent dir to path
from scenegraph._sg_boilerplate import SpinningModelInst, TurntableModelInst, BoilerplateSgApp
from lev2utils.primitives import createGridData

################################################################################
# command line args
################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument('--seed', type=int, default=57, help='random seed' )

args = vars(parser.parse_args())
seed = args["seed"]
random.seed(seed)

################################################################################

class SceneGraphApp(BoilerplateSgApp):

  def __init__(self):
    super().__init__(fullscreen=True,ssaa=0)

    ####################################
    # builtin skybox list
    ####################################

    self.skybox_names = [
      "ork_envmaps|pillars4k",        # pillars of creation (sharp)
      "ork_envmaps|cold4k",           # ice planet (bright, soft)
      "ork_envmaps|ocean4k",          # ocean planet (soft)     
      "ork_envmaps|arena4k",          # the grid  (dark)
      "ork_envmaps|club4k",           # gothic club (dark)
      "ork_envmaps|desert4k",         # desert planet (bright)
      "ork_envmaps|canyon4k",         # big canyon (bright)
      "ork_envmaps|crossroads4k",     # the crossroads (bright)
      "ork_envmaps|futcity4k",        # futuristic city (moderately dark)
      "ork_envmaps|ethereal4k",       # ethereal plane (medium)
      "ork_envmaps|tozenv_nebula",    # (purple, soft)
      "ork_envmaps|tozenv_hellscape", # (red, sharp)
      "ork_envmaps|blender_studio",   # blender studio (hard shadows)
      "ork_envmaps|blender_interior", # blender interior (soft)
      "ork_envmaps|blender_courtyard",# blender courtyard (soft)
      "ork_envmaps|blender_city",     # blender city (hard)
      "ork_envmaps|blender_sunrise",  # sunrise (soft)
      "ork_envmaps|blender_sunset",   # sunset (soft)
      "ork_envmaps|blender_night",    # night (hard)
      "ork_envmaps|blender_forest",   # forest (soft)
    ]
    self.skybox_cache = dict()
    self.skybox = "cold8k"     # gothic club (dark)
    self.skybox_intensity = 1.0 # skybox intensity multiplier
    self.skybox_index = -1
  ##############################################

  def onGpuInit(self,ctx):

    super().onGpuInit(ctx)

    ####################################
    # folders which contain models
    ####################################

    TESTS = asset_path("data://tests")
    MISC_GLTF = TESTS/"misc_gltf_samples"
    BASEOBJS = asset_path("src://environ/objects")
    ART = MISC_GLTF/"art_and_sculpture"
    CHARS = MISC_GLTF/"characters"
    VEHI = MISC_GLTF/"vehicles"
    PLANTS = MISC_GLTF/"plants"
    
    ####################################
    # model assets
    ####################################

    SPIKEE = TESTS/"pbr1"/"pbr1"               # coronavirus looking thing
    PBRCALIB = TESTS/"pbr_calib.glb"           # pbr calibration ball
    TORUS = BASEOBJS/"misc"/"ref"/"torus.glb"  # generic torus
    SCARLETT = CHARS/"scarlett.glb"            # anime girl
    KNIGHT = CHARS/"knight1.glb"               # anime girl
    TITAN = CHARS/"titan.glb"                  # anime girl
    ENCH = CHARS/"enchantress1.glb"            # anime girl
    GOBL = CHARS/"goblin1.glb"            # anime girl
    SITTER = ART/"sitter.glb"            # sitting figure
    ORCHID = ART/"orchid1.glb"           # fractal vase
    OMASK = ART/"omask.glb"              # oni mask
    OBOX = ART/"obox.glb"                # ancient box
    LION = ART/"lion.glb"                # lion statue
    BEAR = ART/"bear.glb"                 # war horn
    DHELM = ART/"dragon_helm.glb"        # dragon helm
    FRACVASE = ART/"fracvase.glb"        # fractal vase
    TEAPOT = ART/"gothic_teapot.glb"     # gothic teapot
    WARHORN = ART/"warhorn.glb"          # war horn
    CAR = VEHI/"car.glb"          # war horn
    PLANT1 = PLANTS/"plant1.glb"          # plant
    PLANT2 = PLANTS/"plant2.glb"          # plant
    PLANT3 = PLANTS/"plant3.glb"          # plant
    PLANT4 = PLANTS/"plant4.glb"          # plant
    PLANT5 = PLANTS/"plant5.glb"          # plant
    
    HELMET = MISC_GLTF/"DamagedHelmet.glb"    # knight helmet

    models = []
    models += [WARHORN]
    models += [LION]
    models += [BEAR]
    models += [OMASK]
    models += [OBOX]
    models += [SITTER]
    models += [FRACVASE]
    models += [DHELM]
    models += [SCARLETT]
    models += [KNIGHT]
    models += [ENCH]
    models += [TITAN]
    models += [GOBL]
    models += [ORCHID]
    models += [TEAPOT]
    models += [CAR]

    models2  = [PLANT1]
    models2 += [PLANT2]
    models2 += [PLANT3]
    models2 += [PLANT4]
    models2 += [PLANT5]

    models3  = [HELMET]

    ####################################
    # load models
    ####################################

    numinstances = len(models)
    numinstances2 = len(models2)
    numinstances3 = len(models3)
    models = [lev2.XgmModel(str(m)) for m in models]
    models2 = [lev2.XgmModel(str(m)) for m in models2]
    models3 = [lev2.XgmModel(str(m)) for m in models3]

    ###################################
    # create scenegraph nodes
    ###################################

    fi = 0.0
    for i in range(numinstances):
      model = models[i%len(models)]
      minst = TurntableModelInst(model,self.layer_fwd,i,fi,range=3.5)
      self.modelinsts += [minst]
      fi += (1.0/numinstances)*math.pi*2.0

    fi = 0.0
    for i in range(numinstances2):
      model = models2[i%len(models2)]
      minst = TurntableModelInst(model,self.layer_fwd,i,fi,range=1.5)
      self.modelinsts += [minst]
      fi += (1.0/numinstances2)*math.pi*2.0

    fi = 0.0
    for i in range(numinstances3):
      model = models3[i%len(models3)]
      minst = TurntableModelInst(model,self.layer_fwd,i,fi,range=0)
      self.modelinsts += [minst]
      fi += (1.0/numinstances3)*math.pi*2.0

    ###################################

    #self.grid_data = createGridData()
    #self.grid_node = self.layer_fwd.createDrawableNodeFromData("grid",self.grid_data)
    #self.grid_node.sortkey = 1

  ################################################

  def onUpdate(self,updinfo):
    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)
    super().onUpdate(updinfo)

  ##############################################

  def onUiEvent(self,uievent):
    res = lev2.ui.HandlerResult()
    if uievent.code == tokens.KEY_DOWN.hashed:
      #######################
      # load a new skybox
      #######################
      if uievent.keycode == ord("S"):
        self.skybox_index = (self.skybox_index+1)%len(self.skybox_names)
        skybox_name = self.skybox_names[self.skybox_index]
        #####################################
        if skybox_name in self.skybox_cache:
          self.skybox = self.skybox_cache[skybox_name]
        else:
          self.skybox = lev2.PbrCommon.requestRadianceMapsAsync(skybox_name)
          self.skybox_cache[skybox_name] = self.skybox
        #####################################
        self.pbr_common.RadianceMaps = self.skybox
        return res
      #######################
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    else:
      handled = lev2.ui.HandlerResult()
    return res

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
