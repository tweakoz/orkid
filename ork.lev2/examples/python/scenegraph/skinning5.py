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

KEY_A = ord("A")
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
      if uievent.keycode in [KEY_S, KEY_A]:
        CHR.deselectBone()
        handled = True

    if uievent.code == tokens.KEY_DOWN.hashed:
      ##############################
      if uievent.keycode == KEY_SPC:
        CHR.resetPose()
        handled = True
      ##############################
      elif uievent.keycode == KEY_MINUS:
        CHR.skeleton.visualBoneScale *= 0.9
      elif uievent.keycode == KEY_EQUAL:
        CHR.skeleton.visualBoneScale *= 1.1
      ##############################
      elif uievent.keycode in [KEY_S, KEY_A]:
        CHR.push_screen_pos = local_coord

        def pick_callback(pixel_fetch_context):
          obj = pixel_fetch_context.value(0)
          sel_bone_index = None
          if obj is not None and isinstance(obj, u32vec4):
            sel_bone_index = int(obj.y)

          if sel_bone_index is not None:
            CHR.selectBoneForFK(sel_bone_index)
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

    # FK rotation handler for A key
    if uictx.isKeyDown(KEY_A):
      if uievent.code == tokens.MOVE.hashed:
        if CHR.sel_joint > 0:
          CHR.rotateOnScreenZ(local_coord, camdat)
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

    self.UIL = self.addComponent("poser_ui", PoserUi)

    self.SGC = self.addComponent("std_scenegraph",
                                 StandardSceneGraphComponent,
                                 enable_ui_camera=True,
                                 eye=vec3(0, 25, -18),
                                 tgt=vec3(0, 0, 10),
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
