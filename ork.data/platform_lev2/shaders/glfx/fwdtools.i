///////////////////////////////////////////////////////////////
libblock lib_fwd //
  : lib_math //
  : lib_brdf //
  : lib_envmapping //
  : lib_def 
  : lib_ssao {
  /////////////////////////////////////////////////////////
  LightCtx lcalc_forward(vec3 wpos, PbrData pbd,vec3 eyepos) {
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
  vec3 plcalc_forward(LightCtx plc, PbrData pbd, float lightRadius) {
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
  vec3 pbrEnvironmentLightingXXX(PbrData pbd, vec3 eyepos) {

    vec3 out_color;

    vec3 wpos = pbd._wpos;
    // vec3 metalbase = vec3(0.04);
    vec3 metalbase = vec3(0.2);
    /////////////////////////
    // vec3 albedo = gbd._wnrm;
    vec3 albedo = pbd._albedo;
    /////////////////////////
    vec3 rawn = pbd._wnrm;
    /////////////////////////
    // if( pbd._emissive )
    // return albedo;
    /////////////////////////
    // pixel was written to in the gbuffer
    float metallic  = clamp(pbd._metallic, 0.02, 0.99);
    float roughness = pbd._roughness;
    float dialetric = 1.0 - metallic;
    /////////////////////////
    vec3 basecolor = albedo;
    vec3 diffcolor = mix(basecolor, vec3(0), metallic);
    /////////////////////////
    vec3 edir = normalize(wpos - eyepos);
    vec3 n    = rawn;
    vec3 refl = normalize(reflect(edir, n));
    refl.x *= -1.0;
    /////////////////////////
    float costheta = clamp(dot(n, edir), 0.01, 0.99);
    vec2 brdf      = textureLod(MapBrdfIntegration, vec2(costheta, roughness * 0.99), 0).rg;
    ///////////////////////////
    // somethings wrong with the brdf output here
    //   we get speckled black noise
    //   the following 2 lines and the clamp above on
    //   costheta are a temporary fix
    ///////////////////////////
    // brdf = vec2(pow(brdf.x,1),pow(brdf.y,1));
    brdf = clamp(brdf, vec2(0), vec2(1));
    /////////////////////////
    vec3 F0    = mix(metalbase, basecolor, metallic);
    vec3 G0    = mix(metalbase, basecolor, 1.0 - metallic);
    vec3 F     = fresnelSchlickRoughness(costheta, F0, roughness);
    vec3 invF  = (vec3(1) - F);
    vec3 diffn = vec3(n.x, n.y, n.z);
    /////////////////////////
    // ambient occlusion
    /////////////////////////
    //vec2 uv = gl_FragCoord.xy * InvViewportSize;
    //float ambocc = texture(SSAOMap, uv).x;
    // filter sample ambocc
    vec2 ssao_uv = (gl_FragCoord.xy) * InvViewportSize;
    float sambocc = 1.0; //texture(AmbOccMap, uv).x;
    float dambocc = 1.0; //texture(SSAOMap, ssao_uv).x;
    //dambocc = pow(dambocc, SSAOPower);
    //dambocc = mix(1.0,dambocc,SSAOWeight);
    float ambocc = sambocc * dambocc;
    //ambocc = 1.0;//
    /////////////////////////
    float ambientshade = clamp(dot(n, -edir), 0, 1) * 0.3 + 0.7;
    vec3 ambient       = AmbientLevel * ambientshade*ambocc;
    vec3 diffuse_env   = env_equirectangular(diffn, MapDiffuseEnv, 0) * DiffuseLevel * SkyboxLevel;
    vec3 diffuse_light = ambient + diffuse_env;
    /////////////////////////
    vec3 diffuse = clamp(basecolor * diffuse_light * dialetric * ambocc, 0, 1);
    /////////////////////////
    // rotate refl by 180 degrees on y to get refl_probe_coord
    vec3 refl_probe_coord = vec3(-refl.x, refl.y, -refl.z);
    vec3 probe_REFL = texture(reflectionPROBE, refl_probe_coord).xyz;
    /////////////////////////
    float spec_ruf      = pow(roughness, 1.3) * 0.7;
    float spec_miplevel = SpecularMipBias + (spec_ruf * EnvironmentMipScale);
    vec3 refl_equi      = vec3(refl.x, -refl.y, refl.z);
    vec3 spec_env       = env_equirectangular(refl_equi, MapSpecularEnv, spec_miplevel)+probe_REFL;
    vec3 specular_light = ambient + spec_env * SkyboxLevel;
    vec3 specularC      = specular_light * F0 * SpecularLevel * SkyboxLevel;
    vec3 specularMask   = clamp(F * brdf.x + brdf.y, 0, 1);
    vec3 specular       = specularMask * specularC*ambocc;

    //vec3 probe_REFL = vec3(0);
    // vec3 ambient = invF*AmbientLevel;
    /////////////////////////
    vec3 finallitcolor = saturateV(diffuse + specular);
    float depth_fogval = saturateF(pow(pbd._fogZ * DepthFogDistance, DepthFogPower));
    vec3 skybox_n      = vec3(0, 0, 1);
    vec3 skyboxColor   = env_equirectangularFlipV(skybox_n, MapSpecularEnv, 0) * SkyboxLevel;

    // return vec3(specular);
    return finallitcolor;
    //return vec3(ssao_linear(uv));
    //return vec3(ambocc);

  } // vec3 environmentLighting(){

  /////////////////////////////////////////////////////////

  vec3 _sample_cookie_lod(int index, vec2 uv, float lod) {
    vec3 rval = vec3(0);
    if (index == 0) {
      rval = textureLod(light_cookie0, uv, lod).xyz;
    } else if (index == 1) {
      rval = textureLod(light_cookie1, uv, lod).xyz;
    } else if (index == 2) {
      //rval = textureLod(light_cookie2, uv, lod).xyz;
    } else if (index == 3) {
      //rval = textureLod(light_cookie3, uv, lod).xyz;
    }
    return rval;
  }

  vec3 _forward_lighting(vec3 modcolor, vec3 eyepos) {

    vec3 wpos = frg_wpos.xyz;
    vec3 TN        = texture(NormalMap, frg_uv0).xyz;
    vec3 N         = TN * 2.0 - vec3(1, 1, 1);
    vec3 normal    = normalize(frg_tbn * N);
    vec3 rufmtlamb = texture(MtlRufMap, frg_uv0).xyz;
    vec3 emission  = texture(EmissiveMap, frg_uv0).xyz;
    vec3 metalbase = vec3(0.2);
    float metallic  = clamp(rufmtlamb.z * MetallicFactor, 0.02, 0.99);
    float roughness = rufmtlamb.y * RoughnessFactor;
    float dialetric = 1.0 - metallic;
    vec3 albedo = (modcolor * frg_clr.xyz * texture(ColorMap, frg_uv0).xyz);
    vec3 basecolor = albedo;
    vec3 diffcolor = mix(basecolor, vec3(0), metallic);
    /////////////////////////
    vec3 edir = normalize(wpos - eyepos);
    vec3 refl = normalize(reflect(edir, normal));
    refl.x *= -1.0;
    /////////////////////////
    // ambient occlusion
    /////////////////////////
    vec2 ssao_uv = (gl_FragCoord.xy) * InvViewportSize;
    vec3 sambocc = texture(AmbOccMap, frg_uv0).xyz*0.7; // static AO
    float dambocc = 1.0; //texture(SSAOMap, ssao_uv).x;  // dynamic AO
    dambocc = pow(dambocc, SSAOPower);
    dambocc = mix(1.0,dambocc,SSAOWeight);
    vec3 ambocc = sambocc * dambocc;
    /////////////////////////
    float ambientshade = clamp(dot(normal, -edir), 0, 1) * 0.3 + 0.7;
    vec3 ambient       = AmbientLevel * ambientshade;
    /////////////////////////
    float costheta = clamp(dot(normal, edir), 0.01, 0.99);
    vec2 brdf      = textureLod(MapBrdfIntegration, vec2(costheta, roughness * 0.99), 0).rg;
    ///////////////////////////
    // somethings wrong with the brdf output here
    //   we get speckled black noise
    //   the following 2 lines and the clamp above on
    //   costheta are a temporary fix
    ///////////////////////////
    // brdf = vec2(pow(brdf.x,1),pow(brdf.y,1));
    brdf = clamp(brdf, vec2(0), vec2(1));
    /////////////////////////
    vec3 F0    = mix(metalbase, basecolor, metallic);
    vec3 G0    = mix(metalbase, basecolor, 1.0 - metallic);
    vec3 F     = fresnelSchlickRoughness(costheta, F0, roughness);
    vec3 invF  = (vec3(1) - F);
    vec3 diffn = normal;
    /////////////////////////

    PbrData pbd;
    pbd._emissive  = length(TN) < 0.1;
    pbd._metallic  = metallic;
    pbd._roughness = roughness;
    pbd._albedo    = albedo;
    pbd._wpos      = wpos;
    pbd._wnrm      = normal;
    pbd._fogZ      = 0.0;
    pbd._atmos     = 0.0;
    pbd._alpha     = 1.0;

    // if(pbd._emissive){
    // return modcolor*pbd._albedo;
    //}

    vec3 env_lighting = pbrEnvironmentLightingXXX(pbd, eyepos);
    env_lighting = env_lighting * sambocc;
    
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
      lighttex += F0 * pbd._albedo * specular_lighttex * NdotL * specular_mask * spec_mix;
      spot_lighting += lightcol * lighttex / pow(Ldist, 2) * float(mask) * shadow_factor;
      //spot_lighting += vec3(specular_lighttex);
       //spot_lighting += pl_c;
    }
    //return spot_lighting;
    //return vec3(ambocc,ambocc,ambocc);
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
