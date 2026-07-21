////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
//
// dflow_gpuupdate.h — the FAMILY-NEUTRAL gpuUpdate seam.
//
// A dflow family (hypermesh / terrain) STAMPS a GpuUpdateStamp onto the durable
// GraphData::_impl (TypeKeyedVars) at first bake, recording which family produced the
// graph and the family params the bake ran with. The generic lev2.dflow.gpuUpdate() entry
// resolves the stamp off the graph and re-dispatches the SAME family bake — re-using the
// stored params (optional kwargs may override) so an already-baked graph re-evaluates with
// one family-neutral call. On a cacheable graph the re-bake serves every unchanged node from
// the WARM disk cache (no spurious stores) and reproduces byte-identical output.
//
// This header carries NO GPU / pyext types — only the plain param record — so it can live
// beside the family bake functions (which stamp it) and the pyext seam (which reads it).
//
////////////////////////////////////////////////////////////////
#pragma once

#include <memory>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

enum class GraphFamily {
  HYPERMESH, // hm::bakeMesh(graph, ctx, vtx_budget)
  TERRAIN,   // trn::bakeHeightfield(graph, ctx, dim, extent_m)
};

// the bake-time param record a family leaves on GraphData::_impl. gpuUpdate() reads it back,
// optionally overriding individual params, and re-invokes the family bake.
struct GpuUpdateStamp {
  GraphFamily _family = GraphFamily::HYPERMESH;
  int   _vtx_budget = (1 << 20); // hypermesh
  int   _dim        = 0;         // terrain field resolution
  float _extent_m   = 4096.0f;   // terrain world extent (meters)
};

using gpuupdate_stamp_ptr_t = std::shared_ptr<GpuUpdateStamp>;

} // namespace ork::lev2
