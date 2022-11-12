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
  /////////////////////////////////////////////////////////
  vec3 pbrEnvironmentLightingXXX(PbrData pbd){

    vec3 out_color;

    vec3 wpos = pbd._wpos;
    vec3 metalbase = vec3(0.04);
    /////////////////////////
    //vec3 albedo = gbd._wnrm;
    vec3 albedo = pbd._albedo;
    /////////////////////////
    vec3 rawn = pbd._wnrm;
    /////////////////////////
    if( pbd._emissive )
      return albedo;
    /////////////////////////
    // pixel was written to in the gbuffer
    float metallic = clamp(pbd._metallic,0.02,0.99);
    float roughness = pbd._roughness;
    float roughnessE = roughness*roughness;
    float roughnessL = max(.01,roughness);
    float dialetric = 1.0-metallic;
    /////////////////////////
    vec3 basecolor = albedo;
    vec3 diffcolor = mix(basecolor,vec3(0),metallic);
    /////////////////////////
    vec3 edir = normalize(wpos-EyePostion);
    vec3 n    = rawn; //normalize(rawn*2-vec3(1));
    vec3 refl = normalize(reflect(edir,n));
    /////////////////////////
    float costheta = clamp(dot(n, edir),0.01,1.0);
    vec2 brdf  = textureLod(MapBrdfIntegration, vec2(costheta,pow(roughness,0.25)),0).rg;
    ///////////////////////////
    // somethings wrong with the brdf output here
    //   we get speckled black noise
    //   the following 2 lines and the clamp above on
    //   costheta are a temporary fix
    ///////////////////////////
    //brdf = vec2(pow(brdf.x,1),pow(brdf.y,1));
    brdf = clamp(brdf,vec2(.001),vec2(1));
    /////////////////////////
    vec3 F0 = mix(metalbase,basecolor,metallic);
    vec3 F        = fresnelSchlickRoughness(costheta, F0, roughness);
    vec3 invF     = (vec3(1)-F);
    vec3 diffn = vec3(n.x,-n.y,n.z);
    /////////////////////////
    float ambocc = 1.0;
    float ambientshade = ambocc*clamp(dot(n,-edir),0,1)*0.3+0.7;
    vec3 ambient = AmbientLevel*ambientshade;
    vec3 diffuse_env = env_equirectangular(diffn,MapDiffuseEnv,0)*DiffuseLevel;
    vec3 diffuse_light = ambient+diffuse_env;
    /////////////////////////
    vec3 diffuse = clamp(basecolor*diffuse_light*dialetric*ambocc,0,10000);
    /////////////////////////
    float spec_miplevel = clamp(EnvironmentMipBias + (roughness * EnvironmentMipScale), 0, 10);
    refl = vec3(refl.x,-refl.y,refl.z);
    vec3 spec_env = env_equirectangularFlipV(refl,MapSpecularEnv,spec_miplevel);
    vec3 specular_light = ambient+spec_env;
    vec3 specular = (F*brdf.x+brdf.y)*specular_light*F0*SpecularLevel;
    //vec3 ambient = invF*AmbientLevel;
    /////////////////////////
    vec3 finallitcolor = saturateV(diffuse+specular);
    float depth_fogval = saturateF(pow(pbd._fogZ*DepthFogDistance,DepthFogPower));
    vec3 skybox_n = vec3(0,0,1);
    vec3 skyboxColor = env_equirectangularFlipV(skybox_n,MapSpecularEnv,0)*SkyboxLevel;


    return mix(finallitcolor,skyboxColor,depth_fogval);

	} // vec3 environmentLighting(){
}