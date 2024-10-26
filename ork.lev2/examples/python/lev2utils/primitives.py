from orkengine.core import *
from orkengine.lev2 import *
dflow = dataflow
tokens = CrcStringProxy()

def createCubePrim( ctx=None, size=1.0 ):
  cube_prim = primitives.CubePrimitive()
  cube_prim.size = size
  cube_prim.topColor = dvec4(1,0,1,1)
  cube_prim.bottomColor = dvec4(0.5,0.0,0.5,1)
  cube_prim.leftColor = dvec4(0.0,0.5,0.5,1)
  cube_prim.rightColor = dvec4(1.0,0.5,0.5,1)
  cube_prim.frontColor = dvec4(0.5,0.5,1.0,1)
  cube_prim.backColor = dvec4(0.5,0.5,0.0,1)
  cube_prim.gpuInit(ctx)
  return cube_prim


def createFrustumPrim( ctx=None, vmatrix=None, pmatrix=None, alpha = 1.0 ):
  frustum = dfrustum()
  frustum.set(vmatrix,pmatrix)
  frustum_prim = primitives.FrustumPrimitive()
  frustum_prim.topColor = dvec4(0.2,1.0,0.2,alpha)
  frustum_prim.bottomColor = dvec4(0.5,0.5,0.5,alpha)
  frustum_prim.leftColor = dvec4(0.2,0.2,1.0,alpha)
  frustum_prim.rightColor = dvec4(1.0,0.2,0.2,alpha)
  frustum_prim.nearColor = dvec4(0.0,0.0,0.0,alpha)
  frustum_prim.farColor = dvec4(1.0,1.0,1.0,alpha)
  frustum_prim.frustum = frustum
  frustum_prim.gpuInit(ctx)
  return frustum_prim


def createPointsPrimV12C4(ctx=None,numpoints=0):
  return primitives.PointsPrimitiveV12C4.create(numpoints)

def createGridData(extent=10.0,majordim=1,minordim=0.1):
  grid_data = GridDrawableData()
  grid_data.shader_suffix = "_V4"
  grid_data.modcolor = vec3(.7)
  grid_data.intensityA = 1.0*0.5
  grid_data.intensityB = 0.97*0.5
  grid_data.intensityC = 0
  grid_data.intensityD = 0
  grid_data.lineWidth = 0.025
  grid_data.extent =  extent
  grid_data.majorTileDim = majordim
  grid_data.minorTileDim = minordim
  return grid_data

def createBillboardData(texpath="lev2://textures/gridcell_blue.png"):
  bb_data = BillboardDrawableData()
  bb_data.texturepath = texpath
  return bb_data

def createGroundPlaneData(extent=10.0,pbrmaterial=None, pipeline=None):
  ground_data = GroundPlaneDrawableData()
  ground_data.extent = extent
  ground_data.pbrmaterial = pbrmaterial
  ground_data.pipeline = pipeline
  return ground_data

gradients = [GradientV4() for i in range(8)]

gradients[0].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,0,0,1),
  0.25: vec4(1,0,1,1),
  0.5: vec4(1,1,1,1),
  1.0: vec4(0,0,0,0),
})
gradients[1].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,0,0,1),
  0.25: vec4(1,0.8,0,1),
  0.5: vec4(1,1,0.5,1),
  1.0: vec4(0,0,0,0),
})
gradients[2].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,0,0,1),
  0.35: vec4(1,0.8,0,1),
  0.5: vec4(1,0,0,1),
  1.0: vec4(0,0,0,0),
})
gradients[3].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(0,0,1,1),
  0.35: vec4(0,0.8,1,1),
  0.5: vec4(0,0,1,1),
  1.0: vec4(0,0,0,0),
})
gradients[4].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(0,1,1,1),
  0.35: vec4(.5,1,1,1),
  0.5: vec4(0,1,1,1),
  1.0: vec4(0,0,0,0),
})
gradients[5].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(0,1,0,1),
  0.35: vec4(0,1,0,1),
  0.5: vec4(0,1,0,1),
  1.0: vec4(0,0,0,0),
})
gradients[6].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,1,1,1),
  0.35: vec4(1,1,1,1),
  0.5: vec4(1,1,1,1),
  1.0: vec4(0,0,0,0),
})
gradients[7].setColorStops({
  0.0: vec4(0,0,0,0),
  0.25: vec4(1,1,.5,1),
  0.35: vec4(1,1,.5,1),
  0.5: vec4(1,1,.5,1),
  1.0: vec4(0,0,0,0),
})

def presetGRAD(index):
  return gradients[index]  

################################################

def presetMaterial(grad=presetGRAD(0),texname="src://effect_textures/knob2"):
  material = particles.GradientMaterial.createShared()
  material.modulation_texture = Texture.load(texname)
  material.gradient = grad
  material.blending = tokens.ADDITIVE
  material.depthtest = tokens.LEQUALS
  return material

def createParticleData( use_streaks = True ):

  class ImplObject(object):
    def __init__(self):
      super().__init__()
      # create a dataflow graph
      self.graphdata = dflow.GraphData.createShared()

      # instantiate modules

      self.ptc_pool   = self.graphdata.create("POOL",particles.Pool)
      self.emitter    = self.graphdata.create("EMITN",particles.NozzleEmitter)
      self.emitter2    = self.graphdata.create("EMITR",particles.RingEmitter)
      self.globals    = self.graphdata.create("GLOB",particles.Globals)
      self.gravity    = self.graphdata.create("GRAV",particles.Gravity)
      self.turbulence = self.graphdata.create("TURB",particles.Turbulence)
      self.vortex     = self.graphdata.create("VORT",particles.Vortex)

      if use_streaks:
        self.streaks    = self.graphdata.create("STRK",particles.StreakRenderer)
      else:
        self.sprites    = self.graphdata.create("SPRI",particles.SpriteRenderer)

      self.ptc_pool.pool_size = 16384 # max number of particles in pool

      # connect modules in a chain configuration

      self.graphdata.connect( self.emitter.inputs.pool,    self.ptc_pool.outputs.pool )
      self.graphdata.connect( self.emitter2.inputs.pool,    self.emitter.outputs.pool )
      self.graphdata.connect( self.gravity.inputs.pool,    self.emitter2.outputs.pool )
      self.graphdata.connect( self.turbulence.inputs.pool, self.gravity.outputs.pool )
      self.graphdata.connect( self.vortex.inputs.pool,     self.turbulence.outputs.pool )

      if use_streaks:
        self.graphdata.connect( self.streaks.inputs.pool,    self.vortex.outputs.pool )
        self.streaks.inputs.Length = .1
        self.streaks.inputs.Width = .05
        self.streaks.material = presetMaterial()
      else:
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
