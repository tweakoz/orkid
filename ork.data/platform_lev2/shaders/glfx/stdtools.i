///////////////////////////////////////////////////////////////
uniform_set uset_std_matrices {
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
///////////////////////////////////////////////////////////////
uniform_set uset_std_viewport {
  vec2 ViewportSize;    // target size
  vec2 InvViewportSize; // inverse target size
}
///////////////////////////////////////////////////////////////
uniform_set uset_std_instancing {
  sampler2D InstanceMatrices;
  sampler2D InstanceColors;
  usampler2D InstanceIds;
}
///////////////////////////////////////////////////////////////
uniform_set uset_std_filtering {
  float FilterRadius;
}
///////////////////////////////////////////////////////////////
uniform_set uset_std_aux {
  vec4 AuxA;
  vec4 AuxB;
}
///////////////////////////////////////////////////////////////
uniform_set uset_std_pick {
  uint obj_pickID;
}
///////////////////////////////////////////////////////////////
uniform_set uset_std_pbr {
  sampler2DArray CNMREA;         
  samplerCube reflectionPROBE;
  samplerCube irradiancePROBE;
  sampler2D MapBrdfIntegration; // 8
  sampler2D MapDiffuseEnv;      // 9
  sampler2DArray MapSpecularEnv;           // 2
  float MetallicFactor;
  float RoughnessFactor;
  float RoughnessPower;
  float SkyboxLevel;
  float SpecularLevel;
  float DiffuseLevel;
  vec3 AmbientLevel;
  float EnvironmentMipBias;
  float EnvironmentMipScale;
  float RoughnessLevels;
  float SpecularMipBias;
  float DepthFogDistance;
  float DepthFogPower;
  vec2 Zndc2eye;
  vec3 EyePostion;
  vec3 EyePostionL;
  vec3 EyePostionR;
  vec4 ModColor;
}
///////////////////////////////////////////////////////////////
uniform_set uset_std_lighting {
  sampler2DArray LightMapArray;            
  sampler2DArray light_cookie_colors;      
  sampler2DArray light_cookie_depths;      
  sampler2D MapDepth;           
  sampler2D MapLinearDepth;     
  vec3 LightMapColors[8];        
  int point_light_count;
  int spot_light_count;
}
///////////////////////////////////////////////////////////////
