////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cvector4.h>

namespace ork::image {

struct Vec4Image {

  inline const fvec4& pixelRead(int x, int y) const {
    int addr = y * _width + x;
    OrkAssert(addr >= 0 and addr < _pixels.size());
    return _pixels[addr];
  }
  inline void pixelWrite(int x, int y, const fvec4& inp) {
    int addr = y * _width + x;
    OrkAssert(addr >= 0 and addr < _pixels.size());
    _pixels[addr] = inp;
  }
  inline const fvec4& pointSample(float u, float v) const {
    int x = roundf(u * float(_width - 1));
    int y = roundf(v * float(_height - 1));
    return pixelRead(x, y);
  }

  int _width  = 0;
  int _height = 0;
  std::vector<fvec4> _pixels;
};

} // namespace ork::image
