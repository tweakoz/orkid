
libblock lib_mmnoise {

  float octavenoise(sampler3D krntex,
                    vec3 pos,
                    vec3 d, 
                    float time, 
                    int numoct){
    float val = 0;
    float freq = 1.0;
    float amp = 0.25;
    for( int i=0; i<numoct; i++ ){
      vec3 uvw = pos*freq;
      uvw += d*(time*0.1/freq);
      val += texture(krntex,uvw).x*amp;
      freq *= 0.7;
      amp *= 0.8;
      time *= 0.5;
    }
    return val;
  }


  //	<https://www.shadertoy.com/view/4dS3Wd>
  //	By Morgan McGuire @morgan3d, http://graphicscodex.com
  //
  float hash(float n) {
    return fract(sin(n) * 1e4);
  }
  float hash(vec2 p) {
    return fract(1e4 * sin(17.0 * p.x + p.y * 0.1) * (0.1 + abs(sin(p.y * 13.0 + p.x))));
  }

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
    return mix(
        mix(mix(hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
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

///////////////////////////////////////////////////////////////////////////////
// cellular noise
///////////////////////////////////////////////////////////////////////////////

libblock lib_cellnoise {
  // from https://github.com/BrianSharpe/Wombat/blob/master/Cellular3D.glsl
  float cellnoise(vec3 P) {
    //	establish our grid cell and unit position
    vec3 Pi = floor(P);
    vec3 Pf = P - Pi;

    // clamp the domain
    Pi.xyz       = Pi.xyz - floor(Pi.xyz * (1.0 / 69.0)) * 69.0;
    vec3 Pi_inc1 = step(Pi, vec3(69.0 - 1.5)) * (Pi + 1.0);

    // calculate the hash ( over -1.0->1.0 range )
    vec4 Pt = vec4(Pi.xy, Pi_inc1.xy) + vec2(50.0, 161.0).xyxy;
    Pt *= Pt;
    Pt                         = Pt.xzxz * Pt.yyww;
    const vec3 SOMELARGEFLOATS = vec3(635.298681, 682.357502, 668.926525);
    const vec3 ZINC            = vec3(48.500388, 65.294118, 63.934599);
    vec3 lowz_mod              = vec3(1.0 / (SOMELARGEFLOATS + Pi.zzz * ZINC));
    vec3 highz_mod             = vec3(1.0 / (SOMELARGEFLOATS + Pi_inc1.zzz * ZINC));
    vec4 hash_x0               = fract(Pt * lowz_mod.xxxx) * 2.0 - 1.0;
    vec4 hash_x1               = fract(Pt * highz_mod.xxxx) * 2.0 - 1.0;
    vec4 hash_y0               = fract(Pt * lowz_mod.yyyy) * 2.0 - 1.0;
    vec4 hash_y1               = fract(Pt * highz_mod.yyyy) * 2.0 - 1.0;
    vec4 hash_z0               = fract(Pt * lowz_mod.zzzz) * 2.0 - 1.0;
    vec4 hash_z1               = fract(Pt * highz_mod.zzzz) * 2.0 - 1.0;

    //  generate the 8 point positions
    const float JITTER_WINDOW = 0.166666666; // 0.166666666 will guarentee no artifacts.
    hash_x0                   = ((hash_x0 * hash_x0 * hash_x0) - sign(hash_x0)) * JITTER_WINDOW + vec4(0.0, 1.0, 0.0, 1.0);
    hash_y0                   = ((hash_y0 * hash_y0 * hash_y0) - sign(hash_y0)) * JITTER_WINDOW + vec4(0.0, 0.0, 1.0, 1.0);
    hash_x1                   = ((hash_x1 * hash_x1 * hash_x1) - sign(hash_x1)) * JITTER_WINDOW + vec4(0.0, 1.0, 0.0, 1.0);
    hash_y1                   = ((hash_y1 * hash_y1 * hash_y1) - sign(hash_y1)) * JITTER_WINDOW + vec4(0.0, 0.0, 1.0, 1.0);
    hash_z0                   = ((hash_z0 * hash_z0 * hash_z0) - sign(hash_z0)) * JITTER_WINDOW + vec4(0.0, 0.0, 0.0, 0.0);
    hash_z1                   = ((hash_z1 * hash_z1 * hash_z1) - sign(hash_z1)) * JITTER_WINDOW + vec4(1.0, 1.0, 1.0, 1.0);

    //	return the closest squared distance
    vec4 dx1 = Pf.xxxx - hash_x0;
    vec4 dy1 = Pf.yyyy - hash_y0;
    vec4 dz1 = Pf.zzzz - hash_z0;
    vec4 dx2 = Pf.xxxx - hash_x1;
    vec4 dy2 = Pf.yyyy - hash_y1;
    vec4 dz2 = Pf.zzzz - hash_z1;
    vec4 d1  = dx1 * dx1 + dy1 * dy1 + dz1 * dz1;
    vec4 d2  = dx2 * dx2 + dy2 * dy2 + dz2 * dz2;
    d1       = min(d1, d2);
    d1.xy    = min(d1.xy, d1.wz);
    return min(d1.x, d1.y) * (9.0 / 12.0); // return a value scaled to 0.0->1.0
  }
}

libblock lib_pnoise {

// Dummy implementations for perlinNoise and perlinGradient functions.
// You'll need to replace these with actual implementations.
float perlinNoise(vec2 p) {
    // Placeholder for actual Perlin noise computation
    return 0.0; // Replace with actual noise calculation
}

vec2 perlinGradient(vec2 p) {
    // Placeholder for gradient computation of Perlin noise at point p
    return vec2(0.0); // Replace with actual gradient calculation
}

vec4 perlinNoiseWithNormal(vec2 p) {
    // Placeholder for Perlin noise function and gradient calculation
    float noiseValue = perlinNoise(p);
    vec2 grad = perlinGradient(p);

    // Construct the normal. In 2D, the normal's x and y components are derived from the gradient,
    // and the z component is set to 1. Then, normalize the vector.
    // Since we're working in 2D and returning a 3D normal, assume a "flat" z-component for the gradient.
    vec3 normal = normalize(vec3(-grad, 1.0));

    // Combine the noise value and the normal into a vec4
    return vec4(noiseValue, normal);
}


}
