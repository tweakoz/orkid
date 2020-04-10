from orkcore import *
from lev2 import *
################################################################################

shadertext = """
fxconfig fxcfg_default {
    glsl_version = "410";
    import "orkshader://mathtools.i"
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
fragment_shader ps_texvtxcolor_noalpha : iface_fdefault {
  vec4 texc = texture(ColorMap, frg_uv0);
  out_clr   = vec4(texc.xyz * frg_clr.xyz, 1.0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_frustum : iface_fdefault : lib_math {
  // octave noise with volume texture
  int numoctaves = 8;
  float val = 0;
  float freq = 1.0;
  float amp = 0.25;
  float timesh = time;
  for( int i=0; i<numoctaves; i++ ){
    vec3 uvw = frg_pos*freq;
    uvw += vec3(timesh*0.1/freq);
    val += texture(VolumeMap,uvw).x*amp;
    freq *= 0.7;
    amp *= 0.8;
    timesh *= 0.5;
  }
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
    self._mtl.bindTechnique(self._tek_frustum)
    self._mtl.begin(RCFD)
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
