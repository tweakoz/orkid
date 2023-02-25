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

  # create a dataflow graph

  graphdata = dflow.GraphData.createShared()

  # instantiate modules

  ptc_pool   = graphdata.create("POOL",particles.Pool)
  emitter    = graphdata.create("EMIT",particles.NozzleEmitter)
  gravity    = graphdata.create("POOL",particles.Gravity)
  turbulence = graphdata.create("GRAV",particles.Turbulence)
  vortex     = graphdata.create("VORT",particles.Vortex)
  sprites    = graphdata.create("SPRI",particles.SpriteRenderer)

  ptc_pool.pool_size = 8192 # max number of particles in pool

  # connect modules in a chain configuration

  graphdata.connect( emitter.inputs.pool,    ptc_pool.outputs.pool )
  graphdata.connect( gravity.inputs.pool,    emitter.outputs.pool )
  graphdata.connect( turbulence.inputs.pool, gravity.outputs.pool )
  graphdata.connect( vortex.inputs.pool,     turbulence.outputs.pool )
  graphdata.connect( sprites.inputs.pool,    vortex.outputs.pool )

  # emitter module settings

  emitter.inputs.LifeSpan = 10
  emitter.inputs.EmissionRate = 800
  emitter.inputs.EmissionVelocity = 1
  emitter.inputs.DispersionAngle = 45
  emitter.inputs.Offset = vec3(1,2,3)

  # gravity module settings

  gravity.inputs.G = 1
  gravity.inputs.Mass = 1
  gravity.inputs.OthMass = 1
  gravity.inputs.MinDistance = 1
  gravity.inputs.Center = vec3(0,0,0)

  # turbulence module settings

  turbulence.inputs.Amount = vec3(1.5,1.5,1.5)

  # vortex module settins

  vortex.inputs.VortexStrength = 1.0
  vortex.inputs.OutwardStrength = 1.0
  vortex.inputs.Falloff = 1.0

  # create and return ParticlesDrawableData object

  ptc_data = ParticlesDrawableData()
  ptc_data.graphdata = graphdata

  return ptc_data
