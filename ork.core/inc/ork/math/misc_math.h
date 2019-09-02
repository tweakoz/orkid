////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <cmath>
#include <ork/math/polar.h>
#include <algorithm>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

#if defined(IX)
#define MathNS
#elif defined(_WIN32)
#define MathNS std
#endif

inline float sqrtf(float finp) { return MathNS::sqrtf(finp); }
inline float acosf(float finp) { return MathNS::acosf(finp); }
inline float cosf(float finp) { return MathNS::cosf(finp); }
inline float sinf(float finp) { return MathNS::sinf(finp); }
inline float tanf(float finp) { return MathNS::tanf(finp); }
inline float fabs(float finp) { return MathNS::fabs(finp); }
inline float atanf(float finp) { return MathNS::atanf(finp); }
inline float atan2f(float finp, float finp2) { return MathNS::atan2f(finp, finp2); }
inline float pow(float finp, float finp2) { return MathNS::pow(finp, finp2); }
inline float powf(float finp, float finp2) { return MathNS::powf(finp, finp2); }
inline float ceil(float finp) { return MathNS::ceil(finp); }
inline float floor(float finp) { return MathNS::floor(finp); }

template <typename T> T inline clamp( T tin, T tmin, T tmax )
{
	return (tin<tmin) ? tmin
	                  : (tin>tmax) ? tmax
								   : tin;
}

///////////////////////////////////////////////////////////////////////////////

inline bool IsPowerOfTwo(int ival) {
  int inumbits = 0;
  int ibitidx = 30;

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

inline int HighestPowerOfTwo(int ival) {
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
  i = (int)ork::floor(x);
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
  i = (int)ork::ceil(x);
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

inline F32 norm_radian_angle(F32 in) {
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

inline F32 calc_dist(F32 x0, F32 y0, F32 z0, F32 x1, F32 y1, F32 z1) {
  F32 dx = (x1 - x0);
  F32 dy = (y1 - y0);
  F32 dz = (z1 - z0);
  F32 dist = ork::sqrtf((dx * dx) + (dy * dy) + (dz * dz));
  return dist;
}

///////////////////////////////////////////////////////////////////////////////

inline bool clip_angle(F32 x0, F32 y0, F32 z0, F32 x1, F32 y1, F32 z1, F32 Tang, F32 Cang) {
  bool do_clip = false;
  F32 dx = (x1 - x0);
  F32 dz = (z1 - z0);
  F32 ang = rect2pol_ang((F32)dx, (F32)dz);
  ang = norm_radian_angle(ang);
  F32 tang = norm_radian_angle(Tang);
  F32 angD = ang - tang;
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

class CPerlin2D {
public:
  // static const f32 ONEDIVNOISETABLESAMPLES =		0.03125f;
  static const int NOISETABLESAMPLES = 32;
  static const int NOISETABLESIZE = 66;    // (NOISETABLESAMPLES*2+2)
  static const int NOISETABLESIZEF4 = 264; // NOISETABLESIZE*4 (CG float4)

  static const int B = 0x1000;
  static const int BM = 0xfff;
  static const int N = 0x1000;

  static int* p;
  static f32* g2;

  void static pnnormalize2(f32* v) {
    f32 s;
    s = ork::sqrtf(v[0] * v[0] + v[1] * v[1]);
    v[0] = v[0] / s;
    v[1] = v[1] / s;
  }

  inline static f32 at2(f32 rx, f32 ry, f32* q) { return f32(rx * q[0] + ry * q[1]); }

  inline static f32 pnlerp(f32 t, f32 a, f32 b) { return f32(a + t * (b - a)); }

  // this is the smoothstep function f(t) = 3t^2 - 2t^3, without the normalization
  inline static f32 s_curve(f32 t) { return f32(t * t * (3.0f - (2.0f * t))); }

  inline static void pnsetup(int i, int& b0, int& b1, f32& r0, f32& r1, f32& t, f32* vec) {
    t = vec[i] + N;
    b0 = ((int)t) & BM;
    b1 = (b0 + 1) & BM;
    r0 = t - (int)t;
    r1 = r0 - 1.0f;
  }

  static void GenerateNoiseTable(void) {
    p = new int[B + B + 2];
    g2 = new f32[(B + B + 2) * 2];

    int i, j, k;
    for (i = 0; i < B; i++) {
      p[i] = i;
      for (j = 0; j < 2; j++) {
        g2[(i * 2) + j] = (f32)((std::rand() % (B + B)) - B) / B;
      }

      pnnormalize2(&g2[(i * 2)]);
    }
    while (--i) {
      k = p[i];
      j = std::rand() % B;
      p[i] = p[j];
      p[j] = k;
    }
    for (i = 0; i < B + 2; i++) {
      p[B + i] = p[i];
      for (j = 0; j < 2; j++)
        g2[((B + i) * 2) + j] = g2[(i * 2) + j];
    }
  }

  static f32 PlaneNoiseFunc(f32 fu, f32 fv, f32 fou, f32 fov, f32 fAmp, f32 fFrq) {
    static bool bINIT = true;

    if (bINIT) {
      GenerateNoiseTable();
      bINIT = false;
    }

    int bx0, bx1, by0, by1, b00, b10, b01, b11;
    f32 rx0, rx1, ry0, ry1, *q, sx, sy, a, b, t, u, v;
    int i, j;

    f32 vec[2] = {fou + (fu * fFrq), fov + (fv * fFrq)};

    pnsetup(0, bx0, bx1, rx0, rx1, t, vec);
    pnsetup(1, by0, by1, ry0, ry1, t, vec);

    i = p[bx0];
    j = p[bx1];
    b00 = p[i + by0];
    b10 = p[j + by0];
    b01 = p[i + by1];
    b11 = p[j + by1];
    sx = s_curve(rx0);
    sy = s_curve(ry0);
    q = &g2[(b00 * 2)];
    u = at2(rx0, ry0, q);
    q = &g2[(b10 * 2)];
    v = at2(rx1, ry0, q);
    a = pnlerp(sx, u, v);
    q = &g2[(b01 * 2)];
    u = at2(rx0, ry1, q);
    q = &g2[(b11 * 2)];
    v = at2(rx1, ry1, q);
    b = pnlerp(sx, u, v);

    f32 val = pnlerp(sy, a, b);

    return val * fAmp;
  }
};

///////////////////////////////////////////////////////////////////////////////

class CPolynomial {
public:
  CPolynomial() {}

  CPolynomial Differentiate() const;
  void SetCoefs(const float* array);
  void SetCoefs(int i, float num);
  float GetCoefs(int i) const;
  float Evaluate(float val) const;
  float operator()(float val) const;
  CPolynomial operator=(const CPolynomial& a);
  CPolynomial operator+(const CPolynomial& a);
  CPolynomial operator-(const CPolynomial& a);

private:
  float coefs[4];
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
