///////////////////////////////////////////////////////////////////////////////
// environment mapping functions
///////////////////////////////////////////////////////////////////////////////

libblock lib_envmapping {

  ////////////////////////////////////////////
  // equirectangular envmap uv from normal
  ////////////////////////////////////////////

  vec2 env_equirectangularN2UV(vec3 normal) {
      vec3 n = vec3(-normal.xz,normal.y);
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
    return sphericalToNormal(phi,theta);
  }

  ////////////////////////////////////////////
  // equirectangular envmap texture sample
  ////////////////////////////////////////////

  vec3 env_equirectangular(vec3 normal, sampler2D envtex, float miplevel) {
    vec2 uv = env_equirectangularN2UV(normal);
    return textureLod(envtex, uv, miplevel).xyz;
  }

}
