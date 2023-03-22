////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <pybind11/numpy.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_gfx_buffers(py::module& module_lev2) {

  /////////////////////////////////////////////////////////////////////////////////

  PYBIND11_NUMPY_DTYPE(VtxV12C4, x, y, z, color);
  PYBIND11_NUMPY_DTYPE(_VtxV12T8, x, y, z, u, v);

}
} // namespace ork::lev2 {