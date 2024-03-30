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
    vec2 lw : TEXCOORD0; // size
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
vertex_shader vs_sprite : vface_sprite {
  gl_Position = position;
  geo_cnrm = normal;
  geo_vel  = velocity;
  geo_lw   = lw;
  geo_ra = ra;
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
    float size = geo_lw[0].y;

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
geometry_shader gs_sprite //
  : gface_sprite //
  : lib_sprite
  : uset_vtx { //
  PtcOutput outp = computeSprite(MatMVP);
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
    vertex_shader   = vs_vtxtexcolor;
    geometry_shader = gs_sprite;
    fragment_shader = ps_modtexclr;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
