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

namespace dflow = dataflow;
namespace trn   = terrain;

///////////////////////////////////////////////////////////////////////////////

void pyinit_gfx_terrain(py::module& module_lev2) {
  auto trn_module = module_lev2.def_submodule("terrain", "lev2 terrain heightfield compute-dataflow");

  /////////////////////////////////////////////////////////////////////////////
  // module classes — DgModuleData subclasses. The generic dflow GraphData
  // create()/connect() + inputs/outputs proxies (pyext_dataflow.cpp) handle
  // construction, edge-wiring, and float-plug assignment, so each class only
  // needs createShared + its BAKED scalar(s) exposed (octaves / op / path).
  // The expression-first DSL (ork.hypergraph.dflow.terrain) composes these.
  /////////////////////////////////////////////////////////////////////////////

  py::class_<trn::FbmModuleData, dflow::DgModuleData, trn::fbmmoduledata_ptr_t>(trn_module, "FbmModule")
      .def_static("createShared", []() -> trn::fbmmoduledata_ptr_t { return trn::FbmModuleData::createShared(); })
      .def_readwrite("octaves", &trn::FbmModuleData::_octaves); // baked loop bound

  py::class_<trn::RemapModuleData, dflow::DgModuleData, trn::remapmoduledata_ptr_t>(trn_module, "RemapModule")
      .def_static("createShared", []() -> trn::remapmoduledata_ptr_t { return trn::RemapModuleData::createShared(); });

  py::class_<trn::ConstModuleData, dflow::DgModuleData, trn::constmoduledata_ptr_t>(trn_module, "ConstModule")
      .def_static("createShared", []() -> trn::constmoduledata_ptr_t { return trn::ConstModuleData::createShared(); });

  py::class_<trn::GradientModuleData, dflow::DgModuleData, trn::gradientmoduledata_ptr_t>(trn_module, "GradientModule")
      .def_static("createShared", []() -> trn::gradientmoduledata_ptr_t { return trn::GradientModuleData::createShared(); });

  py::class_<trn::CombineModuleData, dflow::DgModuleData, trn::combinemoduledata_ptr_t>(trn_module, "CombineModule")
      .def_static("createShared", []() -> trn::combinemoduledata_ptr_t { return trn::CombineModuleData::createShared(); })
      .def_readwrite("op", &trn::CombineModuleData::_op); // baked: 0=add 1=sub 2=mul 3=min 4=max 5=mix

  py::class_<trn::TerraceModuleData, dflow::DgModuleData, trn::terracemoduledata_ptr_t>(trn_module, "TerraceModule")
      .def_static("createShared", []() -> trn::terracemoduledata_ptr_t { return trn::TerraceModuleData::createShared(); });

  py::class_<trn::SlopeModuleData, dflow::DgModuleData, trn::slopemoduledata_ptr_t>(trn_module, "SlopeModule")
      .def_static("createShared", []() -> trn::slopemoduledata_ptr_t { return trn::SlopeModuleData::createShared(); })
      .def_readwrite("radius_m", &trn::SlopeModuleData::_radius_m); // baked: pre-blur / scale (meters)

  py::class_<trn::CurvatureModuleData, dflow::DgModuleData, trn::curvaturemoduledata_ptr_t>(trn_module, "CurvatureModule")
      .def_static("createShared", []() -> trn::curvaturemoduledata_ptr_t { return trn::CurvatureModuleData::createShared(); })
      .def_readwrite("mode", &trn::CurvatureModuleData::_mode)          // baked: 0=convex 1=concave 2=magnitude
      .def_readwrite("radius_m", &trn::CurvatureModuleData::_radius_m); // baked: pre-blur / scale (meters)

  py::class_<trn::MaskBlendModuleData, dflow::DgModuleData, trn::maskblendmoduledata_ptr_t>(trn_module, "MaskBlendModule")
      .def_static("createShared", []() -> trn::maskblendmoduledata_ptr_t { return trn::MaskBlendModuleData::createShared(); });

  py::class_<trn::CaptureModuleData, dflow::DgModuleData, trn::capturemoduledata_ptr_t>(trn_module, "CaptureModule")
      .def_static("createShared", []() -> trn::capturemoduledata_ptr_t { return trn::CaptureModuleData::createShared(); })
      // bake-time output path (wrapper-supplied, deliberately NOT serialized — a
      // stable channel name will carry identity at the asset layer later).
      .def_property(
          "path",
          [](trn::capturemoduledata_ptr_t m) -> std::string { return std::string(m->_path.c_str()); },
          [](trn::capturemoduledata_ptr_t m, std::string p) { m->_path = ork::file::Path(p.c_str()); })
      // stable channel identity (serialized) — the asset wrapper derives the
      // machine-specific path from it at materialize time.
      .def_property(
          "channel",
          [](trn::capturemoduledata_ptr_t m) -> std::string { return m->_channel; },
          [](trn::capturemoduledata_ptr_t m, std::string c) { m->_channel = c; });

  /////////////////////////////////////////////////////////////////////////////
  // FieldStats — per-capture min/max/mean returned by the bake driver.
  /////////////////////////////////////////////////////////////////////////////
  py::class_<trn::FieldStats, trn::fieldstats_ptr_t>(trn_module, "FieldStats")
      .def_readonly("min", &trn::FieldStats::_min)
      .def_readonly("max", &trn::FieldStats::_max)
      .def_readonly("mean", &trn::FieldStats::_mean)
      .def("__repr__", [](trn::fieldstats_ptr_t s) -> std::string {
        return FormatString("FieldStats(min=%g max=%g mean=%g)", s->_min, s->_max, s->_mean);
      });

  /////////////////////////////////////////////////////////////////////////////
  // bake driver — sort, instantiate, dispatch the compute, flush captures to
  // EXR/PNG (by extension). Returns the per-capture FieldStats list.
  /////////////////////////////////////////////////////////////////////////////
  trn_module.def(
      "bake_heightfield",
      [](dflow::graphdata_ptr_t g, ctx_t ctx, int dim, float extent_m, float height_scale_m)
          -> std::vector<trn::fieldstats_ptr_t> {
        return trn::bakeHeightfield(g, ctx.get(), dim, extent_m, height_scale_m);
      },
      py::arg("graph"), py::arg("ctx"), py::arg("dim"),
      py::arg("extent_m") = 4096.0f, py::arg("height_scale_m") = 9830.25f);

  // enumerate a graph's CaptureModule sinks (each carries .channel + .path) so the
  // asset wrapper can derive per-channel output paths on a deserialized graph.
  trn_module.def("capture_modules", [](dflow::graphdata_ptr_t g) -> std::vector<trn::capturemoduledata_ptr_t> {
    std::vector<trn::capturemoduledata_ptr_t> out;
    for (size_t i = 0; i < g->numModules(); i++) {
      if (auto cap = std::dynamic_pointer_cast<trn::CaptureModuleData>(g->module(i)))
        out.push_back(cap);
    }
    return out;
  });

  /////////////////////////////////////////////////////////////////////////////
  // legacy/test entry points (kept on module_lev2 for the existing llgfx tests)
  /////////////////////////////////////////////////////////////////////////////
  module_lev2.def("terrain_bake_test", [](ctx_t ctx, std::string path, int dim) {
    terrain::bakeHeightfieldTest(ctx.get(), ork::file::Path(path.c_str()), dim);
  });
  module_lev2.def("terrain_ops_selftest", [](ctx_t ctx, int dim) -> int {
    return terrain::terrainOpsSelfTest(ctx.get(), dim);
  });
  module_lev2.def("terrain_roundtrip_test", [](ctx_t ctx, int dim) -> int {
    return terrain::terrainRoundTripTest(ctx.get(), dim);
  });
  // per-node cook-cache gate: cold+warm bake, warm must hit the DataBlockCache.
  module_lev2.def("terrain_cache_test", [](ctx_t ctx, int dim) -> int {
    return terrain::terrainCacheTest(ctx.get(), dim);
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
