////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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
  double numer   = saturate(normal.dotWith(dir));
  double divisor = numer * (1.0 - k) + k;
  return numer / divisor;
}

inline double geometrySmith(dvec3 normal, dvec3 viewdir, dvec3 lightdir, double roughness) {
  return geometrySchlickGGX(normal, viewdir, roughness) * geometrySchlickGGX(normal, lightdir, roughness);
}

inline dvec3 fresnelSchlick(dvec3 normal, dvec3 viewdir, dvec3 F0) {
  using namespace std;
  double ndotv_sat = saturate(normal.dotWith(viewdir));
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
    double v_dot_h     = v.dotWith(h);
    dvec3 l            = ((h * 2.0 * v_dot_h) - v).normalized();
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

template <size_t numsamples> inline dvec2 integrateGGXVelvet(double n_dot_v, double roughness) {
  n_dot_v = saturate(n_dot_v);
  dvec3 v(sqrt(1.0 - n_dot_v * n_dot_v), 0, n_dot_v);
  double accum_scale = 0.0;
  double accum_bias  = 0.0;
  
  // Velvet-specific parameters
  const double asperity_density = 0.5;  // Controls density of microfibers
  const double backscatter_gain = 2.5;  // Enhances retro-reflection
  const double rim_strength = 3.0;      // Enhances grazing angle response
  
  // Modify roughness for velvet (velvet appears rougher)
  double velvet_roughness = saturate(roughness * 1.5);
  
  for (int i = 0; i < numsamples; i++) {
    dvec2 e = hammersley(i, numsamples);
    dvec3 h = importanceSampleGGX(e, velvet_roughness);
    double v_dot_h = v.dotWith(h);
    dvec3 l = ((h * 2.0 * v_dot_h) - v).normalized();
    double n_dot_h_sat = saturate(h.z);
    double n_dot_l_sat = saturate(l.z);
    double v_dot_h_sat = saturate(v_dot_h);
    
    if (l.z > 0.0) {
      // Modified geometry term for velvet
      double gsmith = geometrySmith(dvec3(0, 0, 1), v, l, velvet_roughness);
      
      // Asperity scattering - characteristic of velvet-like materials
      double asperity_term = exp(-asperity_density * (1.0 - n_dot_l_sat));
      
      // Backscattering component (stronger when light and view are aligned)
      double backscatter = backscatter_gain * pow(std::max(0.0, v.dotWith(l)), 2.0);
      
      // Rim lighting enhancement (stronger at grazing angles)
      double rim_term = rim_strength * pow(1.0 - n_dot_v, 4.0);
      
      // Combine terms
      double gvis = (gsmith * v_dot_h) / (n_dot_h_sat * n_dot_v);
      gvis = gvis * (asperity_term + backscatter + rim_term);
      
      // Modify Fresnel for velvet (less metallic-looking Fresnel)
      double fc = pow(1.0 - v_dot_h_sat, 3.0); // Softened Fresnel exponent
      
      // Accumulate results
      accum_scale += (1.0 - fc) * gvis;
      accum_bias += fc * gvis * (1.0 + rim_term); // Enhanced rim effect for bias term
    }
  }
  
  // Apply overall scaling to match energy conservation
  const double energy_normalization = 0.8;
  return dvec2((accum_scale / double(numsamples)) * energy_normalization, 
               (accum_bias / double(numsamples)) * energy_normalization);
}

template <size_t numsamples> inline dvec2 integrateGGXStrongRim(double n_dot_v, double roughness) {
  n_dot_v = saturate(n_dot_v);
  dvec3 v(sqrt(1.0 - n_dot_v * n_dot_v), 0, n_dot_v);
  double accum_scale = 0.0;
  double accum_bias = 0.0;
  
  // CRITICAL DIFFERENCE: Make the center of the material almost completely dark
  // This is the key to making it visually distinct from velvet
  const double center_darkness = 0.01;  // Almost no reflection in the center areas
  
  // Extreme rim parameters - far beyond velvet
  const double rim_power = 16.0;        // Much higher than velvet's power
  const double rim_intensity = 50.0;    // Extremely intense rim (5x velvet's intensity)
  const double rim_width = 0.15;        // Control width of the rim (smaller = thinner rim)
  
  // Define the rim curve to create a sharp, well-defined edge
  auto rimFalloff = [rim_power, rim_width](double ndotv) -> double {
    // This creates a much narrower, more defined rim than velvet
    double rimFactor = pow(1.0 - ndotv, rim_power);
    
    // Apply a sigmoid curve to create a sharp cutoff (not in velvet)
    // This creates a more "toon-like" rim rather than a gradual falloff
    double sharpening = 1.0 / (1.0 + exp(-(1.0 - ndotv - rim_width) * 30.0));
    
    return rimFactor * sharpening;
  };
  
  for (int i = 0; i < numsamples; i++) {
    dvec2 e = hammersley(i, numsamples);
    dvec3 h = importanceSampleGGX(e, roughness);
    double v_dot_h = v.dotWith(h);
    dvec3 l = ((h * 2.0 * v_dot_h) - v).normalized();
    double n_dot_h_sat = saturate(h.z);
    double n_dot_l_sat = saturate(l.z);
    double v_dot_h_sat = saturate(v_dot_h);
    
    if (l.z > 0.0) {
      // Calculate standard geometry term
      double gsmith = geometrySmith(dvec3(0, 0, 1), v, l, roughness);
      double standard_gvis = (gsmith * v_dot_h) / (n_dot_h_sat * n_dot_v);
      
      // DISTINCTLY DIFFERENT FROM VELVET:
      // 1. Dark center with almost no reflection in non-rim areas
      // 2. Sharp, well-defined rim rather than a gradual falloff
      
      // Apply center darkness to standard response - extreme attenuation unlike velvet
      double center_factor = center_darkness;
      
      // Calculate rim factor - using a much sharper function than velvet
      double rim_factor = rimFalloff(n_dot_v) * rim_intensity;
      
      // Create light direction dependence (optional)
      double light_factor = pow(n_dot_l_sat, 0.5); // Less dependence on light dir than velvet
      
      // Combine to create the dramatic rim effect
      double rim_contribution = rim_factor * light_factor;
      
      // Use standard Fresnel
      double fc = pow(1.0 - v_dot_h_sat, 5.0);
      
      // Combine central component (nearly black) with rim component
      // Scale component (dielectric/non-metallic)
      accum_scale += (standard_gvis * center_factor * (1.0 - fc)) + (rim_contribution * (1.0 - fc));
      
      // Bias component (metallic)
      // Create even more extreme rim effect for metals
      accum_bias += (standard_gvis * center_factor * fc) + (rim_contribution * fc * 2.0);
    }
  }
  
  // No energy normalization - we want the extreme non-physical effect
  // This deliberately breaks energy conservation for artistic effect
  return dvec2(accum_scale / double(numsamples), 
               accum_bias / double(numsamples));
}

// Helper function for Phong-like importance sampling
// Note: This function would need to be implemented
inline dvec3 importanceSamplePhong(dvec2 e, double shininess) {
  // Convert uniform random samples to a cosine power distribution
  double phi = 2.0 * 3.14159265359 * e.x;
  double cos_theta = pow(e.y, 1.0 / (shininess + 1.0));
  double sin_theta = sqrt(1.0 - cos_theta * cos_theta);
  
  // Convert to cartesian coordinates on hemisphere
  return dvec3(
    sin_theta * cos(phi),
    sin_theta * sin(phi),
    cos_theta
  );
}

template <size_t numsamples> inline dvec2 integratePhongLike(double n_dot_v, double roughness) {
  n_dot_v = saturate(n_dot_v);
  dvec3 v(sqrt(1.0 - n_dot_v * n_dot_v), 0, n_dot_v);
  double accum_scale = 0.0;
  double accum_bias  = 0.0;
  
  // Convert GGX roughness to a Phong-like shininess exponent
  // Roughness of 0 should give a very high shininess, roughness of 1 should give low shininess
  double phong_exponent = std::max(2.0, (1.0 - roughness) * 128.0);
  
  // Use a sharper, more Phong-like specular distribution
  for (int i = 0; i < numsamples; i++) {
    dvec2 e = hammersley(i, numsamples);
    
    // Instead of using GGX importance sampling, use a distribution closer to Phong
    // This is a key change to make the result more Phong-like
    dvec3 h = importanceSamplePhong(e, phong_exponent);
    
    double v_dot_h = v.dotWith(h);
    dvec3 l = ((h * 2.0 * v_dot_h) - v).normalized();
    double n_dot_h_sat = saturate(h.z);
    double n_dot_l_sat = saturate(l.z);
    double v_dot_h_sat = saturate(v_dot_h);
    
    if (l.z > 0.0) {
      // We'll need to compensate for the fact we're still using the GGX-based shader
      // Amplify the specular highlight's intensity to match Phong's sharper appearance
      double phong_factor = pow(n_dot_h_sat, phong_exponent);
      
      // Scale by the Phong normalization factor to conserve energy
      double phong_normalization = (phong_exponent + 2.0) / (2.0 * 3.14159265359);
      phong_factor *= phong_normalization;
      
      // Calculate a simple visibility term (Phong doesn't use the same geometry term as GGX)
      double visibility = 1.0 / (n_dot_v * n_dot_l_sat);
      
      // Combine to create a Phong-like response that will work with the existing shader
      double gvis = phong_factor * visibility;
      
      // Use standard Fresnel
      double fc = pow(1.0 - v_dot_h_sat, 5.0);
      
      // Accumulate results
      accum_scale += (1.0 - fc) * gvis;
      accum_bias += fc * gvis;
    }
  }
  
  // Apply scaling to ensure energy conservation
  // Phong tends to have brighter, more concentrated highlights
  const double energy_adjustment = 0.8;
  
  return dvec2(accum_scale / double(numsamples) * energy_adjustment, 
               accum_bias / double(numsamples) * energy_adjustment);
}

template <size_t numsamples> inline dvec2 integrateBlinn(double n_dot_v, double roughness) {
  n_dot_v = saturate(n_dot_v);
  dvec3 v(sqrt(1.0 - n_dot_v * n_dot_v), 0, n_dot_v);
  double accum_scale = 0.0;
  double accum_bias  = 0.0;
  
  // Convert GGX roughness to a Blinn-like exponent
  // Use a higher exponent multiplier to match GGX brightness
  double blinn_exponent = std::max(2.0, (1.0 - roughness) * 256.0); // Increased from 128 to 256
  
  for (int i = 0; i < numsamples; i++) {
    dvec2 e = hammersley(i, numsamples);
    dvec3 h = importanceSampleBlinn(e, roughness); // Use roughness for sampling
    
    double v_dot_h = v.dotWith(h);
    dvec3 l = ((h * 2.0 * v_dot_h) - v).normalized();
    double n_dot_h_sat = saturate(h.z);
    double n_dot_l_sat = saturate(l.z);
    double v_dot_h_sat = saturate(v_dot_h);
    
    if (l.z > 0.0) {
      // Use the correct roughness for the geometry term, not the exponent
      // The geometry term expects roughness in the range [0,1]
      double gsmith = geometrySmith(dvec3(0, 0, 1), v, l, roughness);
      
      // Add Blinn-specific specular term
      double blinn_specular = pow(n_dot_h_sat, blinn_exponent);
      
      // Proper normalization factor for Blinn-Phong
      double normalization = (blinn_exponent + 8.0) / (8.0 * PI);
      blinn_specular *= normalization;
      
      // Combine with geometry term - adjust weighting for energy match
      double gvis = (gsmith * v_dot_h * blinn_specular) / (n_dot_h_sat * n_dot_v);
      
      // Use standard Fresnel
      double fc = pow(1.0 - v_dot_h_sat, 5.0);
      
      // Accumulate results
      accum_scale += (1.0 - fc) * gvis;
      accum_bias += fc * gvis;
    }
  }
  
  // Apply scaling to ensure energy conservation
  // Increase this value to brighten the result
  const double energy_adjustment = 1.5; // Increased from 0.8 to 1.5
  
  return dvec2(accum_scale / double(numsamples) * energy_adjustment, 
               accum_bias / double(numsamples) * energy_adjustment);
}

} // namespace ork::brdf
