# Orkid GLSL FX Effect Format (Ork.Fx)

---

### Summary

Orkid now has it's own effect file format. Currently this is only compatible with GLSL 1.5+ (OpenGL 3.2+)

Previously Nvidia's CgFx was used on GL targets. Due to CgFx's incompatibility with OpenGL 3.2 on Osx Lion+ I was forced to write a new effect format. Note that this decision was made far prior to the announcement of the existence of NvFx. Maybe in the future I will add support for NvFx, but for now - Say hi to "Ork.Fx".

An OpenGLes2 version will be coming soon - This is the last prerequisite for getting an iOS/Android build of Orkid up and running.

---

### Features

  - import/include from other glfx files 
  - familiar shader/technique/pass layout
  - "state blocks" - reusable state vectors
  - "library blocks" - reusable code libraries
  - shader "interfaces" - uniform/attribute declarations with semantics
  - data inheritance for stateblocks and interfaces
  - supports Vertex, Tessellation, Geometry and Fragment shaders

---

### Example "skintools.i"

```glsl
uniform_set ublock_skinned { mat4 BoneMatrices[32]; }

vertex_interface iface_skintools : ublock_skinned {
  inputs { 
    vec4 boneindices : BONEINDICES;
    vec4 boneweights : BONEWEIGHTS;
  }
}

libblock skin_tools {
  vec3 SkinPosition(vec3 objpos) {
    //ivec4 idcsi = ivec4(idcs);
    ivec4 idcsi = ivec4(boneindices);
    //wghts = vec4(0.25,0.25,0.25,0.25);
    vec4 Pos4   = vec4(objpos, 1.0);

    vec3 WeightedVertex = ((BoneMatrices[idcsi.w] * Pos4).xyz * boneweights.w);
    WeightedVertex += ((BoneMatrices[idcsi.z] * Pos4).xyz * boneweights.z);
    WeightedVertex += ((BoneMatrices[idcsi.y] * Pos4).xyz * boneweights.y);
    WeightedVertex += ((BoneMatrices[idcsi.x] * Pos4).xyz * boneweights.x);

    return WeightedVertex;
 }
  vec3 SkinNormal(vec3 InNrm) {
    ivec4 idcss = ivec4(boneindices);
    vec4 Nrm4   = vec4(InNrm, 0.0f);

    vec3 WeightedNormal = ((BoneMatrices[idcss.w] * Nrm4) * boneweights.w).xyz;
    WeightedNormal += ((BoneMatrices[idcss.z] * Nrm4) * boneweights.z).xyz;
    WeightedNormal += ((BoneMatrices[idcss.y] * Nrm4) * boneweights.y).xyz;
    WeightedNormal += ((BoneMatrices[idcss.x] * Nrm4) * boneweights.x).xyz;

    return normalize(WeightedNormal);
  }

  struct SkinOut {
    vec3 skn_pos;
    vec3 skn_col;
  };
 
  SkinOut LitSkinned(vec3 objpos) {
    SkinOut rval;
    rval.skn_pos = SkinPosition(position.xyz);
    vec3 sknorm  = SkinNormal(normal.xyz);
    vec3 wnorm   = normalize(mrot * sknorm);
    float dif = dot(wnorm, vec3(0, 0, 1));
    float amb = 0.3;
    float tot = dif + amb;
    rval.skn_col = vec3(tot,tot,tot);
    return rval;
  }
}
```

example "pbr.glfx"

```glsl
	///////////////////////////////////////////////////////////////
	// FxConfigs
	///////////////////////////////////////////////////////////////
	fxconfig fxcfg_default
	{
		glsl_version = "130";
		import "skintools.i";
	}
	///////////////////////////////////////////////////////////////
	// Interfaces
	///////////////////////////////////////////////////////////////
	uniform_set ub_vtx
	{
		mat4        mv;
		mat4        mvp;
		mat4 		mvp_l;
		mat4 		mvp_r;
		mat3        mrot;
		vec4        modcolor;
		vec2 InvViewportSize; // inverse target size
	}
	///////////////////////////////////////////////////////////////
	uniform_set ub_frg
	{
    	sampler2D ColorMap;
    	sampler2D NormalMap;
    	sampler2D MtlRufMap;
		vec4 ModColor;
		vec2 InvViewportSize; // inverse target size
		float MetallicFactor;
		float RoughnessFactor;
	}
	///////////////////////////////////////////////////////////////
	// StateBlocks
	///////////////////////////////////////////////////////////////
	state_block sb_default : default
	{
	}
	///////////////////////////////////////////////////////////////
	// shaders
	///////////////////////////////////////////////////////////////
	vertex_interface iface_vgbuffer
		: ub_vtx
	{
    	inputs {
			vec4 position : POSITION;
	        vec3 normal : NORMAL;
	        vec3 binormal : BINORMAL;
	        vec4 vtxcolor : COLOR0;
			vec2 uv0 : TEXCOORD0;
		}
		outputs {
        	vec4 frg_clr;
	    	vec2 frg_uv0;
	    	mat3 frg_tbn;
			float frg_camdist;
			vec3 frg_camz;
		}
	}
	vertex_shader vs_rigid_gbuffer
		: iface_vgbuffer
	{
    	vec4 cpos  = mv * position;
    	vec3 wnorm = normalize(mrot * normal);
    	vec3 wbinorm = normalize(mrot * binormal);
		vec3 wtangent = cross(wbinorm,wnorm);
		gl_Position = mvp*position;
		frg_clr = vec4(1.0,1.0,1.0,1.0);
		frg_uv0 = uv0*vec2(1,-1);
		vec4 nrmd = vec4(wnorm,-cpos.z);
		frg_camdist = nrmd.w;
		frg_tbn = transpose(mat3(
        	-wtangent,
        	-wbinorm,
        	wnorm
    	));
		frg_camz = wnorm.xyz;
	}
	///////////////////////////////////////////////////////////////
	vertex_interface iface_vgbuffer_skinned
		: iface_vgbuffer
		: iface_skintools
	{
	}
	vertex_shader vs_skinned_gbuffer
		: iface_vgbuffer_skinned : skin_tools
	{
		vec3 obj_pos = position.xyz;
  		vec3 skn_pos = SkinPosition(position.xyz);
		vec3 skn_nrm  = SkinNormal(normalize(normal));
		vec3 skn_bin  = SkinNormal(normalize(binormal));

   		vec4 cpos  = mv * vec4(skn_pos,1);
   		vec3 wnorm = mrot * skn_nrm;
   		vec3 wbinorm = mrot * skn_bin;
		vec3 wtangent = cross(wbinorm,wnorm);
		gl_Position = mvp*vec4(skn_pos,1);
		frg_clr = boneindices*1.0/32.0f;
		frg_uv0 = uv0*vec2(1,-1);
		frg_camdist = -cpos.z;
		frg_tbn = transpose(mat3(
        		-wtangent,
        		-wbinorm,
        		wnorm
    	));
		frg_camz = wnorm.xyz;
	}
	///////////////////////////////////////////////////////////////
	vertex_interface iface_vgbuffer_stereo : iface_vgbuffer {
  		outputs {
    		layout(secondary_view_offset=1) int gl_Layer;
  		}
	}
	///////////////////////////////////////////////////////////////
	vertex_shader vs_rigid_gbuffer_stereo
		: extension(GL_NV_stereo_view_rendering)
  		: extension(GL_NV_viewport_array2)
		: iface_vgbuffer_stereo
	{
    	vec4 cpos  = mv * position;
		vec3 wnorm = normalize(mrot * normal);
    	vec3 wbinorm = normalize(mrot * binormal);
		vec3 wtangent = cross(wbinorm,wnorm);
		frg_clr = vec4(1.0,1.0,1.0,1.0);
		frg_uv0 = uv0*vec2(1,-1);
		vec4 nrmd = vec4(wnorm,-cpos.z);
		frg_camdist = nrmd.w;
		frg_tbn = transpose(mat3(
	        	-wtangent,
	        	-wbinorm,
	        	wnorm
	    	));
		frg_camz = wnorm.xyz;
		gl_Position = mvp_l*position;
		gl_SecondaryPositionNV = mvp_r*position;
  		gl_Layer = 0;
		gl_ViewportMask[0] = 1;
  		gl_SecondaryViewportMaskNV[0] = 2;
	}
	///////////////////////////////////////////////////////////////
	fragment_interface iface_fgbuffer
		: ub_frg
	{
    		inputs {
        		vec4 frg_clr;
	    		vec2 frg_uv0;
	    		mat3 frg_tbn;
				float frg_camdist;
				vec3 frg_camz;
		}
		outputs {
        		layout(location = 0) vec4 out_clr;
        		layout(location = 1) vec4 out_normal_mdl;
        		layout(location = 2) vec4 out_rufmtl;
		}
	}
	fragment_shader ps_gbuffer
		: iface_fgbuffer
	{
		vec3 nrm_tanspace = vec3(0,0,1);
		vec3 nrm_w = nrm_tanspace*frg_tbn;
		vec3 rufmtlamb = texture(MtlRufMap,frg_uv0).zyx;
		rufmtlamb.x *= MetallicFactor;
		rufmtlamb.y *= RoughnessFactor;
		out_clr = texture(ColorMap,frg_uv0).xyzw;
		out_normal_mdl.xyz = (nrm_w*0.5)+vec3(0.5,0.5,0.5);
		out_normal_mdl.w = frg_camdist;
		out_rufmtl = vec4(rufmtlamb,0);
	}
	fragment_shader ps_gbuffer_n // normalmap
		: iface_fgbuffer
	{
    	vec3 nrm_tanspace = texture(NormalMap,frg_uv0).xyz*2.0-vec3(1,1,1);
		nrm_tanspace = normalize(mix(vec3(0,0,1),nrm_tanspace,1.0));
		vec3 nrm_w = nrm_tanspace*frg_tbn;
		vec3 rufmtlamb = texture(MtlRufMap,frg_uv0).zyx;
		rufmtlamb.x *= MetallicFactor;
		rufmtlamb.y *= RoughnessFactor;
    	out_clr = ModColor*texture(ColorMap,frg_uv0).xyzw;
		//out_clr = frg_clr;
    	out_normal_mdl.xyz = (nrm_w*0.5)+vec3(0.5,0.5,0.5);
		out_normal_mdl.w = frg_camdist;
		out_rufmtl = vec4(rufmtlamb,0);
	}
	///////////////////////////////////////////////////////////////
	fragment_shader ps_gbuffer_n_stereo // normalmap
		: iface_fgbuffer
	{
    	vec3 nrm_tanspace = texture(NormalMap,frg_uv0).xyz*2.0-vec3(1,1,1);
		nrm_tanspace = normalize(mix(vec3(0,0,1),nrm_tanspace,1.0));
		vec3 nrm_w = nrm_tanspace*frg_tbn;
    	vec3 rufmtlamb = texture(MtlRufMap,frg_uv0).zyx;
		rufmtlamb.x *= MetallicFactor;
		rufmtlamb.y *= RoughnessFactor;
    	out_clr = texture(ColorMap,frg_uv0).xyzw;
		out_normal_mdl.xyz = (nrm_w*0.5)+vec3(0.5,0.5,0.5);
		out_normal_mdl.w = frg_camdist;
		//out_rufmtl = vec4(rufmtlamb,0);
		out_rufmtl = vec4(rufmtlamb,0);
	}
	///////////////////////////////////////////////////////////////

	fragment_shader ps_gbuffer_n_tex_stereo // normalmap (stereo texture - vsplit)
		: iface_fgbuffer
	{
		vec2 screen_uv   = gl_FragCoord.xy * InvViewportSize;
		bool is_right = bool(screen_uv.x <= 0.5);

		vec2 map_uv = frg_uv0*vec2(1,0.5);
		if( is_right )
			map_uv += vec2(0,0.5);

    	vec3 nrm_tanspace = texture(NormalMap,map_uv).xyz*2.0-vec3(1,1,1);
		vec3 nrm_w = nrm_tanspace*frg_tbn;
    	vec3 rufmtlamb = texture(MtlRufMap,map_uv).zyx;
		rufmtlamb.x *= MetallicFactor;
		rufmtlamb.y *= RoughnessFactor;
    	out_clr = texture(ColorMap,map_uv).xyzw;
    	out_normal_mdl.xyz = (nrm_w*0.5)+vec3(0.5,0.5,0.5);
		out_normal_mdl.w = frg_camdist;
		out_rufmtl = vec4(rufmtlamb,0);
	}
	///////////////////////////////////////////////////////////////
	state_block sb_pick : sb_default
	{
		DepthTest=OFF;
		DepthMask=true;
		CullTest=OFF;
		BlendMode = OFF;
	}
	technique rigid_gbuffer
	{
		fxconfig=fxcfg_default;
		pass p0
		{
			vertex_shader=vs_rigid_gbuffer;
			fragment_shader=ps_gbuffer;
			state_block=sb_default;
		}
	}
	technique rigid_gbuffer_n
	{
		fxconfig=fxcfg_default;
		pass p0
		{	vertex_shader=vs_rigid_gbuffer;
			fragment_shader=ps_gbuffer_n;
			state_block=sb_default;
		}
	}
	technique skinned_gbuffer_n
	{
		fxconfig=fxcfg_default;
		pass p0
		{	vertex_shader=vs_skinned_gbuffer;
			fragment_shader=ps_gbuffer_n;
			state_block=sb_default;
		}
	}
	technique rigid_gbuffer_n_stereo
	{
		fxconfig=fxcfg_default;
		pass p0
		{	vertex_shader=vs_rigid_gbuffer_stereo;
			fragment_shader=ps_gbuffer_n_stereo;
			state_block=sb_default;
		}
	}
	technique rigid_gbuffer_n_tex_stereo
	{
		fxconfig=fxcfg_default;
		pass p0
		{	vertex_shader=vs_rigid_gbuffer_stereo;
			fragment_shader=ps_gbuffer_n_tex_stereo;
			state_block=sb_default;
		}
	}
```


