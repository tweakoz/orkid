libblock lib_math {

  vec3 rcp(vec3 inp) {
    return vec3(1.0 / inp.x, 1.0 / inp.y, 1.0 / inp.z);
  }
  float saturateF(float inp) {
    return clamp(inp,0,1);
  }
  vec3 saturateV(vec3 inp) {
    return clamp(inp,0,1);
  }

  uint bitReverse(uint x) {
    x = ((x & 0x55555555u) << 1u) | ((x & 0xaaaaaaaau) >> 1u);
    x = ((x & 0x33333333u) << 2u) | ((x & 0xccccccccu) >> 2u);
    x = ((x & 0x0f0f0f0fu) << 4u) | ((x & 0xf0f0f0f0u) >> 4u);
    x = ((x & 0x00ff00ffu) << 8u) | ((x & 0xff00ff00u) >> 8u);
    x = ((x & 0x0000ffffu) << 16u) | ((x & 0xffff0000u) >> 16u);
    return x;
  }

  vec2 hammersley(uint index, uint sampleCount) {
    // return point from hammersly point set (on hemisphere)
    float u = float(index) / float(sampleCount);
    float v = float(bitReverse(index)) * 0.00000000023283064365386963;
    return vec2(u, v);
  }

  vec3 hemisphereSample_uniform(float u, float v) {
    float phi      = v * 2.0 * PI;
    float cosTheta = 1.0 - u;
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
  }

  vec3 hemisphereSample_cos(float u, float v) {
    float phi      = v * 2.0 * PI;
    float cosTheta = sqrt(1.0 - u);
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    return vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
  }

  vec3 sphericalToNormal(float phi, float theta) {
    float sintheta = sin(theta);
    float costheta = cos(theta);
    return vec3(sintheta * cos(phi), sintheta * sin(phi), costheta);
  }
  vec3 sphericalToNormal(float phi, float costheta, float sintheta) {
    return vec3(sintheta * cos(phi), sintheta * sin(phi), costheta);
  }
  vec2 normalToSpherical(vec3 n) {
    float phi = atan(n.y,n.x);
    float theta = acos(n.z);
    return vec2(phi,theta);
  }

vec3 linear2srgb(vec3 linearRGB)
{
    bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
    vec3 higher = vec3(1.055)*pow(linearRGB, vec3(1.0/2.4)) - vec3(0.055);
    vec3 lower = linearRGB * vec3(12.92);

    return mix(higher, lower, cutoff);
}

// Converts a color from sRGB gamma to linear light gamma
vec3 srgb2linear(vec3 sRGB)
{
    bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
    vec3 higher = pow((sRGB + vec3(0.055))/vec3(1.055), vec3(2.4));
    vec3 lower = sRGB/vec3(12.92);

    return mix(higher, lower, cutoff);
}
float satdot(vec3 a, vec3 b){
  return saturateF(dot(a,b));
}
}
