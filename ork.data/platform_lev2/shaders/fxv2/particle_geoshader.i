///////////////////////////////////////////////////////////////
// geometry shader based techniques
///////////////////////////////////////////////////////////////
vertex_interface vface_streakGEO : uset_vtx {
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
geometry_interface gface_streakGEO //
  : gface_baseGEO { //

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
geometry_interface gface_baseGEO {
  inputs {
    layout(points);
  }
  outputs {
    layout(triangle_strip, max_vertices = 4);
  }
}
///////////////////////////////////////////////////////////////
geometry_interface gface_billGEO
    //: vface_bill // import vtxshader outputs
    : uset_vtx : gface_baseGEO {
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
geometry_interface gface_bill_stereoGEO // 
  : gface_baseGEO   //
  : vface_bill { // import vtxshader outputs
  
  // TODO recursive walk up inheritance tree for attribute out->in inheritance
  outputs {
    vec4 frg_clr;
    vec2 frg_uv0;
    vec2 frg_uv1;
    layout(secondary_view_offset = 1) int gl_Layer;
  }
}
///////////////////////////////////////////////////////////////

libblock lib_billboardGEO {
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
///////////////////////////////////////////////////////////////
libblock lib_streak_typesGEO {
  struct StreakInput {
    vec3 pos;
    vec3 vel;
    vec3 cnrm;
    vec2 lw;
  };
  struct StreakOutput {
    vec4 pos0;
    vec4 pos1;
    vec4 pos2;
    vec4 pos3;
  };
}
///////////////////////////////////////////////////////////////
libblock lib_streakGEO //
  : lib_streak_typesGEO {
  ///////////////////////////////////////////
  StreakOutput computeStreak(mat4 mvp) {
    StreakOutput outp;
    vec3 vel  = geo_vel[0].xyz;
    vec3 cnrm = geo_cnrm[0].xyz;
    float wid = geo_lw[0].y;
    float len = geo_lw[0].x;

    vec3 pos = gl_in[0].gl_Position.xyz;


    vec3 lpos = pos - (vel * len);

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
vertex_shader vs_streakGEO : vface_streakGEO {
  gl_Position = position;
  geo_cnrm = normal;
  geo_vel  = velocity;
  geo_lw   = lw;
  geo_ra = ra;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_vtxtexcolorGEO : vface_bill {
  gl_Position = position;
  geo_clr  = vtxcolor.rgba;
  geo_uv0  = uv0;
  geo_uv1  = uv1;
}
///////////////////////////////////////////////////////////////
vertex_shader vs_vtxcolorGEO : vface_bill {
  gl_Position = position;
  geo_clr  = vtxcolor;
  geo_uv0  = uv0;
  geo_uv1  = uv1;
}
///////////////////////////////////////////////////////////////
geometry_shader gs_identity : gface_bill {
  for (int n = 0; n < gl_in.length(); n++) {
    gl_Position = gl_in[n].gl_Position;
    frg_clr     = geo_clr[n];
    frg_uv0     = geo_uv0[n];
    frg_uv1     = geo_uv1[n];
    EmitVertex();
  }
  EndPrimitive();
}
///////////////////////////////////////////////////////////////
geometry_shader gs_billboardquadGEO //
    : lib_billboardGEO              //
    : gface_billGEO {               //
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
geometry_shader gs_billboardquad_stereoGEO      //
    : gface_bill_stereoGEO                      //
    : extension(GL_NV_stereo_view_rendering) //
    : extension(GL_NV_viewport_array2)       //
    : lib_billboardGEO {                        //
  gl_Layer                      = 0;
  gl_ViewportMask[0]            = 1;
  gl_SecondaryViewportMaskNV[0] = 2;
  Input inp;
  inp.inp_clr            = geo_clr[0];
  inp.inp_ang_siz        = geo_uv0;
  inp.inp_uv1            = geo_uv1;
  inp.inp_pos            = gl_in[0].gl_Position.xyz;
  Output outpL           = Compute(inp, MatMVPL);
  Output outpR           = Compute(inp, MatMVPR);
  gl_Position            = outpL.pos0;
  gl_SecondaryPositionNV = outpR.pos0;
  frg_uv0                = outpL.uv0;
  EmitVertex();
  gl_Position            = outpL.pos1;
  gl_SecondaryPositionNV = outpR.pos1;
  frg_uv0                = outpL.uv1;
  EmitVertex();
  gl_Position            = outpL.pos3;
  gl_SecondaryPositionNV = outpR.pos3;
  frg_uv0                = outpL.uv3;
  EmitVertex();
  gl_Position            = outpL.pos2;
  gl_SecondaryPositionNV = outpR.pos2;
  frg_uv0                = outpL.uv2;
  EmitVertex();
  EndPrimitive();
}
///////////////////////////////////////////////////////////////
geometry_shader gs_streakGEO //
  : gface_streakGEO //
  : lib_streakGEO
  : uset_vtx { //
  StreakOutput outp = computeStreak(MatMVP);
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
technique tbasicparticle_pick {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_vtxcolorGEO;
    geometry_shader = gs_billboardquadGEO;
    fragment_shader = ps_modtex;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tvolnoiseparticle {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_vtxcolorGEO;
    geometry_shader = gs_billboardquadGEO;
    fragment_shader = ps_modtex;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tvolumeparticle {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_vtxcolorGEO;
    geometry_shader = gs_billboardquadGEO;
    fragment_shader = ps_volume;
    state_block     = sb_alpadd;
  }
}
///////////////////////////////////////////////////////////////
technique tflatparticle_sprites {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_vtxtexcolorGEO;
    geometry_shader = gs_billboardquadGEO;
    fragment_shader = ps_flat;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tflatparticle_streaks {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streakGEO;
    geometry_shader = gs_streakGEO;
    fragment_shader = ps_flat;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tgradparticle_sprites {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_vtxtexcolorGEO;
    geometry_shader = gs_billboardquadGEO;
    fragment_shader = ps_flat;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tgradparticle_streaks {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak;
    geometry_shader = gs_streakGEO;
    fragment_shader = ps_grad;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique ttexparticle_sprites {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_vtxtexcolorGEO;
    geometry_shader = gs_billboardquadGEO;
    fragment_shader = ps_modtexclr;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique ttexparticle_streaks {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak;
    geometry_shader = gs_streakGEO;
    fragment_shader = ps_modtexclr;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tbasicparticle_stereo {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_vtxtexcolorGEO;
    geometry_shader = gs_billboardquad_stereoGEO;
    fragment_shader = ps_modtexclr;
    state_block     = sb_default;
  }
}
///////////////////////////////////////////////////////////////
technique tstreakparticle {
  fxconfig = fxcfg_default;
  pass p0 {
    vertex_shader   = vs_streak;
    geometry_shader = gs_streakGEO;
    fragment_shader = ps_modtexclr;
    state_block     = sb_default;
  }
}