#!/usr/bin/env ork.python

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys, os, time
from obt import path

thisdir = path.directoryOfInvokingModule()
sys.path.append(str(thisdir/".."/".."/"ork.lev2"/"examples"/"python"))

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-g", '--showgrid', action="store_true", help='show grid' )
parser.add_argument("-f", '--forceregen', action="store_true", help='force asset regeneration' )
parser.add_argument("-m", "--model", type=str, required=False, default="data://tests/pbr1/pbr1", help='asset to load')
parser.add_argument("-i", "--lightintensity", type=float, default=1.0, help='light intensity')
parser.add_argument("-d", "--camdist", type=float, default=0.0, help='camera distance')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-b", "--bonescale", type=float, default=1.0, help='bone scalar')
parser.add_argument("-t", "--ssaa", type=int, default=0, help='SSAA samples')
parser.add_argument("-u", "--ssao", type=int, default=0, help='SSAO samples')
parser.add_argument('-r', '--rendermodel', type=str, default='forward', help='rendering model (deferred,forward)')

################################################################################

args = vars(parser.parse_args())
showgrid = args["showgrid"]
modelpath = args["model"]
lightintens = args["lightintensity"]
camdist = args["camdist"]
envmap = args["envmap"]
ssaa = args["ssaa"]
ssao = args["ssao"]
bonescale = args["bonescale"]
rendermodel = args["rendermodel"]

################################################################################
# make sure env vars are set before importing the engine...
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
# PoserUi
#  Custom UI for ork.poser with pick texture preview panel and event handling
################################################################################

class PoserUi(UiLayoutComponent):
  """Custom UI with sidebar for pick texture ImageViews + keyboard event handling."""

  def __init__(self):
    super().__init__()
    self.pick_dim = 128  # matches PICKBUFFER_DIM in C++

  def _onBuildLayout(self, lg_group):
    lg_group.margin = 4

    # Create HorizontalPack: [VPack with pick textures] + [main viewport]
    self._hpack_layout = lg_group.makeChild(uiclass=lev2.ui.HorizontalPack, args=["main_hpack"])
    hpack_widget = self._hpack_layout.widget
    hpack_widget.item_width = self.pick_dim  # Fixed width for first child
    hpack_widget.fill = True  # Fill remaining space with last child

    # Create VerticalPack for pick texture ImageViews (left sidebar)
    vpack = hpack_widget.makeChild(uiclass=lev2.ui.VerticalPack, args=["pick_vpack"])
    vpack.uniform = True
    vpack.fill = True

    imgbg = vec4(1,1,1,1)

    # Create 3 ImageViews for pick textures (ID, Position, Normal)
    self.pick_img_id = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_id", imgbg])
    self.pick_img_pos = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_pos", imgbg])
    self.pick_img_nrm = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_nrm", imgbg])

    for imgview in [self.pick_img_id, self.pick_img_pos, self.pick_img_nrm]:
      imgview.maintain_aspect_ratio = True

    # Register slots - "main" is where SceneGraphViewport will go
    self._slots["main"] = hpack_widget
    self._slots["sidebar"] = vpack

  ##############################################

  def _onUiEvent(self, uievent):
    #print("PoserUi::_onUiEvent")
    """Handle keyboard events for bone manipulation."""
    app = self.app
    res = lev2.ui.HandlerResult()
    camdat = app.SGC.uicam.cameradata
    scoord = uievent.pos
    # Compute local viewport coordinates
    sgvpw = app.SGC.SGVPW
    local_coord = vec2(scoord.x - sgvpw.x, scoord.y - sgvpw.y)
    handled = False
    uictx = app.ezapp.uicontext

    if uievent.code == tokens.KEY_UP.hashed:
      if uievent.keycode in [ord("A"), ord("S"), ord("1"), ord("2"), ord("3")]:
        app.skeleton.selectBone(-1)
        app.sel_joint = -1
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
      elif uievent.keycode in [ord("A"), ord("S"), ord("1"), ord("2"), ord("3")]:
        app.descendants = []
        # Store push position in local viewport coordinates
        app.push_screen_pos = local_coord

        def pick_callback(pixel_fetch_context):
          #print(pixel_fetch_context)
          obj = pixel_fetch_context.value(0)
          pos = pixel_fetch_context.value(1).xyz
          nrm = pixel_fetch_context.value(2).xyz
          uv = pixel_fetch_context.value(3).xyz.xy
          eye = camdat.eye + camdat.znormal * 10
          #print(obj,pos,nrm,uv)
          #print(f"obj type: {type(obj)}, is u32vec4: {isinstance(obj, u32vec4) if obj else 'N/A'}")
          # decodePixel returns a u32vec4 with .y = bone ID
          sel_bone_index = None
          if obj is not None and isinstance(obj, u32vec4):
            sel_bone_index = int(obj.y)
          if sel_bone_index is not None:
            app.skeleton.selectBone(sel_bone_index)
            sel_bone = app.skeleton.bone(sel_bone_index)
            sel_parent_index = sel_bone.parentIndex
            sel_child_index = sel_bone.childIndex
            app.sel_joint = sel_parent_index
            # Pivot at parent joint (the origin of the selected bone)
            app.pivot_point = app.localpose.concatMatrices[sel_parent_index].translation
            print(f"bone:{sel_bone_index} parent:{sel_parent_index} child:{sel_child_index} pivot:{app.pivot_point}")
            pname = app.skeleton.jointName(sel_bone.parentIndex)
            cname = app.skeleton.jointName(sel_bone.childIndex)
            ppath = app.skeleton.jointPath(sel_bone.parentIndex)
            cpath = app.skeleton.jointPath(sel_bone.childIndex)
            pID = app.skeleton.jointID(sel_bone.parentIndex)
            cID = app.skeleton.jointID(sel_bone.childIndex)
            app.children = app.skeleton.childJointsOf(sel_parent_index)
            app.descendants = app.skeleton.descendantJointsOf(sel_parent_index)
            app.childrenC = app.skeleton.childJointsOf(sel_bone.childIndex)
            app.descendantsC = app.skeleton.descendantJointsOf(sel_bone.childIndex)
 
            if False:
              print("###########################################")
              print("parent<name>: ", pname)
              print("child<name>: ", cname)
              print("parent<path>: ", ppath)
              print("child<path>: ", cpath)
              print("parent<id>: ", pID)
              print("child<id>: ", cID)
              print("bone index: ", sel_bone_index)
              print("par index: ", sel_bone.parentIndex)
              print("chi index: ", sel_bone.childIndex)
              print("###########################################")
              print("children of p: ", app.children)
              print("descendants of p: ", app.descendants)
              print("children of c: ", app.childrenC)
              print("descendants of c: ", app.descendantsC)
              print("###########################################")

            P = app.localpose.concatMatrices[sel_bone.parentIndex]
            C = app.localpose.concatMatrices[sel_bone.childIndex]
            PT = P.translation
            CT = C.translation
            length = (CT - PT).length

            if False:
              print("concat.pt<%g %g %g>" % (PT.x, PT.y, PT.z))
              print("concat.ct<%g %g %g>" % (CT.x, CT.y, CT.z))
              print("concat.length<%f>" % length)

              print("###########################################")
              P = app.localpose.localMatrices[sel_bone.parentIndex]
              C = app.localpose.localMatrices[sel_bone.childIndex]
              PT = P.translation
              CT = C.translation

              print("local.pt<%g %g %g>" % (PT.x, PT.y, PT.z))
              print("local.ct<%g %g %g>" % (CT.x, CT.y, CT.z))

            app.pmat = app.localpose.concatMatrices[sel_parent_index]
            app.chcmats = [app.localpose.concatMatrices[i] for i in app.descendants]
            app.concats_at_push = app.localpose.concatMatrices[0:]
            app.locals_at_push = app.localpose.localMatrices[0:]
            app.relmats = [app.pmat.inverse * ch for ch in app.chcmats]
            #A = camdat.project(1280 / 720.0, pos).xy * vec2(0.5, 0.5) + vec2(0.5, 0.5)
            #B = scoord * vec2(1.0 / 1280, -1.0 / 720) + vec2(0, 1)
            app.activate_rot = False
            #print(A, B)
            SG = app.scenegraph
            self.pick_img_id.texture = SG.pick_tex_id
            self.pick_img_pos.texture = SG.pick_tex_pos
            self.pick_img_nrm.texture = SG.pick_tex_nrm
            # Mark ImageViews dirty so they redraw with updated pick textures

        self.pick_img_id.setDirty()
        self.pick_img_pos.setDirty()
        self.pick_img_nrm.setDirty()
        app.scenegraph.pickWithScreenCoord(camdat, local_coord, sgvpw.x, sgvpw.y, sgvpw.width, sgvpw.height, pick_callback)
        # Re-assign textures after pick (RtGroup now realized with valid dimensions)
        handled = True
      ##############################

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
    elif uictx.isKeyDown(ord("S")):
      if uievent.code == tokens.MOVE.hashed:
        if app.sel_joint == 2:
          mag = (local_coord - app.push_screen_pos).length
          if app.activate_rot == False:
            if mag > 32:
              app.activate_rot = True
              app.activated_pos = local_coord

          if app.activate_rot:
            deltaA = (app.activated_pos - app.push_screen_pos).normalized
            deltaB = (local_coord - app.push_screen_pos).normalized
            angle = deltaB.orientedAngle(deltaA)
            app.localpose.concatenate()
            X = app.concats_at_push[app.sel_joint]
            OR = X.toRotMatrix4()
            ZN = vec4(camdat.znormal, 0).transform(OR).xyz
            IP = mtx4.transMatrix(app.pivot_point * -1.0)
            P = mtx4.transMatrix(app.pivot_point)
            Q = quat.createFromAxisAngle(ZN, angle)
            R = Q.toMatrix()
            M = P * R * IP
            app.localpose.concatMatrices[app.sel_joint] = X * M
            for i in range(len(app.descendants)):
              ich = app.descendants[i]
              MCH = app.relmats[i]
              app.localpose.concatMatrices[ich] = X * M * MCH
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

    # Build scenegraph params
    params_dict = {
      "SkyboxIntensity": float(lightintens),
      "AmbientLight": vec3(0.05),
      "DiffuseIntensity": 1,
      "SpecularIntensity": 1,
      "depthFogDistance": float(10000),
      #"SSAONumSamples": ssao,
      #"SSAONumSteps": 2,
      #"SSAOBias": -1.0e-5,
      #"SSAORadius": 1.0*25.4/1000.0,
      #"SSAOWeight": 0.5,
      #"SSAOPower": 0.5,
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap

    preset = "ForwardPBR"
    if rendermodel == "deferred":
      preset = "DeferredPBR"

    params_dict["preset"] = preset

    # Add StandardSceneGraphComponent (layout will be set in _onUiInit)
    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 enable_ui_camera=True,
                                 eye=vec3(0, 0.5, 1),
                                 sg_params=params_dict,
                                 grid_variant="_V4" if showgrid else None)

    # Create the ezapp
    self.createEzApp(name="Poser", ssaa=ssaa)

  ##############################################

  def _onUiInit(self):
    """Set up custom UI layout with pick texture sidebar."""
    lg_group = self.ezapp.topLayoutGroup

    # Create and add as component (so _onUiEvent gets called)
    self.UIL = self.addComponent("poser_ui", PoserUi)
    self.UIL._onBuildLayout(lg_group)

    # Replace SGC's default grid with our layout
    if self.SGC.griditems:
      lg_group.replaceChild(self.SGC.griditems[0].layout, self.UIL._hpack_layout)

    # Set layout component so SGC uses it in _onGpuLink
    self.SGC.layout_component = self.UIL

  ##############################################

  def _onGpuInit(self, ctx):
    SGC = self.SGC
    SG = SGC.scenegraph
    layer = SGC.layer_fwd

    # Enable pick HUD
    SG.enablePickHud()

    # Load model
    self.model = lev2.XgmModel(modelpath)
    self.skeleton = self.model.skeleton

    # Create model node
    self.drawable_model = self.model.createDrawable()
    self.modelinst = self.drawable_model.modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.sgnode = SG.createDrawableNodeOnLayers(SGC.fwd_layers, "modelnode", self.drawable_model)

    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose

    ##############################
    self.bindmats = self.skeleton.bindMatrices
    self.invbindmats = self.skeleton.inverseBindMatrices
    self.nodematrices = self.skeleton.nodeMatrices
    self.jointmatrices = self.skeleton.jointMatrices
    self.infcounts = self.skeleton.jointVertexInfluenceCounts
    self.joints_with_infs = dict()
    for i in range(0, len(self.infcounts)):
      infcount = self.infcounts[i]
      if infcount > 0:
        jname = self.skeleton.jointName(i)
        self.joints_with_infs[jname] = infcount
    self.skeleton.visualBoneScale = bonescale
    parents_not_infs = set()
    for jname in self.joints_with_infs.keys():
      ji = self.skeleton.jointIndex(jname)
      par = self.skeleton.jointParent(ji)
      pname = self.skeleton.jointName(par)
      numinfs = self.joints_with_infs[jname]
      if pname not in self.joints_with_infs:
        parents_not_infs.add(pname)
      print("joint<%d:%s> par<%d:%s> infcount<%d>" % (ji, jname, par, pname, numinfs))
    print("####################################################")
    print(parents_not_infs)
    print("####################################################")
    ##############################
    self.localpose.bindPose()
    self.localpose.blendPoses()
    self.localpose.concatenate()
    self.concats = self.localpose.concatMatrices[0:]
    self.locals = self.localpose.localMatrices[0:]
    self.bindrels = self.localpose.bindRelativeMatrices[0:]
    self.descendants = []

    # Ball for visual feedback
    self.ball_model = lev2.XgmModel("data://tests/pbr_calib")
    self.ball_drawable = self.ball_model.createDrawable()
    self.ball_node = SG.createDrawableNodeOnLayers(SGC.fwd_layers, "ball-node", self.ball_drawable)
    self.ball_node.worldTransform.scale = 0.01
    self.ball_node.pickable = False

    ######################
    # Setup camera
    ######################

    center = self.model.boundingCenter
    radius = self.model.boundingRadius * 1.5

    print("center<%s> radius<%s>" % (center, radius))

    if camdist != 0.0:
      radius = camdist

    SGC.uicam.lookAt(center - vec3(0, 0, radius),
                     center,
                     vec3(0, 1, 0))
    SGC.camera.copyFrom(SGC.uicam.cameradata)

    # Store reference to scenegraph for picking
    self.scenegraph = SG
    self.cameralut = SGC.cameralut

  ##############################################

  def _onGpuLink(self, ctx):
    """Assign pick textures to ImageViews after full GPU initialization"""
    SG = self.scenegraph
    UIL = self.UIL

    # Verify pick buffer dimension matches what we used for UI layout
    print(f"Pick buffer dimension from SG: {SG.pick_buffer_dim}")

    # Assign pick textures to ImageViews (textures should be fully initialized now)
    def assign_if_valid(imgview, tex, name):
      if tex is not None:
        w = tex.width
        h = tex.height
        print(f"Assigning {name} ({w}x{h}) to ImageView")
        if w > 0 and h > 0 and w < 16384 and h < 16384:
          imgview.texture = tex
        else:
          print(f"  WARNING: Invalid texture dimensions for {name}")

    # Get ImageViews from the layout component
    #assign_if_valid(UIL.pick_img_id, SG.pick_tex_id, "pick_tex_id")
    #assign_if_valid(UIL.pick_img_pos, SG.pick_tex_pos, "pick_tex_pos")
    #assign_if_valid(UIL.pick_img_nrm, SG.pick_tex_nrm, "pick_tex_nrm")

  ##############################################

  def rotateOnScreenZ(self, cur_screen_pos):
    camdat = self.SGC.uicam.cameradata
    ######################
    mag = (cur_screen_pos - self.push_screen_pos).length
    if self.activate_rot == False:
      if mag > 32:
        self.activate_rot = True
        self.activated_pos = cur_screen_pos
    ######################
    if self.activate_rot:
      deltaA = (self.activated_pos - self.push_screen_pos).normalized
      deltaB = (cur_screen_pos - self.push_screen_pos).normalized
      angle = deltaB.orientedAngle(deltaA)
      #################################
      self.localpose.concatenate()
      X = self.concats_at_push[self.sel_joint]
      OR = X.toRotMatrix4()
      ZN = vec4(camdat.znormal, 0).transform(OR).xyz.normalized
      IP = mtx4.transMatrix(self.pivot_point * -1.0)
      P = mtx4.transMatrix(self.pivot_point)
      Q = quat.createFromAxisAngle(ZN, angle)
      R = Q.toMatrix()
      M = P * R * IP
      self.propogateFromJoint(X, M)

  ##############################################

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

  ##############################################

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

  ##############################################

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

  ##############################################

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
