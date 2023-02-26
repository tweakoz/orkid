from orkengine.core import *
from orkengine.lev2 import *
dflow = dataflow

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


def createParticleData():

  class ImplObject(object):
    def __init__(self):
      super().__init__()
      # create a dataflow graph
      self.graphdata = dflow.GraphData.createShared()

      # instantiate modules

      self.ptc_pool   = self.graphdata.create("POOL",particles.Pool)
      self.emitter    = self.graphdata.create("EMITN",particles.NozzleEmitter)
      self.emitter2    = self.graphdata.create("EMITR",particles.RingEmitter)
      self.gravity    = self.graphdata.create("POOL",particles.Gravity)
      self.turbulence = self.graphdata.create("GRAV",particles.Turbulence)
      self.vortex     = self.graphdata.create("VORT",particles.Vortex)
      self.sprites    = self.graphdata.create("SPRI",particles.SpriteRenderer)

      self.ptc_pool.pool_size = 16384 # max number of particles in pool

      # connect modules in a chain configuration

      self.graphdata.connect( self.emitter.inputs.pool,    self.ptc_pool.outputs.pool )
      self.graphdata.connect( self.emitter2.inputs.pool,    self.emitter.outputs.pool )
      self.graphdata.connect( self.gravity.inputs.pool,    self.emitter2.outputs.pool )
      self.graphdata.connect( self.turbulence.inputs.pool, self.gravity.outputs.pool )
      self.graphdata.connect( self.vortex.inputs.pool,     self.turbulence.outputs.pool )
      self.graphdata.connect( self.sprites.inputs.pool,    self.vortex.outputs.pool )

      # emitter module settings

      self.emitter.inputs.LifeSpan = 10
      self.emitter.inputs.EmissionRate = 800
      self.emitter.inputs.EmissionVelocity = 1
      self.emitter.inputs.DispersionAngle = 45
      self.emitter.inputs.Offset = vec3(1,2,3)

      self.emitter2.inputs.LifeSpan = 10
      self.emitter2.inputs.EmissionRate = 800
      self.emitter2.inputs.EmissionRadius = 2
      self.emitter2.inputs.EmitterSpinRate = 1
      self.emitter2.inputs.EmissionVelocity = 1
      self.emitter2.inputs.DispersionAngle = 45
      self.emitter2.inputs.Offset = vec3(0,4,0)

      # gravity module settings

      self.gravity.inputs.G = 1
      self.gravity.inputs.Mass = 1
      self.gravity.inputs.OthMass = 1
      self.gravity.inputs.MinDistance = 1
      self.gravity.inputs.Center = vec3(0,0,0)

      # turbulence module settings

      self.turbulence.inputs.Amount = vec3(1.5,1.5,1.5)

      # vortex module settins

      self.vortex.inputs.VortexStrength = 1.0
      self.vortex.inputs.OutwardStrength = 1.0
      self.vortex.inputs.Falloff = 1.0

      # create and return ParticlesDrawableData object

      self.drawable_data = ParticlesDrawableData()
      self.drawable_data.graphdata = self.graphdata

  return ImplObject()
