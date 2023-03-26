////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cvector3.h>
#include <ork/math/cmatrix3.h>

namespace ork::color {

///////////////////////////////////////////////////////////////////////////////

fvec3 cieXyz_to_D65AdobeRGB(fvec3 xyx) {
  fmtx3 xyz2adobergb;
  xyz2adobergb.setRow(0, fvec3(2.0413690, -0.5649464, -0.3446944));
  xyz2adobergb.setRow(1, fvec3(-0.9692660, 1.8760108, 0.0415560));
  xyz2adobergb.setRow(2, fvec3(0.0134474, -0.1183897, 1.0154096));
  return xyx.Transform(xyz2adobergb);
}

///////////////////////////////////////////////////////////////////////////////

fvec3 cieXyz_to_D50AdobeRGB(fvec3 xyx) {
  fmtx3 xyz2adobergb;
  xyz2adobergb.setRow(0, fvec3(1.9624274, -0.6105343, -0.3413404));
  xyz2adobergb.setRow(1, fvec3(-0.9787684, 1.9161415, 0.0334540));
  xyz2adobergb.setRow(2, fvec3(0.0286869, -0.1406752, 1.3487655));
  return xyx.Transform(xyz2adobergb);
}

///////////////////////////////////////////////////////////////////////////////

fvec3 cieXyz_to_D65sRGB(fvec3 xyx) {
  fmtx3 xyz2srgb;
  xyz2srgb.setRow(0, fvec3(3.2404542, -1.5371385, -0.4985314));
  xyz2srgb.setRow(1, fvec3(-0.9692660, 1.8760108, 0.0415560));
  xyz2srgb.setRow(2, fvec3(0.0556434, -0.2040259, 1.0572252));
  return xyx.Transform(xyz2srgb);
}

///////////////////////////////////////////////////////////////////////////////

fvec3 cieXyz_to_D50sRGB(fvec3 xyx) {
  fmtx3 xyz2srgb;
  xyz2srgb.setRow(0, fvec3(3.1338561, -1.6168667, -0.4906146));
  xyz2srgb.setRow(1, fvec3(-0.9787684, 1.9161415, 0.0334540));
  xyz2srgb.setRow(2, fvec3(0.0719453, -0.2289914, 1.4052427));
  return xyx.Transform(xyz2srgb);
}

///////////////////////////////////////////////////////////////////////////////

fvec3 sRgb_to_linear(fvec3 sRGB, float gamma) {
  bool ltr     = sRGB.x < 0.04045f;
  bool ltg     = sRGB.y < 0.04045f;
  bool ltb     = sRGB.z < 0.04045f;
  fvec3 select = fvec3(ltr, ltg, ltb);
  fvec3 higher;
  higher.x    = powf((sRGB.x + 0.055f) / 1.055f, gamma);
  higher.y    = powf((sRGB.y + 0.055f) / 1.055f, gamma);
  higher.z    = powf((sRGB.z + 0.055f) / 1.055f, gamma);
  fvec3 lower = sRGB * 1.0f / 12.92;
  fvec3 rval;
  rval.x = lower.x * select.x + higher.x * (1.0f - select.x);
  rval.y = lower.y * select.y + higher.y * (1.0f - select.y);
  rval.z = lower.z * select.z + higher.z * (1.0f - select.z);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

fvec3 linear_to_sRGB(fvec3 linear, float gamma) {
  bool ltr     = linear.x < 0.0031308f;
  bool ltg     = linear.y < 0.0031308f;
  bool ltb     = linear.z < 0.0031308f;
  fvec3 select = fvec3(ltr, ltg, ltb);
  fvec3 higher;
  higher.x    = 1.055f * powf(linear.x, 1.0f / gamma);
  higher.y    = 1.055f * powf(linear.y, 1.0f / gamma);
  higher.z    = 1.055f * powf(linear.z, 1.0f / gamma);
  fvec3 lower = linear * 12.92f;
  fvec3 rval;
  rval.x = lower.x * select.x + higher.x * (1.0f - select.x);
  rval.y = lower.y * select.y + higher.y * (1.0f - select.y);
  rval.z = lower.z * select.z + higher.z * (1.0f - select.z);
  return rval;
}

} // namespace ork::color
