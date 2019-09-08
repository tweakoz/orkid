
libblock lib_mmnoise {

//	<https://www.shadertoy.com/view/4dS3Wd>
//	By Morgan McGuire @morgan3d, http://graphicscodex.com
//
float hash(float n) { return fract(sin(n) * 1e4); }
float hash(vec2 p) { return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x)))); }

float noise(float x) {
	float i = floor(x);
	float f = fract(x);
	float u = f * f * (3.0 - 2.0 * f);
	return mix(hash(i), hash(i + 1.0), u);
}

float noise(vec2 x) {
	vec2 i = floor(x);
	vec2 f = fract(x);

	// Four corners in 2D of a tile
	float a = hash(i);
	float b = hash(i + vec2(1.0, 0.0));
	float c = hash(i + vec2(0.0, 1.0));
	float d = hash(i + vec2(1.0, 1.0));

	// Simple 2D lerp using smoothstep envelope between the values.
	// return vec3(mix(mix(a, b, smoothstep(0.0, 1.0, f.x)),
	//			mix(c, d, smoothstep(0.0, 1.0, f.x)),
	//			smoothstep(0.0, 1.0, f.y)));

	// Same code, with the clamps in smoothstep and common subexpressions
	// optimized away.
	vec2 u = f * f * (3.0 - 2.0 * f);
	return mix(a, b, u.x) + (c - a) * u.y * (1.0 - u.x) + (d - b) * u.x * u.y;
}

// This one has non-ideal tiling properties that I'm still tuning
float noise(vec3 x) {
	const vec3 step = vec3(110, 241, 171);

	vec3 i = floor(x);
	vec3 f = fract(x);

	// For performance, compute the base input to a 1D hash from the integer part of the argument and the
	// incremental change to the 1D based on the 3D -> 1D wrapping
    float n = dot(i, step);

	vec3 u = f * f * (3.0 - 2.0 * f);
	return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
                   mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
               mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
                   mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

float snoise( vec2 p ) {
	vec2 f = fract(p);
	p = floor(p);
	float v = p.x+p.y*1000.0;
	vec4 r = vec4(v, v+1.0, v+1000.0, v+1001.0);
	r = fract(100000.0*sin(r*.001));
	f = f*f*(3.0-2.0*f);
	return 2.0*(mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y))-1.0;
}

} // libblock lib_mmnoise {

libblock lib_terrain {

    struct TerOut {
      vec3 wpos;
      vec3 wpossh;
      vec3 wnrm;
      float wdepth;
      vec2 uv_lowmip;
    };

    vec2 quantize(vec2 inp, float quantum) {
        return round(inp*(1.0/quantum))*quantum;
    }
    float max2(vec2 inp){
        return (inp.x>inp.y) ? inp.x : inp.y;
    }

    TerOut computeTerrain(vec3 campos){
      TerOut rval;

      /////////////////////////////////
      // constants
      /////////////////////////////////
      const float metersPerTexel = 2;
      const float texelsPerMeter = 1.0 / metersPerTexel;
      const float HFDIM = 2048;
      const float INVHFDIM = 1.0 / HFDIM;
      const float UVDIM = 256.0;
      const float hfHeightScale = 1500;
      const float hfHeightBias = 0;
      /////////////////////////////////
      // mipcalc
      /////////////////////////////////
      float vtxlod = floor(position.y);
      float loddis = exp2(vtxlod);
      /////////////////////////////////
      // compute snapped object/worldspace
      /////////////////////////////////
      float d2c = max(abs(position.x),abs(position.z));
      vec2 o2w = campos.xz;
      vec2 o2wo = mod(campos.xz/UVDIM,1)*UVDIM;
      vec2 oq = campos.xz-o2wo;
      vec2 w_xz = position.xz * metersPerTexel + oq;
      vec3 w_xzq3 = vec3(w_xz.x, 0, w_xz.y);
      vec2 opos = (w_xz - o2w);
      /////////////////////////////////
      // uvgen
      /////////////////////////////////
      vec2 uv_xz = position.xz * metersPerTexel + oq.xy;
      vec2 uvd = uv_xz*0.5*texelsPerMeter*INVHFDIM;
      uvd += vec2(0.5,0.5);
      /////////////////////////////////
      float invBaseGridTexelDim = 0.013;
      float size = max(0.5, max2(abs(opos * 2.0 * invBaseGridTexelDim)));
      float llod = max(log2(size) - 0.75, 0.0);
      float lomip = floor(llod);
      float himip = lomip+1;
      float lerpindex = llod - lomip;
      /////////////////////////////////
      float h_hi = textureLod(ColorMap3,uvd,lomip).r;
      float h_lo = textureLod(ColorMap3,uvd,himip).r;
      float h = mix(h_hi,h_lo,lerpindex);
      h = (h * hfHeightScale) + hfHeightBias;
      /////////////////////////////////
      vec3 n_hi = textureLod(ColorMap3,uvd,lomip).yzw;
      vec3 n_lo = textureLod(ColorMap3,uvd,himip).yzw;
      vec3 n = normalize(mix(n_hi,n_lo,lerpindex));
      /////////////////////////////////
      vec3 p_hi = textureLod(ColorMap2,uvd,lomip).xyz;
      vec3 p_lo = textureLod(ColorMap2,uvd,himip).xyz;
      vec3 p = mix(p_hi,p_lo,lerpindex);
      /////////////////////////////////
      w_xzq3.y = h;
      /////////////////////////////////
      rval.wpos = w_xzq3;
      rval.wpossh = p; //vec3(w_xzq3.x,h,w_xzq3.z);
      //rval.wnrm = vec3(lerpindex);
      //rval.wnrm = vec3(mod(h*0.01,1),lerpindex,0);
      //rval.wnrm = vec3(uvd,0);
      rval.wnrm = n;
      rval.wdepth = distance(rval.wpos, vec3(0,0,0));
      rval.uv_lowmip = vec2(0,0);

      return rval;
    }


}
