from orkengine.core import *
from orkengine.lev2 import *

def createCubePrim( ctx=None, size=1.0 ):
  cube_prim = primitives.CubePrimitive()
  cube_prim.size = size
  cube_prim.topColor = vec4(1,0,1,1)
  cube_prim.bottomColor = vec4(0.5,0.0,0.5,1)
  cube_prim.leftColor = vec4(0.0,0.5,0.5,1)
  cube_prim.rightColor = vec4(1.0,0.5,0.5,1)
  cube_prim.frontColor = vec4(0.5,0.5,1.0,1)
  cube_prim.backColor = vec4(0.5,0.5,0.0,1)
  cube_prim.gpuInit(ctx)
  return cube_prim


def createFrustumPrim( ctx=None, vmatrix=None, pmatrix=None, alpha = 1.0 ):
  frustum = Frustum()
  frustum.set(vmatrix,pmatrix)
  frustum_prim = primitives.FrustumPrimitive()
  frustum_prim.topColor = vec4(0.2,1.0,0.2,alpha)
  frustum_prim.bottomColor = vec4(0.5,0.5,0.5,alpha)
  frustum_prim.leftColor = vec4(0.2,0.2,1.0,alpha)
  frustum_prim.rightColor = vec4(1.0,0.2,0.2,alpha)
  frustum_prim.nearColor = vec4(0.0,0.0,0.0,alpha)
  frustum_prim.farColor = vec4(1.0,1.0,1.0,alpha)
  frustum_prim.frustum = frustum
  frustum_prim.gpuInit(ctx)
  return frustum_prim


def createPointsPrimV12C4(ctx=None,numpoints=0):
  return primitives.PointsPrimitiveV12C4.create(numpoints)

def createGridData(extent=10.0,majordim=1,minordim=0.1):
  grid_data = GridDrawableData()
  grid_data.extent = extent
  grid_data.majorTileDim = majordim
  grid_data.minorTileDim = minordim
  grid_data.texturepath = "lev2://textures/gridcell_blue.png"
  return grid_data

