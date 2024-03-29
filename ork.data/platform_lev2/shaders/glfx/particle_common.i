///////////////////////////////////////////////////////////////
// Uniforms
///////////////////////////////////////////////////////////////

uniform_set uset_vtx {
  mat4 MatM;
  mat4 MatV;
  mat4 MatP;
  mat4 MatVP;
  mat4 MatIV;
  mat4 MatIVP;
  mat4 MatMV;
  mat4 MatMVP;
  mat4 MatAux;
  vec4 modcolor;
  vec4 User0;
  vec4 User1;
  vec4 User2;
  vec4 User3;
  float Time;
  vec4 NoiseShift;
  vec4 NoiseFreq;
  vec4 NoiseAmp;
  vec2 Rtg_InvDim;
}

uniform_set uset_geo_stereo {
  mat4 MatMVPL;
  mat4 MatMVPR;
}

uniform_set uset_frg {
  vec4 modcolor;
  vec4 User0;
  sampler2D ColorMap;
  sampler2D GradientMap;
  sampler3D VolumeMap;
  float ColorFactor;
  float AlphaFactor;
}

uniform_set compute_unis {
    //layout (binding = 1, r32ui) uimage2D img_depthclusters;
    vec3 xxx;
}

///////////////////////////////////////////////////////////////
// StateBlocks
///////////////////////////////////////////////////////////////
state_block sb_default : default {
  // DepthTest=LEQUALS;
  // DepthMask=true;
  // CullTest=PASS_FRONT;
}
///////////////////////////////////////////////////////////////
state_block sb_lerpblend : sb_default {
  BlendMode = ADDITIVE;
}
///////////////////////////////////////////////////////////////
state_block sb_additive : sb_default {
  BlendMode = ADDITIVE;
}
///////////////////////////////////////////////////////////////
state_block sb_alpadd : sb_default {
  BlendMode = ALPHA_ADDITIVE;
}

///////////////////////////////////////////////////////////////
// Interfaces
///////////////////////////////////////////////////////////////

fragment_interface fface_minimal : uset_frg {
  inputs {
    vec4 frg_clr;
    vec2 frg_uv0;
    vec2 frg_uv1;
  }
  outputs {
    vec4 out_clr;
  }
}
fragment_interface fface_psys : uset_frg {
  inputs {
    vec4 frg_clr;
    vec2 frg_uv0;
    vec2 frg_uv1;
  }
  outputs {
    layout(location = 0) vec4 out_clr;
    //layout(location = 1) vec4 out_normal_mdl;
    //layout(location = 2) vec4 out_rufmtl;
  }
}

///////////////////////////////////////////////////////////////
vertex_interface vface_nogs : uset_vtx {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
    vec2 uv0 : TEXCOORD0;
    vec2 uv1 : TEXCOORD1;
  }
  outputs {
    vec4 frg_clr;
    vec2 frg_uv0;
    vec2 frg_uv1;
  }
}
///////////////////////////////////////////////////////////////
vertex_interface vface_bill : uset_vtx {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
    vec2 uv0 : TEXCOORD0;
    vec2 uv1 : TEXCOORD1;
  }
  outputs {
    vec4 geo_clr; // NOT an array
    vec2 geo_uv0; // NOT an array
    vec2 geo_uv1; // NOT an array
  }
}
///////////////////////////////////////////////////////////////
fragment_shader ps_flat : fface_psys {
  float unit_age = frg_uv1.x;
  vec3 C  = modcolor.xyz*(1.0-unit_age);
  out_clr = vec4(C, modcolor.w);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_modclr : fface_psys {
  out_clr        = modcolor;
  out_rufmtl     = vec4(0, 1, 0, 0);
  out_normal_mdl = vec4(0, 0, 0, 5.0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_fragclr : fface_minimal {
  out_clr        = frg_clr;
  out_rufmtl     = vec4(0, 1, 0, 0);
  out_normal_mdl = vec4(0, 0, 0, 5.0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_modtex : fface_psys {
  vec4 texc      = texture(ColorMap, frg_uv0.xy);
  out_clr        = texc * modcolor;
  out_rufmtl     = vec4(0, 1, 0, 0);
  out_normal_mdl = vec4(0, 0, 0, 5.0);
  if (out_clr.a == 0.0)
    discard;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_grad : fface_psys {
  float unit_age = frg_uv1.x;
  vec4 gmap = texture(GradientMap, vec2(unit_age, 0.0));
  vec4 cmap = texture(ColorMap, frg_uv0.xy);
  out_clr.xyz = (gmap.xyz*cmap.xyz)*ColorFactor;
  out_clr.w = gmap.w*cmap.w*AlphaFactor;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_modtexclr : fface_psys {

  //float alp      = out_clr.r * User0.r + out_clr.g * User0.g + out_clr.b * User0.b + out_clr.a * User0.a;
  vec4 texc      = texture(ColorMap, frg_uv0.xy);
  float alp      = frg_clr.a*texc.a;
  if (alp == 0.0)
    discard;
  float unit_age = frg_uv1.x;
  out_rufmtl     = vec4(0, 1, 0, 0);
  out_normal_mdl = vec4(0, 0, 5, 0);
  out_clr        = texc*modcolor*(1.0-unit_age); // *  * frg_clr;
  out_clr.a      = alp;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_volume : fface_psys {
  vec2 uv        = frg_uv0.xy;
  float w        = frg_uv1.x;
  vec3 uvw       = vec3(uv, w);
  vec4 TexInp0   = texture(VolumeMap, uvw).xyzw;
  out_clr        = TexInp0.bgra * modcolor * frg_clr.bgra;
  out_rufmtl     = vec4(0, 1, 0, 0);
  out_normal_mdl = vec4(0, 0, 0, 5.0);
}

