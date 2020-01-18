////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <ork/math/polar.h>
#include <algorithm>
#include <ork/orktypes.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

#if defined(LINUX)
#define MathNS
#endif

///////////////////////////////////////////////////////////////////////////////

inline float log_base(float base, float inp) {
  float rval = logf(inp) / logf(base);
  return rval;
}
inline float pow_base(float base, float inp) {
  float rval = powf(base, inp);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

inline int powerOfTwoIndex(size_t inp) {
  int index = 0;
  while (((inp & 1) == 0) && inp > 1) {
    inp >>= 1;
    index++;
  }
  return index;
}

///////////////////////////////////////////////////////////////////////////////

inline bool isPowerOfTwo(int ival) {
  int inumbits = 0;
  int ibitidx  = 30;

  while (ival != 0) {
    int ibitmask = 1 << ibitidx;

    if (ival & ibitmask) {
      inumbits++;
    }

    ival &= (~ibitmask);

    ibitidx--;
  }

  return (inumbits == 1);
}

///////////////////////////////////////////////////////////////////////////////

inline int highestPowerOfTwo(int ival) {
  int ibitidx = 30;

  int irval = -1;

  while ((irval == -1) && (ibitidx >= 0)) {
    int ibitmask = 1 << ibitidx;

    if (ival & ibitmask) {
      irval = ibitidx;
    }

    ibitidx--;
  }

  return irval;
}

///////////////////////////////////////////////////////////////////////////////

inline size_t nextPowerOfTwo(size_t inp) {

  size_t result = inp;
  result--;
  result |= result >> 1;  // 1
  result |= result >> 2;  // 3
  result |= result >> 4;  // 7
  result |= result >> 8;  // 15
  result |= result >> 16; // 31
  result |= result >> 32; // 63
  result++;
  return result;
}

///////////////////////////////////////////////////////////////////////////////

template <size_t S> constexpr size_t nextPowerOfTwo() {

  size_t result = S;
  result--;
  result |= result >> 1;  // 1
  result |= result >> 2;  // 3
  result |= result >> 4;  // 7
  result |= result >> 8;  // 15
  result |= result >> 16; // 31
  result |= result >> 32; // 63
  result++;
  return result;
}

///////////////////////////////////////////////////////////////////////////////

inline size_t clz(size_t inp) {
  size_t x = inp;
  size_t n = 64;
  size_t y = 0;
  y        = x >> 32;
  if (y != 0) {
    n = n - 32;
    x = y;
  }
  y = x >> 16;
  if (y != 0) {
    n = n - 16;
    x = y;
  }
  y = x >> 8;
  if (y != 0) {
    n = n - 8;
    x = y;
  }
  y = x >> 4;
  if (y != 0) {
    n = n - 4;
    x = y;
  }
  y = x >> 2;
  if (y != 0) {
    n = n - 2;
    x = y;
  }
  y = x >> 1;
  if (y != 0)
    return n - 2;
  return n - x;
}

template <size_t S> constexpr size_t clz() {
  size_t x = S;
  size_t n = 64;
  size_t y = 0;
  y        = x >> 32;
  if (y != 0) {
    n = n - 32;
    x = y;
  }
  y = x >> 16;
  if (y != 0) {
    n = n - 16;
    x = y;
  }
  y = x >> 8;
  if (y != 0) {
    n = n - 8;
    x = y;
  }
  y = x >> 4;
  if (y != 0) {
    n = n - 4;
    x = y;
  }
  y = x >> 2;
  if (y != 0) {
    n = n - 2;
    x = y;
  }
  y = x >> 1;
  if (y != 0)
    return n - 2;
  return n - x;
}

inline size_t numbits(size_t inp) {
  return 64 - clz(inp);
}
template <size_t S> constexpr size_t numbits() {
  return 64 - clz<S>();
}

///////////////////////////////////////////////////////////////////////////////

inline int floor_int(float x) {
  // OrkAssert( (x > static_cast <float> (INT_MIN / 2) � 1.0));
  // OrkAssert( (x < static_cast <float> (INT_MAX / 2) + 1.0));
  const float round_towards_m_i = -0.5f;
  int i;

#if 0 // defined( _WIN32 )
    __asm
	{
		fld x
		fadd st, st (0)
		fadd round_towards_m_i
		fistp i
		sar i, 1
	}
#else
  i = (int)floor(x);
#endif
  return (i);
}

///////////////////////////////////////////////////////////////////////////////

inline int ceil_int(float x) {
  // OrkAssert (x > static_cast <float> (INT_MIN / 2) � 1.0);
  // OrkAssert (x < static_cast <float> (INT_MAX / 2) + 1.0);
  const float round_towards_p_i = -0.5f;
  int i;
#if 0 // defined( _WIN32 )
	__asm
	{
		fld x
		fadd st, st (0)
		fsubr round_towards_p_i
		fistp i
		sar i, 1
	}
#else
  i = (int)ceil(x);
#endif
  return (-i);
}

///////////////////////////////////////////////////////////////////////////////

inline int truncate_int(float x) {
  // OrkAssert (x > static_cast <float> (INT_MIN / 2) � 1.0);
  // OrkAssert (x < static_cast <float> (INT_MAX / 2) + 1.0);
  const float round_towards_p_i = -0.5f;
  int i;

#if 0 // defined( _WIN32 )
	__asm
	{
		fld x
		fadd st, st (0)
		//fabs st (0)
		fadd round_towards_p_i
		fistp i
		sar i, 1
	}
#else
  i = int(x);
#endif
  if (x < 0) {
    i = -i;
  }
  return (i);
}

///////////////////////////////////////////////////////////////////////////////

inline int round_int(float x) {
  // assert (x > static_cast <double> (INT_MIN / 2) � 1.0);
  // assert (x < static_cast <double> (INT_MAX / 2) + 1.0);
  const float round_to_nearest = 0.5f;
  int i;
#if 0 // defined( _WIN32 )
	__asm
	{
		fld x
		fadd st, st (0)
		fadd round_to_nearest
		fistp i
		sar i, 1
	}
#else
  i = int(x);
#endif
  return (i);
}

///////////////////////////////////////////////////////////////////////////////

inline float norm_radian_angle(float in) {
  // we want -PI .. PI

  // first go to 0 .. 2PI
  while (in < 0.0f)
    in += PI2;
  while (in >= PI2)
    in -= PI2;

  if (in > PI)
    in = in - PI2;

  return in;
}

///////////////////////////////////////////////////////////////////////////////

inline float calc_dist(float x0, float y0, float z0, float x1, float y1, float z1) {
  float dx   = (x1 - x0);
  float dy   = (y1 - y0);
  float dz   = (z1 - z0);
  float dist = sqrtf((dx * dx) + (dy * dy) + (dz * dz));
  return dist;
}

///////////////////////////////////////////////////////////////////////////////

inline bool clip_angle(float x0, float y0, float z0, float x1, float y1, float z1, float Tang, float Cang) {
  bool do_clip = false;
  float dx     = (x1 - x0);
  float dz     = (z1 - z0);
  float ang    = rect2pol_ang((float)dx, (float)dz);
  ang          = norm_radian_angle(ang);
  float tang   = norm_radian_angle(Tang);
  float angD   = ang - tang;
  if (angD < 0.0f)
    angD *= -1.0f;
  if ((angD > Cang) && (angD < (PI2 - Cang)))
    do_clip = true;
  return do_clip;
}

struct XYZ {
  double x, y, z;
};
XYZ RotatePointAboutLine(XYZ p, double theta, XYZ p1, XYZ p2);
void Normalise(XYZ* p);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// 2D Perlin Noise
namespace math {

class Perlin2D {
public:
  // static const float ONEDIVNOISETABLESAMPLES =		0.03125f;
  static const int NOISETABLESAMPLES = 32;
  static const int NOISETABLESIZE    = 66;  // (NOISETABLESAMPLES*2+2)
  static const int NOISETABLESIZEF4  = 264; // NOISETABLESIZE*4 (CG float4)

  static const int B  = 0x1000;
  static const int BM = 0xfff;
  static const int N  = 0x1000;

  static int* p;
  static float* g2;

  void static pnnormalize2(float* v) {
    float s;
    s    = sqrtf(v[0] * v[0] + v[1] * v[1]);
    v[0] = v[0] / s;
    v[1] = v[1] / s;
  }

  inline static float at2(float rx, float ry, float* q) {
    return float(rx * q[0] + ry * q[1]);
  }

  inline static float pnlerp(float t, float a, float b) {
    return float(a + t * (b - a));
  }

  // this is the smoothstep function f(t) = 3t^2 - 2t^3, without the normalization
  inline static float s_curve(float t) {
    return float(t * t * (3.0f - (2.0f * t)));
  }

  inline static void pnsetup(int i, int& b0, int& b1, float& r0, float& r1, float& t, float* vec) {
    t  = vec[i] + N;
    b0 = ((int)t) & BM;
    b1 = (b0 + 1) & BM;
    r0 = t - (int)t;
    r1 = r0 - 1.0f;
  }

  static void GenerateNoiseTable(void) {
    p  = new int[B + B + 2];
    g2 = new float[(B + B + 2) * 2];

    int i, j, k;
    for (i = 0; i < B; i++) {
      p[i] = i;
      for (j = 0; j < 2; j++) {
        g2[(i * 2) + j] = (float)((std::rand() % (B + B)) - B) / B;
      }

      pnnormalize2(&g2[(i * 2)]);
    }
    while (--i) {
      k    = p[i];
      j    = std::rand() % B;
      p[i] = p[j];
      p[j] = k;
    }
    for (i = 0; i < B + 2; i++) {
      p[B + i] = p[i];
      for (j = 0; j < 2; j++)
        g2[((B + i) * 2) + j] = g2[(i * 2) + j];
    }
  }

  static float PlaneNoiseFunc(float fu, float fv, float fou, float fov, float fAmp, float fFrq) {
    static bool bINIT = true;

    if (bINIT) {
      GenerateNoiseTable();
      bINIT = false;
    }

    int bx0, bx1, by0, by1, b00, b10, b01, b11;
    float rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
    int i, j;

    float vec[2] = {fou + (fu * fFrq), fov + (fv * fFrq)};

    pnsetup(0, bx0, bx1, rx0, rx1, t, vec);
    pnsetup(1, by0, by1, ry0, ry1, t, vec);

    i   = p[bx0];
    j   = p[bx1];
    b00 = p[i + by0];
    b10 = p[j + by0];
    b01 = p[i + by1];
    b11 = p[j + by1];
    sx  = s_curve(rx0);
    sy  = s_curve(ry0);
    q   = &g2[(b00 * 2)];
    u   = at2(rx0, ry0, q);
    q   = &g2[(b10 * 2)];
    v   = at2(rx1, ry0, q);
    a   = pnlerp(sx, u, v);
    q   = &g2[(b01 * 2)];
    u   = at2(rx0, ry1, q);
    q   = &g2[(b11 * 2)];
    v   = at2(rx1, ry1, q);
    b   = pnlerp(sx, u, v);

    float val = pnlerp(sy, a, b);

    return val * fAmp;
  }
};

///////////////////////////////////////////////////////////////////////////////

class Polynomial {
public:
  Polynomial() {
  }

  Polynomial Differentiate() const;
  void SetCoefs(const float* array);
  void SetCoefs(int i, float num);
  float GetCoefs(int i) const;
  float Evaluate(float val) const;
  float operator()(float val) const;
  Polynomial operator=(const Polynomial& a);
  Polynomial operator+(const Polynomial& a);
  Polynomial operator-(const Polynomial& a);

private:
  float coefs[4];
};

///////////////////////////////////////////////////////////////////////////////

inline bool areValuesClose(double a, double b, double tolerance) {
  return abs(a - b) < tolerance;
}
} // namespace math
///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
