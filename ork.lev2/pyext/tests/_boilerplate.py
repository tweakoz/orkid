################################################################################
# Copyright 1996-2023, Michael T. Mayers.
# Distributed under the MIT License
# see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
################################################################################
import math, random, argparse, sys

from orkengine.core import *
from orkengine.lev2 import *
from ork.app.application import ComponentizedApplication

print(FxShaderTechnique)

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
  out_clr = vec4(frg_col,1);
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


class BasicUiCamSgApp(ComponentizedApplication):

    def __init__(self,ssaa=0):
        super().__init__()
        self.materials = set()
        setupUiCamera(app=self, eye=vec3(5, 5, 5), tgt=vec3(0, 0, 0))
        self.createEzApp(height=640,width=1280,ssaa=ssaa)

    ##############################################

    def _onUiInit(self):
        lg = self.ezapp.topLayoutGroup
        self._sgviewport_item = lg.makeChild(
            uiclass=ui.SceneGraphViewport,
            args=["PrimarySG"],
            fill=True
        )

    ##############################################

    def _onGpuInit(self, ctx,
                  add_grid=False,
                  cam_overlay=True,
                  params_dict = None):

        self.context = ctx

        # Create scene directly (NOT through ezapp.createScene/createSceneGraph
        # which overwrites enableUiDraw's onDraw callback, preventing UI overlay rendering)
        sceneparams = VarMap()
        sceneparams.preset = "ForwardPBR"
        sceneparams.SkyboxIntensity = float(1)
        sceneparams.SpecularIntensity = float(1)
        sceneparams.DiffuseIntensity = float(1)
        sceneparams.AmbientLight = vec3(0.0)
        sceneparams.DepthFogDistance = float(1e6)
        sceneparams.SkyboxTexPathStr = "nebula"

        rendermodel = "ForwardPBR"
        if params_dict:
            for k, v in params_dict.items():
                if k == "preset":
                    rendermodel = v
                setattr(sceneparams, k, v)
        sceneparams.preset = rendermodel

        self.scene = scenegraph.Scene(sceneparams)

        if rendermodel in ["ForwardPBR", "FWDPBRVR", "FWDPBRVRDM"]:
            layer_name = "std_forward"
        elif rendermodel in ["DeferredPBR", "PBRVR"]:
            layer_name = "std_deferred"
        else:
            layer_name = "std_forward"

        self.layer1 = self.scene.createLayer(layer_name)
        self.layer_std = self.layer1
        self.layer_dpp = self.scene.createLayer("depth_prepass")
        self.std_layers = [self.layer_std, self.layer_dpp]
        self.rendernode = self.scene.compositorrendernode
        self.outputnode = self.scene.compositoroutputnode

        # Connect SceneGraphViewport to scene
        sgviewport = self._sgviewport_item.widget
        sgviewport.cameraName = "spawncam"
        sgviewport.scenegraph = self.scene
        sgviewport.forkDB()
        sgviewport.evhandler = lambda ev: self._onUiEvent(ev)
        sgviewport.ignoreEvents = False
        self.scene.lightingmanager.gpuInit(ctx)

        if cam_overlay:
            self.cam_overlay = self.layer1.createDrawableNode(
                "camoverlay", self.uicam.createDrawable())
        if add_grid:
            self.grid_data = createGridData()
            self.grid_node = self.layer1.createDrawableNodeFromData("grid", self.grid_data)
            self.grid_node.sortkey = 1

    ##############################################

    def _onGpuUpdate(self, ctx):
        pass

    ##############################################

    def _onUiEvent(self, uievent):
      handled = self.uicam.uiEventHandler(uievent)
      if handled:
        self.camera.copyFrom(self.uicam.cameradata)
      return ui.HandlerResult()

    ################################################

    def _onUpdate(self, updinfo):
        self.abstime = updinfo.absolutetime
        self.scene.updateScene(self.cameralut)
        self._sgviewport_item.widget.setDirty()

    ################################################

    def createPipeline(self,
                       name="unnamed",
                       rendermodel="ForwardPBR",
                       depthtest=tokens.LEQUALS,
                       blending=tokens.OFF,
                       culltest=tokens.PASS_FRONT,
                       shaderfile="orkshader://manip",
                       shadertext=None,
                       techname="std_mono_fwd"):

        material = FreestyleMaterial()
        material.name = name
        if shadertext != None:
            material.gpuInitFromShaderText(
                self.context, "myshader", shadertext)
        else:
            material.gpuInit(self.context, shaderfile)
        #
        material.rasterstate.setBlendingMacro(blending)
        material.rasterstate.culltest = culltest
        material.rasterstate.depthtest = depthtest
        #
        permu = FxPipelinePermutation(rendermodel = rendermodel)
        permu.technique = material.shader.technique(techname)
        #
        pipeline = material.fxcache.findPipeline(permu)
        pipeline.name = name
        print(f"shaderfile<{shaderfile}> shader<{material.shader}> mtlnam: {name} tek nam:{techname} tek:{permu.technique} pip:{pipeline}")
        pipeline.bindParam(material.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
        #
        pipeline.sharedMaterial = material
        return pipeline

    def createPbrPipeline(self, ctx, rendermodel="ForwardPBR"):

        material = PBRMaterial()
        white = Image.createFromFile("src://effect_textures/white_64.dds")
        nrmap = Image.createFromFile("src://effect_textures/default_normal.dds")
        material.assignImages(
          ctx,
          color = white,
          normal = nrmap,
          mtlruf = white,
          doConform=True
        )
        #
        permu = FxPipelinePermutation()
        
        permu.rendermodel = rendermodel
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
                                   shaderfile="orkshader://basic",
                                   techname="tek_fnormal_wire")
    def createVtxColorPipeline(self):
        return self.createPipeline(rendermodel="ForwardPBR",
                                   shaderfile="orkshader://basic",
                                   techname="tek_vtxcolor")
    def createPointsPipeline(self):
        pipeline =  self.createPipeline( shadertext = SHADERTEXT_POINTS,
                                         blending=tokens.ADDITIVE,
                                         depthtest=tokens.LEQUALS,
                                         techname = "tek_points_fwd",
                                         rendermodel = "ForwardPBR" )
        
        param_psize = pipeline.sharedMaterial.param("pointsize")
        param_modcolor = pipeline.sharedMaterial.param("modcolor")
        pipeline.bindParam(param_psize, float(4))
        pipeline.bindParam(param_modcolor, vec4(1,1,1,1))
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

    def createPbrDrawableFromVertsAndFaces(self, ctx, verts, faces, scale):
        material = PBRMaterial()
        white = Image.createFromFile("src://effect_textures/white_64.dds")
        nrmap = Image.createFromFile("src://effect_textures/default_normal.dds")
        material.assignImages(
          ctx,
          color = white,
          normal = nrmap,
          mtlruf = white,
          doConform=True
        )
        #this_pipeline.bindParam( material.param("m"), tokens.RCFD_M)
        result_submesh = lev2.meshutil.SubMesh.createFromDict({
            "vertices": [{  "p": vec3(item[0], item[1], item[2])*scale} for item in verts],
            "faces": faces
        })
        barysubmesh = result_submesh.withBarycentricUVs()
        union_prim = lev2.RigidPrimitive(barysubmesh,ctx)
        union_sgnode = union_prim.createNode("union",self.layer1,material)
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

    
