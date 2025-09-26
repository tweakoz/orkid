# Orkid GLSL FX Effect Format (Ork.Fx)

---

### Summary

Orkid has it's own effect file format. This works on Vulkan 1.3.

---

### Features

  - import/include from other glfx files 
  - familiar shader/technique/pass layout
  - "state blocks" - reusable rasterization states
  - "library blocks" - reusable code libraries
  - "type blocks" - reusable data structures
  - "sampler sets" - sets of texture samplers
  - "uniform sets" - map to vulkan push constants
  - "uniform blocks" - map to vulkan uniform buffers
  - "storage blocks" - maps to vulkan shader storage buffers
  - "vertex interface" - IO schema for vertex shaders
  - "geometry interface" - IO schema for geometry shaders
  - "fragment interface" - IO schema for fragment shaders
  - "compute interface" - IO schema for compute shaders
  - data inheritance for most block/set types
  - supports Vertex, Tessellation, Geometry and Fragment shaders

---

### Example "stdtools.i2"

```glsl
///////////////////////////////////////////////////////////////

uniform_block ublk_std_matrices (descriptor_set 0) {
  mat4 m;
  mat4 v;
  mat4 p;
  mat4 mv;
  mat4 vp;
  mat4 mvp;
  mat3 mrot;
  //
  mat4 inv_v;
  mat4 inv_p;
  mat4 inv_vp;
  //
  mat4 v_l;
  mat4 v_r;
  mat4 vp_l;
  mat4 vp_r;
  mat4 inv_vp_l;
  mat4 inv_vp_r;
  mat4 mvp_l;
  mat4 mvp_r;
}
//
///////////////////////////////////////////////////////////////
uniform_set uset_std_viewport {
  vec2 ViewportSize;    // target size
  vec2 InvViewportSize; // inverse target size
}
///////////////////////////////////////////////////////////////
uniform_block ublk_std_viewport (descriptor_set 0) {
  vec2 ViewportSize;    // target size
  vec2 InvViewportSize; // inverse target size
}
///////////////////////////////////////////////////////////////
sampler_set sset_std_instancing(descriptor_set 0) {
  sampler2D InstanceMatrices;
  sampler2D InstanceColors;
  usampler2D InstanceIds;
}
///////////////////////////////////////////////////////////////
uniform_set uset_std_filtering {
  float FilterRadius;
}
///////////////////////////////////////////////////////////////
uniform_block ublk_std_filtering (descriptor_set 0) {
  float FilterRadius;
}
///////////////////////////////////////////////////////////////
uniform_set uset_std_pick {
  uint obj_pickID;
}
///////////////////////////////////////////////////////////////
uniform_block ublk_std_depth(descriptor_set 0) {
  vec2 Zndc2eye;
}
///////////////////////////////////////////////////////////////
uniform_block ublk_std_pbr(descriptor_set 0) {
  //////////////////////////////
  vec3 AmbientLevel;          // 0x00 
  vec3 EyePostion;            // 0x10
  vec3 EyePostionL;           // 0x20
  vec3 EyePostionR;           // 0x30
  vec4 ModColor;              // 0x40
  //////////////////////////////
  float MetallicFactor;       // 0x50
  float RoughnessFactor;      // 0x54
  float RoughnessPower;       // 0x58
  float SkyboxLevel;          // 0x5c
  float SpecularLevel;        // 0x60
  float DiffuseLevel;         // 0x64
  float EnvironmentMipBias;   // 0x68
  float EnvironmentMipScale;  // 0x6c
  float RoughnessLevels;      // 0x70
  float SpecularMipBias;      // 0x74
  float DepthFogDistance;     // 0x78
  float DepthFogPower;        // 0x7c
  //////////////////////////////
  vec4 AuxA;                  // 0x80
  vec4 AuxB;                  // 0x90

}
///////////////////////////////////////////////////////////////
sampler_set sset_std_lighting (descriptor_set 0) {
  sampler2DArray LightMapArray;            
  sampler2DArray light_cookie_colors;      
  sampler2DArray light_cookie_depths;      
  sampler2D MapDepth;           
  sampler2D MapLinearDepth;     
}
///////////////////////////////////////////////////////////////
sampler_set sset_std_pbr(descriptor_set 0) {
  //////////////////////////////
  sampler2DArray CNMREA; // slices: albedo, normal,mtlruf,emission,AO
  samplerCube reflectionPROBE;
  samplerCube RadiancePROBE;
  sampler2D MapBrdfIntegration; 
  sampler2D MapDiffuseEnv;      
  sampler2DArray MapSpecularEnv; // 1 slice per roughness level
}
///////////////////////////////////////////////////////////////
uniform_block ublk_std_lighting (descriptor_set 0) {
  //////////////////////////////
  vec3 LightMapColors[8];        
  //////////////////////////////
  int point_light_count;
  int spot_light_count;
}

///////////////////////////////////////////////////////////////
storage_interface storage_fwd_lighting (descriptor_set 0) {
  buffer layout(std430) lights {
    vec4 _lightcolor[64];    // 1024 : 1024 
    vec4 _lightsizbias[64];  // 1024 : 2048
    vec4 _lightpos[64];      // 1024 : 3072
    mat4 _shadowmatrix[64];  // 4096 : 7168
    uint _lightTexSlice[64]; // 256  : 7424
  };
}
///////////////////////////////////////////////////////////////
uniform_block ublk_deferred_lighting(descriptor_set 0) {
  vec4 LightColorD[256];   // 4096   : 4096
  mat4 LightMatrix[256];   // 163384 : 167480
  mat4 ShadowMatrix[256];  // 163384 : 330864
  float LightRadius[256];  // 1024   : 331888
}
///////////////////////////////////////////////////////////////
vertex_interface vif_PC {
  inputs {
    vec4 position : POSITION;
    vec4 vtxcolor : COLOR0;
  }
  outputs {
    vec4 frg_clr;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface fif_PC : vif_PC {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
///////////////////////////////////////////////////////////////
vertex_interface vif_PT {
  inputs {
    vec4 position : POSITION;
    vec2 uv0 : TEXCOORD0;
  }
  outputs {
    vec2 frg_uv0;
  }
}
///////////////////////////////////////////////////////////////
vertex_interface vif_PTT {
  inputs {
    vec4 position : POSITION;
    vec2 uv0 : TEXCOORD0;
    vec2 uv1 : TEXCOORD1;
  }
  outputs {
    vec2 frg_uv0;
    vec2 frg_uv1;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface fif_T : vif_PT : ublock_frg {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface fif_min_T : vif_PT {
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface fif_skybox : vif_PC : ublk_std_matrices : uset_std_pbr : sset_std_pbr {
  outputs {
    layout(location = 0) vec4 out_color;
  }
}
///////////////////////////////////////////////////////////////
libblock lib_pbr_vtx_instanced : ublk_std_matrices : sset_std_instancing {
  void vs_instanced(vec4 pos, vec3 nrm, vec3 bin, mat4 instance_matrix) {
    mat3 instance_rot = mat3(instance_matrix);
    vec4 cpos         = mv * (instance_matrix * pos);
    vec3 wnormal      = normalize(instance_rot * normal);
    vec3 wbitangent   = normalize(instance_rot * binormal); // technically binormal is a bitangent
    vec3 wtangent     = cross(wbitangent, wnormal);
    // frg_clr = vtxcolor;
    frg_wpos    = m * (instance_matrix * pos);
    //frg_clr     = vec4(1, 1, 1, 1); // TODO - split vs_rigid_gbuffer into vertexcolor vs identity
    frg_uv0     = uv0 * vec2(1, -1);
    frg_tbn     = mat3(wtangent, wbitangent, wnormal);
    frg_camz    = wnormal.xyz;
    frg_camdist = -cpos.z;
    ////////////////////////////////
    int modcolor_u = (gl_InstanceIndex & 0xfff);
    int modcolor_v = (gl_InstanceIndex >> 12);
    frg_modcolor   = texelFetch(InstanceColors, ivec2(modcolor_u, modcolor_v), 0);
    ////////////////////////////////
  }
} // lib_pbr_vtx_instanced
///////////////////////////////////////////////////////////////
```

example "pbr.fxv2"

```glsl
///////////////////////////////////////////////////////////////
// FxConfigs
///////////////////////////////////////////////////////////////
fxconfig fxcfg_default {
	import "orkshader://pbrtools.i2";
}
///////////////////////////////////////////////////////////////
state_block sb_no_cull : sb_default {
	CullTest = OFF;
}
///////////////////////////////////////////////////////////////
technique PIK_RI_NI {
	fxconfig=fxcfg_default;
	vf_pass={vs_forward_test,ps_forward_test,sb_default}
}
///////////////////////////////////////////////////////////////
technique FWD_SKYBOX_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_forward_skybox_mono,ps_forward_skybox_mono,sb_no_cull}
}
///////////////////////////////////////////////////////////////
technique FWD_UNLIT_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_forward_unlit,ps_forward_unlit,sb_default}
}

///////////////////////////////////////////////////////////////
technique FWD_CV_EMI_RI_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_forward_rigid_vtxcolor,ps_forward_frgcolor,sb_default}
}
///////////////////////////////////////////////////////////////
technique FWD_CT_NM_RI_NI_MO { 
	fxconfig=fxcfg_default;
	vf_pass={vs_forward_test,ps_forward_test,sb_default}
}
///////////////////////////////////////////////////////////////
technique FWD_CT_NM_RI_IN_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_forward_instanced,ps_forward_test_instanced_mono,sb_default}
}
///////////////////////////////////////////////////////////////
state_block sb_dpp : sb_default {
	CullTest = OFF;
	DepthTest = LESS;
	DepthMask = ON;
}
///////////////////////////////////////////////////////////////
technique FWD_DEPTHPREPASS_RI_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_forward_depthprepass_mono,ps_forward_depthprepass_mono,sb_dpp}
}
///////////////////////////////////////////////////////////////
technique GBU_CT_VN_RI_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_rigid_gbuffer,ps_gbuffer,sb_default}
}
///////////////////////////////////////////////////////////////
technique GBU_CV_EMI_RI_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_rigid_gbuffer_vtxcolor,ps_gbuffer_vtxcolor,sb_default}
}
///////////////////////////////////////////////////////////////
technique GBU_CF_NI_MO { // deferred font non-instanced
	fxconfig=fxcfg_default;
	vf_pass={vs_rigid_gbuffer_font,ps_gbuffer_font,sb_default}
}
///////////////////////////////////////////////////////////////
technique GBU_DB_NM_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_rigid_gbuffer,ps_gbuffer_vizn,sb_default}
}
///////////////////////////////////////////////////////////////
technique GBU_CT_NM_RI_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_rigid_gbuffer,ps_gbuffer_n,sb_default}
}
///////////////////////////////////////////////////////////////
technique GBU_CM_NM_RI_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_rigid_gbuffer,ps_gbuffer_n,sb_default}
}
///////////////////////////////////////////////////////////////
technique GBU_CT_NM_SK_NI_MO {
	fxconfig=fxcfg_default;
	vf_pass={vs_skinned_gbuffer,ps_gbuffer_n,sb_default}
}
///////////////////////////////////////////////////////////////
```

