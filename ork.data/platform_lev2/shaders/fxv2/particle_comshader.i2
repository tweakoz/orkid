///////////////////////////////////////////////////////////////
// compute shader based techniques
///////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// stereo streak compute shaders
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

libblock typelib_compute_streaks {
  pragma_typelib;
  struct InputVertex {
    vec4 pos;      // 12
    //ec4 nrm; // 24
    vec4 vel;      // 24
    vec4 age_rand; // 32
  };
  struct OutputVertex {
    vec4 hposL;    // 16
    vec4 hposR;    // 32
    vec2 uv;       // 40
    vec2 age_rand; // 48
  };
}
compute_interface iface_compute
    : typelib_compute_streaks
    : compute_unis {
    inputs {
        layout(local_size_x = 1, local_size_y = 1, local_size_z = 1);
    }
    storage {
        layout(std430, binding = 0) buffer {
            int          num_vertices;      // 128
            mat4         mvp_L;             // 0
            mat4         mvp_R;             // 64
            vec4         obj_nrmz;          // 132
            vec4         LW;                // 144
            InputVertex  inp_vertex[16384]; // 152
            OutputVertex out_vertex[65536]; // 152 + 16384*44
            // total size = 152 + 16384*40 + 65536*40 = 3276952
        } ssbo_compute;
    }
}
///////////////////////////////////////////////////////////////
vertex_interface vface_streak_stereoCI {
  storage {
      layout(std430, binding = 0) buffer {
          int          num_vertices;      // 128
          mat4         mvp_L;             // 0
          mat4         mvp_R;             // 64
          vec3         obj_nrmz;          // 132
          vec2         LW;                // 144
          InputVertex  inp_vertex[16384]; // 152
          OutputVertex out_vertex[65536]; // 152 + 16384*44
          // total size = 152 + 16384*40 + 65536*40 = 3276952
      } ssbo_compute;
  }
  outputs {
    vec2 frg_uv;
    vec2 frg_age_rand;
  }
}
///////////////////////////////////////////////////////////////
vertex_shader vs_streak_stereoCI //
  : extension(GL_NV_stereo_view_rendering) //
  : extension(GL_NV_viewport_array2) //
  : typelib_compute_streaks
  : vface_streak_stereoCI {

    gl_Position = out_vertex[gl_VertexID].hposL;
    gl_SecondaryPositionNV = out_vertex[gl_VertexID].hposR;
    frg_uv = out_vertex[gl_VertexID].uv;
    frg_age_rand = out_vertex[gl_VertexID].age_rand;
    gl_Layer = 0;
    gl_ViewportMask[0] = 1;
    gl_SecondaryViewportMaskNV[0] = 2;
}
///////////////////////////////////////////////////////////////
compute_shader compute_streaks
    : extension(GL_NV_gpu_shader5)
    : iface_compute {

    int index = int(gl_WorkGroupID.x);

    float len = LW.x;
    float wid = LW.y;
      
    vec3 inp_pos  = inp_vertex[index].pos.xyz;
    vec3 inp_vel  = inp_vertex[index].vel.xyz;
    vec2 inp_ar   = inp_vertex[index].age_rand.xy;
    //vec3 inp_nrm  = inp_vertex[index].nrm.xyz;

    vec3 lpos = inp_pos - (inp_vel * len);
    vec3 crs = normalize(cross(inp_vel, obj_nrmz.xyz)) * wid;

    vec3 p0 = inp_pos + crs;
    vec3 p1 = inp_pos - crs;
    vec3 p2 = lpos - crs;
    vec3 p3 = lpos + crs;

    vec4 p0L = mvp_L * vec4(p0,1);
    vec4 p1L = mvp_L * vec4(p1,1);
    vec4 p2L = mvp_L * vec4(p2,1);
    vec4 p3L = mvp_L * vec4(p3,1);

    vec4 p0R = mvp_R * vec4(p0,1);
    vec4 p1R = mvp_R * vec4(p1,1);
    vec4 p2R = mvp_R * vec4(p2,1);
    vec4 p3R = mvp_R * vec4(p3,1);


    int o = index*6;

    // 0 2 1
    // 0 3 2

    out_vertex[o+0].hposL = p0L;
    out_vertex[o+0].hposR = p0R;
    out_vertex[o+0].uv = vec2(0,0);
    out_vertex[o+0].age_rand = inp_ar;

    out_vertex[o+1].hposL = p2L;
    out_vertex[o+1].hposR = p2R;
    out_vertex[o+1].uv = vec2(0,1);
    out_vertex[o+1].age_rand = inp_ar;

    out_vertex[o+2].hposL = p1L;
    out_vertex[o+2].hposR = p1R;
    out_vertex[o+2].uv = vec2(1,0);
    out_vertex[o+2].age_rand = inp_ar;

    //

    out_vertex[o+3].hposL = p0L;
    out_vertex[o+3].hposR = p0R;
    out_vertex[o+3].uv = vec2(0,0);
    out_vertex[o+3].age_rand = inp_ar;

    out_vertex[o+4].hposL = p3L;
    out_vertex[o+4].hposR = p3R;
    out_vertex[o+4].uv = vec2(1,1);
    out_vertex[o+4].age_rand = inp_ar;

    out_vertex[o+5].hposL = p2L;
    out_vertex[o+5].hposR = p2R;
    out_vertex[o+5].uv = vec2(0,1);
    out_vertex[o+5].age_rand = inp_ar;
}
///////////////////////////////////////////////////////////////
fragment_interface fface_psys_stereoCI : uset_frg {
  inputs {
    vec2 frg_uv;
    vec2 frg_age_rand;
  }
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}///////////////////////////////////////////////////////////////
fragment_shader ps_flat_stereoCI //
  : fface_psys_stereoCI { //
  float unit_age = frg_age_rand.x;
  vec3 C  = modcolor.xyz*(1.0-unit_age);
  out_clr = vec4(C, modcolor.w);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_grad_stereoCI //
  : fface_psys_stereoCI { //
  float unit_age = frg_age_rand.x;
  vec4 gmap = texture(GradientMap, vec2(unit_age, 0.0));
  vec4 cmap = texture(ColorMap, frg_uv.xy);
  out_clr.xyz = gmap.xyz*cmap.xyz;
  out_clr.w = gmap.w*cmap.w;
}
///////////////////////////////////////////////////////////////
technique tflatparticle_streaks_stereoCI {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak_stereoCI;
    fragment_shader = ps_flat_stereoCI;
    state_block     = sb_default;
  }
}
technique tgradparticle_streaks_stereo {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak_stereoCI;
    fragment_shader = ps_grad_stereoCI;
    state_block     = sb_default;
  }
}
