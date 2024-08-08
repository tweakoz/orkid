///////////////////////////////////////////////////////////////
libblock lib_def 
	: lib_math
	: lib_brdf
	: lib_envmapping {
  /////////////////////////////////////////////////////////
  struct XXX {
    vec3 _finallitcolor;
    vec3 _diffuse_env;
    float _depth_fogval;
  };
  /////////////////////////////////////////////////////////
  struct LightCtx {
    vec3 _viewdir;
    vec3 _normal;
    vec3 _lightdel;
    vec3 _F0;
    float _roughness;
    float _metallic;
  };

}
