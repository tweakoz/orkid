import "orkshader://mathtools.i";
import "orkshader://brdftools.i";
import "orkshader://envtools.i";

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
  /////////////////////////////////////////////////////////
  struct PbrData{
  	bool _emissive;
  	vec3 _wpos;
  	vec3 _wnrm;
  	float _metallic;
  	float _roughness;
  	float _fogZ;
  	float _atmos;
  	float _alpha;
  	vec3 _albedo;
  };

}
