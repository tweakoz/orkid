import "orkshader://stdtools.i";

libblock lib_math        //
  : uset_std_filtering { //

  float rand(vec2 co) {
    return fract(sin(dot(co.xy, vec2(12.9898, 78.233))) * 43758.5453);
  }
  vec3 rcp(vec3 inp) {
    return vec3(1.0 / inp.x, 1.0 / inp.y, 1.0 / inp.z);
  }
  float saturateF(float inp) {
    return clamp(inp, 0, 1);
  }
  vec3 saturateV(vec3 inp) {
    return clamp(inp, 0, 1);
  }
  vec4 saturateV4(vec4 inp) {
    return clamp(inp, 0, 1);
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
    float phi   = atan(n.y, n.x);
    float theta = acos(n.z);
    return vec2(phi, theta);
  }

  vec3 linear2srgb(vec3 linearRGB) {
    bvec3 cutoff = lessThan(linearRGB, vec3(0.0031308));
    vec3 higher  = vec3(1.055) * pow(linearRGB, vec3(1.0 / 2.4)) - vec3(0.055);
    vec3 lower   = linearRGB * vec3(12.92);

    return mix(higher, lower, cutoff);
  }

  // Converts a color from sRGB gamma to linear light gamma
  vec3 srgb2linear(vec3 sRGB) {
    bvec3 cutoff = lessThan(sRGB, vec3(0.04045));
    vec3 higher  = pow((sRGB + vec3(0.055)) / vec3(1.055), vec3(2.4));
    vec3 lower   = sRGB / vec3(12.92);

    return mix(higher, lower, cutoff);
  }
  float satdot(vec3 a, vec3 b) {
    return saturateF(dot(a, b));
  }

  float pointPlaneDistance(vec4 plane, vec3 point) {
    return dot(plane.xyz, point) - plane.w;
  }

  vec3 rgb_to_hsv(vec3 rgb) {
    vec4 K  = vec4(0.0, -1.0 / 3.0, 2.0 / 3.0, -1.0);
    vec4 p  = mix(vec4(rgb.bg, K.wz), vec4(rgb.gb, K.xy), step(rgb.b, rgb.g));
    vec4 q  = mix(vec4(p.xyw, rgb.r), vec4(rgb.r, p.yzx), step(p.x, rgb.r));
    float d = q.x - min(q.w, q.y);
    float e = 1.0e-10;
    return vec3(abs(q.z + (q.w - q.y) / (6.0 * d + e)), d / (q.x + e), q.x);
  }

  vec3 hsv_to_rgb(vec3 hsv) {
    vec3 rgb;
    if (hsv.y == 0.0) {
      // Grayscale
      rgb.x = hsv.z;
      rgb.y = hsv.z;
      rgb.z = hsv.z;
    } else {
      if (1.0 <= hsv.x)
        hsv.x -= 1.0;
      hsv.x *= 6.0;
      float i  = float(floor(hsv.x));
      float f  = hsv.x - i;
      float aa = hsv.z * (1.0 - hsv.y);
      float bb = hsv.z * (1.0 - (hsv.y * f));
      float cc = hsv.z * (1.0 - (hsv.y * (1.0 - f)));
      if (i < 1.0) {
        rgb.x = hsv.z;
        rgb.y = (cc);
        rgb.z = (aa);
      } else if (i < 2.0) {
        rgb.x = (bb);
        rgb.y = hsv.z;
        rgb.z = (aa);
      } else if (i < 3.0) {
        rgb.x = (aa);
        rgb.y = hsv.z;
        rgb.z = (cc);
      } else if (i < 4.0) {
        rgb.x = (aa);
        rgb.y = (bb);
        rgb.z = hsv.z;
      } else if (i < 5.0) {
        rgb.x = (cc);
        rgb.y = (aa);
        rgb.z = hsv.z;
      } else {
        rgb.x = hsv.z;
        rgb.y = (aa);
        rgb.z = (bb);
      }
    }
    return rgb;
  }

  float bicubicWeight(float t) {
    t = abs(t);
    if (t < 1.0) {
      return (4.0 - 6.0 * t * t + 3.0 * t * t * t) / 6.0;
    } else if (t < 2.0) {
      float s = 2.0 - t;
      return (s * s * s) / 6.0;
    }
    return 0.0;
  }
  // Bicubic texture sample
  vec4 textureBicubic(sampler2D tex, vec2 uv) {
    vec2 texSize    = textureSize(tex, 0);
    vec2 invTexSize = 1.0 / texSize;

    vec2 coord  = uv * texSize - 0.5;
    vec2 fcoord = fract(coord);
    coord -= fcoord;

    vec4 result = vec4(0.0);

    for (int y = -1; y <= 2; y++) {
      for (int x = -1; x <= 2; x++) {
        vec2 sampleUV = (coord + vec2(x, y) + 0.5) * invTexSize;
        sampleUV      = clamp(sampleUV, 0.0, 1.0);

        float weightX = bicubicWeight(float(x) - fcoord.x);
        float weightY = bicubicWeight(float(y) - fcoord.y);
        float weight  = weightX * weightY;

        result += textureLod(tex, sampleUV, 0) * weight;
      }
    }

    return result;
  }

  float lanczosWeight(float x) {
    if (x == 0.0)
      return 1.0;
    if (x >= 1.0)
      return 0.0;
    float pix = PI * x;
    return sin(pix) * sin(pix * FilterRadius) / (pix * pix * FilterRadius);
  }

  vec4 textureLancsozEWA(sampler2D tex, vec2 uv ) {

    vec2 srcSize    = vec2(textureSize(tex, 0));
    vec2 invSrcSize = 1.0 / srcSize;

    // Calculate pixel centers in source texture space
    vec2 srcPos = uv * srcSize - 0.5;

    // Compute Jacobian matrix for the inverse mapping
    // This determines the elliptical filter shape
    vec2 scale    = srcSize * InvViewportSize;
    mat2 jacobian = mat2(dFdx(srcPos), dFdy(srcPos));

    // Alternative if derivatives aren't available:
    // mat2 jacobian = mat2(scale.x, 0.0, 0.0, scale.y);

    // Compute ellipse parameters from Jacobian
    mat2 Jinv = inverse(jacobian);
    mat2 A    = transpose(Jinv) * Jinv;

    // EWA filter radius (Lanczos-2 or Lanczos-3)

    // Compute bounding box for filter kernel
    float a = A[0][0], b = A[0][1], c = A[1][1];
    float det = a * c - b * b;
    float F   = FilterRadius * FilterRadius;
    float ddx = sqrt(F * c / det);
    float ddy = sqrt(F * a / det);

    vec2 center = floor(srcPos + 0.5);
    int x0      = int(floor(center.x - ddx));
    int x1      = int(ceil(center.x + ddx));
    int y0      = int(floor(center.y - ddy));
    int y1      = int(ceil(center.y + ddy));

    vec4 colorSum   = vec4(0.0);
    float weightSum = 0.0;

    // EWA filtering
    for (int y = y0; y <= y1; y++) {
      for (int x = x0; x <= x1; x++) {
        vec2 samplePos = vec2(float(x), float(y));
        vec2 d         = samplePos - srcPos;

        // Compute squared distance in ellipse-transformed space
        float distSq = dot(d, A * d);

        if (distSq < F) {
          // Sample is inside the ellipse
          float dist   = sqrt(distSq);
          float weight = lanczosWeight(dist / FilterRadius) / FilterRadius;

          // Apply Jacobian determinant for proper area weighting
          weight /= abs(determinant(jacobian));

          vec2 tc    = (samplePos + 0.5) * invSrcSize;
          vec4 color = textureLod(tex, tc, 0);

          colorSum += color * weight;
          weightSum += weight;
        }
      }
    }

    vec4 rval = vec4(0.0);

    // Normalize and output
    if (weightSum > 0.0) {
      rval = saturateV4(colorSum / weightSum);
    } else {
      // Fallback for degenerate cases
      rval = textureLod(tex, uv, 0);
    }
    return rval;
  }

  float lanczosKernel(float x, float a) {
    if (x < 1e-5) return 1.0;
    if (x >= 1.0) return 0.0;
    
    float pix = PI * x;
    // The kernel should be: sin(πx) * sin(πx/a) / (π²x²/a)
    // Which simplifies to: a * sin(πx) * sin(πx/a) / (π²x²)
    
    return sin(pix) * sin(pix / a) / (pix * pix / a);
  }

  vec4 textureLanczosEWA2(sampler2D tex, vec2 uv) {
    vec2 srcSize    = vec2(textureSize(tex, 0));
    vec2 invSrcSize = 1.0 / srcSize;

    // Get derivatives for EWA
    vec2 srcPos = uv * srcSize - 0.5;
    vec2 dx     = dFdx(srcPos);
    vec2 dy     = dFdy(srcPos);

    // Form the ellipse matrix (inverse covariance)
    float a = dot(dx, dx);
    float b = dot(dx, dy);
    float c = dot(dy, dy);

    // Regularize to prevent numerical issues
    const float epsilon = 1e-6;
    a                   = max(a, epsilon);
    c                   = max(c, epsilon);

    // Compute filter support ellipse
    float det    = a * c - b * b;
    float invDet = 1.0 / max(det, epsilon);

    // Lanczos radius (2 or 3)
    const float radius   = 3.0;
    const float radiusSq = radius * radius;

    // Compute axis-aligned bounding box
    float s   = sqrt(invDet);
    float ddx = radius * s * sqrt(c);
    float ddy = radius * s * sqrt(a);

    // Clamp filter size for performance
    ddx = min(ddx, 5.0);
    ddy = min(ddy, 5.0);

    vec2 center = floor(srcPos + 0.5);

    // Initialize accumulators
    vec4 colorSum   = vec4(0.0);
    float weightSum = 0.0;

    // For anti-ringing
    vec4 localMin = vec4(1e10);
    vec4 localMax = vec4(-1e10);
    vec4 M1       = vec4(0.0); // First moment
    vec4 M2       = vec4(0.0); // Second moment

    // Main filter loop
    int x0 = int(center.x - ddx);
    int x1 = int(center.x + ddx);
    int y0 = int(center.y - ddy);
    int y1 = int(center.y + ddy);

    for (int y = y0; y <= y1; y++) {
      for (int x = x0; x <= x1; x++) {
        vec2 samplePos = vec2(float(x), float(y));
        vec2 d         = samplePos - srcPos;

        // Evaluate elliptical distance
        float ellipDist = (c * d.x * d.x - 2.0 * b * d.x * d.y + a * d.y * d.y) * invDet;

        if (ellipDist < radiusSq) {
          float dist   = sqrt(ellipDist) / radius;
          float weight = lanczosKernel(dist, radius);

          // Area compensation
          weight *= s;

          // Fetch sample
          vec2 tc    = clamp((samplePos + 0.5) * invSrcSize, 0.0, 1.0);
          vec4 color = textureLod(tex, tc, 0);

          // Accumulate
          colorSum += color * weight;
          weightSum += weight;

          // Track statistics for anti-ringing
          localMin = min(localMin, color);
          localMax = max(localMax, color);
          M1 += color * weight;
          M2 += color * color * weight;
        }
      }
    }

    vec4 rval = vec4(0.0);

    // Final color with anti-ringing
    if (weightSum > 0.001) {
      vec4 filteredColor = colorSum / weightSum;

      // Compute local variance
      vec4 mean     = M1 / weightSum;
      vec4 variance = M2 / weightSum - mean * mean;
      vec4 sigma    = sqrt(max(variance, vec4(0.0)));

      // Adaptive clamping based on local statistics
      vec4 minClamp = mean - sigma * 1.5;
      vec4 maxClamp = mean + sigma * 1.5;
      minClamp      = max(minClamp, localMin);
      maxClamp      = min(maxClamp, localMax);

      rval = clamp(filteredColor, minClamp, maxClamp);
    } else {
      // Fallback
      rval = textureLod(tex, uv, 0);
    }
    return rval;
  }
}