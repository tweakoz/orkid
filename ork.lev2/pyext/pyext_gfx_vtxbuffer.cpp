////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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