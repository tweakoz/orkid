////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

void pyinit_gfx_terrain(py::module& module_lev2) {
  // first-slice exerciser: build the fbm -> capture heightfield bake graph and
  // write `path` (EXR/PNG by extension). dim = square grid resolution.
  module_lev2.def("terrain_bake_test", [](ctx_t ctx, std::string path, int dim) {
    terrain::bakeHeightfieldTest(ctx.get(), ork::file::Path(path.c_str()), dim);
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
