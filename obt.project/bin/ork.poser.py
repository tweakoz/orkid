#!/usr/bin/env python3

################################################################################
# lev2 sample which renders a scenegraph, optionally in VR mode
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################

import math, random, argparse, sys, os, time
from obt import path

#time.sleep(10)
################################################################################

thisdir = path.directoryOfInvokingModule()

sys.path.append(str(thisdir/".."/".."/"ork.lev2"/"examples"/"python")) # add parent dir to path

################################################################################

parser = argparse.ArgumentParser(description='scenegraph example')
parser.add_argument("-g", '--showgrid', action="store_true", help='show grid' )
parser.add_argument("-f", '--forceregen', action="store_true", help='force asset regeneration' )
parser.add_argument("-m", "--model", type=str, required=False, default="data://tests/pbr1/pbr1", help='asset to load')
parser.add_argument("-i", "--lightintensity", type=float, default=1.0, help='light intensity')
parser.add_argument("-r", "--camdist", type=float, default=0.0, help='camera distance')
parser.add_argument("-e", "--envmap", type=str, default="", help='environment map')
parser.add_argument("-t", "--ssaa", type=int, default=4, help='ssaa')

################################################################################

args = vars(parser.parse_args())
showgrid = args["showgrid"]
modelpath = args["model"]
lightintens = args["lightintensity"]
camdist = args["camdist"]
envmap = args["envmap"]
ssaa = args["ssaa"]

################################################################################
# make sure env vars are set before importing the engine...
################################################################################

if args["forceregen"]:
  os.environ["ORKID_LEV2_FORCE_MODEL_REGEN"] = "1"

os.environ["ORKID_LEV2_SHOW_SKELETON"] = "1"

################################################################################

from orkengine.core import *
from orkengine.lev2 import *
from common.cameras import *
from common.shaders import *
from common.primitives import createGridData
from common.scenegraph import createSceneGraph
tokens = CrcStringProxy()

################################################################################

class SceneGraphApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self,ssaa=ssaa)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    self.editor = Editor()
    self.uictx = self.ezapp.uicontext
    setupUiCamera(app=self,eye=vec3(0,0.5,1))
    self.sel_joint = -1

  ##############################################

  def onGpuInit(self,ctx):

    params_dict = {
      "SkyboxIntensity": float(lightintens),
      "AmbientLight": vec3(0.05),
      "DiffuseIntensity": 1,
      "SpecularIntensity": 1,
      "depthFogDistance": float(10000),
    }

    if envmap != "":
      params_dict["SkyboxTexPathStr"] = envmap

    rendermodel = "DeferredPBR"

    #rendermodel = "PICKTEST"

    createSceneGraph( app=self,
                      params_dict=params_dict,
                      rendermodel=rendermodel )
    
    self.scene.enablePickHud()
    
    self.model = XgmModel(modelpath)
    self.skeleton = self.model.skeleton
    self.sgnode = self.model.createNode("node",self.layer1)
    self.modelinst = self.sgnode.user.pyext_retain_modelinst
    self.modelinst.enableSkinning()
    self.modelinst.enableAllMeshes()
    self.localpose = self.modelinst.localpose
    self.worldpose = self.modelinst.worldpose
    ##############################
    self.bindmats = self.skeleton.bindMatrices
    self.invbindmats = self.skeleton.inverseBindMatrices
    self.nodematrices = self.skeleton.nodeMatrices
    self.jointmatrices = self.skeleton.jointMatrices
    self.infcounts = self.skeleton.jointVertexInfluenceCounts
    self.joints_with_infs = dict()
    for i in range(0,len(self.infcounts)):
      infcount = self.infcounts[i]
      if infcount>0:
        jname = self.skeleton.jointName(i)
        self.joints_with_infs[jname] = infcount
    #print(self.joints_with_infs)
    parents_not_infs = set()
    for jname in self.joints_with_infs.keys():
      ji = self.skeleton.jointIndex(jname)
      par = self.skeleton.jointParent(ji)
      pname = self.skeleton.jointName(par)
      numinfs = self.joints_with_infs[jname]
      if(pname not in self.joints_with_infs):
        parents_not_infs.add(pname)
      print("joint<%d:%s> par<%d:%s> infcount<%d>"%(ji,jname, par, pname, numinfs))
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
    self.children = []
    #print(self.locals)
    #print(self.concats)
    #print(self.bindrels)
    ##############################
    
    #self.animinst = XgmAnimInst()
    #self.animinst.weight = 1.0
    #self.animinst.mask.enableAll()
    #self.animinst.use_temporal_lerp = True
    #self.animinst.bindToSkeleton(self.skeleton)

    self.ball_model = XgmModel("data://tests/pbr_calib")
    self.ball_node = self.ball_model.createNode("ball-node",self.layer1)
    self.ball_node.worldTransform.scale = 0.01
    self.ball_node.pickable = False

    ######################

    center = self.model.boundingCenter
    radius = self.model.boundingRadius*1.5

    print("center<%s> radius<%s>"%(center,radius))
    
    if camdist!=0.0:
      radius = camdist

    self.uicam.lookAt( center-vec3(0,0,radius), 
                       center, 
                       vec3(0,1,0) )

    self.camera.copyFrom( self.uicam.cameradata )

    ###################################

    if showgrid:
      self.grid_data = createGridData()
      self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
      self.grid_node.sortkey = 1

  ##############################################

  def onUiEvent(self,uievent):
    camdat = self.uicam.cameradata
    scoord = uievent.pos
    handled = False
    if uievent.code == tokens.KEY_UP.hashed:
      if uievent.keycode == ord("A") or uievent.keycode == ord("S"):
        self.skeleton.selectBone(-1)
        self.sel_joint = -1
        handled = True
    if uievent.code == tokens.KEY_DOWN.hashed:
      ##############################
      if uievent.keycode == 32:
        self.localpose.bindPose()
        self.localpose.blendPoses()
        self.localpose.concatenate()
        handled = True
      ##############################
      elif uievent.keycode == 66:
        numbones = self.skeleton.numBones
        numjoints = self.skeleton.numJoints
        for i in range(0,numbones):
          bone = self.skeleton.bone(i)
          p = bone.parentIndex
          c = bone.childIndex
          pname = self.skeleton.jointName(p)
          cname = self.skeleton.jointName(c)
          pname = pname.split("/")[-1]
          cname = cname.split("/")[-1]
          print("bone<%d> par<%d:%s> child<%d:%s>"%(i,p,pname,c,cname))
        for i in range(0,numjoints):
          jname = self.skeleton.jointName(i)
          jname = jname.split("/")[-1]
          print("joint<%d:%s>"%(i,jname))

      ##############################
      elif uievent.keycode == ord("A") or uievent.keycode == ord("S"):
        self.children = []
        self.push_screen_pos = scoord
        self.push_cam_z_dir = camdat.znormal
        self.vpmatrix = camdat.vpMatrix(1280/720.0)
        def pick_callback(pixel_fetch_context):
          obj = pixel_fetch_context.value(0)
          pos = pixel_fetch_context.value(1).xyz()
          nrm = pixel_fetch_context.value(2).xyz()
          uv  = pixel_fetch_context.value(3).xyz().xy()
          eye = camdat.eye+camdat.znormal*10
          if obj is not None:
            sel_bone_index = obj["y"]
            self.skeleton.selectBone(sel_bone_index)
            sel_bone = self.skeleton.bone(sel_bone_index)
            sel_parent_index = sel_bone.parentIndex
            self.pivot = self.localpose.concatMatrices[self.sel_joint].translation
            self.sel_joint = sel_parent_index
            jname = self.skeleton.jointName(sel_parent_index)
            print(jname)
            self.children = self.skeleton.childrenOf(sel_parent_index)
            print(self.children)
            self.pmat = self.localpose.concatMatrices[sel_parent_index]
            self.chcmats = [self.localpose.concatMatrices[i] for i in self.children]
            self.concats_push = self.localpose.concatMatrices[0:]
            # compute matrices relative to pmat
            self.relmats = [self.pmat.inverse * ch for ch in self.chcmats]
            A = camdat.project(1280/720.0,pos).xy()*vec2(0.5,0.5)+vec2(0.5,0.5)
            B = scoord*vec2(1.0/1280,-1.0/720)+vec2(0,1)
            self.activate_rot = False
            print(A, B)
        self.scene.pickWithScreenCoord(camdat,scoord,pick_callback)
        handled = True
      ##############################
    elif self.uictx.isKeyDown(ord("A")):
      if uievent.code == tokens.MOVE.hashed:
        if self.sel_joint > 0:
          # determine angle of rotation
          # from scoord delta and camdat.znormal and acos
          mag = (scoord - self.push_screen_pos).length
          if self.activate_rot == False:
            if mag > 32:
              self.activate_rot = True
              self.acive_pos = scoord

          if self.activate_rot:
            deltaA = (self.acive_pos - self.push_screen_pos).normalized()
            deltaB = (scoord - self.push_screen_pos).normalized()
            angle = deltaB.orientedAngle(deltaA)
            # transform selected bone
            self.localpose.concatenate()
            #
            X = self.concats_push[self.sel_joint]
            OR = X.toRotMatrix4()
            ZN = vec4(camdat.znormal,0).transform(OR).xyz()
            IP = mtx4.transMatrix(self.pivot*-1.0)
            P = mtx4.transMatrix(self.pivot)
            Q = quat.createFromAxisAngle(ZN,angle)
            R = Q.toMatrix()
            M = P*R*IP
            self.localpose.concatMatrices[self.sel_joint] = X*M
            # transform children bones (concatenated)
            for i in range(len(self.children)):
              ich = self.children[i]
              MCH = self.relmats[i]
              self.localpose.concatMatrices[ich]=X*M*MCH
            # recompute from concatenated
            self.localpose.decomposeConcatenated()
            #self.localpose.blendPoses()
            self.localpose.concatenate()
            #
          handled = True
      ##############################
    elif self.uictx.isKeyDown(ord("S")):
      if uievent.code == tokens.MOVE.hashed:
        if self.sel_joint == 2:
          # determine angle of rotation
          # from scoord delta and camdat.znormal and acos
          mag = (scoord - self.push_screen_pos).length
          if self.activate_rot == False:
            if mag > 32:
              self.activate_rot = True
              self.acive_pos = scoord

          if self.activate_rot:
            deltaA = (self.acive_pos - self.push_screen_pos).normalized()
            deltaB = (scoord - self.push_screen_pos).normalized()
            angle = deltaB.orientedAngle(deltaA)
            # transform selected bone
            self.localpose.concatenate()
            #
            X = self.concats_push[self.sel_joint]
            OR = X.toRotMatrix4()
            ZN = vec4(camdat.znormal,0).transform(OR).xyz()
            IP = mtx4.transMatrix(self.pivot*-1.0)
            P = mtx4.transMatrix(self.pivot)
            Q = quat.createFromAxisAngle(ZN,angle)
            R = Q.toMatrix()
            M = P*R*IP
            self.localpose.concatMatrices[self.sel_joint] = X*M
            # transform children bones (concatenated)
            for i in range(len(self.children)):
              ich = self.children[i]
              MCH = self.relmats[i]
              self.localpose.concatMatrices[ich]=X*M*MCH
          handled = True
    if uievent.code == tokens.PUSH.hashed:
        self.concats = self.localpose.concatMatrices[0:]
        self.locals = self.localpose.localMatrices[0:]
        self.bindrels = self.localpose.bindRelativeMatrices[0:]
        print(len(self.concats))
        print(len(self.concats))
        handled = True
    
    if not handled:
      handled = self.uicam.uiEventHandler(uievent)
      if handled:
        self.camera.copyFrom( self.uicam.cameradata )
          
    

  ################################################

  def onUpdate(self,updinfo):
    self.scene.updateScene(self.cameralut) 

###############################################################################

SceneGraphApp().ezapp.mainThreadLoop()
