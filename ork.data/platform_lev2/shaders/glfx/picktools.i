///////////////////////////////////////////////////////////////
uniform_set ub_pick {
  vec4 ModColor;
  mat4 mvp;
  sampler2D InstanceMatrices;
}
///////////////////////////////////////////////////////////////
vertex_interface iface_vpick : ub_pick {
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
  }
  //
  outputs {
    vec4 frg_clr;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface iface_fpick : ub_pick {
  inputs {
    vec4 frg_clr;
  }
  outputs {
    layout(location = 0) vec4 out_clr;
    layout(location = 1) vec4 out_nrmd;
  }
}
///////////////////////////////////////////////////////////////
// picking
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_picking
	: iface_vpick {
		gl_Position = mvp*position;
    frg_clr = ModColor;
}
///////////////////////////////////////////////////////////////
// picking
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_picking_instanced
	: iface_vpick {
    int row = (gl_InstanceID&0xff)<<2;
		int col = (gl_InstanceID>>8);
		mat4 instancemtx = mat4(
			texelFetch(InstanceMatrices, ivec2(row+0,col), 0),
		  texelFetch(InstanceMatrices, ivec2(row+1,col), 0),
		  texelFetch(InstanceMatrices, ivec2(row+2,col), 0),
		  texelFetch(InstanceMatrices, ivec2(row+3,col), 0));
      vec4 instanced_pos = (instancemtx*position);
		gl_Position = mvp*instanced_pos;
    frg_clr = ModColor;
}
///////////////////////////////////////////////////////////////
fragment_shader fs_picking
	: iface_fpick {
		out_clr = vec4(1,1,0,1);//ModColor;
    out_nrmd = vec4(0,1,0,0);
}
///////////////////////////////////////////////////////////////
state_block sb_pick : default {
	DepthTest=OFF;
	DepthMask=true;
	CullTest=OFF;
	BlendMode = OFF;
}
///////////////////////////////////////////////////////////////
technique picking_rigid {
	fxconfig=fxcfg_default;
	pass p0 {
    vertex_shader=vs_rigid_picking;
		fragment_shader=fs_picking;
		state_block=sb_pick;
	}
}
///////////////////////////////////////////////////////////////
technique picking_rigid_instanced {
	fxconfig=fxcfg_default;
	pass p0 {
    vertex_shader=vs_rigid_picking_instanced;
		fragment_shader=fs_picking;
		state_block=sb_pick;
	}
}
///////////////////////////////////////////////////////////////
