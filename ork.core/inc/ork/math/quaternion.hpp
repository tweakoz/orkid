////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <cmath>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/cvector4.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T>::Quaternion(T _x, T _y, T _z, T _w) {
  x = (_x);
  y = (_y);
  z = (_z);
  w = (_w);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::Lerp(const Quaternion<T>& a, const Quaternion<T>& b, T alpha) {
  bool bflip;

  Quaternion q;

  T cos_t = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

  /* if B is on opposite hemisphere from A, use -B instead */
  if (cos_t < T(0.0f)) {
    bflip = TRUE;
  } else {
    bflip = FALSE;
  }

  T beta   = T(1.0f) - alpha;
  T alpha2 = alpha;

  if (bflip) {
    alpha2 = -alpha2;
  }

  q.x = (beta * a.x + alpha2 * b.x);
  q.y = (beta * a.y + alpha2 * b.y);
  q.z = (beta * a.z + alpha2 * b.z);
  q.w = (beta * a.w + alpha2 * b.w);

  return q;
}

template <typename T> Quaternion<T>::Quaternion(const Matrix44<T>& matrix) {
  FromMatrix(matrix);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::FromMatrix(const Matrix44<T>& M) {
  T q[4];
  const int nxt[3] = {1, 2, 0};

  T tr = M.GetElemYX(0, 0) + M.GetElemYX(1, 1) + M.GetElemYX(2, 2);

  if (tr > T(0.0f)) {
    T s  = sqrtf(tr + T(1.0f));
    q[3] = s * T(0.5f);
    s    = T(0.5f) / s;
    q[0] = (M.GetElemYX(1, 2) - M.GetElemYX(2, 1)) * s;
    q[1] = (M.GetElemYX(2, 0) - M.GetElemYX(0, 2)) * s;
    q[2] = (M.GetElemYX(0, 1) - M.GetElemYX(1, 0)) * s;
  } else {
    int i = 0;
    if (M.GetElemYX(1, 1) > M.GetElemYX(0, 0))
      i = 1;
    if (M.GetElemYX(2, 2) > M.GetElemYX(i, i))
      i = 2;
    int j = nxt[i];
    int k = nxt[j];
    T s   = sqrtf((M.GetElemYX(i, i) - (M.GetElemYX(j, j) + M.GetElemYX(k, k))) + T(1.0f));
    q[i]  = s * T(0.5f);

    if (fabs(s) < T(EPSILON)) {
      Identity();
    } else {
      s    = T(0.5f) / s;
      q[3] = (M.GetElemYX(j, k) - M.GetElemYX(k, j)) * s;
      q[j] = (M.GetElemYX(i, j) + M.GetElemYX(j, i)) * s;
      q[k] = (M.GetElemYX(i, k) + M.GetElemYX(k, i)) * s;
    }
  }

  x = q[0];
  y = q[1];
  z = q[2];
  w = q[3];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::FromMatrix3(const Matrix33<T>& M) {
  T q[4];
  const int nxt[3] = {1, 2, 0};

  T tr = M.GetElemYX(0, 0) + M.GetElemYX(1, 1) + M.GetElemYX(2, 2);

  if (tr > T(0.0f)) {
    T s  = sqrtf(tr + T(1.0f));
    q[3] = s * T(0.5f);
    s    = T(0.5f) / s;
    q[0] = (M.GetElemYX(1, 2) - M.GetElemYX(2, 1)) * s;
    q[1] = (M.GetElemYX(2, 0) - M.GetElemYX(0, 2)) * s;
    q[2] = (M.GetElemYX(0, 1) - M.GetElemYX(1, 0)) * s;
  } else {
    int i = 0;
    if (M.GetElemYX(1, 1) > M.GetElemYX(0, 0))
      i = 1;
    if (M.GetElemYX(2, 2) > M.GetElemYX(i, i))
      i = 2;
    int j = nxt[i];
    int k = nxt[j];
    T s   = sqrtf((M.GetElemYX(i, i) - (M.GetElemYX(j, j) + M.GetElemYX(k, k))) + T(1.0f));
    q[i]  = s * T(0.5f);

    if (fabs(s) < T(EPSILON)) {
      Identity();
    } else {
      s    = T(0.5f) / s;
      q[3] = (M.GetElemYX(j, k) - M.GetElemYX(k, j)) * s;
      q[j] = (M.GetElemYX(i, j) + M.GetElemYX(j, i)) * s;
      q[k] = (M.GetElemYX(i, k) + M.GetElemYX(k, i)) * s;
    }
  }

  x = q[0];
  y = q[1];
  z = q[2];
  w = q[3];
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix44<T> Quaternion<T>::ToMatrix(void) const {
  Matrix44<T> result;

  T s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz, l;

  l = x * x + y * y + z * z + w * w;

  // should this be T::Epsilon() ?
  if (fabs(l) < T(EPSILON)) {
    s = T(1.0f);
  } else {
    s = T(2.0f) / l;
  }

  xs = x * s;
  ys = y * s;
  zs = z * s;
  wx = w * xs;
  wy = w * ys;
  wz = w * zs;
  xx = x * xs;
  xy = x * ys;
  xz = x * zs;
  yy = y * ys;
  yz = y * zs;
  zz = z * zs;

  result.SetElemYX(0, 0, T(1.0f) - (yy + zz));
  result.SetElemYX(1, 0, xy - wz);
  result.SetElemYX(2, 0, xz + wy);
  result.SetElemYX(0, 1, xy + wz);
  result.SetElemYX(1, 1, T(1.0f) - (xx + zz));
  result.SetElemYX(2, 1, yz - wx);
  result.SetElemYX(0, 2, xz - wy);
  result.SetElemYX(1, 2, yz + wx);
  result.SetElemYX(2, 2, T(1.0f) - (xx + yy));

  return result;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Matrix33<T> Quaternion<T>::ToMatrix3(void) const {
  Matrix33<T> result;

  T s, xs, ys, zs, wx, wy, wz, xx, xy, xz, yy, yz, zz, l;

  l = x * x + y * y + z * z + w * w;

  // should this be T::Epsilon() ?
  if (fabs(l) < T(EPSILON)) {
    s = T(1.0f);
  } else {
    s = T(2.0f) / l;
  }

  xs = x * s;
  ys = y * s;
  zs = z * s;
  wx = w * xs;
  wy = w * ys;
  wz = w * zs;
  xx = x * xs;
  xy = x * ys;
  xz = x * zs;
  yy = y * ys;
  yz = y * zs;
  zz = z * zs;

  result.SetColumn(0, Vector3<T>(T(1.0f) - (yy + zz), xy - wz, xz + wy));
  result.SetColumn(1, Vector3<T>(xy + wz, T(1.0f) - (xx + zz), yz - wx));
  result.SetColumn(2, Vector3<T>(xz - wy, yz + wx, T(1.0f) - (xx + yy)));

  return result;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::Scale(T scalar) {
  x *= scalar;
  y *= scalar;
  z *= scalar;
  w *= scalar;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::Slerp(const Quaternion<T>& a, const Quaternion<T>& b, T alpha) {
  // Original code from Graphics Gems III - (Morrison, quaternion interpolation with extra spins).

  T beta;         /* complementary interp parameter */
  T theta;        /* angle between A and B */
  T sin_t, cos_t; /* sine, cosine of theta */
  T phi;          /* theta plus spins */
  bool bflip;     /* use negation of B? */
  int spin = 0;
  // const F32 EPSILON = 0.0001f;
  // const F32 PI=3.14159254f;
  Quaternion<T> q;

  if ((a.x == b.x) && (a.y == b.y) && (a.z == b.z) && (a.w == b.w)) {
    q.x = (a.x);
    q.y = (a.y);
    q.z = (a.z);
    q.w = (a.w);
    return (q);
  }

  /* cosine theta = dot product of A and B */
  cos_t = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;

  /* if B is on opposite hemisphere from A, use -B instead */
  if (cos_t < T(0.0f)) {
    cos_t = -cos_t;
    bflip = true;
  } else
    bflip = false;

  /* if B is (within precision limits) the same as A,
   * just linear interpolate between A and B.
   * Can't do spins, since we don't know what direction to spin.
   */
  if (T(1.0f) - cos_t < Float::Epsilon()) {
    beta = T(1.0f) - alpha;
  } else { /* normal case */
    theta = acosf(cos_t);
    phi   = theta + T(spin) * Float::Pi();
    sin_t = sinf(theta);
    beta  = sinf(theta - alpha * phi) / sin_t;
    alpha = sinf(alpha * phi) / sin_t;
  }

  if (bflip)
    alpha = -alpha;

  /* interpolate */
  q.x = (beta * a.x + alpha * b.x);
  q.y = (beta * a.y + alpha * b.y);
  q.z = (beta * a.z + alpha * b.z);
  q.w = (beta * a.w + alpha * b.w);

  return (q);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::Negate(void) {
  Quaternion<T> result;

  result.x = (-x);
  result.y = (-y);
  result.z = (-z);
  result.w = (-w);

  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::Square(void) {
  T temp = T(2) * w;
  Quaternion<T> result;

  result.x = (x * temp);
  result.y = (y * temp);
  result.z = (z * temp);
  result.w = (w * w - (x * x + y * y + z * z));

  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::Conjugate(Quaternion<T>& a) {
  Quaternion<T> result;

  result.x = (-x);
  result.y = (-y);
  result.z = (-z);
  result.w = (w);

  return (result);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> T Quaternion<T>::Magnitude(void) {
  return (w * w + x * x + y * y + z * z);
}

///////////////////////////////////////////////////////////////////////////////
//	DESC: Converts a normalized axis and angle to a unit quaternion.

template <typename T> void Quaternion<T>::FromAxisAngle(const Vector4<T>& v) {
  T l = v.Mag();

  if (l < Float::Epsilon()) {
    x = y = z = T(0.0f);
    w         = T(1.0f);
    return;
  }

  T omega = -T(0.5f) * v.w;
  T s     = sinf(omega) / l;
  x       = (s * v.x);
  y       = (s * v.y);
  z       = (s * v.z);
  w       = (cosf(omega));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Vector4<T> Quaternion<T>::ToAxisAngle(void) const {
  static const double kAAC = (114.591559026 * DTOR);
  T tr                     = acosf(w);
  T dsin                   = sinf(tr);
  T invscale               = (dsin == T(0.0f)) ? T(1.0f) : T(1.0f) / dsin;
  T vx                     = x * invscale;
  T vy                     = y * invscale;
  T vz                     = z * invscale;
  T ang                    = tr * T((float)kAAC);
  return Vector4<T>(vx, vy, vz, -ang);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::Add(Quaternion<T>& a) {
  x += a.x;
  y += a.y;
  z += a.z;
  w += a.w;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::Sub(Quaternion<T>& a) {
  x -= a.x;
  y -= a.y;
  z -= a.z;
  w -= a.w;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> Quaternion<T> Quaternion<T>::Multiply(const Quaternion<T>& b) const {
  Quaternion<T> a;

  a.x = (x * b.w + y * b.z - z * b.y + w * b.x);
  a.y = (-x * b.z + y * b.w + z * b.x + w * b.y);
  a.z = (x * b.y - y * b.x + z * b.w + w * b.z);
  a.w = (-x * b.x - y * b.y - z * b.z + w * b.w);

  return (a);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::Divide(Quaternion<T>& a) {
  x /= a.x;
  y /= a.y;
  z /= a.z;
  w /= a.w;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::Identity(void) {
  w = (T(1.0f));
  x = (T(0.0f));
  y = (T(0.0f));
  z = (T(0.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::ShortestRotationArc(Vector4<T> v0, Vector4<T> v1) {
  v0.Normalize();
  v1.Normalize();

  Vector4<T> cross = v1.Cross(v0); // Cross is non destructive
  T dot            = v1.Dot(v0);
  T s              = sqrtf((T(1.0f) + dot) * T(2.0f));

  x = (cross.x / s);
  y = (cross.y / s);
  z = (cross.z / s);
  w = (s / T(2.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::dump(void) {
  orkprintf("quat %f %f %f %f\n", x, y, z, w);
}

///////////////////////////////////////////////////////////////////////////////
// smallest 3 compression - 16 bytes -> 4 bytes  (game gems 3, page 189)
///////////////////////////////////////////////////////////////////////////////

template <typename T> QuatCodec Quaternion<T>::Compress(void) const {
  static const T frange = T((1 << 9) - 1);

  QuatCodec uquat;

  T qf[4];
  T fqmax(-2.0f);
  qf[0] = x;
  qf[1] = y;
  qf[2] = z;
  qf[3] = w;

  ////////////////////////////////////////
  // normalize quaternion

  T fmag = sqrtf(qf[0] * qf[0] + qf[1] * qf[1] + qf[2] * qf[2] + qf[3] * qf[3]);

  // qf[0] /= fmag;
  // qf[1] /= fmag;
  // qf[2] /= fmag;
  // qf[3] /= fmag;

  ////////////////////////////////////////
  // find smallest 3

  int iqlargest = -1;

  for (int i = 0; i < 4; i++) {
    T fqi = fabs(qf[i]);

    if (fqi > fqmax) {
      fqmax     = fqi;
      iqlargest = i;
    }
  }

  uquat.milargest = iqlargest;

  ////////////////////////////////////////
  // scale quat component from -1 .. 1
  const T fsqr2d2 = T(1.0f) / sqrtf(T(2.0f));
  ////////////////////////////////////////

  int iq3[3];
  T ftq3[3];

  int iidx = 0;

  static T fmin(100.0f);
  static T fmax(-100.0f);

  for (int i = 0; i < 4; i++) {
    T fqi = fabs(qf[i]);

    if (i != iqlargest) {
      OrkAssert(fqi <= fsqr2d2);
      T fi = qf[i] * fsqr2d2 * T(2.0f);

      if (fi > fmax)
        fmax = fi;
      if (fi < fmin)
        fmin = fi;

      iq3[iidx]  = int(fi * frange);
      ftq3[iidx] = qf[i];

      iidx++;
    }
  }

  uquat.miElem0 = iq3[0];
  uquat.miElem1 = iq3[1];
  uquat.miElem2 = iq3[2];

  OrkAssert(uquat.miElem0 == iq3[0]);
  OrkAssert(uquat.miElem1 == iq3[1]);
  OrkAssert(uquat.miElem2 == iq3[2]);

  uquat.miwsign = (qf[iqlargest] >= T(0.0f));

  ////////////////////////////////

  iidx = 0;

  iidx   = (iidx == iqlargest) ? iidx + 1 : iidx;
  T ftq0 = qf[iidx++];
  iidx   = (iidx == iqlargest) ? iidx + 1 : iidx;
  T ftq1 = qf[iidx++];
  iidx   = (iidx == iqlargest) ? iidx + 1 : iidx;
  T ftq2 = qf[iidx++];

  T ftq012 = ((ftq0 * ftq0) + (ftq1 * ftq1) + (ftq2 * ftq2));

  // 1.0f = ftq0123 + (ftqD*ftqD)
  // ftqD*ftqD = 1.0f - ftq0123

  T ftqD = sqrtf(T(1.0f) - ftq012);

  T ferr = fabs(ftqD - qf[iqlargest]);

  ////////////////////////////////

  return uquat;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::DeCompress(QuatCodec uquat) {
  static const T frange  = T((1 << 9) - 1);
  static const T fsqr2d2 = sqrtf(T(2.0f)) / T(2.0f);
  static const T firange = fsqr2d2 / frange;

  int iqlargest = int(uquat.milargest);
  int iqlsign   = int(uquat.miwsign);

  T* pfq = (T*)&x;

  int iidx = 0;

  iidx      = (iidx == iqlargest) ? iidx + 1 : iidx;
  pfq[iidx] = T(int(uquat.miElem0)) * firange;
  T fq0     = pfq[iidx++];
  iidx      = (iidx == iqlargest) ? iidx + 1 : iidx;
  pfq[iidx] = T(int(uquat.miElem1)) * firange;
  T fq1     = pfq[iidx++];
  iidx      = (iidx == iqlargest) ? iidx + 1 : iidx;
  pfq[iidx] = T(int(uquat.miElem2)) * firange;
  T fq2     = pfq[iidx++];

  T fq012 = ((fq0 * fq0) + (fq1 * fq1) + (fq2 * fq2));

  pfq[iqlargest] = sqrtf(T(1.0f) - fq012) * (iqlsign ? T(1.0f) : T(-1.0f));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void Quaternion<T>::Normalize() {
  float x2 = x * x;
  float y2 = y * y;
  float z2 = z * z;
  float w2 = w * w;

  float sq = sqrtf(w2 + x2 + y2 + z2);

  x /= sq;
  y /= sq;
  z /= sq;
  w /= sq;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
