#!/usr/bin/env ork.python

import math, sys
#import openvdb as vdb
from obt import path as obt_path 
from ork import path as ork_path
from orkengine.core import vec2,vec3,CrcStringProxy
from orkengine.lev2 import vdb as ork_vdb, OrkEzApp, RefreshFastest, ui, primitives
sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path
from cameras import *
from shaders import POINTCLOUD_SHADERTEXT, createPipeline
from primitives import createPointsPrimV12C4, createGridData
from scenegraph import createSceneGraph

tokens = CrcStringProxy()

#############################
# create levelset sphere
#############################

radius = 50.0 
desired_num_points = 10000000
voxel_size = radius / math.cbrt(desired_num_points);
sphere = ork_vdb.FloatGrid.createLevelSetSphere( "a", radius, vec3(0,0,0), voxel_size, 3.0)
outside = sphere.background
width = 1.1 * outside

#############################
# execute AX "voxel shader"
#############################

voxel_shader = f"""

int@ix = getcoordx();
int@iy = getcoordy();
int@iz = getcoordz();
float@fx = int@ix;
float@fy = int@iy;
float@fz = int@iz;
vec3f@pos = float@fx, float@fy, float@fz;
float@dist = length(vec3f@pos);
float@phi = atan2(float@fz,float@fx);
float@theta = atan2(float@fy,float@fz);

f@a = sin(float@phi*8.0)*0.5+0.5;
f@a = f@a * cos(float@theta*8.0)*0.5+0.5;

"""
ve = ork_vdb.AxVolumeExecutable.compile(voxel_shader)
ve.executeOnGrid(sphere)


################################################################################

class PointsPrimApp(object):

  def __init__(self):
    super().__init__()
    self.ezapp = OrkEzApp.create(self)
    self.ezapp.setRefreshPolicy(RefreshFastest, 0)
    self.materials = set()
    setupUiCamera( app=self, eye = vec3(6,6,6), constrainZ=True, up=vec3(0,1,0))
    self.phi = 0.0
    
  ################################################
  # gpu data init:
  #  called on main thread when graphics context is
  #   made available
  ##############################################

  def onGpuInit(self,ctx):

    ###################################
    # create scenegraph
    ###################################

    sg_params = {
      "SkyboxIntensity": 1.0, 
      "DiffuseIntensity": 6.0, 
    }
    
    createSceneGraph(app=self,rendermodel="ForwardPBR",params_dict=sg_params)

    ###################################
    # create grid
    ###################################

    self.grid_data = createGridData()
    self.grid_node = self.layer1.createGridNode("grid",self.grid_data)
    self.grid_node.sortkey = 1

    ###################################
    # create points primitive 
    ###################################
    
    self.points_prim = primitives.PointsPrimitiveV12C4.createFromVdbFloatGrid(sphere,ctx)

    ##################
    # create shading pipeline
    ##################

    pipeline = createPipeline( app = self,
                               ctx = ctx,
                               shadertext = POINTCLOUD_SHADERTEXT,
                               blending=tokens.OFF,
                               depthtest=tokens.LESS,
                               techname = "tek_points_fwd",
                               rendermodel = "ForwardPBR" )

    def _pointsize():
      val = float(float(2.0+math.sin(self.phi*2.0)*2.0))
      return 1.0

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam( pointsize_param, lambda : _pointsize() ) # set pointsize

    ##################
    # create points sg node
    ##################

    self.primnode = self.points_prim.createNode("node1",self.layer1,pipeline)
    self.primnode.sortkey = 2;


  ################################################

  def onUpdate(self,updinfo):
    self.abstime = updinfo.absolutetime
    self.scene.updateScene(self.cameralut) # update and enqueue all scenenodes
    self.phi = self.abstime
    
  ################################################

  def onDraw(self,drawevent):
    context = drawevent.context
    self.ezapp.processMainSerialQueue()
    self.scene.renderOnContext(context);

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
