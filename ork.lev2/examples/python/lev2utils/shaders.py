from orkengine.core import *
from orkengine import lev2
################################################################################
tokens = CrcStringProxy()

def createPipeline( app=None,
                    ctx=None,
                    shadername="myshader",
                    rendermodel="ForwardPBR",
                    depthtest=tokens.LEQUALS,
                    blending=tokens.OFF,
                    culltest=tokens.PASS_FRONT,
                    shaderfile=Path("orkshader://manip"),
                    shadertext=None,
                    techname = "std_mono_fwd" ):
    material = lev2.FreestyleMaterial()
    if shadertext!=None:
      material.gpuInitFromShaderText(ctx,shadername,shadertext)
    else:
      material.gpuInit(ctx,shaderfile)
    #
    material.rasterstate.blending = blending
    material.rasterstate.culltest = culltest
    material.rasterstate.depthtest = depthtest
    #
    permu = lev2.FxPipelinePermutation()
    permu.rendering_model = rendermodel
    permu.technique = material.shader.technique(techname)
    #
    pipeline = material.fxcache.findPipeline(permu) 
    pipeline.bindParam( material.param("mvp"), tokens.RCFD_Camera_MVP_Mono)
    #
    pipeline.sharedMaterial = material;
    return  pipeline

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
    float width = 8.0;

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

def pseudowire_pipeline(app = None,ctx=None):
    pipeline = createPipeline( app = app,
                               ctx = ctx,
                               shadertext = PSEUDOWIRE_SHADERTEXT,
                               blending=tokens.ADDITIVE,
                               culltest=tokens.OFF,
                               depthtest=tokens.OFF,
                               techname = "tek_pseudowire",
                               rendermodel = "ForwardPBR" )

    param_world = pipeline.sharedMaterial.param("m")
    param_modcolor = pipeline.sharedMaterial.param("modcolor")
    pipeline.bindParam( param_world, tokens.RCFD_M )
    pipeline.bindParam( param_modcolor, tokens.RCFD_MODCOLOR )
    return pipeline 

################################################################################

shadertext = """
fxconfig fxcfg_default {
    glsl_version = "410";
    import "orkshader://mathtools.i"
	import "orkshader://gbuftools.i"
	import "orkshader://misctools.i"
}
///////////////////////////////////////////////////////////////
uniform_set ublock_vtx {
  mat4 MatMVP;
  mat3 MatNormal;
}
///////////////////////////////////////////////////////////////
uniform_set ublock_frg {
  sampler2D ColorMap;
  sampler3D VolumeMap;
  float time;
}
///////////////////////////////////////////////////////////////
vertex_interface iface_vdefault : ublock_vtx {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
    vec3 normal : NORMAL;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec4 frg_clr;
    vec3 frg_pos;
    vec2 frg_uv0;
  }
}
vertex_interface iface_vstereo : iface_vdefault {
  outputs {
    layout(secondary_view_offset=1) int gl_Layer;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface iface_fdefault : ublock_frg {
  inputs {
    vec4 frg_clr;
    vec3 frg_pos;
    vec2 frg_uv0;
  }
  outputs { layout(location = 0) vec4 out_clr; }
}
///////////////////////////////////////////////////////////////
fragment_interface iface_pbr_deferred : ublock_frg {
    inputs {
        vec4 frg_clr;
	    vec2 frg_uv;
	}
	outputs {
		layout(location = 0) uvec4 out_gbuf;
	}
}
fragment_shader ps_deferred_emissive_fragclr
  : iface_pbr_deferred
  : lib_gbuf_encode {
	out_gbuf = packGbuffer_unlit(frg_clr.xyz);
}
///////////////////////////////////////////////////////////////
state_block sb_default : default { CullTest = PASS_FRONT; }
state_block sb_additive : sb_default { BlendMode = ADDITIVE; }
///////////////////////////////////////////////////////////////
vertex_shader vs_lines : iface_vdefault {
  gl_Position = MatMVP * position;
  frg_pos = position.xyz;
  frg_clr     = vtxcolor;
  frg_uv0     = uv0;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_frustum : iface_vdefault : lib_math {
  gl_Position = MatMVP * position;
  frg_pos = MatNormal*position.xyz;
  float light = 3*saturateF(dot(MatNormal*normal,vec3(0,0,1)));
  frg_clr     = vec4(vtxcolor.xyz*light,1);
  frg_uv0     = uv0;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_frustum_stereo
  : iface_vstereo
  : lib_math
  : extension(GL_NV_stereo_view_rendering)
  : extension(GL_NV_viewport_array2) {
  gl_Position = MatMVPL*position;
  gl_SecondaryPositionNV = MatMVPR*position;
  frg_pos = MatNormal*position.xyz;
  float light = 3*saturateF(dot(MatNormal*normal,vec3(0,0,1)));
  frg_clr     = vec4(vtxcolor.xyz*light,1);
  frg_uv0     = uv0;
  gl_Layer = 0;
  gl_ViewportMask[0] = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_texvtxcolor_noalpha : iface_fdefault {
  vec4 texc = texture(ColorMap, frg_uv0);
  out_clr   = vec4(texc.xyz * frg_clr.xyz, 1.0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_frustum 
  : iface_fdefault 
  : lib_math 
  : lib_mmnoise {
  // octave noise with volume texture
  float val = octavenoise(VolumeMap,frg_pos,vec3(1,0,0),0,8);
  val = pow(saturateF(val),2);
  vec3 color = vec3(val,val,val)*frg_clr.xyz;
  out_clr = vec4(1,0,1,1);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_frustum_pbr
  : iface_pbr_deferred
  : lib_math
  : lib_mmnoise {
  // octave noise with volume texture
  float val = octavenoise(VolumeMap,frg_pos,8);
  val = pow(saturateF(val),2);
  vec3 color = vec3(val,val,val)*frg_clr.xyz;
  out_clr = vec4(color,1);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_lines : iface_fdefault {
  out_clr = frg_clr;
}
///////////////////////////////////////////////////////////////
technique tek_frustum {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_frustum;
    fragment_shader = ps_frustum;
    state_block     = sb_additive;
  }
}
///////////////////////////////////////////////////////////////
technique tek_frustum_stereo {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_frustum_stereo;
    fragment_shader = ps_frustum_pbr;
    state_block     = sb_additive;
  }
}
///////////////////////////////////////////////////////////////
technique tek_lines {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_lines;
    fragment_shader = ps_lines;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
"""

class Shader(object):
  def __init__(self,ctx):
    super().__init__()
    ctx.makeCurrent()
    self._mtl = FreestyleMaterial()
    self._mtl.gpuInitFromShaderText(ctx,"frusprim",shadertext)
    self._tek_frustum = self._mtl.shader.technique("tek_frustum")
    self._tek_lines = self._mtl.shader.technique("tek_lines")
    self._par_mvp = self._mtl.shader.param("MatMVP")
    self._par_mnormal = self._mtl.shader.param("MatNormal")
    self._par_tex = self._mtl.shader.param("ColorMap")
    self._par_time = self._mtl.shader.param("time")
    self._par_tex3d = self._mtl.shader.param("VolumeMap")
  def beginNoise(self,RCFD,time):
    self._mtl.begin(self._tek_frustum,RCFD)
    self._mtl.bindParamFloat(self._par_time,time)
  def beginLines(self,RCFD):
    self._mtl.bindTechnique(self._tek_lines)
    self._mtl.begin(RCFD)
  def end(self,RCFD):
    self._mtl.end(RCFD)
  def bindMvpMatrix(self,mvpmtx):
    self._mtl.bindParamMatrix4(self._par_mvp,mvpmtx)
  def bindRotMatrix(self,rotmtx):
    self._mtl.bindParamMatrix3(self._par_mnormal,rotmtx)
  def bindColorTex(self,tex):
    self._mtl.bindParamTexture(self._par_tex,tex)
  def bindVolumeTex(self,tex):
    self._mtl.bindParamTexture(self._par_tex3d,tex)
