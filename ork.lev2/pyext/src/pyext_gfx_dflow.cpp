////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
// Python bindings for the FAMILY-NEUTRAL dflow bake seam. Today: gpuUpdate() — one call
// re-evaluates an already-baked graph, resolving the GpuUpdateStamp the family bake left on
// the durable GraphData and re-dispatching that family's bake with the stored params (optional
// kwargs override). Deliberately family-neutral: this one TU knows BOTH families so the
// individual family modules never have to import each other.
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/dflow_gpuupdate.h>
#include <ork/lev2/gfx/hypermesh/hmdflow.h>            // hm::bakeMesh
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h>        // trn::bakeHeightfield
#include <stdexcept>

namespace ork::lev2 {

namespace dflow = dataflow;
namespace hm    = hypermesh;
namespace trn   = terrain;

void pyinit_gfx_dflow(py::module& module_lev2) {
  auto dflowmod = module_lev2.def_submodule("dflow", "family-neutral dataflow bake seam");

  /////////////////////////////////////////////////////////////////////////////
  // gpuUpdate(graph, ctx, **overrides) — re-evaluate an already-baked graph.
  //
  // The family bake (hypermesh materialize / terrain bake_heightfield) stamped its
  // identity + params onto graph._impl. We resolve that stamp and re-dispatch the SAME
  // family bake, re-using the stored params unless an override kwarg supplies a new one:
  //   hypermesh: vtx_budget          -> returns the re-baked GpuMesh
  //   terrain:   dim, extent_m       -> returns the re-baked [FieldStats] (files re-emitted)
  // On a cacheable graph the re-bake serves every unchanged node from the WARM disk cache
  // (no spurious stores) and reproduces byte-identical output. A graph that was never baked
  // fails LOUDLY — there is no stamp to dispatch.
  /////////////////////////////////////////////////////////////////////////////
  dflowmod.def(
      "gpuUpdate",
      [](dflow::graphdata_ptr_t graph, ctx_t ctx, py::kwargs kw) -> py::object {
        auto stamp = graph->_impl.getShared<GpuUpdateStamp>();
        if (not stamp)
          throw std::runtime_error(
              "dflow.gpuUpdate: graph has no gpuUpdate stamp — bake it once first "
              "(hypermesh materialize / terrain bake_heightfield)");
        switch (stamp->_family) {
          case GraphFamily::HYPERMESH: {
            int vtx_budget = stamp->_vtx_budget;
            if (kw.contains("vtx_budget"))
              vtx_budget = kw["vtx_budget"].cast<int>();
            return py::cast(hm::bakeMesh(graph, ctx.get(), vtx_budget));
          }
          case GraphFamily::TERRAIN: {
            int dim        = stamp->_dim;
            float extent_m = stamp->_extent_m;
            if (kw.contains("dim"))
              dim = kw["dim"].cast<int>();
            if (kw.contains("extent_m"))
              extent_m = kw["extent_m"].cast<float>();
            return py::cast(trn::bakeHeightfield(graph, ctx.get(), dim, extent_m));
          }
        }
        throw std::runtime_error("dflow.gpuUpdate: unknown graph family");
      },
      py::arg("graph"),
      py::arg("ctx"));
}

} // namespace ork::lev2
