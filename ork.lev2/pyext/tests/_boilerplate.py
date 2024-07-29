################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, random, argparse, sys, signal

from orkengine.core import *
from orkengine.lev2 import *

################################################################################

l2exdir = (lev2exdir()/"python").normalized.as_string
sys.path.append(l2exdir) # add parent dir to path
from lev2utils.primitives import createGridData
from lev2utils.cameras import *
from lev2utils.shaders import *
from lev2utils.misc import *
from lev2utils.scenegraph import createSceneGraph

tokens = CrcStringProxy()
constants = mathconstants()

################################################################################

PSEUDOWIRE_SHADERTEXT = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 m;
  mat4 mvp;
}
////////////////////////////////////////
uniform_set ublock_frg {
  vec4 modcolor;
}
////////////////////////////////////////
vertex_interface iface_vtx : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec3 nrm : NORMAL;
    vec3 uvQ : BINORMAL;
  }
  outputs {
    vec3 world_nrm;
    vec3 world_pos;
    vec3 frg_uvq;
  }
}
////////////////////////////////////////
fragment_interface iface_frg : ublock_frg {
  inputs {
    vec3 world_nrm;
    vec3 world_pos;
    vec3 frg_uvq;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_pseudowire : iface_vtx {
  gl_Position =  mvp * pos;
  world_pos = (m * pos).xyz;
  world_nrm = (m * vec4(nrm,0)).xyz;
  frg_uvq = uvQ;
}
////////////////////////////////////////
fragment_shader ps_pseudowire : iface_frg {
  
    float intens = 0.0;
    float width = 12.0;

    // https://www.reedbeta.com/blog/quadrilateral-interpolation-part-1/
    vec2 UV = frg_uvq.xy / frg_uvq.z;

    //vec2 param_space = mod(UV*10,1);
    vec2 param_space = UV;

    vec2 df = fwidth(param_space);
    float dd = min(df.x,df.y);
    
    width *= dd;

    if(param_space.x<width)
        intens += 1;
    if(param_space.y<width)
        intens += 1;
    if((1-param_space.x)<width)
        intens += 1;
    if((1-param_space.y)<width)
        intens += 1;
    if(intens>0)
        intens = 1;
    else
        intens = 0;


    out_clr = vec4(modcolor.xyz*intens,1);
}

////////////////////////////////////////
technique tek_pseudowire {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_pseudowire;
    fragment_shader = ps_pseudowire;
    state_block     = default;
  }
}
"""

################################################################################

SHADERTEXT_POINTS = """
////////////////////////////////////////
fxconfig fxcfg_default { glsl_version = "330"; }
////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 mvp;
  float pointsize;
}
////////////////////////////////////////
uniform_set ublock_frg {
  vec4 modcolor;
}
////////////////////////////////////////
vertex_interface iface_vtx_points : ublock_vtx {
  inputs {
    vec4 pos : POSITION;
    vec4 col : COLOR0;
  }
  outputs {
    vec3 frg_col;
  }
}
////////////////////////////////////////
fragment_interface iface_frg_points : ublock_frg {
  inputs {
    vec3 frg_col;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
////////////////////////////////////////
vertex_shader vs_points : iface_vtx_points {
  frg_col = col.xyz;
  gl_Position = mvp * vec4(pos.x,pos.y,pos.z,1);
  gl_PointSize = pointsize;
}
////////////////////////////////////////
fragment_shader ps_points : iface_frg_points {
  out_clr = modcolor;
}

////////////////////////////////////////
technique tek_points_fwd {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_points;
    fragment_shader = ps_points;
    state_block     = default;
  }
}
"""
################################################################################


class BasicUiCamSgApp(object):

    def __init__(self,ssaa=0):
        super().__init__()
        self.ezapp = OrkEzApp.create(self,height=640,width=1280,ssaa=ssaa)
        self.ezapp.setRefreshPolicy(RefreshFastest, 0)
        self.materials = set()
        setupUiCamera(app=self, eye=vec3(5, 5, 5), tgt=vec3(0, 0, 0))
        ##################################
        def onCtrlC(signum, frame):
          print("signalling EXIT to ezapp")
          self.onExitSignal()
          self.ezapp.signalExit()
        ##################################
        signal.signal(signal.SIGINT, onCtrlC)
        ##################################

    ##############################################

    def onGpuInit(self, ctx, add_grid=False, cam_overlay=True):
        self.context = ctx
        createSceneGraph(app=self, rendermodel="ForwardPBR")
        if cam_overlay:
            self.cam_overlay = self.layer1.createDrawableNode(
                "camoverlay", self.uicam.createDrawable())
        if add_grid:
            self.grid_data = createGridData()
            self.grid_node = self.layer1.createGridNode("grid", self.grid_data)
            self.grid_node.sortkey = 1

    ##############################################

    def onGpuIter(self):
        pass

    ##############################################

    def onUiEvent(self, uievent):
      handled = self.uicam.uiEventHandler(uievent)
      if handled:
        self.camera.copyFrom(self.uicam.cameradata)
      return ui.HandlerResult()

    ################################################

    def onUpdate(self, updinfo):
        self.abstime = updinfo.absolutetime
        self.scene.updateScene(self.cameralut)

    ################################################

    def onExitSignal(self):
        pass

    ################################################

    def createPipeline(self,
                       rendermodel="ForwardPBR",
                       depthtest=tokens.LEQUALS,
                       blending=tokens.OFF,
                       culltest=tokens.PASS_FRONT,
                       shaderfile=Path("orkshader://manip"),
                       shadertext=None,
                       techname="std_mono_fwd"):

        material = FreestyleMaterial()
        if shadertext != None:
            material.gpuInitFromShaderText(
                self.context, "myshader", shadertext)
        else:
            material.gpuInit(self.context, shaderfile)
        #
        material.rasterstate.blending = blending
        material.rasterstate.culltest = culltest
        material.rasterstate.depthtest = depthtest
        #
        permu = FxPipelinePermutation()
        permu.rendering_model = rendermodel
        permu.technique = material.shader.technique(techname)
        #
        pipeline = material.fxcache.findPipeline(permu)
        pipeline.bindParam(material.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
        #
        pipeline.sharedMaterial = material
        return pipeline

    def createPbrPipeline(self,
                       rendermodel="ForwardPBR"):

        material = PBRMaterial()
        #
        permu = FxPipelinePermutation()
        permu.rendering_model = rendermodel
        #permu.technique = material.shader.technique(techname)
        #
        pipeline = material.fxcache.findPipeline(permu)
        pipeline.bindParam(material.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
        #
        pipeline.sharedMaterial = material
        return pipeline

    def createPseudoWirePipeline(self):
        pipeline = self.createPipeline(shadertext=PSEUDOWIRE_SHADERTEXT,
                                       blending=tokens.ADDITIVE,
                                       culltest=tokens.OFF,
                                       depthtest=tokens.OFF,
                                       techname="tek_pseudowire",
                                       rendermodel="ForwardPBR")

        param_world = pipeline.sharedMaterial.param("m")
        param_modcolor = pipeline.sharedMaterial.param("modcolor")
        pipeline.bindParam(param_world, tokens.RCFD_M)
        pipeline.bindParam(param_modcolor, tokens.RCFD_MODCOLOR)
        return pipeline

    def createBaryWirePipeline(self):
        return self.createPipeline(rendermodel="ForwardPBR",
                                   shaderfile=Path("orkshader://basic"),
                                   techname="tek_fnormal_wire")
    def createPointsPipeline(self):
        pipeline =  self.createPipeline( shadertext = SHADERTEXT_POINTS,
                                         blending=tokens.ADDITIVE,
                                         depthtest=tokens.LEQUALS,
                                         techname = "tek_points_fwd",
                                         rendermodel = "ForwardPBR" )
        
        param_psize = pipeline.sharedMaterial.param("pointsize")
        param_modcolor = pipeline.sharedMaterial.param("modcolor")
        pipeline.bindParam(param_psize, float(16))
        pipeline.bindParam(param_modcolor, vec4(1,0,0,1))
        return pipeline

    ################################################

    def createBaryDrawableFromVertsAndFaces(self, ctx, verts, faces, scale):
        solid_wire_pipeline =  self.createBaryWirePipeline()
        material = solid_wire_pipeline.sharedMaterial
        solid_wire_pipeline.bindParam( material.param("m"), tokens.RCFD_M)
        result_submesh = lev2.meshutil.SubMesh.createFromDict({
            "vertices": [{  "p": vec3(item[0], item[1], item[2])*scale} for item in verts],
            "faces": faces
        })
        barysubmesh = result_submesh.withBarycentricUVs()
        union_prim = lev2.RigidPrimitive(barysubmesh,ctx)
        union_sgnode = union_prim.createNode("union",self.layer1,solid_wire_pipeline)
        union_sgnode.enabled = True
        return (barysubmesh,union_prim, union_sgnode)

    ################################################

################################################################################

def stripSubmesh(inpsubmesh):
  stripped = inpsubmesh.copy(preserve_normals=False,
                            preserve_colors=False,
                            preserve_texcoords=False)
  return stripped

################################################################################

def clipMeshWithPlane(inpsubmesh,plane,debug=False):
  clipped = inpsubmesh.clippedWithPlane(plane=plane,
                                        close_mesh=True, 
                                        flip_orientation=False,
                                        debug=debug )
  return clipped#.convexHull(0)

################################################################################

def dumpMeshVertices(inpsubmesh):
  for i,a in enumerate(inpsubmesh.vertices):
    print(i,a.position)

################################################################################

def clipMeshWithFrustum(inpsubmesh,frustum, nsteps=10, debug=False):
  #print("#####################")
  submesh_out = inpsubmesh
  if nsteps>0:
    submesh_out = stripSubmesh(inpsubmesh).prune()
  if nsteps>1:
    if debug:
       print("### CLIP_NEAR_PLANE")
    submesh_out = clipMeshWithPlane(submesh_out,frustum.nearPlane,debug=debug).prune()
  if nsteps>2:
    if debug:
      print("### CLIP_FAR_PLANE")
    submesh_out = clipMeshWithPlane(submesh_out,frustum.farPlane,debug=debug).prune()
  if nsteps>3:
    if debug:
      print("### CLIP_LEFT_PLANE")
    submesh_out = clipMeshWithPlane(submesh_out,frustum.leftPlane,debug=debug).prune()
  if nsteps>4:
    if debug:
      print("### CLIP_RIGHT_PLANE")
    submesh_out = clipMeshWithPlane(submesh_out,frustum.rightPlane,debug=debug).prune()
  if nsteps>5:
    if debug:
      print("### CLIP_TOP_PLANE")
    submesh_out = clipMeshWithPlane(submesh_out,frustum.topPlane,debug=debug).prune()
  if nsteps>6:
    if debug:
      print("### CLIP_BOTTOM_PLANE")
    submesh_out = clipMeshWithPlane(submesh_out,frustum.bottomPlane,debug=debug).prune()

  return submesh_out

    