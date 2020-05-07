///////////////////////////////////////////////////////////////
uniform_set ub_pick {
  vec4 ModColor;
  mat4 mvp;
  sampler2D InstanceMatrices;
  usampler2D InstanceIds;
}
///////////////////////////////////////////////////////////////
state_block sb_pick : default {
	DepthTest=OFF;
	DepthMask=true;
	CullTest=OFF;
	BlendMode = OFF;
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
    layout(location = 0) uvec4 out_clr;
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
fragment_shader fs_picking
	: iface_fpick {
		out_clr = uvec4(1,1,0,1);//ModColor;
    out_nrmd = vec4(0,1,0,0);
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
// instanced picking
///////////////////////////////////////////////////////////////
vertex_interface iface_vpick_instanced : ub_pick {
  inputs {
    vec4 position : POSITION;
    vec3 normal : NORMAL;
  }
  //
  outputs {
    vec4 frg_clr;
    vec4 frg_iid;
  }
}
///////////////////////////////////////////////////////////////
fragment_interface iface_fpick_instanced : ub_pick {
  inputs {
    vec4 frg_clr;
    vec4 frg_iid;
  }
  outputs {
    layout(location = 0) uvec4 out_clr;
    layout(location = 1) vec4 out_nrmd;
    //layout(location = 2) uvec4 out_iid;
  }
}
///////////////////////////////////////////////////////////////
vertex_shader vs_rigid_picking_instanced
	: iface_vpick_instanced {
    //////////////////////////////
    // we are assuming texture width is always 1024 atm..
    //////////////////////////////
    int mtx_row = (gl_InstanceID&0xff)<<2; // 1 matrix spans 4 RGBA32F texels
		int mtx_col = (gl_InstanceID>>8); // 256 matrices per 1K row
		mat4 instancemtx = mat4(
			texelFetch(InstanceMatrices, ivec2(mtx_row+0,mtx_col), 0),
		  texelFetch(InstanceMatrices, ivec2(mtx_row+1,mtx_col), 0),
		  texelFetch(InstanceMatrices, ivec2(mtx_row+2,mtx_col), 0),
		  texelFetch(InstanceMatrices, ivec2(mtx_row+3,mtx_col), 0));
      vec4 instanced_pos = (instancemtx*position);
    //////////////////////////////
    int iid_u = (gl_InstanceID&0x3ff); // 1 uint64 id per RGBA16UI texel
  	int iid_v = (gl_InstanceID>>10); // 1024 id's per 1K row
    uvec4 iid = texelFetch(InstanceIds,ivec2(iid_u,iid_v),0);
    //////////////////////////////
    gl_Position = mvp*instanced_pos;
    frg_clr = ModColor;
    frg_iid = vec4(iid);
}
//0x0001 0000 0000 0001
///////////////////////////////////////////////////////////////
fragment_shader fs_picking_instanced
	: iface_fpick_instanced {
		out_clr = uvec4(frg_iid); //uvec4(100,2000,3000,9000);//ModColor;
    out_nrmd = vec4(frg_iid);
}
///////////////////////////////////////////////////////////////
technique picking_rigid_instanced {
	fxconfig=fxcfg_default;
	pass p0 {
    vertex_shader=vs_rigid_picking_instanced;
		fragment_shader=fs_picking_instanced;
		state_block=sb_pick;
	}
}
///////////////////////////////////////////////////////////////
