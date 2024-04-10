///////////////////////////////////////////////////////////////
// FxConfigs
///////////////////////////////////////////////////////////////
fxconfig fxcfg_sprite_default {
  glsl_version = "330";
}
///////////////////////////////////////////////////////////////
geometry_interface gface_sprite_base {
  inputs {
    layout(points);
  }
  outputs {
    layout(triangle_strip, max_vertices = 4);
  }
}
///////////////////////////////////////////////////////////////
vertex_interface vface_sprite : uset_vtx {
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
    vec3 velocity : BINORMAL;
    vec2 lw : TEXCOORD1; // size
    vec2 ra : TEXCOORD2; // random and age
  }
  outputs {
    vec3 geo_cnrm; // NOT an array
    vec3 geo_vel; // NOT an array
    vec2 geo_lw; // NOT an array
    vec2 geo_ra; // NOT an array
  }
}
///////////////////////////////////////////////////////////////
geometry_interface gface_sprite //
  : gface_sprite_base { //

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
libblock lib_sprite //
  : lib_ptc_types {
  ///////////////////////////////////////////
  PtcOutput computeSprite(mat4 mvp) {
    PtcOutput outp;
    //vec3 vel  = geo_vel[0].xyz;
    //vec3 cnrm = geo_cnrm[0].xyz;
    float size = 0.1;//geo_lw[0].y;

    mat4 iv = transpose(MatIV);
    vec3 UP = normalize(vec3(iv[0][0], iv[1][0], iv[2][0])) * size;
    vec3 RT = normalize(vec3(iv[0][1], iv[1][1], iv[2][1])) * size; 
    
    vec3 inp_pos = gl_in[0].gl_Position.xyz;

    vec3 p0 = inp_pos - UP - RT; // 0 0 
    vec3 p1 = inp_pos + UP - RT; // 1 0
    vec3 p2 = inp_pos + UP + RT; // 1 1
    vec3 p3 = inp_pos - UP + RT; // 0 1

    outp.pos0 = mvp * vec4(p0, 1.0);
    outp.pos1 = mvp * vec4(p1, 1.0);
    outp.pos2 = mvp * vec4(p2, 1.0);
    outp.pos3 = mvp * vec4(p3, 1.0);

    return outp;
  }
}
///////////////////////////////////////////////////////////////
vertex_shader vs_sprite : vface_sprite {
  gl_Position = position;
  geo_cnrm = normal;
  geo_vel  = velocity;
  geo_lw   = lw;
  geo_ra = ra;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_sprite_tex : vface_sprite {
  gl_Position = position;
  geo_cnrm = normal;
  geo_vel  = velocity;
  geo_lw   = lw;
  geo_ra = ra;
}
///////////////////////////////////////////////////////////////
geometry_shader gs_sprite //
  : gface_sprite //
  : lib_sprite
  : uset_vtx { //
  PtcOutput outp = computeSprite(MatMVP);
  vec3 NRM = geo_cnrm[0];
  vec3 VEL = geo_vel[0];
  vec2 LW = geo_lw[0];
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
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
technique tflatparticle_sprites {
  fxconfig = fxcfg_sprite_default;
  pass p0 {
    vertex_shader   = vs_sprite;
    geometry_shader = gs_sprite;
    fragment_shader = ps_flat;
    state_block     = sb_default;
  }
}
technique tgradparticle_sprites {
  fxconfig = fxcfg_sprite_default;
  pass p0 {
    vertex_shader   = vs_sprite;
    geometry_shader = gs_sprite;
    fragment_shader = ps_grad;
    state_block     = sb_default;
  }
}
technique ttexparticle_sprites {
  fxconfig = fxcfg_sprite_default;
  pass p0 {
    vertex_shader   = vs_sprite_tex;
    geometry_shader = gs_sprite;
    fragment_shader = ps_modtexclr;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// stereo sprite compute shaders
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

libblock typelib_compute_sprites {
  pragma_typelib;
  struct InputVertexSprite {
    vec4 pos;      // 16
    vec4 lw;       // 32
    vec4 vel;      // 48
    vec4 age_rand; // 64
  };
  struct OutputVertexSprite {
    vec4 hposL;    // 16
    vec4 hposR;    // 32
    vec2 uv;       // 40
    vec2 age_rand; // 48
  };
}
uniform_set compute_unis_sprites {
    //layout (binding = 1, r32ui) uimage2D img_depthclusters;
    vec3 xxx;
}
compute_interface iface_compute_sprites
    : typelib_compute_sprites
    : compute_unis_sprites {
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
            InputVertexSprite  inp_vertex[16384]; // 152
            OutputVertexSprite out_vertex[65536]; // 152 + 16384*44
            // total size = 152 + 16384*40 + 65536*40 = 3276952
        } ssbo_compute;
    }
}
compute_shader compute_sprites
    : extension(GL_NV_gpu_shader5)
    : iface_compute_sprites {

    int index = int(gl_WorkGroupID.x);
    
    vec3 inp_pos  = inp_vertex[index].pos.xyz;
    vec3 inp_lw   = inp_vertex[index].lw.xyz;
    vec3 inp_vel  = inp_vertex[index].vel.xyz;
    vec2 inp_ar   = inp_vertex[index].age_rand.xy;

    float len = inp_lw.x;
    float wid = inp_lw.y;

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
vertex_interface vface_sprite_stereoCI {
  storage {
      layout(std430, binding = 0) buffer {
          int          num_vertices;      // 128
          mat4         v_L;
          mat4         v_R;
          mat4         mvp_L;             // 0
          mat4         mvp_R;             // 64
          vec3         obj_nrmz;          // 132
          InputVertexSprite  inp_vertex[16384]; // 152
          OutputVertexSprite out_vertex[65536]; // 152 + 16384*44
          // total size = 152 + 16384*40 + 65536*40 = 3276952
      } ssbo_compute;
  }
  outputs {
    vec2 frg_uv;
    vec2 frg_age_rand;
  }
}
fragment_interface fface_psys_sprite_stereo : uset_frg {
  inputs {
    vec2 frg_uv;
    vec2 frg_age_rand;
  }
  outputs {
    layout(location = 0) vec4 out_clr;
  }
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
vertex_shader vs_sprite_stereoCI //
  : extension(GL_NV_stereo_view_rendering) //
  : extension(GL_NV_viewport_array2) //
  : typelib_compute_sprites
  : vface_sprite_stereoCI {

    gl_Position = out_vertex[gl_VertexID].hposL;
    gl_SecondaryPositionNV = out_vertex[gl_VertexID].hposR;
    frg_uv = out_vertex[gl_VertexID].uv;
    frg_age_rand = out_vertex[gl_VertexID].age_rand;
    gl_Layer = 0;
    gl_ViewportMask[0] = 1;
    gl_SecondaryViewportMaskNV[0] = 2;
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
fragment_shader ps_sprite_flat_stereo //
  : fface_psys_sprite_stereo { //
  float unit_age = frg_age_rand.x;
  vec3 C  = modcolor.xyz*(1.0-unit_age);
  out_clr = vec4(C, modcolor.w);
}
///////////////////////////////////////////////////////////////
fragment_shader ps_sprite_grad_stereo //
  : fface_psys_sprite_stereo { //
  float unit_age = frg_age_rand.x;
  vec4 gmap = texture(GradientMap, vec2(unit_age, 0.0));
  vec4 cmap = texture(ColorMap, frg_uv.xy);
  out_clr.xyz = gmap.xyz*cmap.xyz;
  out_clr.w = gmap.w*cmap.w;
}
///////////////////////////////////////////////////////////////
technique tflatparticle_sprites_stereoCI {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_sprite_stereoCI;
    fragment_shader = ps_sprite_flat_stereo;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tgradparticle_sprites_stereo {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_sprite_stereoCI;
    fragment_shader = ps_sprite_grad_stereo;
    state_block     = sb_default;
  }
}
