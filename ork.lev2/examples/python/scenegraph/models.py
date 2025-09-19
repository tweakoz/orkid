#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
################################################################################

import math, random, argparse, sys
from ork import path as ork_path
from orkengine.core import vec2, vec3, vec4, quat, mtx4, Path as asset_path
from orkengine import lev2


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
parser.add_argument('--numinstances', metavar="numinstances", type=int, default=10, help='number of mesh instances' )
parser.add_argument('--seed', type=int, default=57, help='random seed' )

args = vars(parser.parse_args())
numinstances = args["numinstances"]
seed = args["seed"]
random.seed(seed)

################################################################################

class SceneGraphApp(BoilerplateSgApp):

  def __init__(self):
    super().__init__()
    #self.skybox = "nebula"     # (purple, soft)
    #self.skybox = "pillars8k"  # pillars of creation (sharp)
    self.skybox = "cold8k"     # ice planet (bright, soft)
    #self.skybox = "ocean8k"     # ocean planet (soft)
    #self.skybox = "arena8k"    # the grid  (dark)
    #self.skybox = "club8k"     # gothic club (dark)
    #self.skybox = "desert8k"   # desert planet (bright)
    self.skybox_intensity = 1.0 # skybox intensity multiplier
  ##############################################

  def onGpuInit(self,ctx):

    super().onGpuInit(ctx)

    TESTS = asset_path("data://tests")
    MISC_GLTF = TESTS/"misc_gltf_samples"
    BASEOBJS = asset_path("src://environ/objects")

    SPIKEE = TESTS/"pbr1"/"pbr1"               # coronavirus looking thing
    PBRCALIB = TESTS/"pbr_calib.glb"           # pbr calibration ball
    TORUS = BASEOBJS/"misc"/"ref"/"torus.glb"  # generic torus
    SITTER = MISC_GLTF/"sitter.glb"            # sitting figure
    SCARLETT = MISC_GLTF/"scarlett.glb"        # anime girl
    OMASK = MISC_GLTF/"omask.glb"              # oni mask
    OBOX = MISC_GLTF/"obox.glb"                # ancient box
    LION = MISC_GLTF/"lion.glb"                # lion statue
    DHELM = MISC_GLTF/"dragon_helm.glb"        # dragon helm
    FRACVASE = MISC_GLTF/"fracvase.glb"        # fractal vase
    ORCHID = MISC_GLTF/"orchid1.glb"           # fractal vase
    TEAPOT = MISC_GLTF/"gothic_teapot.glb"     # gothic teapot
    WARHORN = MISC_GLTF/"warhorn.glb"          # war horn

    models = []
    models += [lev2.XgmModel(WARHORN)]
    models += [lev2.XgmModel(LION)]
    models += [lev2.XgmModel(OMASK)]
    models += [lev2.XgmModel(OBOX)]
    models += [lev2.XgmModel(SITTER)]
    models += [lev2.XgmModel(FRACVASE)]
    models += [lev2.XgmModel(DHELM)]
    models += [lev2.XgmModel(SCARLETT)]
    models += [lev2.XgmModel(ORCHID)]
    models += [lev2.XgmModel(TEAPOT)]

    ###################################

    fi = 0.0
    for i in range(numinstances):
      model = models[i%len(models)]
      minst = TurntableModelInst(model,self.layer_fwd,i,fi)
      self.modelinsts += [minst]
      fi += (1.0/numinstances)*math.pi*2.0

    ###################################

    #self.grid_data = createGridData()
    #self.grid_node = self.layer_fwd.createDrawableNodeFromData("grid",self.grid_data)
    #self.grid_node.sortkey = 1

  ################################################

  def onUpdate(self,updinfo):
    for minst in self.modelinsts:
      minst.update(updinfo.deltatime)
    super().onUpdate(updinfo)

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
