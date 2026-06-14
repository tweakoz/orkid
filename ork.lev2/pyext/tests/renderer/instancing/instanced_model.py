#!/usr/bin/env ork.python

################################################################################
# InstancedModelDrawable forward-path test (E2A2).
#
# The ONLY coverage of technique FWD_CT_NM_RI_IN_MO (instanced glb model) in
# the modern ForwardPBR pipeline — instancing.py covers the rigid-primitive
# drawable and the hypermesh demos cover FWD_SSBO_CUSTOM_INSTANCED; the
# instanced MODEL drawable (the ECS "2000 spheres" / projectile-pool recipe)
# had no test, and the ren_scatter shootable-balls work found it broken in
# the player (draws missing IBL samplers in merged resources / invisible).
#
# Renders to screen for human inspection (repo convention):
#   - a row of instanced pbr_calib_lopoly spheres (the suspect path)
#   - one NON-instanced node of the same model on the left (known-good
#     reference: if the reference is lit and the row is not, the bug is in
#     the instanced program, not the scene/env setup)
#
# Modes (--mode) mirror how the ECS SceneGraphSystem declares the node:
#   twonodes : TWO DrawableNodes sharing ONE drawable, one on std_forward,
#              one on depth_prepass — exactly _instantiateDeclaredNodes'
#              system-level instanced path (the ren_scatter balls_node).
#   addnode  : one node on std_forward, ALSO added to depth_prepass
#              (the component-level auto-dpp shape).
#   nodpp    : one node on std_forward only (the legacy 2000-spheres shape).
################################################################################

import math, sys, signal, argparse
from orkengine.core import vec3, vec4, quat, mtx4, CrcStringProxy, lev2_pyexdir
from orkengine import lev2

lev2_pyexdir.addToSysPath()
from lev2utils.cameras import setupUiCamera
from lev2utils.primitives import createGridData
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()

parser = argparse.ArgumentParser(description="InstancedModelDrawable forward-path test")
parser.add_argument("--mode", choices=["twonodes", "addnode", "nodpp"],
                    default="twonodes", help="node/layer declaration shape (default: twonodes — the ECS system-level path)")
parser.add_argument("--num", type=int, default=8, help="instance count")
parser.add_argument("--noref", action="store_true", help="omit the non-instanced reference model")
args = parser.parse_args()

MODEL_PATH = "data://tests/pbr_calib_lopoly.glb"


class InstancedModelApp(object):

  def __init__(self):
    super().__init__()
    # subsystem-based (HFSM-driven) init — the same lifecycle the player uses;
    # the legacy ad-hoc inline path is deprecated (and aborts on teardown).
    self.ezapp = lev2.OrkEzApp.create(self, width=1280, height=720,
                                      use_subsystems=["opq", "core", "gpu", "lev2"])
    self.ezapp.setRefreshPolicy(lev2.RefreshFastest, 0)
    setupUiCamera(app=self, eye=vec3(0, 4, 14), tgt=vec3(0, 1.5, 0))
    signal.signal(signal.SIGINT, lambda s, f: self.ezapp.signalExit())
    self.time = 0.0

  ##############################################

  def onGpuInit(self, ctx):
    createSceneGraph(app=self, params_dict={
      "SkyboxTexPathStr": "<ork_envmaps2>/cold4k.xir",
      "SkyboxIntensity": float(1),
      "DiffuseIntensity": float(1),
      "SpecularIntensity": float(1),
      "AmbientLight": vec3(0.1),
    })

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
    self.grid_node.sortkey = 1

    model = lev2.XgmModel(MODEL_PATH)

    ##################################
    # the suspect path: instanced model node(s)
    ##################################

    self.ball_node = model.createInstancedNode(args.num, "balls_node", self.layer_std)
    if args.mode == "twonodes":
      # second, separate node sharing the drawable — the system-level
      # instanced declaration creates one DrawableNode PER layer.
      self.layer_dpp.createDrawableNode("balls_node", self.ball_node.drawable)
    elif args.mode == "addnode":
      self.layer_dpp.addDrawableNode(self.ball_node)
    # nodpp: nothing — std_forward only

    self.instdata = self.ball_node.instanceData
    for i in range(args.num):
      x = (i - (args.num - 1) * 0.5) * 1.5
      m = mtx4.composed(vec3(x, 1.0, 0.0), quat(), 0.5)
      self.ball_node.setInstanceMatrix(i, m)
      hue = i / max(1, args.num - 1)
      self.ball_node.setInstanceColor(i, vec4(1.0 - 0.5 * hue, 0.5 + 0.5 * hue, 0.5, 1.0))

    ##################################
    # known-good reference: NON-instanced node of the same model, left side
    ##################################

    if not args.noref:
      ref_data = lev2.ModelDrawableData(MODEL_PATH)
      self.ref_node = self.layer1.createDrawableNodeFromData("reference", ref_data)
      self.ref_node.worldTransform.translation = vec3(-8, 1, 0)
      self.ref_node.worldTransform.scale = 0.5

    print("instanced_model: mode<%s> num<%d> ref<%d>" % (
        args.mode, args.num, int(not args.noref)), flush=True)

  ##############################################

  def onUpdate(self, updinfo):
    self.time = updinfo.absolutetime
    # gentle bob on instance 0 proves the per-frame matrix upload is live
    # (the motion-state analog in the ECS path)
    if hasattr(self, "ball_node"):
      x = -(args.num - 1) * 0.5 * 1.5
      y = 1.0 + 0.5 * math.sin(self.time * 2.0)
      self.ball_node.setInstanceMatrix(0, mtx4.composed(vec3(x, y, 0.0), quat(), 0.5))
    self.scene.updateScene(self.cameralut)

  def onUiEvent(self, uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom(self.uicam.cameradata)
    return lev2.ui.HandlerResult()


################################################################################

if __name__ == "__main__":
  app = InstancedModelApp()
  app.ezapp.mainThreadLoop()
