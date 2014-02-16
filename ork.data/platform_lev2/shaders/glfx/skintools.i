//

uniform_block ublock_skinned
{
	uniform mat4 BoneMatrices[32];
}

vertex_interface iface_skintools
	: ublock_skinned
{
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