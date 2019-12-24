#pragma once

#include <ork/math/cvector4.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/cmatrix4.hpp>
#include <ork/math/cmatrix3.hpp>
#include <algorithm>

namespace ork::brdf {

inline dvec3 rcp(const dvec3& inp) {
  return dvec3(1.0 / inp.x, 1.0 / inp.y, 1.0 / inp.z);
}

inline double saturate(double inp) {
  return (inp > 1.0) ? 1.0 : ((inp < 0.0) ? 0.0 : inp);
}

///////////////////////////////////////////////////////////
// https://gist.github.com/reinsteam/12a81a6fbff178b298310ae7b6d6ca2f
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
///////////////////////////////////////////////////////////

inline uint32_t bitReverse(uint32_t x) {
  x = ((x & 0x55555555) << 1) | ((x & 0xaaaaaaaa) >> 1);
  x = ((x & 0x33333333) << 2) | ((x & 0xcccccccc) >> 2);
  x = ((x & 0x0f0f0f0f) << 4) | ((x & 0xf0f0f0f0) >> 4);
  x = ((x & 0x00ff00ff) << 8) | ((x & 0xff00ff00) >> 8);
  x = ((x & 0x0000ffff) << 16) | ((x & 0xffff0000) >> 16);
  return x;
}

inline dvec2 hammersley(uint32_t index, uint32_t sampleCount) {
  // return point from hammersly point set (on hemisphere)
  double u = double(index) / double(sampleCount);
  double v = double(bitReverse(index)) * 2.3283064365386963e-10;
  return dvec2(u, v);
}

inline dvec3 hemisphereSample_uniform(double u, double v) {
  double phi      = v * PI2;
  double cosTheta = 1.0 - u;
  double sinTheta = sqrt(1.0 - cosTheta * cosTheta);
  return dvec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

inline dvec3 hemisphereSample_cos(double u, double v) {
  double phi      = v * PI2;
  double cosTheta = sqrt(1.0 - u);
  double sinTheta = sqrt(1.0 - cosTheta * cosTheta);
  return dvec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
}

inline dvec3 sphericalToCartesian(double PhiAngle, double CosTheta, double SinTheta) {
  return dvec3(SinTheta * cos(PhiAngle), SinTheta * sin(PhiAngle), CosTheta);
}

inline dvec3 importanceSampleBlinn(dvec2 e, double roughness) {
  const double rufsq    = roughness * roughness;
  const double n        = 2.0 * 1.0 / (rufsq * rufsq) - 2.0;
  const double phi      = PI2 * e.x;
  const double cosTheta = pow(1.0 - e.y, 1.0 * 1.0 / (n + 1.0));
  const double sinTheta = sqrt(1.0 - cosTheta * cosTheta);
  return sphericalToCartesian(phi, cosTheta, sinTheta);
}

inline dvec3 importanceSampleGGX(dvec2 e, float roughness) {
  const double rufsq         = roughness * roughness;
  const double rufp4         = rufsq * rufsq;
  const double phi           = PI2 * e.x;
  const double cosThetaSqNum = (1.0 - e.y);
  const double cosThetaSqDiv = (1.0 + (rufp4 - 1.0) * e.y);
  const double cosThetaSq    = cosThetaSqNum / cosThetaSqDiv;
  const double cosTheta      = sqrt(cosThetaSq);
  const double sinTheta      = sqrt(1.0 - cosThetaSq);
  return sphericalToCartesian(phi, cosTheta, sinTheta);
}

///////////////////////////////////////////////////////////
// https://pastebin.com/7Vua8Ngt
///////////////////////////////////////////////////////////

inline double geometrySchlickGGX(dvec3 normal, dvec3 dir, double roughness) {
  double k       = roughness * roughness / 2.0;
  double numer   = saturate(normal.Dot(dir));
  double divisor = numer * (1.0 - k) + k;
  return numer / divisor;
}

inline double geometrySmith(dvec3 normal, dvec3 viewdir, dvec3 lightdir, double roughness) {
  return geometrySchlickGGX(normal, viewdir, roughness) * geometrySchlickGGX(normal, lightdir, roughness);
}

inline dvec3 fresnelSchlick(dvec3 normal, dvec3 viewdir, dvec3 F0) {
  using namespace std;
  double ndotv_sat = saturate(normal.Dot(viewdir));
  return F0 + (dvec3(1, 1, 1) - F0) * pow(1.0 - ndotv_sat, 5.0);
}

///////////////////////////////////////////////////////////

template <size_t numsamples> inline dvec2 integrateGGX(double n_dot_v, double roughness) {
  n_dot_v = saturate(n_dot_v);
  dvec3 v(sqrt(1.0 - n_dot_v * n_dot_v), 0, n_dot_v);
  double accum_scale = 0.0;
  double accum_bias  = 0.0;
  for (int i = 0; i < numsamples; i++) {
    dvec2 e            = hammersley(i, numsamples);
    dvec3 h            = importanceSampleGGX(e, roughness);
    double v_dot_h     = v.Dot(h);
    dvec3 l            = ((h * 2.0 * v_dot_h) - v).Normal();
    double n_dot_h_sat = saturate(h.z);
    double v_dot_h_sat = saturate(v_dot_h);
    if (l.z > 0.0) {
      double gsmith = geometrySmith(dvec3(0, 0, 1), v, l, roughness);
      double gvis   = (gsmith * v_dot_h) / (n_dot_h_sat * n_dot_v);
      double fc     = pow(1.0 - v_dot_h_sat, 5.0);
      accum_scale += (1.0 - fc) * gvis;
      accum_bias += fc * gvis;
    }
  }
  return dvec2(accum_scale / double(numsamples), accum_bias / double(numsamples));
}

} // namespace ork::brdf
