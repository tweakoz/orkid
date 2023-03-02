///////////////////////////////////////////////////////////////////////////////
// environment mapping functions
///////////////////////////////////////////////////////////////////////////////

libblock lib_envmapping {

  ////////////////////////////////////////////
  // equirectangular envmap uv from normal
  ////////////////////////////////////////////

  vec2 env_equirectangularN2UV(vec3 normal) {
      vec3 n = vec3(normal.xy,normal.z);
      vec2 s = normalToSpherical(n) * INV_PI;
      float u = (s.x+1.0)*0.5;
      float v = (s.y);
      return vec2(u,v);
  }

  ////////////////////////////////////////////
  // equirectangular envmap normal from uv
  ////////////////////////////////////////////

  vec3 env_equirectangularUV2N(vec2 tex_uv) {
    float phi = tex_uv.x*PI2-PI;
    float theta = tex_uv.y * PI;
    vec3 n = sphericalToNormal(phi,theta);
    return vec3(n.x,n.z,n.y);
  }

  ////////////////////////////////////////////
  // equirectangular envmap texture sample
  ////////////////////////////////////////////

  vec3 env_equirectangular(vec3 normal, sampler2D envtex, float miplevel) {
    vec3 n = vec3(normal.x,normal.y,normal.z);
    vec2 uv = env_equirectangularN2UV(n);
    return textureLod(envtex, vec2(-uv.x,-uv.y), miplevel).xyz;
  }
  vec3 env_equirectangularFlipV(vec3 normal, sampler2D envtex, float miplevel) {
    vec3 n = vec3(normal.x,normal.y,normal.z);
    vec2 uv = env_equirectangularN2UV(n);
    return textureLod(envtex, vec2(-uv.x,uv.y), miplevel).xyz;
  }
  vec3 env_equirectangularFlipVBL(vec3 normal, sampler2D envtex_spec, sampler2D envtex_diff, float blend) {
    vec3 n = vec3(normal.x,normal.y,normal.z);
    vec2 uv = env_equirectangularN2UV(n);
    float miplevel = clamp(EnvironmentMipBias + (blend*EnvironmentMipScale), 0, 10);
    vec3 spec = textureLod(envtex_spec, vec2(-uv.x,uv.y), miplevel).xyz;
    vec3 diff = textureLod(envtex_diff, vec2(-uv.x,uv.y), miplevel).xyz;
    return mix(spec,diff,blend);
  }

}
