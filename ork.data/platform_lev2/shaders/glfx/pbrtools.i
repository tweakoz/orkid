import "gbuftools.i";
import "brdftools.i";
import "deftools.i";
import "fwdtools.i";
import "skintools.i";
///////////////////////////////////////////////////////////////
// Interfaces
///////////////////////////////////////////////////////////////
uniform_set ub_vtx {
  mat4 m;
  mat4 v;
  mat4 mv;
  mat4 vp;
  mat4 v_l;
  mat4 v_r;
  mat4 vp_l;
  mat4 vp_r;
  mat4 inv_vp;
  mat4 inv_vp_l;
  mat4 inv_vp_r;
  mat4 mvp;
  mat4 mvp_l;
  mat4 mvp_r;
  mat3 mrot;
  vec4 modcolor;
  vec2 InvViewportSize; // inverse target size
  sampler2D InstanceMatrices;
  sampler2D InstanceColors;
  usampler2D InstanceIds;
}
///////////////////////////////////////////////////////////////
uniform_set ub_frg {

  sampler2DArray CNMREA; // 4
  sampler2D EmissiveMap;

  samplerCube reflectionPROBE;
  samplerCube irradiancePROBE;
  vec4 BaseColor;
  vec4 ModColor;
  uint obj_pickID;
  mat4 v;
  vec2 InvViewportSize; // inverse target size
  float MetallicFactor;
  float RoughnessFactor;
  float RoughnessPower;
  vec3 EyePostion;
  vec4 AuxA;
  vec4 AuxB;
}

uniform_set ub_frg_fwd {

  vec4 BaseColor;

  sampler2D SSAOMap;      // 0
  sampler2D SSAOKernel;   // 1
  sampler2D SSAOScrNoise; // 2

  sampler2D MapBrdfIntegration; // 3
  sampler2D MapSpecularEnv;     // 4
  sampler2D MapDiffuseEnv;      // 5
  sampler2D MapDepth;           // 6
  sampler2D MapLinearDepth;     // 7

  sampler2D light_cookie0; // 8
  sampler2D light_cookie1; // 9
  sampler2D light_cookie2; // 10
  sampler2D light_cookie3; // 11
  sampler2D light_cookie4; // 12
  sampler2D light_cookie5; // 13
  sampler2D light_cookie6; // 14
  sampler2D light_cookie7; // 15

  samplerCube reflectionPROBE; // 16
  samplerCube irradiancePROBE; // 17

  sampler2DArray CNMREA; // 18

  //

  mat4 m;
  mat4 vp;
  mat4 v_l;
  mat4 v_r;
  mat4 vp_l;
  mat4 vp_r;
  mat4 inv_vp;
  mat4 inv_mvp;
  mat4 inv_vp_l;
  mat4 inv_vp_r;

  float SkyboxLevel;
  float SpecularLevel;
  float DiffuseLevel;
  vec3 AmbientLevel;

  float MetallicFactor;
  float RoughnessFactor;
  float RoughnessPower;

  float SSAOPower;
  float SSAOWeight;
  float SSAORadius;
  float SSAOBias;
  int SSAONumSteps;
  int SSAONumSamples;

  float DepthFogDistance;
  float DepthFogPower;
  vec4 ShadowParams;

  float SpecularMipBias;

  float EnvironmentMipBias;
  float EnvironmentMipScale;
  vec2 Zndc2eye;
  mat4 MatP;
  mat4 MatInvP;

  int point_light_count;
  int spot_light_count;
  // sampler2D UnTexPointLightsData;

  // sampler2D light_cookies[8];

  vec4 ModColor;
  vec4 AuxA;
  vec4 AuxB;
  uint obj_pickID;
  vec2 InvViewportSize; // inverse target size
  vec3 EyePostion;
  vec3 EyePostionL;
  vec3 EyePostionR;
}
///////////////////////////////////////////////////////////////
uniform_block ub_frg_fwd_lighting {
  vec4 _lightcolor[64];
  vec4 _lightsizbias[64];
  vec4 _lightpos[64];
  mat4 _shadowmatrix[64];
}
///////////////////////////////////////////////////////////////
// Vertex Interfaces
///////////////////////////////////////////////////////////////
vertex_interface iface_vgbuffer : ub_vtx {
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
    vec3 binormal : BINORMAL;
    vec4 vtxcolor : COLOR0;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec4 frg_wpos;
    vec4 frg_clr;
    vec2 frg_uv0;
    mat3 frg_tbn;
    float frg_camdist;
    vec3 frg_camz;
  }
}
///////////////////////////////////////////////////////////////
vertex_interface iface_vgbuffer_instanced : iface_vgbuffer {
  outputs {
    vec4 frg_modcolor;
  }
}
///////////////////////////////////////////////////////////////
vertex_interface iface_vgbuffer_stereo : iface_vgbuffer {
  outputs {
    layout(secondary_view_offset = 1) int gl_Layer;
  }
}
///////////////////////////////////////////////////////////////
vertex_interface iface_vgbuffer_stereo_instanced : iface_vgbuffer_instanced {
  outputs {
    layout(secondary_view_offset = 1) int gl_Layer;
  }
}
///////////////////////////////////////////////////////////////
vertex_interface iface_vgbuffer_skinned : iface_vgbuffer : iface_skintools {
}
///////////////////////////////////////////////////////////////
// Fragmentertex Interfaces
///////////////////////////////////////////////////////////////
fragment_interface iface_forward : ub_frg_fwd : ub_frg_fwd_lighting {
  inputs {
    vec4 frg_wpos;
    vec4 frg_clr;
    vec2 frg_uv0;
    mat3 frg_tbn;
    float frg_camdist;
    vec3 frg_camz;
    vec4 frg_modcolor;
  }
  outputs {
    layout(location = 0) vec4 out_color;
  }
}
///////////////////////////////////////////////////////////////
// Fragmentertex Interfaces
///////////////////////////////////////////////////////////////
fragment_interface iface_fdprepass : ub_frg_fwd {
  inputs {
    float frg_depth;
  }
  outputs {
    // layout(location = 0) vec4 out_color;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface iface_fgbuffer : ub_frg {
  inputs {
    vec4 frg_wpos;
    vec4 frg_clr;
    vec2 frg_uv0;
    mat3 frg_tbn;
    float frg_camdist;
    vec3 frg_camz;
  }
  outputs {
    layout(location = 0) uvec4 out_gbuf;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface iface_fgbuffer_instanced : iface_fgbuffer {
  inputs {
    vec4 frg_modcolor;
  }
}
///////////////////////////////////////////////////////////////
// StateBlocks
///////////////////////////////////////////////////////////////
state_block sb_default : default {
}
///////////////////////////////////////////////////////////////
// Library Blocks
///////////////////////////////////////////////////////////////
libblock lib_pbr_vtx {
  void vs_common(vec4 pos, vec3 nrm, vec3 bin) {
    vec4 cpos       = mv * pos;
    vec3 wnormal    = normalize(mrot * normal);
    vec3 wbitangent = normalize(mrot * binormal); // technically binormal is a bitangent
    vec3 wtangent   = cross(wbitangent, wnormal);
    frg_wpos        = m * pos;
    frg_clr         = vec4(1, 1, 1, 1); // TODO - split vs_rigid_gbuffer into vertexcolor vs identity
    frg_uv0         = uv0 * vec2(1, -1);
    frg_tbn         = mat3(wtangent, wbitangent, wnormal);
    frg_camz        = wnormal.xyz;
    frg_camdist     = -cpos.z;
  }
}
///////////////////////////////////////////////////////////////
libblock lib_pbr_vtx_instanced {
  void vs_instanced(vec4 pos, vec3 nrm, vec3 bin, mat4 instance_matrix) {
    mat3 instance_rot = mat3(instance_matrix);
    vec4 cpos         = mv * (instance_matrix * pos);
    vec3 wnormal      = normalize(instance_rot * normal);
    vec3 wbitangent   = normalize(instance_rot * binormal); // technically binormal is a bitangent
    vec3 wtangent     = cross(wbitangent, wnormal);
    // frg_clr = vtxcolor;
    frg_wpos    = m * (instance_matrix * pos);
    frg_clr     = vec4(1, 1, 1, 1); // TODO - split vs_rigid_gbuffer into vertexcolor vs identity
    frg_uv0     = uv0 * vec2(1, -1);
    frg_tbn     = mat3(wtangent, wbitangent, wnormal);
    frg_camz    = wnormal.xyz;
    frg_camdist = -cpos.z;
    ////////////////////////////////
    int modcolor_u = (gl_InstanceID & 0xfff);
    int modcolor_v = (gl_InstanceID >> 12);
    frg_modcolor   = texelFetch(InstanceColors, ivec2(modcolor_u, modcolor_v), 0);
    ////////////////////////////////
  }
}
///////////////////////////////////////////////////////////////
libblock lib_pbr_frg : lib_gbuf_encode {
  void ps_common_n(vec4 modc, vec3 N, vec2 UV) {
    vec3 normal    = normalize(frg_tbn * N);
    vec3 rufmtlamb = texture(CNMREA, vec3(UV, 2)).xyz;
    float mtl      = rufmtlamb.z * MetallicFactor;
    float ruf      = rufmtlamb.y * RoughnessFactor;
    vec3 color     = (modc * frg_clr * texture(CNMREA, vec3(UV, 0))).xyz;
    vec3 emission  = texture(CNMREA, vec3(UV, 3)).xyz * modc.xyz;
    out_gbuf       = packGbuffer(color, emission, normal, ruf, mtl);
  }
  void ps_common_vizn(vec4 modc, vec3 N) {
    vec3 normal = normalize(frg_tbn * N);
    // vec3 rufmtlamb = texture(MtlRufMap,UV).zyx;
    float mtl = 0; // rufmtlamb.x * MetallicFactor;
    float ruf = 1; // rufmtlamb.y * RoughnessFactor;
    // vec3 color = (modc*frg_clr*texture(ColorMap,UV)).xyz;
    out_gbuf = packGbuffer(vec3(0, 0, 0), normal, vec3(0, 0, 0), ruf, mtl);
  }
}
libblock lib_ssao {
  /////////////////////////////////////////////////////////

  vec3 getViewPositionNL(vec2 uv) {
    float depth            = texture(MapDepth, uv).r;
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth, 1.0);
    vec4 viewSpacePosition = MatInvP * clipSpacePosition;
    viewSpacePosition /= viewSpacePosition.w;
    return viewSpacePosition.xyz;
  }
  vec3 getViewPosition(vec2 uv) {
    float lin_depth        = texture(MapLinearDepth, uv).r;
    float near             = Zndc2eye.x;
    float far              = Zndc2eye.y;
    float unit_depth       = (lin_depth - near) / (far - near) * 2.0 - 1.0;
    vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, unit_depth, 1.0);
    vec4 viewSpacePosition = MatInvP * clipSpacePosition;
    viewSpacePosition /= viewSpacePosition.w;
    return vec3(viewSpacePosition.xy, unit_depth);
  }

  vec3 ssao_normal(vec2 frg_uv) {
    vec3 base_pos = getViewPosition(frg_uv);

    // compute surface normal @ base_pos (via differential normal calculation)

    vec2 uv_l = frg_uv + vec2(InvViewportSize.x, 0.0);
    vec2 uv_r = frg_uv - vec2(InvViewportSize.x, 0.0);
    vec2 uv_t = frg_uv + vec2(0.0, InvViewportSize.y);
    vec2 uv_b = frg_uv - vec2(0.0, InvViewportSize.y);

    vec3 pos_l = getViewPosition(uv_l);
    vec3 pos_r = getViewPosition(uv_r);
    vec3 pos_t = getViewPosition(uv_t);
    vec3 pos_b = getViewPosition(uv_b);

    vec3 dx     = pos_l - base_pos;
    vec3 dy     = pos_t - base_pos;
    vec3 normal = normalize(cross(dx, dy));
    return normal * vec3(-1, 1, 1);
  }
  vec3 ssao_normal2(vec2 frg_uv) {
    // compute surface normal @ base_pos (via differential normal calculation)
    vec3 normal    = ssao_normal(frg_uv);
    vec3 up        = vec3(0, 1, 0);
    vec3 nxu       = normalize(cross(normal, up));
    vec3 nxv       = normalize(cross(normal, nxu));
    vec3 randomVec = texture(SSAOScrNoise, frg_uv).xyz;

    // Accumulate occlusion
    vec3 NN             = normal;
    float rad_div_steps = SSAORadius / float(SSAONumSteps);
    for (int i = 0; i < SSAONumSamples; ++i) {
      vec3 skern    = texture(SSAOKernel, vec2(float(i) / float(SSAONumSamples), 0)).xyz;
      vec3 NOISEOUT = normalize(reflect(skern, randomVec));
      if (dot(NOISEOUT, normal) < 0.0) {
        NOISEOUT = -NOISEOUT;
      }
      vec3 sampleDir = normalize(NOISEOUT.x * nxu + NOISEOUT.y * nxv + NOISEOUT.z * normal);
      NN += sampleDir * 0.02;
    }
    return normalize(NN);
  }

  // SSAO calculation function
  float ssao_linear(vec2 frg_uv) {
    vec3 base_pos    = getViewPosition(frg_uv);
    float base_depth = base_pos.z;

    vec3 up        = vec3(0, 1, 0);
    vec3 normal    = ssao_normal(frg_uv);
    vec3 nxu       = normalize(cross(normal, up));
    vec3 nxv       = normalize(cross(normal, nxu));
    vec3 randomVec = texture(SSAOScrNoise, frg_uv).xyz;

    // Accumulate occlusion
    float occlusion     = 0.0;
    float rad_div_steps = SSAORadius / float(SSAONumSteps);
    for (int i = 0; i < SSAONumSamples; ++i) {

      // generate hemisphere of rays centered on "normal" at base_pos

      vec3 skern    = texture(SSAOKernel, vec2(float(i) / float(SSAONumSamples), 0)).xyz;
      vec3 NOISEOUT = normalize(reflect(skern, randomVec));

      // construct sample direction from NOISEOUT, normal, nxu, nxv
      vec3 sampleDir = normalize(normal + NOISEOUT * 0.4);
      // vec3 sampleDir = normalize(normal + NOISEOUT);

      // ensure sampledir is on hemisphere of normal by reflecting of plane defined by normal

      vec3 trv_per_step = sampleDir.xyz * rad_div_steps;
      for (int j = 1; j <= SSAONumSteps; ++j) {

        // ray cast depth
        vec3 sample_pos    = base_pos + trv_per_step * float(j);
        float casted_depth = -sample_pos.z;

        // depth map sample
        vec2 sample_uv      = clamp(sample_pos.xy, vec2(0.0), vec2(1.0));
        float depth_sample2 = -getViewPosition(sample_uv).z;

        float rangeCheck = smoothstep(0.0, 1.0, SSAORadius / abs(base_depth - depth_sample2));
        if (casted_depth < (depth_sample2 - SSAOBias)) {
          occlusion += rangeCheck;
        }
        // occlusion += casted_depth*0.5;
      }
      // occlusion = sampleDir.z;
    }
    occlusion = (occlusion / (SSAONumSamples * SSAONumSteps));

    return occlusion;
  }

  /////////////////////////////////////////////////////////
  float ssao_nonlinear(vec2 frg_uv) {
    float depth = texture(MapDepth, frg_uv).r;

    // Random noise texture
    vec3 randomVec = texture(SSAOScrNoise, frg_uv).xyz;

    // Accumulate occlusion
    float occlusion = 0.0;
    for (int i = 0; i < SSAONumSamples; ++i) {
      vec3 skern     = texture(SSAOKernel, vec2(float(i) / float(SSAONumSamples), 0)).xyz;
      vec3 sampleDir = reflect(skern, randomVec); // reflect sample around the random vector
      sampleDir      = normalize(sampleDir);

      vec2 trv_per_step = sampleDir.xy * (SSAORadius / float(SSAONumSteps));
      for (int j = 1; j <= SSAONumSteps; ++j) {
        vec2 samplePos = frg_uv + trv_per_step * float(j);

        // Clamp sample positions to screen boundaries
        samplePos = clamp(samplePos, vec2(0.0), vec2(1.0));

        float sampleDepth = texture(MapDepth, samplePos.xy).r;

        float rangeCheck = smoothstep(0.0, 1.0, SSAORadius / abs(depth - sampleDepth));
        if (sampleDepth < depth + SSAOBias) {
          occlusion += rangeCheck;
        }
      }
    }
    occlusion = (occlusion / (SSAONumSamples * SSAONumSteps));

    return 1.0 - occlusion;
  }

  float ssao(vec2 frg_uv) {
    return ssao_linear(frg_uv);
  }
}

libblock lib_pbr {

  struct PbrInpA {
    vec3 _albedo;
    vec3 _wpos;
    vec3 _epos;
    vec3 _wnrm;
    float _metallic;
    float _roughness;
    float _fogZ;
    float _atmos;
    float _alpha;
    bool _emissive;
  };
  struct PbrOutA {
    vec3 _metalbase;
    vec3 _F0;
    vec3 _G0;
    vec3 _F;
    vec3 _invF;
    vec3 _edir;
    vec3 _refl;
    vec3 _diffuseColor;
    vec3 _diffuseEnv;
    float _metallic;
    float _dialetric;
    vec3 _ambient;
    vec2 _BRDF;
  };

  PbrOutA pbrIoEnvA(PbrInpA inp) {
    PbrOutA _out;
    // float roughness = rufmtlamb.y * RoughnessFactor;
    // roughness = pow(roughness, RoughnessPower);
    vec3 N = normalize(inp._wnrm);
    vec3 edir = normalize(inp._wpos - inp._epos);
    _out._metallic  = clamp(inp._metallic, 0.02, 0.99);
    _out._dialetric = 1.0 - inp._metallic;
    _out._metalbase = mix(vec3(0.04), inp._albedo, _out._metallic);
    _out._F0        = mix(_out._metalbase, inp._albedo, _out._metallic);
    _out._G0        = mix(_out._metalbase, inp._albedo, 1.0 - _out._metallic);
    _out._edir      = edir;

    float costheta = clamp(dot(N, edir), 0.01, 0.99);
    _out._BRDF     = textureLod(MapBrdfIntegration, vec2(costheta, inp._roughness * 0.99), 0).rg;
    _out._BRDF     = clamp(_out._BRDF, vec2(0), vec2(1));

    _out._F    = fresnelSchlickRoughness(costheta, _out._F0, inp._roughness);
    _out._invF = (vec3(1) - _out._F);

    _out._ambient = (clamp(dot(N, edir), 0, 1) * 0.3 + 0.7)*AmbientLevel;
    _out._refl     = normalize(reflect(edir, N));
    //_out._refl *= vec3(-1, -1, -1);

    _out._diffuseColor = mix(inp._albedo, vec3(0), _out._metallic);
    _out._diffuseEnv = env_equirectangular(N,MapDiffuseEnv,0)*DiffuseLevel*SkyboxLevel;
    return _out;
  }
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// DEFERRED
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
// vs-non-instanced-rigid
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_gbuffer : iface_vgbuffer : lib_pbr_vtx {
  vs_common(position, normal, binormal);
  gl_Position = mvp * position;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_gbuffer_stereo : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2)
    : iface_vgbuffer_stereo : lib_pbr_vtx {
  vs_common(position, normal, binormal);
  gl_Position                   = mvp_l * position;
  gl_SecondaryPositionNV        = mvp_r * position;
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
}
///////////////////////////////////////////////////////////////
// vs-instanced-rigid
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_gbuffer_instanced //
    : iface_vgbuffer_instanced           //
    : lib_pbr_vtx_instanced {            //
  int matrix_v     = (gl_InstanceID >> 10);
  int matrix_u     = (gl_InstanceID & 0x3ff) << 2;
  mat4 instancemtx = mat4(
      texelFetch(InstanceMatrices, ivec2(matrix_u + 0, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 1, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 2, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 3, matrix_v), 0));
  ////////////////////////////////
  vec4 instanced_pos = (instancemtx * position);
  vs_instanced(position, normal, binormal, instancemtx);
  ////////////////////////////////
  gl_Position = mvp * instanced_pos;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_gbuffer_instanced_stereo //
    : extension(GL_NV_stereo_view_rendering)    //
    : extension(GL_NV_viewport_array2)          //
    : iface_vgbuffer_stereo_instanced           //
    : lib_pbr_vtx_instanced {                   //
  ////////////////////////////////
  int matrix_v     = (gl_InstanceID >> 10);
  int matrix_u     = (gl_InstanceID & 0x3ff) << 2;
  mat4 instancemtx = mat4(
      texelFetch(InstanceMatrices, ivec2(matrix_u + 0, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 1, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 2, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 3, matrix_v), 0));
  ////////////////////////////////
  vec4 instanced_pos = (instancemtx * position);
  vs_instanced(position, normal, binormal, instancemtx);
  ////////////////////////////////
  gl_Position            = mvp_l * instanced_pos;
  gl_SecondaryPositionNV = mvp_r * instanced_pos;
  ////////////////////////////////
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
}
///////////////////////////////////////////////////////////////
// vs-non-instanced-skinned
///////////////////////////////////////////////////////////////
vertex_shader vs_skinned_gbuffer : iface_vgbuffer_skinned : skin_tools : lib_pbr_vtx {
  vec4 skn_pos = vec4(SkinPosition(position.xyz), 1);
  vec3 skn_nrm = SkinNormal(normal);
  vec3 skn_bit = SkinNormal(binormal); // // technically binormal is a bitangent
  vs_common(skn_pos, skn_nrm, skn_bit);
  gl_Position = mvp * skn_pos;
}
///////////////////////////////////////////////////////////////
// fragment shaders
///////////////////////////////////////////////////////////////
fragment_shader ps_gbuffer : iface_fgbuffer : lib_pbr_frg {
  ps_common_n(ModColor, vec3(0, 0, 1), frg_uv0);
}
///////////////////////////////////////////////////////////////
// vs-non-instanced-rigid
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_gbuffer_vtxcolor : iface_vgbuffer : lib_pbr_vtx {
  frg_clr     = vtxcolor;
  gl_Position = mvp * position;
}
vertex_shader vs_forward_rigid_vtxcolor : iface_vgbuffer : lib_pbr_vtx {
  frg_clr     = vec4(1, 1, 1, 1); // vtxcolor;
  gl_Position = mvp * position;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_gbuffer_vtxcolor : iface_fgbuffer : lib_pbr_frg {
  out_gbuf = packGbuffer(vec3(0), frg_clr.xyz, vec3(0, 0, 1), 1, 0);
  // out_gbuf = packGbuffer(vec3(0,1,0), vec3(1,1,0), vec3(0,0,1), 1, 0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_forward_frgcolor : iface_forward {
  out_color = frg_clr;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_gbuffer_font : iface_vgbuffer : lib_pbr_vtx {
  vs_common(position, normal, binormal);
  frg_clr     = vtxcolor;
  frg_uv0     = uv0;
  gl_Position = mvp * position;
}
vertex_shader vs_rigid_gbuffer_font_instanced : iface_vgbuffer : lib_pbr_vtx {
  vs_common(position, normal, binormal);
  frg_clr     = vtxcolor;
  frg_uv0     = uv0;
  gl_Position = (vp * m) * position;
}
fragment_shader ps_gbuffer_font : iface_fgbuffer : lib_pbr_frg {
  vec4 s     = texture(ColorMap, frg_uv0.xy);
  float texa = pow(s.a * s.r, 0.75);
  if (s.r < 0.001)
    discard;
  // vec3 color = s.rrr;
  out_gbuf = packGbufferA(1, s.a * s.r);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_gbuffer_vizn : iface_fgbuffer : lib_pbr_frg {
  ps_common_vizn(ModColor, vec3(0, 0, 1));
}
///////////////////////////////////////////////////////////////
fragment_shader ps_gbuffer_n // normalmap
    : iface_fgbuffer : lib_pbr_frg {
  vec3 TN = texture(CNMREA, vec3(frg_uv0, 1)).xyz;
  TN      = mix(TN, vec3(0.5, 1, 0.5), 0.0);
  vec3 N  = normalize(TN * 2.0 - vec3(1, 1, 1));
  ps_common_n(vec4(1, 1, 1, 1), N, frg_uv0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_gbuffer_n_stereo // normalmap
    : iface_fgbuffer : lib_pbr_frg {
  vec3 TN = texture(CNMREA, vec3(frg_uv0, 1)).xyz;
  vec3 N  = normalize(TN * 2.0 - vec3(1, 1, 1));
  if (length(TN) < 0.1)
    N = vec3(0, 0, 0);
  ps_common_n(ModColor, N, frg_uv0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_gbuffer_n_instanced : iface_fgbuffer_instanced : lib_pbr_frg {
  vec3 TN = texture(CNMREA, vec3(frg_uv0, 1)).xyz;
  TN      = mix(TN, vec3(0.5, 1, 0.5), 0.0);
  vec3 N  = normalize(TN * 2.0 - vec3(1, 1, 1));
  if (length(TN) < 0.1)
    N = vec3(0, 0, 0);
  ps_common_n(frg_modcolor, N, frg_uv0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_gbuffer_n_stereo_instanced : iface_fgbuffer_instanced : lib_pbr_frg {
  vec3 TN = texture(CNMREA, vec3(frg_uv0, 1)).xyz;
  vec3 N  = normalize(TN * 2.0 - vec3(1, 1, 1));
  if (length(TN) < 0.1)
    N = vec3(0, 0, 0);
  ps_common_n(frg_modcolor, N, frg_uv0);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_gbuffer_n_tex_stereo // normalmap (stereo texture - vsplit)
    : iface_fgbuffer : lib_pbr_frg {
  vec2 screen_uv = gl_FragCoord.xy * InvViewportSize;
  bool is_right  = bool(screen_uv.x <= 0.5);
  vec2 map_uv    = frg_uv0 * vec2(1, 0.5);
  if (is_right)
    map_uv += vec2(0, 0.5);
  vec3 TN = texture(CNMREA, vec3(map_uv, 1)).xyz;
  vec3 N  = TN * 2.0 - vec3(1, 1, 1);
  ps_common_n(vec3(1, 1, 1), N, map_uv);
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// FORWARD
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

vertex_interface iface_vdprepass : ub_vtx {
  inputs {
    vec4 position : POSITION;
  }
  outputs {
    float frg_depth;
  }
}
vertex_interface iface_vdprepass_stereo : ub_vtx {
  inputs {
    vec4 position : POSITION;
  }
  outputs {
    layout(secondary_view_offset = 1) int gl_Layer;
    float frg_depthL;
    float frg_depthR;
  }
}
vertex_interface iface_vdprepass_skinned : ub_vtx : iface_skintools {
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
  }
  outputs {
    float frg_depth;
  }
}
vertex_interface iface_vdprepass_skinned_stereo : ub_vtx : iface_skintools {
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
  }
  outputs {
    float frg_depthL;
    float frg_depthR;
  }
}

fragment_interface iface_fdprepass_stereo : ub_frg_fwd {
  inputs {
    float frg_depthL;
    float frg_depthR;
  }
  outputs {
    // layout(location = 0) vec4 out_color;
  }
}

///////////////////////////////////////////////////////////////
// Forward Depth Prepass
///////////////////////////////////////////////////////////////

vertex_shader vs_forward_depthprepass_mono : iface_vdprepass {
  vec4 hpos   = mvp * position;
  gl_Position = hpos;
  frg_depth   = (hpos.z) / (hpos.w);
}
vertex_shader vs_forward_depthprepass_skinned_mono : iface_vdprepass_skinned : skin_tools {
  vec4 skn_pos = vec4(SkinPosition(position.xyz), 1);
  vec4 hpos    = mvp * skn_pos;
  gl_Position  = hpos;
  frg_depth    = (hpos.z) / (hpos.w);
}
vertex_shader vs_forward_depthprepass_instanced_mono : iface_vdprepass {
  int matrix_v     = (gl_InstanceID >> 10);
  int matrix_u     = (gl_InstanceID & 0x3ff) << 2;
  mat4 instancemtx = mat4(
      texelFetch(InstanceMatrices, ivec2(matrix_u + 0, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 1, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 2, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 3, matrix_v), 0));
  ////////////////////////////////
  vec4 instanced_pos = (instancemtx * position);
  vec4 hpos          = mvp * instanced_pos;
  gl_Position        = hpos;
  // gl_FragDepth = hpos.z/hpos.w;
}
vertex_shader vs_forward_depthprepass_skinned_instanced_mono : iface_vdprepass {
  vec4 skn_pos     = vec4(SkinPosition(position.xyz), 1);
  int matrix_v     = (gl_InstanceID >> 10);
  int matrix_u     = (gl_InstanceID & 0x3ff) << 2;
  mat4 instancemtx = mat4(
      texelFetch(InstanceMatrices, ivec2(matrix_u + 0, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 1, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 2, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 3, matrix_v), 0));
  ////////////////////////////////
  vec4 instanced_pos = (instancemtx * skn_pos);
  vec4 hpos          = mvp * instanced_pos;
  gl_Position        = hpos;
  // gl_FragDepth = hpos.z/hpos.w;
}

vertex_shader vs_forward_depthprepass_stereo : iface_vdprepass_stereo : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2) {
  vec4 hposL                    = mvp_l * position;
  vec4 hposR                    = mvp_r * position;
  gl_Position                   = hposL;
  gl_SecondaryPositionNV        = hposR;
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
  frg_depthL                    = hposL.z / hposL.w;
  frg_depthR                    = hposR.z / hposR.w;
}
vertex_shader vs_forward_depthprepass_skinned_stereo : iface_vdprepass_skinned_stereo : skin_tools
    : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2) {
  vec4 skn_pos                  = vec4(SkinPosition(position.xyz), 1);
  vec4 hposL                    = mvp_l * skn_pos;
  vec4 hposR                    = mvp_r * skn_pos;
  gl_Position                   = hposL;
  gl_SecondaryPositionNV        = hposR;
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
  frg_depthL                    = hposL.z / hposL.w;
  frg_depthR                    = hposR.z / hposR.w;
}
vertex_shader vs_forward_depthprepass_instanced_stereo : iface_vdprepass_skinned : skin_tools {
  vec4 skn_pos = vec4(SkinPosition(position.xyz), 1);
  vec4 hpos    = mvp * skn_pos;
  gl_Position  = hpos;
  frg_depth    = (hpos.z) / (hpos.w);
}
vertex_shader vs_forward_depthprepass_skinned_instanced_stereo : iface_vdprepass_skinned : skin_tools {
  vec4 skn_pos = vec4(SkinPosition(position.xyz), 1);
  vec4 hpos    = mvp * skn_pos;
  gl_Position  = hpos;
  frg_depth    = (hpos.z) / (hpos.w);
}
fragment_shader ps_forward_depthprepass_mono : iface_fdprepass {
  // gl_FragDepth = frg_depth;
  gl_FragDepth = gl_FragCoord.z + 1e-6;
}
fragment_shader ps_forward_depthprepass_stereo : iface_fdprepass_stereo : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2) {
}

///////////////////////////////////////////////////////////////
// Forward pbr
///////////////////////////////////////////////////////////////
vertex_interface iface_forward_stereo_instanced : iface_vgbuffer_instanced {
  outputs {
    layout(secondary_view_offset = 1) int gl_Layer;
  }
}

vertex_shader vs_forward_test_vtxcolor : iface_vgbuffer : lib_pbr_vtx {
  vs_common(position, normal, binormal);
  gl_Position = mvp * position;
  frg_clr     = vtxcolor;
}
vertex_shader vs_forward_test : iface_vgbuffer : lib_pbr_vtx {
  vs_common(position, normal, binormal);
  gl_Position = mvp * position;
}
vertex_shader vs_forward_test_stereo : iface_vgbuffer_stereo : lib_pbr_vtx : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2) {
  vs_common(position, normal, binormal);
  gl_Position                   = mvp_l * position;
  gl_SecondaryPositionNV        = mvp_r * position;
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
}
vertex_shader vs_forward_instanced : iface_vgbuffer_instanced : lib_pbr_vtx_instanced {
  int matrix_v     = (gl_InstanceID >> 10);
  int matrix_u     = (gl_InstanceID & 0x3ff) << 2;
  mat4 instancemtx = mat4(
      texelFetch(InstanceMatrices, ivec2(matrix_u + 0, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 1, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 2, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 3, matrix_v), 0));
  ////////////////////////////////
  vec4 instanced_pos = (instancemtx * position);
  vs_instanced(position, normal, binormal, instancemtx);
  ////////////////////////////////
  gl_Position = mvp * instanced_pos;
}
vertex_shader vs_forward_instanced_stereo : iface_forward_stereo_instanced : lib_pbr_vtx_instanced
    : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2) {

  int matrix_v     = (gl_InstanceID >> 10);
  int matrix_u     = (gl_InstanceID & 0x3ff) << 2;
  mat4 instancemtx = mat4(
      texelFetch(InstanceMatrices, ivec2(matrix_u + 0, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 1, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 2, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 3, matrix_v), 0));
  ////////////////////////////////
  vec4 instanced_pos = (instancemtx * position);
  vs_instanced(position, normal, binormal, instancemtx);
  ////////////////////////////////
  gl_Position                   = mvp_l * instanced_pos;
  gl_SecondaryPositionNV        = mvp_r * instanced_pos;
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
}
vertex_shader vs_forward_skinned_mono : iface_vgbuffer_skinned : skin_tools : lib_pbr_vtx {
  vec4 skn_pos = vec4(SkinPosition(position.xyz), 1);
  vec3 skn_nrm = SkinNormal(normal);
  vec3 skn_bit = SkinNormal(binormal); // // technically binormal is a bitangent
  vs_common(skn_pos, skn_nrm, skn_bit);
  ////////////////////////////////
  gl_Position = mvp * skn_pos;
}
vertex_shader vs_forward_skinned_stereo : iface_vgbuffer_skinned : skin_tools : lib_pbr_vtx : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2) {
  vec4 skn_pos = vec4(SkinPosition(position.xyz), 1);
  vec3 skn_nrm = SkinNormal(normal);
  vec3 skn_bit = SkinNormal(binormal); // // technically binormal is a bitangent
  vs_common(skn_pos, skn_nrm, skn_bit);
  ////////////////////////////////
  gl_Position                   = mvp_l * skn_pos;
  gl_SecondaryPositionNV        = mvp_r * skn_pos;
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
}
//////////////////////////////////////
fragment_shader ps_forward_test_fragcolor //
    : iface_forward                       //
    : lib_math                            //
    : lib_brdf                            //
    : lib_def                             //
    : lib_fwd {                           //
  out_color = vec4(forward_lighting_mono(frg_clr.xyz * ModColor.xyz), 1);
}
//////////////////////////////////////
fragment_shader ps_forward_test //
    : iface_forward             //
    : lib_math                  //
    : lib_brdf                  //
    : lib_def                   //
    : lib_fwd {                 //
  out_color = vec4(forward_lighting_mono(ModColor.xyz), 1);
}
fragment_shader ps_forward_test_instanced_mono : iface_forward : lib_math : lib_brdf : lib_def : lib_fwd {
  out_color = vec4(forward_lighting_mono(frg_modcolor.xyz), 1);
}
//////////////////////////////////////
fragment_shader ps_forward_test_stereo : iface_forward : lib_math : lib_brdf : lib_def : lib_fwd : lib_fwd_stereo
    : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2) {
  out_color = vec4(forward_lighting_stereo(ModColor.xyz), 1);
}
fragment_shader ps_forward_test_instanced_stereo : iface_forward : lib_math : lib_brdf : lib_def : lib_fwd : lib_fwd_stereo
    : extension(GL_NV_stereo_view_rendering)
    : extension(GL_NV_viewport_array2) {
  out_color = vec4(forward_lighting_stereo(frg_modcolor.xyz), 1);
}

///////////////////////////////////////////////////////////////
// Forward SkyBox
///////////////////////////////////////////////////////////////
vertex_shader vs_forward_skybox_mono : iface_vgbuffer : lib_pbr_vtx {
  gl_Position = position; // screen space quad
  frg_clr     = position;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_forward_skybox_stereo       //
    : iface_vgbuffer_stereo                  //
    : lib_pbr_vtx                            //
    : extension(GL_NV_stereo_view_rendering) //
    : extension(GL_NV_viewport_array2) {     //
  gl_Position                   = position;
  gl_SecondaryPositionNV        = position;
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
  frg_uv0                       = uv0;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_forward_skybox_mono //
    : iface_forward                    //
    : lib_math                         //
    : lib_brdf                         //
    : lib_def                          //
    : lib_fwd {                        //

  ///////////////////////
  // compute view normal vector
  ///////////////////////

  vec2 uvn = frg_clr.xy;

  vec4 xyzw = vec4(uvn, 0, 1);
  xyzw      = inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posA = xyzw.xyz;
  xyzw      = vec4(uvn, 1, 1);
  xyzw      = inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posB = xyzw.xyz;
  vec3 VN   = normalize(posA - posB);
  // VN.z      = -VN.z;
  // VN.x      = -VN.x;
  ///////////////////////
  // environment map
  ///////////////////////

  vec3 rgb  = env_equirectangularFlipV(VN, MapSpecularEnv, EnvironmentMipBias) * SkyboxLevel;
  out_color = vec4(rgb, 1);

  ///////////////////////
}
///////////////////////////////////////////////////////////////
fragment_shader ps_forward_skybox_stereo //
    : iface_forward                      //
    : lib_math                           //
    : lib_brdf                           //
    : lib_def                            //
    : lib_fwd                            //
    : extension(GL_NV_viewport_array) {  //

  ///////////////////////
  // compute view normal vector
  ///////////////////////

  mat4 inv_vp = bool(gl_ViewportIndex) ? inv_vp_r : inv_vp_l;

  vec2 uvn = (frg_uv0.xy - vec2(.5, .5)) * 2;

  vec4 xyzw = vec4(uvn, 0, 1);
  xyzw      = inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posA = xyzw.xyz;
  xyzw      = vec4(uvn, 1, 1);
  xyzw      = inv_vp * xyzw;
  xyzw.xyz *= (1.0 / xyzw.w);
  vec3 posB = xyzw.xyz;
  vec3 VN   = normalize(posA - posB);

  ///////////////////////
  // environment map
  ///////////////////////

  vec3 rgb  = env_equirectangularFlipV(VN, MapSpecularEnv, EnvironmentMipBias) * SkyboxLevel;
  out_color = vec4(rgb, 1);
}

///////////////////////////////////////////////////////////////
vertex_shader vs_forward_unlit : iface_vgbuffer : lib_pbr_vtx {
  gl_Position = mvp * position; // screen space quad
  frg_uv0     = uv0;
}
fragment_shader ps_forward_unlit : iface_forward {
  vec3 rgb = texture(CNMREA, vec3(frg_uv0, 0)).xyz;
  rgb *= ModColor.xyz;
  out_color = vec4(ModColor.xyz, 1);
}
///////////////////////////////////////////////////////////////
// picking
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
vertex_interface iface_vtx_pick_skinned //
    : iface_skintools {                 //
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
    vec2 uv0 : TEXCOORD0;
    uvec3 pickSUBID : TEXCOORD1;
  }
  outputs {
    vec3 frg_wpos;
    vec3 frg_wnrm;
    vec2 frg_uv;
    flat uvec3 frg_pickSUBID;
  }
}
///////////////////////////////////////////////////////////////
vertex_interface iface_vtx_pick_rigid {
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
    uvec3 pickSUBID : TEXCOORD1;
  }
  outputs {
    vec3 frg_wpos;
    vec3 frg_wnrm;
    vec2 frg_uv;
    flat uvec3 frg_pickSUBID;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface iface_frg_pick : ub_frg_fwd {
  inputs {
    vec3 frg_wpos;
    vec3 frg_wnrm;
    vec2 frg_uv;
    flat uvec3 frg_pickSUBID;
  }
  outputs {
    layout(location = 0) uvec4 out_pickID;
    layout(location = 1) vec4 out_wpos;
    layout(location = 2) vec4 out_wnrm;
    layout(location = 3) vec4 out_uv;
  }
}
///////////////////////////////////////////////////////////////
vertex_shader vs_pick_skinned_mono : iface_vtx_pick_skinned : skin_tools : ub_vtx {
  vec4 skn_pos  = vec4(SkinPosition(position.xyz), 1);
  vec3 skn_nrm  = SkinNormal(normal);
  gl_Position   = mvp * skn_pos;
  frg_wpos      = (m * skn_pos).xyz;
  frg_wnrm      = normalize(mrot * skn_nrm);
  frg_uv        = uv0;
  frg_pickSUBID = pickSUBID;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_pick_rigid_mono : iface_vtx_pick_rigid : ub_vtx {
  gl_Position   = mvp * position;
  frg_wpos      = (m * position).xyz;
  frg_wnrm      = normalize(mrot * normal);
  frg_uv        = vec2(0, 0);
  frg_pickSUBID = pickSUBID;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_pick_rigid_instanced_mono : iface_vtx_pick_rigid : ub_vtx {
  int matrix_v   = (gl_InstanceID >> 10);
  int matrix_u   = (gl_InstanceID & 0x3ff) << 2;
  int modcolor_u = (gl_InstanceID & 0xfff);
  int modcolor_v = (gl_InstanceID >> 12);
  ////////////////////////////////
  mat4 instance_matrix = mat4(
      texelFetch(InstanceMatrices, ivec2(matrix_u + 0, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 1, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 2, matrix_v), 0),
      texelFetch(InstanceMatrices, ivec2(matrix_u + 3, matrix_v), 0));
  mat3 instance_rot = mat3(instance_matrix);
  ////////////////////////////////
  // vec4 instanced_pos      = (instance_matrix * position);
  // vs_instanced(position, normal, binormal, instance_matrix);
  ////////////////////////////////
  gl_Position     = mvp * instance_matrix * position;
  frg_wpos        = (m * position).xyz;
  frg_wnrm        = normalize(mrot * normal);
  frg_uv          = vec2(0, 0);
  frg_pickSUBID.x = gl_InstanceID;
  frg_pickSUBID.y = 2;
  frg_pickSUBID.z = 3;
}
///////////////////////////////////////////////////////////////
fragment_shader ps_pick //
    : iface_frg_pick {

  out_pickID = uvec4(obj_pickID, frg_pickSUBID.x, frg_pickSUBID.y, frg_pickSUBID.z);

  out_wpos = vec4(frg_wpos, 0);
  out_wnrm = vec4(normalize(frg_wnrm), 0);
  out_uv   = vec4(frg_uv, 0, 0);
}
///////////////////////////////////////////////////////////////
