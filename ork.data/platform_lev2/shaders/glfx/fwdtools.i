///////////////////////////////////////////////////////////////
libblock lib_fwd 
	: lib_math
	: lib_brdf
	: lib_envmapping
	: lib_def {
  /////////////////////////////////////////////////////////
  LightCtx lcalc_forward(vec3 wpos,PbrData pbd){
    LightCtx plc;
    const vec3 metalbase = vec3(0.04);
    float metallic = clamp(pbd._metallic,0.02,0.99);
    vec3 basecolor = pbd._albedo;
    vec3 diffcolor = mix(basecolor,vec3(0),metallic);
    vec3 speccolor = mix(vec3(0.02),basecolor,metallic);
    /////////////////////////
    plc._viewdir = normalize(EyePostion-wpos);
    plc._metallic = metallic; //pbd._metallic;
    plc._roughness = pbd._roughness;
    plc._normal = pbd._wnrm;
    plc._F0 = mix(metalbase,basecolor,metallic);
    return plc;
  }
  /////////////////////////////////////////////////////////
  vec3 plcalc_forward(LightCtx plc,PbrData pbd, float lightRadius){
    float dist2light = sqrt(plc._lightdel.x*plc._lightdel.x + plc._lightdel.y*plc._lightdel.y + plc._lightdel.z*plc._lightdel.z);
    float atten = 1.0 / max(.05, dist2light*dist2light);
    vec3 lightdir = normalize(plc._lightdel);
    vec3 halfdir = normalize(plc._viewdir + lightdir);
    float angularRadius = atan(lightRadius, dist2light);
    float ggx = computeGGX(plc._normal, halfdir, plc._roughness + angularRadius); // Example adjustment
    
    //float ggx = computeGGX(plc._normal, halfdir, plc._roughness);
      float geo = geometrySmith(plc._normal, plc._viewdir, lightdir, plc._roughness);
      vec3 fres = fresnelSchlickRoughness(satdot(halfdir,plc._viewdir), plc._F0,plc._roughness);

      vec3 diffusel = vec3(1) - fres;
      diffusel *= (1 - plc._metallic);
      vec3 diffuse_term = (diffusel*pbd._albedo*INV_PI)*DiffuseLevel;

      vec3 numerator  = min(ggx * geo * SpecularLevel, 16)*fres;
      float denominator = 4 * satdot(plc._normal,plc._viewdir) * satdot(plc._normal, lightdir) + EPSILON;
      float ndotl = satdot(plc._normal,lightdir);
      vec3 specular_term = numerator / max(.0625, denominator);

      return(diffuse_term + specular_term) * atten * ndotl;
      //return lightdir*(atten*ndotl);
  }
  /////////////////////////////////////////////////////////
  vec3 pbrEnvironmentLightingXXX(PbrData pbd, vec3 eyepos){

    vec3 out_color;

    vec3 wpos = pbd._wpos;
    //vec3 metalbase = vec3(0.04);
    vec3 metalbase = vec3(0.2);
    /////////////////////////
    //vec3 albedo = gbd._wnrm;
    vec3 albedo = pbd._albedo;
    /////////////////////////
    vec3 rawn = pbd._wnrm;
    /////////////////////////
    //if( pbd._emissive )
      //return albedo;
    /////////////////////////
    // pixel was written to in the gbuffer
    float metallic = clamp(pbd._metallic,0.02,0.99);
    float roughness = pbd._roughness;
    float dialetric = 1.0-metallic;
    /////////////////////////
    vec3 basecolor = albedo;
    vec3 diffcolor = mix(basecolor,vec3(0),metallic);
    /////////////////////////
    vec3 edir = normalize(wpos-eyepos);
    vec3 n    = rawn; 
    vec3 refl = normalize(reflect(edir,n));
    refl.x *= -1.0;
    /////////////////////////
    float costheta = clamp(dot(n, edir),0.01,0.99);
    vec2 brdf  = textureLod(MapBrdfIntegration, vec2(costheta,roughness*0.99),0).rg;
    ///////////////////////////
    // somethings wrong with the brdf output here
    //   we get speckled black noise
    //   the following 2 lines and the clamp above on
    //   costheta are a temporary fix
    ///////////////////////////
    //brdf = vec2(pow(brdf.x,1),pow(brdf.y,1));
    brdf = clamp(brdf,vec2(0),vec2(1));
    /////////////////////////
    vec3 F0 = mix(metalbase,basecolor,metallic);
    vec3 G0 = mix(metalbase,basecolor,1.0-metallic);
    vec3 F        = fresnelSchlickRoughness(costheta, F0, roughness);
    vec3 invF     = (vec3(1)-F);
    vec3 diffn = vec3(n.x,n.y,n.z);
    /////////////////////////
    float ambocc = 1.0;
    float ambientshade = ambocc*clamp(dot(n,-edir),0,1)*0.3+0.7;
    vec3 ambient = AmbientLevel*ambientshade;
    vec3 diffuse_env = env_equirectangular(diffn,MapDiffuseEnv,0)*DiffuseLevel*SkyboxLevel;
    vec3 diffuse_light = ambient+diffuse_env;
    /////////////////////////
    vec3 diffuse = clamp(basecolor*diffuse_light*dialetric*ambocc,0,1);
    /////////////////////////
    float spec_ruf = pow(roughness,1.3)*0.7;
    float spec_miplevel = SpecularMipBias + (spec_ruf * EnvironmentMipScale);
    refl = vec3(refl.x,-refl.y,refl.z);
    vec3 spec_env = env_equirectangular(refl,MapSpecularEnv,spec_miplevel);
    vec3 specular_light = ambient+spec_env*SkyboxLevel;
    vec3 specularC = specular_light*F0*SpecularLevel*SkyboxLevel;
    vec3 specularMask = clamp(F*brdf.x+brdf.y,0,1);
    vec3 specular = specularMask*specularC;
    //vec3 ambient = invF*AmbientLevel;
    /////////////////////////
    vec3 finallitcolor = saturateV(diffuse+specular);
    float depth_fogval = saturateF(pow(pbd._fogZ*DepthFogDistance,DepthFogPower));
    vec3 skybox_n = vec3(0,0,1);
    vec3 skyboxColor = env_equirectangularFlipV(skybox_n,MapSpecularEnv,0)*SkyboxLevel;

    //return vec3(specular);
    return mix(finallitcolor,skyboxColor,depth_fogval);
    //return vec3(spec_env);

	} // vec3 environmentLighting(){

  /////////////////////////////////////////////////////////

  vec3 _forward_lighting(vec3 modcolor, vec3 eyepos){

    const float inverse_255 = 1.0/255.0;

    vec3 TN = texture(NormalMap,frg_uv0).xyz;
    vec3 N = TN*2.0-vec3(1,1,1);
    vec3 normal = normalize(frg_tbn*N);
    vec3 rufmtlamb = texture(MtlRufMap,frg_uv0).xyz;
    vec3 emission  = texture(EmissiveMap, frg_uv0).xyz;

    vec3 wpos = frg_wpos.xyz;
    PbrData pbd;
    pbd._emissive = length(TN)<0.1;
    pbd._metallic = rufmtlamb.z *  MetallicFactor;
    pbd._roughness = rufmtlamb.y *  RoughnessFactor;
    pbd._albedo = (modcolor*frg_clr.xyz*texture(ColorMap,frg_uv0).xyz);
    pbd._wpos = wpos;
    pbd._wnrm = normal;
    pbd._fogZ = 0.0;
    pbd._atmos = 0.0;
    pbd._alpha = 1.0;

    //if(pbd._emissive){
      //return modcolor*pbd._albedo;
    //}

    vec3 env_lighting = pbrEnvironmentLightingXXX(pbd,eyepos);

    ///////////////////////////////////////////////
    // point lighting
    ///////////////////////////////////////////////

    LightCtx plc = lcalc_forward(wpos,pbd);
    vec3 point_lighting       = vec3(0, 0, 0);
    vec3 spot_lighting       = vec3(0, 0, 0);
    for(int i=0; i<point_light_count; i++){
      plc._lightdel = _lightpos[i].xyz - wpos;
      vec3 LC = _lightcolor[i].xyz*_lightcolor[i].w;
      float LR = _lightradius[i];
      point_lighting += plcalc_forward(plc,pbd,LR)*LC;
    }
    for(int i=0; i<spot_light_count; i++){
      int j = i+point_light_count;
      mat4 shmtx = _shadowmatrix[j];
      vec3 lightpos = _lightpos[j].xyz;
      vec3 lightdel = lightpos-wpos;
      float lightrange = _lightradius[j];
      vec4 light_hpos = (shmtx)*vec4(wpos,1);
      vec3 light_ndc = (light_hpos.xyz / light_hpos.w);
      float lightz = light_ndc.z;
      vec2 lightuv = light_ndc.xy*0.5+vec2(0.5);
      bool mask = bool(light_ndc.x>=-1 && light_ndc.x<1) &&
                   bool(light_ndc.y>=-1 && light_ndc.y<1)&&
                   bool(light_hpos.z>=0.0 && light_hpos.z<=lightrange) ;
      int LCI = _samplerIndex[j];
      vec3 lightcol = _lightcolor[j].xyz;
      vec3 lighttex = texture(light_cookies[LCI],lightuv).xyz;
        vec3 LN = normalize(lightdel);
        float Ldist = length(lightdel);
        float NdotL = max(0.0,dot(normal,LN));

      spot_lighting = lightcol*lighttex*NdotL/pow(Ldist,2)*float(mask);

    }

    ///////////////////////////////////////////////
    //return emission;
    //return point_lighting;
    //return env_lighting;
    //return normal;
    return (env_lighting+point_lighting+spot_lighting+emission);//*modcolor;    
    //return vec3(env_lighting);//*modcolor;    

    //return vec3(rufmtlamb.x * MetallicFactor, //
    //           rufmtlamb.y * RoughnessFactor, //
    //           0);    
  }
  vec3 forward_lighting_mono(vec3 modcolor){
    vec3 eyepos = EyePostion;
    return _forward_lighting(modcolor,eyepos);
  }

}

libblock lib_fwd_stereo : lib_fwd {

  vec3 forward_lighting_stereo(vec3 modcolor){
    vec3 eyepos = bool(gl_ViewportIndex) ? EyePostionR : EyePostionL;
    return _forward_lighting(modcolor,eyepos);
  }

} 
