///////////////////////////////////////////////////////////////
vertex_shader vs_vtxcolor : vface_psys {
  gl_Position = position;
  geo_clr  = vtxcolor;
  geo_uv0  = uv0;
  geo_uv1  = uv1;
}
///////////////////////////////////////////////////////////////
geometry_interface gface_base {
  inputs {
    layout(points);
  }
  outputs {
    layout(triangle_strip, max_vertices = 4);
  }
}
///////////////////////////////////////////////////////////////
geometry_interface gface_bill
    : uset_vtx : gface_base {
  inputs {
    vec4 geo_clr[]; // NOW an array (broadcast)
    vec2 geo_uv0[]; // NOW an array (broadcast)
    vec2 geo_uv1[]; // NOW an array (broadcast)
  }
  outputs {
    vec4 frg_clr;
    vec2 frg_uv0;
    vec2 frg_uv1;
  }
}
///////////////////////////////////////////////////////////////
libblock lib_billboard {
  struct Input {
    vec2 inp_ang_siz;
    vec3 inp_pos;
  };
  struct Output {
    vec4 pos0;
    vec4 pos1;
    vec4 pos2;
    vec4 pos3;
  };
  Output Compute(Input inp) {
    Output outp;

    float ang         = inp.inp_ang_siz.x;
    float size_pixels = inp.inp_ang_siz.y;

    float sizX = size_pixels * Rtg_InvDim.x;
    float sizY = size_pixels * Rtg_InvDim.y;

    vec4 wpos = MatM * vec4(inp.inp_pos, 1);
    vec4 hpos = MatVP * wpos;
    vec4 dpos = hpos / hpos.w;

    vec3 p0 = dpos.xyz + vec3(-sizX, -sizY, 0);
    vec3 p1 = dpos.xyz + vec3(+sizX, -sizY, 0);
    vec3 p2 = dpos.xyz + vec3(+sizX, +sizY, 0);
    vec3 p3 = dpos.xyz + vec3(-sizX, +sizY, 0);

    outp.pos0 = vec4(p0, 1.0);
    outp.pos1 = vec4(p1, 1.0);
    outp.pos2 = vec4(p2, 1.0);
    outp.pos3 = vec4(p3, 1.0);

    return outp;
  }
}
geometry_shader gs_billboardquad //
    : lib_billboard              //
    : gface_bill {               //
  Input inp;
  inp.inp_ang_siz = geo_uv0[0];
  inp.inp_pos     = gl_in[0].gl_Position.xyz;
  Output outp     = Compute(inp);
  frg_uv1     = geo_uv1[0];
  frg_clr     = geo_clr[0];
  //////////////////////////
  gl_Position     = outp.pos0;
  frg_uv0     = vec2(0,0);
  EmitVertex();
  gl_Position = outp.pos1;
  frg_uv0     = vec2(1,0);
  EmitVertex();
  gl_Position = outp.pos3;
  frg_uv0     = vec2(0,1);
  EmitVertex();
  gl_Position = outp.pos2;
  frg_uv0     = vec2(1,1);
  EmitVertex();
  //////////////////////////
  EndPrimitive();
}
///////////////////////////////////////////////////////////////
vertex_interface vface_streak : uset_vtx {
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
    vec3 velocity : BINORMAL;
    vec2 lw : TEXCOORD0; // length and width
    vec2 ra : TEXCOORD1; // random and age
  }
  outputs {
    vec3 geo_cnrm; // NOT an array
    vec3 geo_vel; // NOT an array
    vec2 geo_lw; // NOT an array
    vec2 geo_ra; // NOT an array
  }
}
///////////////////////////////////////////////////////////////
vertex_shader vs_streak : vface_streak {
  gl_Position = position;
  geo_cnrm = normal;
  geo_vel  = velocity;
  geo_lw   = lw;
  geo_ra = ra;
}
///////////////////////////////////////////////////////////////
geometry_interface gface_streak //
  : gface_base { //

  // inputs passed from vertex shader

  inputs {
    vec3 geo_cnrm[];
    vec3 geo_vel[];
    vec2 geo_lw[];
    vec2 geo_ra[];
  }

  outputs {
    vec4 frg_clr;
    vec2 frg_uv0;
    vec2 frg_uv1;
  }
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
libblock lib_streak //
  : lib_ptc_types {
  PtcOutput computeStreak(mat4 mvp) {
    PtcOutput outp;
    vec3 vel  = geo_vel[0].xyz;
    vec3 cnrm = geo_cnrm[0].xyz;
    float wid = geo_lw[0].y;
    float len = geo_lw[0].x;

    vec3 pos = gl_in[0].gl_Position.xyz;

    vec3 lpos = pos - vel * len;
    vec3 crs  = wid * normalize(cross(vel, cnrm));

    vec3 p0 = pos + crs;
    vec3 p1 = pos - crs;
    vec3 p2 = lpos - crs;
    vec3 p3 = lpos + crs;

    outp.pos0 = mvp * vec4(p0, 1.0);
    outp.pos1 = mvp * vec4(p1, 1.0);
    outp.pos2 = mvp * vec4(p2, 1.0);
    outp.pos3 = mvp * vec4(p3, 1.0);

    return outp;
  }
}
///////////////////////////////////////////////////////////////
geometry_shader gs_streak //
  : gface_streak //
  : lib_streak
  : uset_vtx { //
  PtcOutput outp = computeStreak(MatMVP);
  gl_Position       = outp.pos0;
  frg_uv1     = geo_ra[0];
  frg_clr     = vec4(0,0,0,0);
  frg_uv0     = vec2(0,0);
  EmitVertex();
  gl_Position = outp.pos1;
  frg_uv0     = vec2(1,0);
  EmitVertex();
  gl_Position = outp.pos3;
  frg_uv0     = vec2(0,1);
  EmitVertex();
  gl_Position = outp.pos2;
  frg_uv0     = vec2(1,1);
  EmitVertex();
  //EndPrimitive();
}
///////////////////////////////////////////////////////////////
fragment_shader ps_volume : fface_psys {
  vec2 uv        = frg_uv0.xy;
  float w        = frg_uv1.x;
  vec3 uvw       = vec3(uv, w);
  vec4 TexInp0   = texture(VolumeMap, uvw).xyzw;
  out_clr        = TexInp0.bgra * modcolor * frg_clr.bgra;
  out_rufmtl     = vec4(0, 1, 0, 0);
  out_normal_mdl = vec4(0, 0, 0, 5.0);
}
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
uniform_set compute_unis {
    //layout (binding = 1, r32ui) uimage2D img_depthclusters;
    vec3 xxx;
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
            mat4         v_L;             // 0
            mat4         v_R;             // 64
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
compute_shader compute_streaks
    : extension(GL_NV_gpu_shader5)
    : iface_compute {

    int index = int(gl_WorkGroupID.x);

    //float len = LW.x;
    //float wid = LW.y;
      
    vec3 inp_pos  = inp_vertex[index].pos.xyz;
    vec3 inp_vel  = inp_vertex[index].vel.xyz;
    vec2 inp_ar   = inp_vertex[index].age_rand.xy;
    //vec3 inp_nrm  = inp_vertex[index].nrm.xyz;

    //vec3 lpos = inp_pos - (inp_vel * len);

    mat4 ivL = transpose(inverse(v_L));
    mat4 ivR = transpose(inverse(v_R));
    float wid = 0.002*LW.x;
    float hei = 0.002*LW.x;
    vec3 UP = normalize(vec3(ivL[0][0], ivL[1][0], ivL[2][0])) * wid;
    vec3 RT = normalize(vec3(ivL[0][1], ivL[1][1], ivL[2][1])) * hei; // Assume 'hei' is the height of the billboard

    vec3 crs = normalize(cross(inp_vel, obj_nrmz.xyz)) * wid;

    vec3 p0 = inp_pos - UP - RT; // 0 0 
    vec3 p1 = inp_pos + UP - RT; // 1 0
    vec3 p2 = inp_pos + UP + RT; // 1 1
    vec3 p3 = inp_pos - UP + RT; // 0 1

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
    out_vertex[o+1].uv = vec2(1,1);
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
    out_vertex[o+4].uv = vec2(0,1);
    out_vertex[o+4].age_rand = inp_ar;

    out_vertex[o+5].hposL = p2L;
    out_vertex[o+5].hposR = p2R;
    out_vertex[o+5].uv = vec2(1,1);
    out_vertex[o+5].age_rand = inp_ar;
}

///////////////////////////////////////////////////////////////
vertex_interface vface_streak_stereoCI {
  storage {
      layout(std430, binding = 0) buffer {
          int          num_vertices;      // 128
          mat4         v_L;
          mat4         v_R;
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
fragment_interface fface_psys_stereo : uset_frg {
  inputs {
    vec2 frg_uv;
    vec2 frg_age_rand;
  }
  outputs {
    layout(location = 0) vec4 out_clr;
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
fragment_shader ps_flat_stereo //
  : fface_psys_stereo { //
  float unit_age = frg_age_rand.x;
  vec3 C  = modcolor.xyz*(1.0-unit_age);
  out_clr = vec4(C, modcolor.w);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_grad_stereo //
  : fface_psys_stereo { //
  float unit_age = frg_age_rand.x;
  vec4 gmap = texture(GradientMap, vec2(unit_age, 0.0));
  vec4 cmap = texture(ColorMap, frg_uv.xy);
  out_clr.xyz = gmap.xyz*cmap.xyz;
  out_clr.w = gmap.w*cmap.w;
}
///////////////////////////////////////////////////////////////
technique tflatparticle_streaks {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak;
    geometry_shader = gs_streak;
    fragment_shader = ps_flat;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tgradparticle_streaks {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak;
    geometry_shader = gs_streak;
    fragment_shader = ps_grad;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique ttexparticle_streaks {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak;
    geometry_shader = gs_streak;
    fragment_shader = ps_modtexclr;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tstreakparticle {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak;
    geometry_shader = gs_streak;
    fragment_shader = ps_modtexclr;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tvolumeparticle {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_vtxcolor;
    geometry_shader = gs_billboardquad;
    fragment_shader = ps_volume;
    state_block     = sb_alpadd;
  }
}
///////////////////////////////////////////////////////////////
technique tflatparticle_streaks_stereoCI {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak_stereoCI;
    fragment_shader = ps_flat_stereo;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tgradparticle_streaks_stereo {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak_stereoCI;
    fragment_shader = ps_grad_stereo;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
