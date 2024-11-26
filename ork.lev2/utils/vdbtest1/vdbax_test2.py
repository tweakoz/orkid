#!/usr/bin/env ork.python

import math, sys, random
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

radius = 10.0 
desired_num_points = 10000000
voxel_size = 0.05 #radius / math.cbrt(desired_num_points);
sphere = ork_vdb.FloatGrid.createLevelSetSphere( "a", radius, vec3(0,0,0), voxel_size, 1.01)
outside = sphere.background

print(f"voxel_size:{voxel_size}")

#############################
# execute AX "voxel shader"
#############################

voxel_shader = """

vec3f@pos = getvoxelpws();
float@dist = length(vec3f@pos);
float@phi = atan2(vec3f@pos.z,vec3f@pos.x);
float@theta = atan2(vec3f@pos.y,vec3f@pos.x);
float@omega = atan2(vec3f@pos.z,vec3f@pos.y);

f@a = 1.0;
f@a = f@a * cos(float@phi*8.0)*0.5+0.5;
f@a = f@a * cos(float@theta*f$freq)*0.5+0.5;
f@a = f@a * cos(float@omega*f$freq)*0.5+0.5;

//if (f@a<0.5) {
//  deletepoint(); // only for point grids, not volume grids
//}

"""

cdata = ork_vdb.ax.CustomData()
ve = ork_vdb.ax.VolumeExecutable.compile(voxel_shader,cdata)

#############################
# draw dda lines in volume
#############################

def _draw_line(p1,p2,value):
  p1 = sphere.worldToIndex(p1)
  p2 = sphere.worldToIndex(p2)
  sphere.drawLineI(p1,p2,value)

nx = vec3(-radius,0,0)
px = vec3(+radius,0,0)
ny = vec3(0,-radius,0)
py = vec3(0,+radius,0)
nz = vec3(0,0,-radius)
pz = vec3(0,0,+radius)

_draw_line(nx,px,1.0)
_draw_line(ny,py,1.0)
_draw_line(nz,pz,1.0)

_draw_line(nx+ny+nz,px+ny+nz,1.0)
_draw_line(nx+py+nz,px+py+nz,1.0)
_draw_line(nx+ny+pz,px+ny+pz,1.0)
_draw_line(nx+py+pz,px+py+pz,1.0)

_draw_line(nx+ny+nz,nx+ny+pz,1.0)
_draw_line(px+ny+nz,px+ny+pz,1.0)
_draw_line(nx+py+nz,nx+py+pz,1.0)
_draw_line(px+py+nz,px+py+pz,1.0)

_draw_line(nx+ny+nz,nx+py+nz,1.0)
_draw_line(px+ny+nz,px+py+nz,1.0)
_draw_line(nx+ny+pz,nx+py+pz,1.0)
_draw_line(px+ny+pz,px+py+pz,1.0)

#############################
# pset random voxels
##############################

for i in range(1000):
  rx = random.uniform(-radius,radius)
  ry = random.uniform(-radius,radius)
  rz = random.uniform(-radius,radius)
  p = sphere.worldToIndex(vec3(rx,ry,rz)*0.1)
  sphere.setVoxel(p,1.0)

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
    
    self.points_prim = primitives.PointsPrimitiveV12C4.create(40<<20)
    self.points_prim.updateWithVdbFloatGrid(sphere,ctx)

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
      return 1.5

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
    cdata.set("freq",float(self.phi))
    ve.executeOnGrid(sphere)
    self.points_prim.updateWithVdbFloatGrid(sphere,context)
    self.scene.renderOnContext(context);

  ##############################################

  def onUiEvent(self,uievent):
    handled = self.uicam.uiEventHandler(uievent)
    if handled:
      self.camera.copyFrom( self.uicam.cameradata )
    return ui.HandlerResult()
###############################################################################

PointsPrimApp().ezapp.mainThreadLoop()
