
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
    return mix(mix(mix(hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
                   mix(hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x),
                   u.y),
               mix(mix(hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
                   mix(hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x),
                   u.y),
               u.z);
  }

  float snoise(vec2 p) {
    vec2 f  = fract(p);
    p       = floor(p);
    float v = p.x + p.y * 1000.0;
    vec4 r  = vec4(v, v + 1.0, v + 1000.0, v + 1001.0);
    r       = fract(100000.0 * sin(r * .001));
    f       = f * f * (3.0 - 2.0 * f);
    return 2.0 * (mix(mix(r.x, r.y, f.x), mix(r.z, r.w, f.x), f.y)) - 1.0;
  }

} // libblock lib_mmnoise {

libblock lib_terrain {

  struct TerOut {
    vec3 wpos;
    vec3 wpossh;
    vec3 wnrm;
    float wdepth;
    vec2 uv_lowmip;
    vec2 uvxplane;
    vec2 uvyplane;
    vec2 uvzplane;
  };

  vec2 quantize(vec2 inp, float quantum) { return round(inp * (1.0 / quantum)) * quantum; }
  float max2(vec2 inp) { return (inp.x > inp.y) ? inp.x : inp.y; }

  TerOut computeTerrain(vec3 campos) {
    TerOut rval;

    /////////////////////////////////
    // constants
    /////////////////////////////////
    const float metersPerTexel = 2;
    const float texelsPerMeter = 1.0 / metersPerTexel;
    const float HFDIM          = 2048;
    const float INVHFDIM       = 1.0 / HFDIM;
    const float UVDIM          = 256.0;
    const float hfHeightScale  = 1500;
    const float hfHeightBias   = 0;
    /////////////////////////////////
    // mipcalc
    /////////////////////////////////
    float vtxlod = floor(position.y);
    float loddis = exp2(vtxlod);
    /////////////////////////////////
    // compute snapped object/worldspace
    /////////////////////////////////
    float d2c   = max(abs(position.x), abs(position.z));
    vec2 o2w    = campos.xz;
    vec2 o2wo   = mod(campos.xz / UVDIM, 1) * UVDIM;
    vec2 oq     = campos.xz - o2wo;
    vec2 w_xz   = position.xz * metersPerTexel + oq;
    vec3 w_xzq3 = vec3(w_xz.x, 0, w_xz.y);
    vec2 opos   = (w_xz - o2w);
    /////////////////////////////////
    // uvgen
    /////////////////////////////////
    vec2 uv_xz = position.xz * metersPerTexel + oq.xy;
    vec2 uvd   = uv_xz * 0.5 * texelsPerMeter * INVHFDIM;
    uvd += vec2(0.5, 0.5);
    /////////////////////////////////
    float invBaseGridTexelDim = 1 / (32.0 * (8 - vtxlod));
    float size                = max(0.5, max2(abs(opos * 2.0 * invBaseGridTexelDim)));
    float llod                = max(log2(size) - 2.75, 0.0);
    float hires_mip           = floor(llod);
    float lores_mip           = hires_mip + 1;
    float lerpindex           = (llod - hires_mip);
    lerpindex                 = pow(lerpindex, 2);
    /////////////////////////////////
    vec4 hires_sample = textureLod(HFBMap, uvd, hires_mip);
    vec4 lores_sample = textureLod(HFBMap, uvd, lores_mip);
    /////////////////////////////////
    float h = mix(hires_sample.x, lores_sample.x, lerpindex);
    h       = (h * hfHeightScale) + hfHeightBias;
    /////////////////////////////////
    vec3 n = mix(hires_sample.yzw, lores_sample.yzw, lerpindex);
    /////////////////////////////////
    w_xzq3.y = h;
    /////////////////////////////////
    rval.wpos      = w_xzq3;
    rval.wpossh    = w_xzq3;
    rval.uvxplane  = w_xzq3.zy;
    rval.uvyplane  = w_xzq3.xz;
    rval.uvzplane  = w_xzq3.xy;
    rval.wnrm      = n;
    rval.wdepth    = distance(rval.wpos, vec3(0, 0, 0));
    rval.uv_lowmip = vec2(0, 0);

    return rval;
  }
  void applyTerrain(TerOut tero, vec3 campos) {
    frg_nrm     = tero.wnrm;
    frg_camdist = distance(tero.wpos, campos);
    frg_uvxp    = tero.uvxplane;
    frg_uvyp    = tero.uvyplane;
    frg_uvzp    = tero.uvzplane;
    frg_wpos    = tero.wpossh;
  }
}

libblock lib_terrain_frg {

  vec2 encode_nsphenv(vec3 raw) {
    vec2 n2ch = raw.xy * (sqrt(-raw.z * 0.5 + 0.5));
    return (n2ch * 0.5) + 0.5;
  }

  vec3 decode_nsphenv(vec2 encoded) {
    vec4 nn = vec4(encoded, 0, 0) * vec4(2, 2, 0, 0) + vec4(-1, -1, 1, -1);
    float l = dot(nn.xyz, -nn.xyw);
    nn.z    = l;
    nn.xy *= sqrt(l);
    return nn.xyz * 2 + vec3(0, 0, -1);
  }
  vec3 albedo(vec3 n) {
    float ddd1 = clamp(1.0 - frg_camdist * 0.0012, 0, 1);
    float ddd2 = clamp(1.0 - frg_camdist * 0.002, 0, 1);
    float ddd3 = clamp(1.0 - frg_camdist * 0.003, 0, 1);
    vec3 tpw   = abs(n);
    tpw /= (tpw.x + tpw.y + tpw.z);

    const vec3 grass = vec3(0.5, 0.5, 0.7);
    const vec3 snow  = vec3(1, 1, 1);
    const vec3 rock1 = vec3(0.6, .6, 0.8);
    const vec3 rock2 = vec3(0.6, .6, 0.7);

    float gsblend = clamp(GBlendYBias + frg_wpos.y * GBlendYScale, 0, 1);
    gsblend       = smoothstep(GBlendStepLo, GBlendStepHi, gsblend);
    vec3 hor      = mix(GrassColor, SnowColor, gsblend);

    vec3 c = hor * pow(tpw.y, testxxx) + Rock1Color * pow(tpw.x, testxxx) + Rock2Color * pow(tpw.z, testxxx);
    return c;
  }
}

libblock lib_pointlight {

  vec3 pointlight(vec3 worldpos, vec3 worldnormal, vec3 lightpos, float lightradius, float cutoff, vec3 color) {
    vec3 postolight = lightpos - worldpos;
    float dis2light = length(postolight);
    vec3 dir2light  = normalize(postolight);
    float clampdist = max(dis2light - lightradius, 0);
    postolight /= dis2light;
    float denom = clampdist / lightradius + 1.0;
    float atten = 1.0 / (denom * denom);
    atten       = (atten - cutoff) / (1.0 - cutoff);
    atten       = max(atten, 0.0);
    atten *= max(dot(worldnormal, dir2light), 0);
    return color * atten;
  }
}
