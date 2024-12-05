#!/usr/bin/env ork.python

import sys, time, threading, math, random
from os import path as os_path
from obt import deco
from obt import path as obt_path
from ork import path as ork_path

sys.path.append(str(ork_path.py_lev2utils)) # add parent dir to path

from orkengine.core import vec2,vec3,vec4,quat,mtx4
from orkengine.core import CrcStringProxy, lev2_pyexdir
from orkengine import lev2

from testapp import TestSystem
from shaders import POINTCLOUD_SHADERTEXT, createPipeline

deco = deco.Deco()
tokens = CrcStringProxy()

#############################################

CLEAR_GRID_PER_FRAME = False
VOXEL_GRID_SIZE = 0.0125
POINT_SIZE = 1.0
MAX_TILE_UPDATE_RATE = 1
  
print( "LEAF_DIM: ", lev2.vdb.TestGrid.leaf_dim )
print( "INT2_DIM: ", lev2.vdb.TestGrid.int2_dim )
print( "INT1_DIM: ", lev2.vdb.TestGrid.int1_dim )
print( "LEAF_MAX_VOXELS: ", lev2.vdb.TestGrid.leaf_dim )
print( "INT2_MAX_VOXELS: ", lev2.vdb.TestGrid.int2_max_voxels )
print( "INT1_MAX_VOXELS: ", lev2.vdb.TestGrid.int1_max_voxels )


#############################################
# main
#############################################

class System (TestSystem):

  #############################

  def newGrid(self):
    self.voxel_grid = lev2.vdb.TestGrid.create( "rgb",              # element name
                                                self.voxel_xform,   # voxel<>world mapping
                                                -1 )                # background

  #############################
  def __init__(self,app):
    super().__init__()

    self.app = app
    self.voxel_xform = lev2.vdb.Transform.create(VOXEL_GRID_SIZE)
    self.newGrid()
    
    self.voxel_grid_gpu_upd = None
    self.voxel_grid_gpu_cur = None
    self.color_scale = 1.0

    #################################################################
    def run_loop():
      
      time.sleep(1)
      
      i = 0
      while(not self.ok_to_exit):
        if( i % 1000 == 0 ):
          time.sleep(0.01)
          
        curindex = i/10000.0
        curtime = curindex/3.7
        x = random.uniform(-1.0,1.0)
        y = random.uniform(-1.0,1.0)
        z = random.uniform(-1.0,1.0)
        
        c = math.sin(curtime*2.1)*0.5+0.5
        hsv = vec3(c,1.0,1.0)
        c = hsv.hsv2rgb()
        
        sign_x = 1.0 if x > 0.0 else -1.0
        sign_y = 1.0 if y > 0.0 else -1.0
        sign_z = 1.0 if z > 0.0 else -1.0
        x = abs(x)
        y = abs(y)
        z = abs(z)
        power = 0.25
        x = math.pow(x,power)
        y = math.pow(y,power)
        z = math.pow(z,power)
        x = sign_x * x
        y = sign_y * y
        z = sign_z * z
        
        scale = 4.0
        pos = vec3(x,y,z)*scale
        #coord = self.voxel_grid.worldToIndex(pos)
        #leaf_count = self.voxel_grid.leafCount
        #nonleaf_count = self.voxel_grid.nonLeafCount
        #print("pos<%s> coord<%s> leafcount<%d> nonleafcount<%d>" % (pos,coord,leaf_count,nonleaf_count))
        self.voxel_grid.accumVoxelRGB(pos,c*0.05)
        

        if (None == self.voxel_grid_gpu_upd):
          #self.voxel_grid_gpu_upd = self.voxel_grid.clone
          if CLEAR_GRID_PER_FRAME: # clear previous grid ?
            self.newGrid()
            
        i = i + 1

        if (i % 10000 == 0):
          #self.voxel_grid.tileStats()
          self.voxel_grid_gpu_upd = self.voxel_grid.clone



      print("thread ended")

    #################################################################
    self.thr = threading.Thread(target=run_loop)
    self.thr.start()

  #############################

  def onGpuInit(self,ctx):
    super().onGpuInit(ctx)
    self.points_prim = lev2.primitives.TiledPointsPrimitiveV12C4.create(100<<20)
    self.points_prim.max_tile_update_rate = MAX_TILE_UPDATE_RATE
    self.gpu_upd_timer = -10

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

    pointsize_param = pipeline.sharedMaterial.param("pointsize")
    pipeline.bindParam( pointsize_param, POINT_SIZE ) # set pointsize

    ##################
    # create points sg node
    ##################

    self.primnode = self.points_prim.createNode("node1",self.app.layer1,pipeline)
    self.primnode.sortkey = 2;

  #############################

  def onGpuUpdate(self,ctx):

    super().onGpuUpdate(ctx)

    should_update_prim = True 
    should_update_prim = should_update_prim and (self.voxel_grid_gpu_upd is not None)
    should_update_prim = should_update_prim and (self.voxel_grid_gpu_upd != self.voxel_grid_gpu_cur)    
    if should_update_prim:
      self.points_prim.updateWithVdbTestGrid(self.voxel_grid_gpu_upd,self.color_scale,ctx)
      self.voxel_grid_gpu_cur = self.voxel_grid_gpu_upd
      self.voxel_grid_gpu_upd = None

  #############################

