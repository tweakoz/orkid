///////////////////////////////////////////////////////////////
libblock lib_fwd //
  : lib_math //
  : lib_brdf //
  : lib_envmapping //
  : lib_def 
  : lib_ssao
  : lib_pbr {
  /////////////////////////////////////////////////////////
  LightCtx lcalc_forward(vec3 wpos, PbrInpA pbd,vec3 eyepos) {
    LightCtx plc;
    const vec3 metalbase = vec3(0.04);
    float metallic       = clamp(pbd._metallic, 0.02, 0.99);
    vec3 basecolor       = pbd._albedo;
    vec3 diffcolor       = mix(basecolor, vec3(0), metallic);
    vec3 speccolor       = mix(vec3(0.02), basecolor, metallic);
    /////////////////////////
    plc._viewdir   = normalize(eyepos - wpos);
    plc._metallic  = metallic; // pbd._metallic;
    plc._roughness = pbd._roughness;
    plc._normal    = pbd._wnrm;
    plc._F0        = mix(metalbase, basecolor, metallic);
    return plc;
  }
  /////////////////////////////////////////////////////////
  vec3 plcalc_forward(LightCtx plc, PbrInpA pbd, float lightRadius) {
    float dist2light =
        sqrt(plc._lightdel.x * plc._lightdel.x + plc._lightdel.y * plc._lightdel.y + plc._lightdel.z * plc._lightdel.z);
    float atten         = 1.0 / max(.05, dist2light * dist2light);
    vec3 lightdir       = normalize(plc._lightdel);
    vec3 halfdir        = normalize(plc._viewdir + lightdir);
    float angularRadius = atan(lightRadius, dist2light);
    float ggx           = computeGGX(plc._normal, halfdir, plc._roughness + angularRadius); // Example adjustment

    // float ggx = computeGGX(plc._normal, halfdir, plc._roughness);
    float geo = geometrySmith(plc._normal, plc._viewdir, lightdir, plc._roughness);
    vec3 fres = fresnelSchlickRoughness(satdot(halfdir, plc._viewdir), plc._F0, plc._roughness);

    vec3 diffusel = vec3(1) - fres;
    diffusel *= (1 - plc._metallic);
    vec3 diffuse_term = (diffusel * pbd._albedo * INV_PI) * DiffuseLevel;

    vec3 numerator     = min(ggx * geo * SpecularLevel, 16) * fres;
    float denominator  = 4 * satdot(plc._normal, plc._viewdir) * satdot(plc._normal, lightdir) + EPSILON;
    float ndotl        = satdot(plc._normal, lightdir);
    vec3 specular_term = numerator / max(.0625, denominator);

    return (diffuse_term + specular_term) * atten * ndotl;
    // return lightdir*(atten*ndotl);
  }
  /////////////////////////////////////////////////////////

  vec3 _sample_cookie_lod(int index, vec2 uv, float lod) {
    vec3 rval = vec3(0);
    if (index == 0) {
      rval = textureLod(light_cookie0, uv, lod).xyz;
    } else if (index == 1) {
      rval = textureLod(light_cookie1, uv, lod).xyz;
    } else if (index == 2) {
      rval = textureLod(light_cookie2, uv, lod).xyz;
    } else if (index == 3) {
      rval = textureLod(light_cookie3, uv, lod).xyz;
    } else if (index == 4) {
      rval = textureLod(light_cookie4, uv, lod).xyz;
    } else if (index == 5) {
      rval = textureLod(light_cookie5, uv, lod).xyz;
    } else if (index == 6) {
      rval = textureLod(light_cookie6, uv, lod).xyz;
    } else if (index == 7) {
      rval = textureLod(light_cookie7, uv, lod).xyz;
    }
    return rval;
  }
  /////////////////////////////////////////////////////////
  vec3 pbrEnvironmentLightingXXX(PbrInpA pbd, PbrOutA OA) {
    /////////////////////////
    vec3 wpos = pbd._wpos;
    vec3 albedo = pbd._albedo;
    vec3 rawn = pbd._wnrm;
    /////////////////////////
    // ambient occlusion
    /////////////////////////
    // filter sample ambocc
    vec2 uv = (gl_FragCoord.xy) * InvViewportSize;
    float ambocc = texture(SSAOMap, uv).x;
    ambocc = pow(ambocc, SSAOPower);
    ambocc = mix(1.0,ambocc,SSAOWeight);
    /////////////////////////
    //float ambientshade = clamp(dot(n, -edir), 0, 1) * 0.3 + 0.7;
    vec3 ambient       = OA._ambient*ambocc;
    //vec3 diffuse_env   = env_equirectangular(rawn, MapDiffuseEnv, 0) * DiffuseLevel * SkyboxLevel;
    vec3 diffuse_env = OA._diffuseEnv; //env_equirectangular(rawn,MapDiffuseEnv,0)*DiffuseLevel*SkyboxLevel;
    vec3 diffuse_light = ambient + diffuse_env;
    /////////////////////////
    vec3 diffuse = clamp(albedo * diffuse_light * OA._dialetric * ambocc, 0, 1);
    /////////////////////////
    // rotate refl by 180 degrees on y to get refl_probe_coord
    vec3 refl = OA._refl;
    vec3 refl_probe_coord = vec3(-refl.x, refl.y, -refl.z);
    vec3 probe_REFL = texture(reflectionPROBE, refl_probe_coord).xyz;
    /////////////////////////
    float spec_ruf      = pow(pbd._roughness, 1.3) * 0.7;
    float spec_miplevel = SpecularMipBias + (spec_ruf * EnvironmentMipScale);
    vec3 refl_equi      = vec3(-refl.x, -refl.y, refl.z);
    vec3 spec_env       = env_equirectangular(refl_equi, MapSpecularEnv, spec_miplevel)+probe_REFL;
    vec3 specular_light = ambient + spec_env * SkyboxLevel;
    vec3 specularC      = specular_light * OA._F0 * SpecularLevel;
    vec3 specularMask   = clamp(OA._F * OA._BRDF.x + OA._BRDF.y, 0, 1);
    vec3 specular       = specularMask * specularC*ambocc;
    /////////////////////////
    //return albedo;
    return saturateV(diffuse + specular);
  } // vec3 environmentLighting(){
  /////////////////////////////////////////////////////////  
  vec3 _forward_lighting(vec3 modcolor, vec3 eyepos) {

    // sample PBR material textures
    vec3 albedo = (BaseColor.xyz*modcolor * frg_clr.xyz * texture(CNMREA, vec3(frg_uv0, 0)).xyz);
    vec3 TN        = texture(CNMREA, vec3(frg_uv0, 1)).xyz;
    vec3 rufmtlamb = texture(CNMREA, vec3(frg_uv0, 2)).xyz;
    vec3 emission  = texture(CNMREA, vec3(frg_uv0, 3)).xyz;

    vec3 wpos = frg_wpos.xyz;
    vec3 N         = TN * 2.0 - vec3(1, 1, 1);
    vec3 normal    = normalize(frg_tbn * N);
    /////////////////////////
    // ambient occlusion
    /////////////////////////
    vec2 uv = (gl_FragCoord.xy) * InvViewportSize;
    float ambocc = texture(SSAOMap, uv).x;
    ambocc = pow(ambocc, SSAOPower);
    ambocc = mix(1.0,ambocc,SSAOWeight);
    /////////////////////////
    PbrInpA pbd;
    pbd._emissive  = length(TN) < 0.1;
    pbd._metallic  = rufmtlamb.z * MetallicFactor;
    pbd._roughness = rufmtlamb.y * RoughnessFactor;
    pbd._albedo    = albedo;
    pbd._epos      = eyepos;
    pbd._wpos      = wpos;
    pbd._wnrm      = normal;
    pbd._fogZ      = 0.0;
    pbd._atmos     = 0.0;
    pbd._alpha     = 1.0;

    // if(pbd._emissive){
    // return modcolor*pbd._albedo;
    //}

    PbrOutA OA = pbrIoEnvA(pbd);

    ///////////////////////////////////////////////

    vec3 ambient       = AmbientLevel * OA._ambient*ambocc;

    ///////////////////////////////////////////////
    // point lighting
    ///////////////////////////////////////////////

    LightCtx plc        = lcalc_forward(wpos, pbd,eyepos);
    vec3 point_lighting = vec3(0, 0, 0);
    for (int i = 0; i < point_light_count; i++) {
      plc._lightdel = _lightpos[i].xyz - wpos;
      vec3 LC       = _lightcolor[i].xyz * _lightcolor[i].w;
      float LR      = _lightsizbias[i].x;
      point_lighting += plcalc_forward(plc, pbd, LR) * LC;
    }

    ///////////////////////////////////////////////
    // spot lighting
    ///////////////////////////////////////////////

    vec3 spot_lighting = vec3(0, 0, 0);

    for (int i = 0; i < spot_light_count; i++) {
      int j = i + point_light_count;

      int LCI_STD = j * 2 + 0;
      int LCI_DEP = j * 2 + 1;

      vec4 LSB = _lightsizbias[j];

      mat4 shmtx           = _shadowmatrix[j];
      vec3 lightpos        = _lightpos[j].xyz;
      vec3 lightdel        = lightpos - wpos;
      float lightrange     = LSB.x;
      vec4 light_hpos      = (shmtx)*vec4(wpos, 1);
      vec3 light_ndc       = (light_hpos.xyz / light_hpos.w);
      float lightz         = light_ndc.z;
      vec2 diffuse_lightuv = light_ndc.xy * 0.5 + vec2(0.5);

      bool mask = bool(light_ndc.x >= -1 && light_ndc.x < 1) && bool(light_ndc.y >= -1 && light_ndc.y < 1) &&
                  bool(light_ndc.z >= 0.0 && light_ndc.z <= 1);

      ////////////////////////////////////////////////////////////
      // compute specular lightuv
      ////////////////////////////////////////////////////////////

      vec3 lightdir         = normalize(lightdel * -1);
      vec3 halfdir          = normalize(lightdir - normalize(eyepos - wpos));
      vec4 light_hpos2      = (shmtx)*vec4(wpos + halfdir, 1);
      vec3 light_ndc2       = (light_hpos2.xyz / light_hpos2.w);
      vec2 specular_lightuv = light_ndc2.xy * 0.5 + vec2(0.5);

      bool _specular_mask = bool(light_ndc2.x >= -1 && light_ndc2.x < 1) && bool(light_ndc2.y >= -1 && light_ndc2.y < 1) &&
                           bool(light_ndc2.z >= 0.0 && light_ndc2.z <= 1);

      vec3 specular_mask = vec3(float(_specular_mask));

      //vec3 specularMask   = clamp(F * brdf.x + brdf.y, 0, 1);

      ////////////////////////////////////////////////////////////

      plc._lightdel = lightdel;
      vec3 LC       = _lightcolor[i].xyz * _lightcolor[i].w;
      float LR      = 50;
      vec3 pl_c = plcalc_forward(plc, pbd, LR) * LC;
      pl_c = mix(pl_c,vec3(1),0.25);



      ////////////////////////////////////////////////////////////
      // compute wpos in shadow space
      ////////////////////////////////////////////////////////////

      vec4 shadow_hpos    = (shmtx)*vec4(wpos, 1);
      vec3 shadow_ndc     = (shadow_hpos.xyz / shadow_hpos.w);
      vec2 shadow_uv      = shadow_ndc.xy * 0.5 + vec2(0.5);
      float shadow_factor = 0.0;
      bool shadow_mask    = shadow_ndc.x >= -1 && shadow_ndc.x < 1 && shadow_ndc.y >= -1 && shadow_ndc.y < 1;
      if (!shadow_mask) {
        shadow_factor = 1.0;
      } else {
        float bias             = LSB.y; // Increased bias to help with shadow acne
        float far              = lightrange;
        float near             = lightrange*0.001;
        float shadow_depth_ndc = _sample_cookie_lod(LCI_DEP, shadow_uv, 0).x * 2.0 - 1.0;

        // Percentage-Closer Filtering (PCF)
        int pcf_width         = 1;            // Size of the PCF kernel
        float pcf_filter_size = 1.0 / LSB.z; // Adjust based on your shadow map resolution

        for (int x = -pcf_width; x <= pcf_width; x++) {
          for (int y = -pcf_width; y <= pcf_width; y++) {
            vec2 pcf_uv     = shadow_uv + vec2(x, y) * pcf_filter_size;
            float pcf_depth = _sample_cookie_lod(LCI_DEP, pcf_uv, 0).x * 2.0 - 1.0;
            shadow_factor += (pcf_depth + bias) >= lightz ? 1.0 : 0.0;
          }
        }

        // Normalize the shadow factor by the number of samples taken
        shadow_factor /= float((pcf_width * 2 + 1) * (pcf_width * 2 + 1));
      }

      ///////////////////////

      vec3 lightcol          = _lightcolor[j].xyz;
      float level            = pbd._roughness*4;
      vec3 diffuse_lighttex  = _sample_cookie_lod(LCI_STD, diffuse_lightuv, 0).xyz;   // diffuse WIP
      vec3 specular_lighttex = _sample_cookie_lod(LCI_STD, specular_lightuv, level).xyz; // specular WIP

      //specular_lighttex *= plc._F0 * pl_c * float(specular_mask);

      // vec3 lighttex = specular_lighttex; //mix(diffuse_lighttex,specular_lighttex,1.0-pow(pbd._roughness,1.0));

      vec3 LN     = normalize(lightdel);
      float Ldist = length(lightdel);
      float NdotL = max(0.0, dot(normal, LN));

      float spec_mix = (1.0 - pow(pbd._roughness, 1.0));


      vec3 diffuse = pbd._albedo*diffuse_lighttex * NdotL;// * plc._F0;// * pl_c;// * (1.0 - spec_mix);
      vec3 lighttex  = diffuse;
      lighttex += OA._F0 * pbd._albedo * specular_lighttex * NdotL * specular_mask * spec_mix;
      spot_lighting += lightcol * lighttex / pow(Ldist, 2) * float(mask) * shadow_factor;
      //spot_lighting += vec3(specular_lighttex);
       //spot_lighting += pl_c;
    }
    //return vec3(1,1,1);
    vec3 env_lighting = pbrEnvironmentLightingXXX(pbd, OA);
    //return env_lighting;
    return (env_lighting + point_lighting + spot_lighting + emission); //*modcolor;
  }
  vec3 forward_lighting_mono(vec3 modcolor) {
    vec3 eyepos = EyePostion;
    return _forward_lighting(modcolor, eyepos);
  }
}

libblock lib_fwd_stereo : lib_fwd {

  vec3 forward_lighting_stereo(vec3 modcolor) {
    vec3 eyepos = bool(gl_ViewportIndex) ? EyePostionR : EyePostionL;
    return _forward_lighting(modcolor, eyepos);
  }
}
