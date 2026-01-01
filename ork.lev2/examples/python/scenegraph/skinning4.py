#!/usr/bin/env ork.python

################################################################################
# Skinning4 - Poser with IK support
# Based on ork.poser.py, adds IK manipulation with "D" key
# Uses the char_mesh model from ork.data/tests/chartest/
################################################################################

import math, random, argparse, sys, os, time
from obt import path

thisdir = path.directoryOfInvokingModule()
sys.path.append(str(thisdir/".."/".."/".."/"obt.project"/"bin"))
sys.path.append(str(thisdir))

################################################################################

parser = argparse.ArgumentParser(description='skinning4 - poser with IK')
parser.add_argument("-f", '--forceregen', action="store_true", help='force asset regeneration')
parser.add_argument("-i", "--lightintensity", type=float, default=1.0, help='light intensity')
parser.add_argument("-d", "--camdist", type=float, default=0.0, help='camera distance')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-b", "--bonescale", type=float, default=4.0, help='bone scalar')
parser.add_argument("-t", "--ssaa", type=int, default=0, help='SSAA samples')
parser.add_argument('-r', '--rendermodel', type=str, default='forward', help='rendering model (deferred,forward)')

################################################################################

args = vars(parser.parse_args())
showgrid = True
# Fixed model path for char_mesh
modelpath = "data://tests/chartest/char_mesh"
lightintens = args["lightintensity"]
camdist = args["camdist"]
envmap = args["envmap"]
ssaa = args["ssaa"]
bonescale = args["bonescale"]
rendermodel = args["rendermodel"]

################################################################################

if args["forceregen"]:
  os.environ["ORKID_LEV2_FORCE_MODEL_REGEN"] = "1"

os.environ["ORKID_LEV2_SHOW_SKELETON"] = "1"

################################################################################

from orkengine.core import vec2, vec3, vec4, quat, mtx4, CrcStringProxy, VarMap, u32vec4
from orkengine import lev2
from ork.app.application import ComponentizedApplication, UiLayoutComponent
from ork.app.std_scenegraph import StandardSceneGraphComponent
from lev2utils.primitives import createGridData

tokens = CrcStringProxy()

################################################################################
# Hardcoded IK chains (like skinning2.py)
# For now, just arm chains - click on hand/fingers to drag
################################################################################

ARM_IK_CHAINS = {
  "Left": {
    "arm": "mixamorig.LeftArm",
    "forearm": "mixamorig.LeftForeArm",
    "hand": "mixamorig.LeftHand",
  },
  "Right": {
    "arm": "mixamorig.RightArm",
    "forearm": "mixamorig.RightForeArm",
    "hand": "mixamorig.RightHand",
  }
}

################################################################################
# PoserUi with IK support
################################################################################

class PoserUi(UiLayoutComponent):
  """Custom UI with sidebar for pick texture ImageViews + keyboard event handling."""

  def __init__(self):
    super().__init__()
    self.pick_dim = 256

  def _onBuildLayout(self, lg_group):
    lg_group.margin = 4

    self._hpack_layout = lg_group.makeChild(uiclass=lev2.ui.HorizontalPack, args=["main_hpack"])
    hpack_widget = self._hpack_layout.widget
    hpack_widget.item_width = self.pick_dim
    hpack_widget.fill = True

    vpack = hpack_widget.makeChild(uiclass=lev2.ui.VerticalPack, args=["pick_vpack"])
    vpack.uniform = True
    vpack.fill = True

    imgbg = vec4(1,1,1,1)

    self.pick_img_id = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_id", imgbg])
    self.pick_img_pos = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_pos", imgbg])
    self.pick_img_nrm = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_nrm", imgbg])

    for imgview in [self.pick_img_id, self.pick_img_pos, self.pick_img_nrm]:
      imgview.maintain_aspect_ratio = True

    self._slots["main"] = hpack_widget
    self._slots["sidebar"] = vpack

  ##############################################

  def _onUiEvent(self, uievent):
    app = self.app
    res = lev2.ui.HandlerResult()
    camdat = app.SGC.uicam.cameradata
    scoord = uievent.pos
    sgvpw = app.SGC.SGVPW
    local_coord = vec2(scoord.x - sgvpw.x, scoord.y - sgvpw.y)
    handled = False
    uictx = app.ezapp.uicontext

    if uievent.code == tokens.KEY_UP.hashed:
      if uievent.keycode in [ord("A"), ord("S"), ord("1"), ord("2"), ord("3"), ord("D")]:
        app.skeleton.selectBone(-1)
        app.sel_joint = -1
        app.ik_chain = None  # Clear IK chain on key up
        handled = True

    if uievent.code == tokens.KEY_DOWN.hashed:
      ##############################
      if uievent.keycode == ord(" "):
        app.localpose.bindPose()
        app.localpose.blendPoses()
        app.localpose.concatenate()
        handled = True
      ##############################
      elif uievent.keycode == ord("B"):
        numbones = app.skeleton.numBones
        numjoints = app.skeleton.numJoints
        for i in range(0, numbones):
          bone = app.skeleton.bone(i)
          p = bone.parentIndex
          c = bone.childIndex
          pname = app.skeleton.jointName(p)
          cname = app.skeleton.jointName(c)
          pname = pname.split("/")[-1]
          cname = cname.split("/")[-1]
          print("bone<%d> par<%d:%s> child<%d:%s>" % (i, p, pname, c, cname))
        for i in range(0, numjoints):
          jname = app.skeleton.jointName(i)
          jname = jname.split("/")[-1]
          print("joint<%d:%s>" % (i, jname))
      ##############################
      elif uievent.keycode == ord("-"):
        app.skeleton.visualBoneScale *= 0.9
      elif uievent.keycode == ord("="):
        app.skeleton.visualBoneScale *= 1.1
      ##############################
      elif uievent.keycode in [ord("A"), ord("S"), ord("1"), ord("2"), ord("3"), ord("D")]:
        app.descendants = []
        app.push_screen_pos = local_coord
        is_ik_mode = (uievent.keycode == ord("D"))

        def pick_callback(pixel_fetch_context):
          obj = pixel_fetch_context.value(0)
          pos = pixel_fetch_context.value(1).xyz
          nrm = pixel_fetch_context.value(2).xyz
          uv = pixel_fetch_context.value(3).xyz.xy

          sel_bone_index = None
          if obj is not None and isinstance(obj, u32vec4):
            sel_bone_index = int(obj.y)

          if sel_bone_index is not None:
            app.skeleton.selectBone(sel_bone_index)
            sel_bone = app.skeleton.bone(sel_bone_index)
            sel_parent_index = sel_bone.parentIndex
            sel_child_index = sel_bone.childIndex
            app.sel_joint = sel_parent_index
            app.pivot_point = app.localpose.concatMatrices[sel_parent_index].translation

            print(f"bone:{sel_bone_index} parent:{sel_parent_index} child:{sel_child_index} pivot:{app.pivot_point}")

            # Setup for FK rotation
            app.children = app.skeleton.childJointsOf(sel_parent_index)
            app.descendants = app.skeleton.descendantJointsOf(sel_parent_index)
            app.pmat = app.localpose.concatMatrices[sel_parent_index]
            app.chcmats = [app.localpose.concatMatrices[i] for i in app.descendants]
            app.concats_at_push = app.localpose.concatMatrices[0:]
            app.locals_at_push = app.localpose.localMatrices[0:]
            app.relmats = [app.pmat.inverse * ch for ch in app.chcmats]
            app.activate_rot = False

            # Setup IK if D key
            if is_ik_mode:
              print(f"Setting up IK for joint {sel_child_index}: {app.skeleton.jointName(sel_child_index)}")
              # Store initial pose matrices BEFORE setupIkChain modifies them
              app.ik_initial_locals = [app.localpose.localMatrices[i] for i in range(app.skeleton.numJoints)]
              app.ik_initial_concats = [app.localpose.concatMatrices[i] for i in range(app.skeleton.numJoints)]
              app.setupIkChain(sel_child_index)
              if app.ik_chain is not None:
                # Store initial hand position from the ORIGINAL pose (before IK warp)
                app.ik_initial_hand_pos = app.ik_initial_concats[app.ik_hand_joint].translation
                print(f"IK initial hand pos: {app.ik_initial_hand_pos}")
                # Reset pose immediately to undo the warp from setupIkChain
                for i in range(app.skeleton.numJoints):
                  app.localpose.localMatrices[i] = app.ik_initial_locals[i]
                  app.localpose.concatMatrices[i] = app.ik_initial_concats[i]

            # Update pick texture views
            SG = app.scenegraph
            self.pick_img_id.texture = SG.pick_tex_id
            self.pick_img_pos.texture = SG.pick_tex_pos
            self.pick_img_nrm.texture = SG.pick_tex_nrm

        self.pick_img_id.setDirty()
        self.pick_img_pos.setDirty()
        self.pick_img_nrm.setDirty()
        app.scenegraph.pickWithScreenCoord(camdat, local_coord, sgvpw.x, sgvpw.y, sgvpw.width, sgvpw.height, pick_callback)
        handled = True
      ##############################

    # FK rotation handlers
    elif uictx.isKeyDown(ord("A")):
      if uievent.code == tokens.MOVE.hashed:
        if app.sel_joint > 0:
          app.rotateOnScreenZ(local_coord)
          handled = True
    elif uictx.isKeyDown(ord("1")):
      if uievent.code == tokens.MOVE.hashed:
        if app.sel_joint > 0:
          app.rotateOnLocalX(local_coord)
          handled = True
    elif uictx.isKeyDown(ord("2")):
      if uievent.code == tokens.MOVE.hashed:
        if app.sel_joint > 0:
          app.rotateOnLocalY(local_coord)
          handled = True
    elif uictx.isKeyDown(ord("3")):
      if uievent.code == tokens.MOVE.hashed:
        if app.sel_joint > 0:
          app.rotateOnLocalZ(local_coord)
          handled = True
    # IK handler
    elif uictx.isKeyDown(ord("D")):
      if uievent.code == tokens.MOVE.hashed:
        if app.ik_chain is not None:
          app.updateIk(local_coord, camdat)
          handled = True

    if uievent.code == tokens.PUSH.hashed:
      app.concats = app.localpose.concatMatrices[0:]
      app.locals = app.localpose.localMatrices[0:]
      app.bindrels = app.localpose.bindRelativeMatrices[0:]
      print(len(app.concats))
      handled = True

    return lev2.ui.HandlerResult() if handled else None

################################################################################

class SceneGraphApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.materials = set()
    self.sel_joint = -1
    self.activate_rot = False
    self.ik_chain = None
    self.ik_target = None
    self.ik_end_joint = -1
    self.ik_middle_joint = -1
    self.ik_arm_joint = -1
    self.ik_forearm_joint = -1
    self.ik_hand_joint = -1
    self.ik_extend_length = 0.0
    self.ik_fixup_joints = []
    self.ik_initial_hand_pos = None
    self.ik_initial_locals = None
    self.ik_initial_concats = None

    params_dict = {
      "SkyboxIntensity": float(lightintens),
      "AmbientLight": vec3(0.05),
      "DiffuseIntensity": 1,
      "SpecularIntensity": 1,
      "depthFogDistance": float(10000),
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap

    preset = "ForwardPBR"
    if rendermodel == "deferred":
      preset = "DeferredPBR"

    params_dict["preset"] = preset

    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 enable_ui_camera=True,
                                 eye=vec3(0, 0.5, 1),
                                 sg_params=params_dict,
                                 grid_variant="_V4" if showgrid else None)

    self.createEzApp(name="Skinning4-IK", ssaa=ssaa, fullscreen=True)

  ##############################################

  def _onUiInit(self):
    lg_group = self.ezapp.topLayoutGroup
    self.UIL = self.addComponent("poser_ui", PoserUi)
    self.UIL._onBuildLayout(lg_group)

    if self.SGC.griditems:
      lg_group.replaceChild(self.SGC.griditems[0].layout, self.UIL._hpack_layout)

    self.SGC.layout_component = self.UIL

  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    SG = SGC.scenegraph
    layer = SGC.layer_fwd

    SG.enablePickHud()

    # Load the char_mesh model
    self.model = lev2.XgmModel(modelpath)
    self.skeleton = self.model.skeleton

    self.drawable_model = self.model.createDrawable()
    self.modelinst = self.drawable_model.modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.sgnode = SG.createDrawableNodeOnLayers(SGC.fwd_layers, "modelnode", self.drawable_model)

    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose

    # Print joint info
    self.infcounts = self.skeleton.jointVertexInfluenceCounts
    for i in range(0, len(self.infcounts)):
      infcount = self.infcounts[i]
      if infcount > 0:
        jname = self.skeleton.jointName(i)
        par = self.skeleton.jointParent(i)
        pname = self.skeleton.jointName(par)
        print("joint<%d:%s> par<%d:%s> infcount<%d>" % (i, jname, par, pname, infcount))

    self.skeleton.visualBoneScale = bonescale

    self.localpose.bindPose()
    self.localpose.blendPoses()
    self.localpose.concatenate()
    self.concats = self.localpose.concatMatrices[0:]
    self.locals = self.localpose.localMatrices[0:]
    self.bindrels = self.localpose.bindRelativeMatrices[0:]
    self.descendants = []

    # Ball for IK target visualization
    self.ball_model = lev2.XgmModel("data://tests/pbr_calib")
    self.ball_drawable = self.ball_model.createDrawable()
    self.ball_node = SG.createDrawableNodeOnLayers(SGC.fwd_layers, "ball-node", self.ball_drawable)
    self.ball_node.worldTransform.scale = 0.01
    self.ball_node.pickable = False

    # Setup camera
    center = self.model.boundingCenter
    radius = self.model.boundingRadius * 1.5

    print("center<%s> radius<%s>" % (center, radius))

    if camdist != 0.0:
      radius = camdist

    SGC.uicam.lookAt(center - vec3(0, 0, radius),
                     center,
                     vec3(0, 1, 0))
    SGC.camera.copyFrom(SGC.uicam.cameradata)

    self.scenegraph = SG
    self.cameralut = SGC.cameralut

  ##############################################

  def _onGpuLink(self, ctx):
    SG = self.scenegraph
    UIL = self.UIL
    print(f"Pick buffer dimension from SG: {SG.pick_buffer_dim}")

  ##############################################
  # IK Methods - Simplified, hardcoded arm chains like skinning2.py
  ##############################################

  def detectArmFromJoint(self, joint_index):
    """
    Detect if a joint belongs to left or right arm.
    Returns "Left", "Right", or None.
    """
    jname = self.skeleton.jointName(joint_index)
    if "Left" in jname and ("Hand" in jname or "Arm" in jname):
      return "Left"
    elif "Right" in jname and ("Hand" in jname or "Arm" in jname):
      return "Right"
    return None

  def setupIkChain(self, clicked_joint_index):
    """
    Setup IK chain for the arm that was clicked.
    Uses hardcoded arm chains like skinning2.py.
    """
    # Detect which arm was clicked
    arm_side = self.detectArmFromJoint(clicked_joint_index)
    if arm_side is None:
      print(f"Joint {clicked_joint_index} is not part of an arm, IK disabled")
      self.ik_chain = None
      return

    print(f"Setting up {arm_side} arm IK")
    chain_info = ARM_IK_CHAINS[arm_side]

    # Get joint indices
    self.ik_arm_joint = self.skeleton.jointIndex(chain_info["arm"])
    self.ik_forearm_joint = self.skeleton.jointIndex(chain_info["forearm"])
    self.ik_hand_joint = self.skeleton.jointIndex(chain_info["hand"])

    print(f"  Arm: {chain_info['arm']} ({self.ik_arm_joint})")
    print(f"  ForeArm: {chain_info['forearm']} ({self.ik_forearm_joint})")
    print(f"  Hand: {chain_info['hand']} ({self.ik_hand_joint})")

    # Create IK chain exactly like skinning2.py
    self.ik_chain = lev2.IkChain(self.skeleton)
    self.ik_chain.bindToJointNamed(chain_info["arm"])
    self.ik_chain.bindToJointNamed(chain_info["forearm"])
    self.ik_chain.prepare()
    self.ik_chain.compute(self.localpose, vec3(0,0,0))
    # Use hardcoded values exactly like skinning2
    self.ik_chain.C1 = 0.079
    self.ik_chain.C2 = 0.029

    # Store state
    self.ik_end_joint = self.ik_hand_joint
    self.ik_middle_joint = self.ik_forearm_joint

    # Fixup joints exactly like skinning2: hand + thumb joints + index joints
    self.ik_fixup_joints = [self.ik_hand_joint] + self.skeleton.descendantJointsOf(self.ik_hand_joint)
    print(f"  Fixup joints: {len(self.ik_fixup_joints)}")

  def updateIk(self, cur_screen_pos, camdat):
    """
    Update IK - matches skinning2.py approach.
    Resets pose each frame, then applies IK with offset from initial position.
    """
    if self.ik_chain is None:
      return

    # Reset pose to initial state (like skinning2 resets from animation each frame)
    for i in range(self.skeleton.numJoints):
      self.localpose.localMatrices[i] = self.ik_initial_locals[i]
      self.localpose.concatMatrices[i] = self.ik_initial_concats[i]

    concats = self.localpose.concatMatrices

    # Get forearm and hand matrices from reset pose
    mtx_forearm = concats[self.ik_forearm_joint]
    mtx_hand = concats[self.ik_hand_joint]

    # Compute extend length (like skinning2)
    extend_length = (mtx_forearm.translation - mtx_hand.translation).length

    # Mouse X/Y -> world X/Z offset (simple, small amounts)
    delta = cur_screen_pos - self.push_screen_pos
    offset = vec3(delta.x * 0.001, 0, delta.y * 0.001)

    # Target = initial hand position + offset (NOT current, to avoid accumulation)
    target = self.ik_initial_hand_pos + offset

    # Update ball visualization
    self.ball_node.worldTransform.translation = target

    # Solve IK
    self.ik_chain.compute(self.localpose, target)

    # Fixup: reconnect hand to forearm (like skinning2.py)
    fixup_base = concats[self.ik_forearm_joint]
    fixup_old = concats[self.ik_hand_joint].translation
    fixup_new = vec3(0, extend_length, 0).transform(fixup_base)
    fixup_delta = fixup_new - fixup_old

    xf_delta = mtx4()
    xf_delta.setColumn(3, vec4(fixup_delta, 1))

    for ji in self.ik_fixup_joints:
      O = concats[ji]
      concats[ji] = xf_delta * O

  ##############################################
  # FK Rotation Methods (from ork.poser)
  ##############################################

  def rotateOnScreenZ(self, cur_screen_pos):
    camdat = self.SGC.uicam.cameradata
    mag = (cur_screen_pos - self.push_screen_pos).length
    if self.activate_rot == False:
      if mag > 32:
        self.activate_rot = True
        self.activated_pos = cur_screen_pos
    if self.activate_rot:
      deltaA = (self.activated_pos - self.push_screen_pos).normalized
      deltaB = (cur_screen_pos - self.push_screen_pos).normalized
      angle = deltaB.orientedAngle(deltaA)
      self.localpose.concatenate()
      X = self.concats_at_push[self.sel_joint]
      ZN = camdat.znormal
      IP = mtx4.transMatrix(self.pivot_point * -1.0)
      P = mtx4.transMatrix(self.pivot_point)
      Q = quat.createFromAxisAngle(ZN, angle)
      R = Q.toMatrix()
      M = P * R * IP
      self.propogateFromJoint(X, M)

  def rotateOnLocalX(self, cur_screen_pos):
    delta = (cur_screen_pos.x - self.push_screen_pos.x)
    angle = delta * 0.01
    self.localpose.concatenate()
    C = self.concats_at_push[self.sel_joint]
    R = quat.createFromAxisAngle(vec3(1, 0, 0), angle).toMatrix()
    IP = mtx4.transMatrix(self.pivot_point * -1.0)
    P = mtx4.transMatrix(self.pivot_point)
    M = P * R * IP
    self.propogateFromJoint(C, M)

  def rotateOnLocalY(self, cur_screen_pos):
    delta = (cur_screen_pos.x - self.push_screen_pos.x)
    angle = delta * 0.01
    self.localpose.concatenate()
    C = self.concats_at_push[self.sel_joint]
    R = quat.createFromAxisAngle(vec3(0, 1, 0), angle).toMatrix()
    IP = mtx4.transMatrix(self.pivot_point * -1.0)
    P = mtx4.transMatrix(self.pivot_point)
    M = P * R * IP
    self.propogateFromJoint(C, M)

  def rotateOnLocalZ(self, cur_screen_pos):
    delta = (cur_screen_pos.x - self.push_screen_pos.x)
    angle = delta * 0.01
    self.localpose.concatenate()
    C = self.concats_at_push[self.sel_joint]
    R = quat.createFromAxisAngle(vec3(0, 0, 1), angle).toMatrix()
    IP = mtx4.transMatrix(self.pivot_point * -1.0)
    P = mtx4.transMatrix(self.pivot_point)
    M = P * R * IP
    self.propogateFromJoint(C, M)

  def propogateFromJoint(self, C, M):
    self.localpose.concatMatrices[self.sel_joint] = M * C
    for i in range(len(self.descendants)):
      ich = self.descendants[i]
      MCH = self.relmats[i]
      self.localpose.concatMatrices[ich] = M * C * MCH
    self.localpose.deconcatenate()
    self.localpose.concatenate()

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
