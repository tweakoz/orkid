//

uniform_set ublock_skinned { mat4 BoneMatrices[32]; }

vertex_interface iface_skintools : ublock_skinned {
  inputs {
    vec4 boneindices : BONEINDICES;
    vec4 boneweights : BONEWEIGHTS;
  }
}

libblock skin_tools {
  vec3 SkinPosition(vec4 idcs, vec4 wghts, vec3 objpos) {
    ivec4 idcss = ivec4(idcs * 255.0);
    vec4 Pos4   = vec4(objpos, 1.0);

    vec3 WeightedVertex = ((BoneMatrices[idcss.w] * Pos4) * wghts.w).xyz;
    WeightedVertex += ((BoneMatrices[idcss.z] * Pos4) * wghts.z).xyz;
    WeightedVertex += ((BoneMatrices[idcss.y] * Pos4) * wghts.y).xyz;
    WeightedVertex += ((BoneMatrices[idcss.x] * Pos4) * wghts.x).xyz;

    return WeightedVertex;
  }
  vec3 SkinNormal(vec4 idcs, vec4 wghts, vec3 InNrm) {
    ivec4 idcss = ivec4(idcs * 255.0);
    vec4 Nrm4   = vec4(InNrm, 0.0f);

    vec3 WeightedNormal = ((BoneMatrices[idcss.w] * Nrm4) * wghts.w).xyz;
    WeightedNormal += ((BoneMatrices[idcss.z] * Nrm4) * wghts.z).xyz;
    WeightedNormal += ((BoneMatrices[idcss.y] * Nrm4) * wghts.y).xyz;
    WeightedNormal += ((BoneMatrices[idcss.x] * Nrm4) * wghts.x).xyz;

    return normalize(WeightedNormal);
  }

  struct SkinOut {
    vec3 skn_pos;
    vec3 skn_col;
  };

  SkinOut LitSkinned(vec4 idcs, vec4 wghts, vec3 objpos) {
		SkinOut rval;
		rval.skn_pos = SkinPosition(boneindices, boneweights, position.xyz);
	  vec3 sknorm  = SkinNormal(boneindices, boneweights, normal.xyz);
	  vec3 wnorm   = normalize(WRotMatrix * sknorm);
	  float dif = dot(wnorm, vec3(0, 0, 1));
	  float amb = 0.3;
	  float tot = dif + amb;
		rval.skn_col = vec3(tot,tot,tot);
		return rval;
	}
}
