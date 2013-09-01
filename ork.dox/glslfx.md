#Orkid GLSL FX Effect Format (Ork.Fx)

---

Orkid now has it's own effect file format. Currently this is only compatible with GLSL 1.5+ (OpenGL 3.2+)

Previously Nvidia's CgFx was used on GL targets. Due to CgFx's incompatibility with OpenGL 3.2 on Osx Lion+ I was forced to write a new effect format. Note that this decision was made far prior to the announcement of the existence of NvFx. Maybe in the future I will add support for NvFx, but for now - Say hi to "Ork.Fx".

An OpenGLes2 version will be coming soon - This is the last prerequisite for getting an iOS/Android build of Orkid up and running.

---

* Features
	 - import/include from other glfx files 
	 - familiar shader/technique/pass layout
	 - "state blocks" - reusable state vectors
	 - "library blocks" - reusable code libraries
	 - shader "interfaces" - uniform/attribute declarations with semantics
	 - data inheritance for stateblocks and interfaces 
	 

---

Example "skintools.i"

	//
	
	vertex_interface iface_skintools
	{
		uniform mat4 BoneMatrices[32];
	
		in vec4 boneindices : BONEINDICES;
		in vec4 boneweights : BONEWEIGHTS;
	}
	
	libblock skin_tools
	{
		vec3 SkinPosition( vec4 idcs, vec4 wghts, vec3 InPos )
		{
			ivec4 idcss = ivec4(idcs*255.0);
			vec4 Pos4 = vec4( InPos, 1.0 );
	
			vec3 WeightedVertex =	((BoneMatrices[idcss.w]*Pos4)*wghts.w).xyz;
			     WeightedVertex += 	((BoneMatrices[idcss.z]*Pos4)*wghts.z).xyz;
			     WeightedVertex +=	((BoneMatrices[idcss.y]*Pos4)*wghts.y).xyz;
			     WeightedVertex +=	((BoneMatrices[idcss.x]*Pos4)*wghts.x).xyz;
		 
			return WeightedVertex;
		}
		vec3 SkinNormal( vec4 idcs, vec4 wghts, vec3 InNrm )
		{
			ivec4 idcss = ivec4(idcs*255.0);
			vec4 Nrm4 = vec4( InNrm, 0.0f );
	
			vec3 WeightedNormal =  ((BoneMatrices[idcss.w]*Nrm4)*wghts.w).xyz;
			     WeightedNormal += ((BoneMatrices[idcss.z]*Nrm4)*wghts.z).xyz;
			     WeightedNormal += ((BoneMatrices[idcss.y]*Nrm4)*wghts.y).xyz;
			     WeightedNormal += ((BoneMatrices[idcss.x]*Nrm4)*wghts.x).xyz;
	
			return normalize(WeightedNormal);
		}
	
	}	 
	
example "basic.glfx"

	///////////////////////////////////////////////////////////////
	// FxConfigs
	///////////////////////////////////////////////////////////////
	fxconfig fxcfg_default
	{
		glsl_version = "150";
		import "skintools.i";
	}
	///////////////////////////////////////////////////////////////
	// Interfaces
	///////////////////////////////////////////////////////////////
	vertex_interface iface_vdefault
		: iface_skintools // inherit skinned unis/attrs
	{
		uniform mat4 WVPMatrix;
		uniform mat4 VPMatrix;
		uniform mat4 PMatrix;
		uniform mat4 WMatrix;
		uniform mat4 WVMatrix;
		uniform mat4 VMatrix;
		uniform mat4 IWMatrix;
		uniform mat3 WRotMatrix;
		uniform mat4 DiffuseMapMatrix;
		uniform mat4 NormalMapMatrix;
		uniform mat4 SpecularMapMatrix;
	
		uniform vec4 modcolor;
		uniform float time;
	
		uniform vec3 AmbientLight;
		uniform int NumDirectionalLights;
		uniform vec3 DirectionalLightDir[4];
		uniform vec3 DirectionalLightColor[4];
		uniform vec3 EmissiveColor;
	
		uniform vec3 WCamLoc;
		uniform float SpecularPower;
	
		in vec4 position : POSITION;
		in vec3 normal : NORMAL;
		in vec2 uv0 : TEXCOORD0;
	
		out vec4 frg_clr;
		out vec2 frg_uv0;
	}
	///////////////////////////////////////////////////////////////
	fragment_interface iface_fdefault
	{
		in vec4 frg_clr;
		in vec2 frg_uv0;
	
		out vec4 out_clr;
	}
	///////////////////////////////////////////////////////////////
	fragment_interface iface_fmt
	{
		uniform vec4 modcolor;
		uniform sampler2D DiffuseMap;
	
		in vec4 frg_clr;
		in vec2 frg_uv0;
	
		out vec4 out_clr;
	}
	///////////////////////////////////////////////////////////////
	// StateBlocks
	///////////////////////////////////////////////////////////////
	state_block sb_default
	{
		//BlendMode = ADDITIVE;
		DepthTest=LESS;
		DepthMask=true;
		CullTest=PASS_FRONT;
	}
	///////////////////////////////////////////////////////////////
	state_block sb_lerpblend : sb_default
	{
		BlendMode = ALPHA;
	}
	///////////////////////////////////////////////////////////////
	state_block sb_additive : sb_default
	{
		BlendMode = ADDITIVE;
	}
	///////////////////////////////////////////////////////////////
	// shaders
	///////////////////////////////////////////////////////////////
	vertex_shader vs_vtxtexcolor : iface_vdefault
	{
		gl_Position = WVPMatrix*position;
		frg_clr = vec4(1.0f,1.0f,1.0f,1.0f); //vtxcolor.bgra;
		frg_uv0 = uv0;
		//frg_uv1 = uv1;
	}
	
	///////////////////////////////////////////////////////////////
	vertex_shader vs_wnormal : iface_vdefault
	{
		gl_Position = WVPMatrix*position;
		vec3 wnorm = normalize(WRotMatrix*normal);
		frg_clr = vec4(wnorm.xyz,1.0);
		frg_uv0 = uv0;
	}
	
	///////////////////////////////////////////////////////////////
	vertex_shader vs_wnormalsk 
		: iface_vdefault
		: skin_tools
	{
		vec3 obj_pos = position.xyz;
		vec3 skn_pos = SkinPosition( boneindices, boneweights, position.xyz );
		gl_Position = WVPMatrix*vec4(skn_pos,1.0);
		vec3 sknorm = SkinNormal(boneindices, boneweights, normal.xyz);
		vec3 wnorm = normalize(WRotMatrix*sknorm);
		frg_clr = vec4(wnorm.xyz,1.0);
		frg_uv0 = uv0;
	}
	
	///////////////////////////////////////////////////////////////
	
	fragment_shader ps_modtex : iface_fmt
	{
		vec4 texc = texture( DiffuseMap, frg_uv0 );
		out_clr = texc*modcolor*frg_clr;
		if( out_clr.a==0.0f ) discard;
	}
	///////////////////////////////////////////////////////////////
	vertex_shader vs_vtxcolor : iface_vdefault
	{
		gl_Position = WVPMatrix*position;
		//frg_clr = vtxcolor;
		frg_clr = vec4(1.0f,0.0f,0.0f,1.0f); //vtxcolor.bgra;
		frg_uv0 = uv0;
		//frg_uv1 = uv0;
	}
	///////////////////////////////////////////////////////////////
	fragment_shader ps_fragclr : iface_fdefault
	{
		out_clr = frg_clr;
	}
	fragment_shader ps_wnormalsk : iface_fdefault
	{
		out_clr = vec4(0,1,0,1);//frg_clr;
	}
	
	///////////////////////////////////////////////////////////////
	fragment_shader ps_modclr : iface_fmt
	{
		out_clr = frg_clr;
		//out_clr = modcolor;
	}
	///////////////////////////////////////////////////////////////
	technique tek_modcolor
	{	fxconfig=fxcfg_default;
		pass p0
		{	vertex_shader=vs_vtxcolor;
			fragment_shader=ps_modclr;
			state_block=sb_default;
		}
	}
	technique tek_wnormal
	{	fxconfig=fxcfg_default;
		pass p0
		{	vertex_shader=vs_wnormal;
			fragment_shader=ps_fragclr;
			state_block=sb_default;
		}
	}
	technique tek_wnormal_skinned
	{	fxconfig=fxcfg_default;
		pass p0
		{	vertex_shader=vs_wnormalsk;
			fragment_shader=ps_fragclr;
			state_block=sb_default;
		}
	}
	///////////////////////////////////////////////////////////////

