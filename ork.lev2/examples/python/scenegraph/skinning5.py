#!/usr/bin/env ork.python

################################################################################
# Skinning4 - Poser with IK support
# Based on ork.poser.py, adds IK manipulation with "D" key
# Uses the char_mesh model from ork.data/tests/chartest/
################################################################################

import math, random, argparse, sys, os, time
from obt import path

################################################################################

parser = argparse.ArgumentParser(description='skinning4 - poser with IK')
parser.add_argument("-b", "--bonescale", type=float, default=4.0, help='bone scalar')

################################################################################

args = vars(parser.parse_args())
modelpath = "data://tests/chartest/char_mesh" # Fixed model path for char_mesh
bonescale = args["bonescale"]

os.environ["ORKID_LEV2_SHOW_SKELETON"] = "1"

################################################################################

KEY_S = ord("S")
KEY_SPC = ord(" ")
KEY_MINUS = ord("-")
KEY_EQUAL = ord("=")

################################################################################

from orkengine.core import vec2, vec3, vec4, quat, mtx4, CrcStringProxy, VarMap, u32vec4
from orkengine import lev2
from ork.app.application import ComponentizedApplication, UiLayoutComponent
from ork.app.std_scenegraph import StandardSceneGraphComponent
from ork.app.testlib.chartest import CharacterComponent

tokens = CrcStringProxy()

################################################################################
# PoserUi with IK support
################################################################################

class PoserUi(UiLayoutComponent):
  """Custom UI with sidebar for pick texture ImageViews + keyboard event handling."""

  def __init__(self):
    super().__init__()

  def _onBuildLayout(self, lg_group):

    bg_color = vec4(0.1, 0.1, 0.1, 1)

    lg_group.clearColorGuide = vec4(0.8,0.6,0.2,1)
    lg_group.clearColorStd = bg_color

    ##################################
    # Create 2x1 grid of Box placeholders
    ##################################

    self._griditems = lg_group.makeGrid(
      width=2,
      height=1,
      margin=4,
      h_proportions = [0.2],
      uiclass=lev2.ui.Box,
      args=["cell", bg_color]
    ) 
     
    ##################################
    # Create SGVP and TABS 
    ##################################

    sgvp_layout = lg_group.makeChild(uiclass=lev2.ui.SceneGraphViewport, args=["SGVP", bg_color])
    tabs_layout = lg_group.makeChild(uiclass=lev2.ui.TabsWidget, args=["tabs", bg_color])

    lg_group.replaceChild(self._griditems[1].layout, sgvp_layout)
    lg_group.replaceChild(self._griditems[0].layout, tabs_layout)

    ##################################
    # configure SGVP
    ##################################

    self._sgvp_widget = sgvp_layout.widget

    ##################################
    # configure TABS
    ##################################

    tabs = tabs_layout.widget
    tabs.content_background = bg_color
    tabs.draw_background = True
    
    ##################################
    # Tab 1: vpack with ImageViews
    ##################################

    vpack = tabs.makeChild(uiclass=lev2.ui.VerticalPack, args=["Pick"])
    vpack.uniform = True
    vpack.fill = True
    self._slots["sidebar"] = vpack

    self.pick_img_id = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_id", bg_color])
    self.pick_img_pos = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_pos", bg_color])
    self.pick_img_nrm = vpack.makeChild(uiclass=lev2.ui.ImageView, args=["pick_nrm", bg_color])

    for imgview in [self.pick_img_id, self.pick_img_pos, self.pick_img_nrm]:
      imgview.maintain_aspect_ratio = True
      imgview.flip_x = True
      imgview.flip_y = True

    ##################################
    # Tab 2: Help window
    ##################################

    help_box = tabs.makeChild(uiclass=lev2.ui.TextBox, args=["HELP", bg_color, "hello"])
    help_box.setText("TODO: IK Help Info\n\n\n    S : Select Bone\n- / = : Scale Bone Display")
    help_box.halign = tokens.CENTER_ALL
    help_box.valign = tokens.CENTER
    help_box.font = lev2.FontManager.fontForId("i22")

    tabs.setActiveTabByName("HELP")


  def provideWidgetForSlot(self, slot_name, widget_class, args):
    # Return existing viewport instead of creating new one
    if slot_name == "main":
      return self._sgvp_widget
    return None

  ##############################################

  def _onUiEvent(self, uievent):
    app = self.app
    CHR = app.CHR
    res = lev2.ui.HandlerResult()
    camdat = app.SGC.uicam.cameradata
    scoord = uievent.pos
    sgvpw = app.SGC.SGVPW
    local_coord = vec2(scoord.x - sgvpw.x, scoord.y - sgvpw.y)
    handled = False
    uictx = app.ezapp.uicontext

    if uievent.code == tokens.KEY_UP.hashed:
      if uievent.keycode in [KEY_S]:
        CHR.skeleton.selectBone(-1)
        app.sel_joint = -1
        app.ik_chain = None  # Clear IK chain on key up
        app.ik_mode = None
        handled = True

    if uievent.code == tokens.KEY_DOWN.hashed:
      ##############################
      if uievent.keycode == KEY_SPC:
        CHR.localpose.bindPose()
        CHR.localpose.blendPoses()
        CHR.localpose.concatenate()
        handled = True
      ##############################
      elif uievent.keycode == KEY_MINUS:
        CHR.skeleton.visualBoneScale *= 0.9
      elif uievent.keycode == KEY_EQUAL:
        CHR.skeleton.visualBoneScale *= 1.1
      ##############################
      elif uievent.keycode in [KEY_S]:
        app.descendants = []
        app.push_screen_pos = local_coord

        def pick_callback(pixel_fetch_context):
          obj = pixel_fetch_context.value(0)
          pos = pixel_fetch_context.value(1).xyz
          nrm = pixel_fetch_context.value(2).xyz
          uv = pixel_fetch_context.value(3).xyz.xy

          sel_bone_index = None
          if obj is not None and isinstance(obj, u32vec4):
            sel_bone_index = int(obj.y)

          if sel_bone_index is not None:
            CHR.skeleton.selectBone(sel_bone_index)
            sel_bone = CHR.skeleton.bone(sel_bone_index)
            sel_parent_index = sel_bone.parentIndex
            sel_child_index = sel_bone.childIndex
            app.sel_joint = sel_parent_index
            app.pivot_point = CHR.localpose.concatMatrices[sel_parent_index].translation

            print(f"bone:{sel_bone_index} parent:{sel_parent_index} child:{sel_child_index} pivot:{app.pivot_point}")

            # Setup for FK rotation
            app.children = CHR.skeleton.childJointsOf(sel_parent_index)
            app.descendants = CHR.skeleton.descendantJointsOf(sel_parent_index)
            app.pmat = CHR.localpose.concatMatrices[sel_parent_index]
            app.chcmats = [CHR.localpose.concatMatrices[i] for i in app.descendants]
            app.concats_at_push = CHR.localpose.concatMatrices[0:]
            app.locals_at_push = CHR.localpose.localMatrices[0:]
            app.relmats = [app.pmat.inverse * ch for ch in app.chcmats]
            app.activate_rot = False

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

    if uievent.code == tokens.PUSH.hashed:
      app.concats = CHR.localpose.concatMatrices[0:]
      app.locals = CHR.localpose.localMatrices[0:]
      app.bindrels = CHR.localpose.bindRelativeMatrices[0:]
      print(len(app.concats))
      handled = True

    return lev2.ui.HandlerResult() if handled else None

###############################################################################
###############################################################################
################################################################################

class SceneGraphApp(ComponentizedApplication):

  def __init__(self):
    super().__init__()
    self.sel_joint = -1
    self.activate_rot = False
    self.descendants = []

    params_dict = {
      "SkyboxIntensity": float(1.0),
      "AmbientLight": vec3(0.05),
      "DiffuseIntensity": 1,
      "SpecularIntensity": 1,
      "depthFogDistance": float(10000),
      "preset": "ForwardPBR",
    }

    self.UIL = self.addComponent("poser_ui", PoserUi)

    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 enable_ui_camera=True,
                                 eye=vec3(0, 25, -18),
                                 tgt=vec3(0, 0, 10),
                                 sg_params=params_dict,
                                 layout_component=self.UIL,
                                 grid_variant="_V4")

    self.CHR = self.addComponent("character",
                                 CharacterComponent,
                                 modelpath=modelpath,
                                 bonescale=bonescale)

    self.createEzApp(name="Skinning5-IK",
                     fullscreen=True)

    self.ezapp.uicontext.debug_event_routing = True

  ##############################################

  def _onUiInit(self):
    lg_group = self.ezapp.topLayoutGroup
    self.UIL._onBuildLayout(lg_group)

  ##############################################

  def _onGpuInit(self, ctx):
    self.scenegraph = self.SGC.scenegraph

###############################################################################

SGA = SceneGraphApp()
SGA.ezapp.mainThreadLoop()
