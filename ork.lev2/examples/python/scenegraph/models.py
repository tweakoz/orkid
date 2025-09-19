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
parser.add_argument('--seed', type=int, default=57, help='random seed' )

args = vars(parser.parse_args())
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
    ART = MISC_GLTF/"art_and_sculpture"
    CHARS = MISC_GLTF/"characters"

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
    DHELM = ART/"dragon_helm.glb"        # dragon helm
    FRACVASE = ART/"fracvase.glb"        # fractal vase
    TEAPOT = ART/"gothic_teapot.glb"     # gothic teapot
    WARHORN = ART/"warhorn.glb"          # war horn

    models = []
    models += [WARHORN]
    models += [LION]
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
    print(models)

    numinstances = len(models)
    models = [lev2.XgmModel(str(m)) for m in models]
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
