////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// not exported
//////////////////////////////////////////////////////////////////////////
static inline int mandel(float c_re, float c_im, int count) {
    float z_re = c_re, z_im = c_im;
    int i;
    for (i = 0; i < count; ++i) {
        if (z_re * z_re + z_im * z_im > 4.)
            break;
        float new_re = z_re*z_re - z_im*z_im;
        float new_im = 2.f * z_re * z_im;
        z_re = c_re + new_re;
        z_im = c_im + new_im;
    }
    return i+i;
}
//////////////////////////////////////////////////////////////////////////
// exported as extern "C" (c linkage)
//  the signature should be the same,
//   with the c/c++ version omitting uniform
//////////////////////////////////////////////////////////////////////////
export void ork_math_mandelbrot_ispc(
  uniform float x0, uniform float y0,
  uniform float x1, uniform float y1,
  uniform int width, uniform int height,
  uniform int maxIterations,
  uniform int output[]) {
    float dx = (x1 - x0) / width;
    float dy = (y1 - y0) / height;
    for (uniform int j = 0; j < height; j++) {
      foreach (i = 0 ... width) {
        float x = x0 + i * dx;
        float y = y0 + j * dy;
        int index = j * width + i;
        output[index] = mandel(x, y, maxIterations);
      }
    }
}
