
libblock lib_ssao {
  /////////////////////////////////////////////////////////
  
  vec3 viewpos_nonlin(vec2 uv) {
      float depth = textureLod(MapDepth, uv, 0).r;
      vec4 clipSpacePosition = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
      vec4 viewSpacePosition = MatInvP * clipSpacePosition;
      viewSpacePosition /= viewSpacePosition.w;
      return -viewSpacePosition.xyz;
  }
  
  vec3 ssao_normal_nonlinear(vec2 frg_uv) {
    vec3 base_pos = viewpos_nonlin(frg_uv);
  
    // compute surface normal @ base_pos (via differential normal calculation)
  
    vec2 uv_l = frg_uv + vec2(InvViewportSize.x, 0.0);
    vec2 uv_r = frg_uv - vec2(InvViewportSize.x, 0.0);
    vec2 uv_t = frg_uv + vec2(0.0, InvViewportSize.y);
    vec2 uv_b = frg_uv - vec2(0.0, InvViewportSize.y);
  
    vec3 pos_l = viewpos_nonlin(uv_l);
    vec3 pos_r = viewpos_nonlin(uv_r);
    vec3 pos_t = viewpos_nonlin(uv_t);
    vec3 pos_b = viewpos_nonlin(uv_b);
  
    vec3 dx = pos_l - base_pos;
    vec3 dy = pos_t - base_pos;
    vec3 normal = normalize(cross(dx, dy));
    return normal;
  }
  
  
    /////////////////////////////////////////////////////////
  float ssao_nonlinear(vec2 frg_uv) {
      vec3 base_pos = viewpos_nonlin(frg_uv);
      vec3 normal2 = ssao_normal_nonlinear(frg_uv);
      float base_depth = base_pos.z;
  
      // Make sampling direction more stable with consistent basis
      vec3 tangent = normalize(cross(normal2, vec3(0.0, 1.0, 0.0)));
      vec3 bitangent = cross(normal2, tangent);
      mat3 TBN = mat3(tangent, bitangent, normal2);
  
      float total_occlusion = 0.0;
      int NUM_SAMPLES = 16;
  
      // Make the noise pattern more stable
      vec2 noise_uv = fract(gl_FragCoord.xy / 64.0);
      vec3 rand_vec = texture(SSAOScrNoise, noise_uv).xyz;
      
      // Create rotation matrix for random rotation
      float rot_angle = rand_vec.x * 6.28318;
      mat2 rot_matrix = mat2(
          cos(rot_angle), -sin(rot_angle),
          sin(rot_angle), cos(rot_angle)
      );
      
      int num_passes = 0;
      for(int i = 0; i < NUM_SAMPLES; i++) {
          // Get sample from kernel and ensure it's in the upper hemisphere
          vec3 kernel_sample = texture(SSAOKernel, vec2(float(i)/float(NUM_SAMPLES), 0)).xyz;
          kernel_sample.z = abs(kernel_sample.z); // Ensure positive z (upper hemisphere)
          
          // Create rotation angles for this sample
          float theta = rand_vec.x * 6.28318;
          float phi = rand_vec.y * 3.1415; // Only rotate up to 90 degrees
          
          // Apply rotation to maintain hemispherical distribution
          float sin_theta = sin(theta);
          float cos_theta = cos(theta);
          float sin_phi = sin(phi);
          float cos_phi = cos(phi);
          
          vec3 rotated_sample = vec3(
              kernel_sample.x * cos_theta + kernel_sample.y * sin_theta,
              -kernel_sample.x * sin_theta + kernel_sample.y * cos_theta,
              kernel_sample.z
          );
          rotated_sample = normalize(rotated_sample);
          
          // Bias samples towards normal direction
          float scale = 1.0; //float(i) / float(NUM_SAMPLES);
          scale = mix(0.1, 1.0, scale * scale);
          rotated_sample *= scale;
          
          // Create offset vector in tangent space
          vec3 offset = rotated_sample * SSAORadius;
          
          // convert offset to screen space
          //vec2 offset_uv = vec2(offset.x, offset.y);
          //vec2 offset_screen = offset.xy * 0.1;
  
          // Convert to screen space offset
          vec2 sample_uv = frg_uv + offset.xy;
          
          // Get sample depth
          float sample_depth = viewpos_nonlin(sample_uv).z;
          
          // Compare depths in view space
          float depth_diff = base_depth - sample_depth;
          
          // Accumulate occlusion with distance-based falloff
          total_occlusion += float(depth_diff<SSAOBias);
          num_passes ++;
      }
  
      // Normalize and invert occlusion
      float occlusion = (total_occlusion / float(NUM_SAMPLES));
      
      // Enhance contrast slightly
      occlusion = pow(occlusion, SSAOPower);
      return mix(1.0, occlusion, SSAOWeight);
  }
  
  } // libblock lib_ssao

  