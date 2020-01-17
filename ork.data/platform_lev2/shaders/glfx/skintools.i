//

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
