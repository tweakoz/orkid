################################################################################
# Copyright 1996-2020, Michael T. Mayers.
# Distributed under the Boost Software License - Version 1.0 - August 17, 2003
# see http://www.boost.org/LICENSE_1_0.txt
################################################################################
import math
import random
import argparse
import sys
import ork.path

from orkengine.core import *
from orkengine.lev2 import *
# add parent dir to path
sys.path.append((thisdir()/".."/".."/"examples"/"python").normalized.as_string)
from common.primitives import createGridData
from common.cameras import *
from common.shaders import *
from common.misc import *
from common.scenegraph import createSceneGraph

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


class BasicUiCamSgApp(object):

    def __init__(self):
        super().__init__()
        self.ezapp = OrkEzApp.create(self)
        self.ezapp.setRefreshPolicy(RefreshFastest, 0)
        self.materials = set()
        setupUiCamera(app=self, eye=vec3(5, 5, 5), tgt=vec3(0, 0, 0))

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

    ################################################

    def onUpdate(self, updinfo):
        self.abstime = updinfo.absolutetime
        self.scene.updateScene(self.cameralut)

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
