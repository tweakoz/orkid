
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
      const float hfHeightScale = 1500;
      const float hfHeightBias = 0;
      // float invBaseGridTexelDim = (0.1*metersPerTexel / 4.0)+(1/129.0); // inverse of num texels of base lod
      float invBaseGridTexelDim = 0.013; // inverse of num texels of base lod
      /////////////////////////////////
      // testuv
      /////////////////////////////////
      vec4 surfaceColor = vec4(0, 0, .1, 1);
      /////////////////////////////////
      // mipcalc
      /////////////////////////////////
      float vtxlod = position.y;
      float mippedMetersPerTexel = metersPerTexel * exp2(vtxlod);
      float invMippedMetersPerTexel = 1.0f / mippedMetersPerTexel;
      /////////////////////////////////
      // compute snapped object/worldspace
      /////////////////////////////////
      vec2 o2w = quantize(campos.xz, mippedMetersPerTexel);
      float w_x = position.x * metersPerTexel + o2w.x;
      float w_z = position.z * metersPerTexel + o2w.y;
      vec3 wpos = vec3(w_x, 0, w_z);
      vec3 opos = (wpos - campos);
      /////////////////////////////////
      // uvgen
      /////////////////////////////////
      float size = max(0.5, max2(abs(opos.xz * 2.0 * invBaseGridTexelDim)));
      float llod = max(log2(size) - 0.75, 0.0);
      float lowMIP = floor(llod);
      float highMIP = lowMIP + 1.0;
      float htexoffsetHi = exp2(lowMIP);
      float htexoffsetLo = htexoffsetHi * 0.5;
      rval.uv_lowmip = (wpos.xz * texelsPerMeter + htexoffsetLo) * INVHFDIM;
      vec2 uvHiMip = (wpos.xz * texelsPerMeter + htexoffsetHi) * INVHFDIM;
      rval.uv_lowmip = (rval.uv_lowmip + vec2(1, 1)) * 0.5;
      uvHiMip = (uvHiMip + vec2(1, 1)) * 0.5;
      /////////////////////////////////
      // sample height
      /////////////////////////////////
      vec4 sampleLoMip = textureLod(ColorMap2, rval.uv_lowmip, lowMIP);
      float heightSampleLoMip = sampleLoMip.r;
      vec4 sampleHiMip = textureLod(ColorMap2, uvHiMip, highMIP);
      float heightSampleHiMip = sampleHiMip.r;
      float lerpIndex = llod - lowMIP;
      float h = heightSampleLoMip; //mix(heightSampleLoMip, heightSampleHiMip, lerpIndex);
      vec3 n = sampleLoMip.yzw; //mix(sampleLoMip.yzw, sampleHiMip.yzw, lerpIndex);
      /////////////////////////////////
      // final computation
      /////////////////////////////////
      wpos.y = (h * hfHeightScale) + hfHeightBias;

      rval.wpos = wpos;
      rval.wnrm = normalize(n);
      rval.wdepth = distance(wpos, campos);

      return rval;
    }


}
