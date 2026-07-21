////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/terrain/dflow/hfdflow.h>
#include <ork/lev2/gfx/terrain/terrain_chunk_drawable.h> // #88 v2 — in-place display plane rebind
#include <ork/lev2/gfx/terrain/dflow/hfdflow_scatter.h> // E.2 — the C++ scatter placer
#include <ork/lev2/gfx/hypermesh/hmdflow.h>
#include <ork/lev2/gfx/asset_gen.h>                     // ScatterSinkData (the reflected placement contract)
#include <ork/lev2/gfx/meshutil/geometry.h>

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

  // NoiseModule — basis-selectable noise generator (perlin/simplex/worley/voronoi).
  py::class_<trn::NoiseModuleData, dflow::DgModuleData, trn::noisemoduledata_ptr_t>(trn_module, "NoiseModule")
      .def_static("createShared", []() -> trn::noisemoduledata_ptr_t { return trn::NoiseModuleData::createShared(); })
      // _basis is a reflected enum in C++; the DSL layer (ops.py) still trades int
      // CODES, so expose it int-backed (0 perlin/1 simplex/2 worley/3 voronoi).
      .def_property("basis",
          [](const trn::NoiseModuleData& m) -> int { return int(m._basis); },
          [](trn::NoiseModuleData& m, int v) { m._basis = trn::NoiseBasis(v); })
      .def_readwrite("octaves", &trn::NoiseModuleData::_octaves);  // baked loop bound (1 = primitive)

  // ExprModule — generic expression generator (the unified procedural substrate);
  // `shadertext` is the full compute text emitted by the ptex3d codegen; `expr_source`
  // is the authored expression string (editor T.expr) the DSL recompiles shadertext from;
  // `expr_tree` is the canonical ExprIR JSON (E2.5) the writer re-renders DSL from and the
  // cook hash keys off (the storage-form identity).
  py::class_<trn::ExprModuleData, dflow::DgModuleData, trn::exprmoduledata_ptr_t>(trn_module, "ExprModule")
      .def_static("createShared", []() -> trn::exprmoduledata_ptr_t { return trn::ExprModuleData::createShared(); })
      .def_readwrite("shadertext", &trn::ExprModuleData::_shadertext)
      .def_readwrite("expr_source", &trn::ExprModuleData::_expr_source)
      .def_readwrite("expr_tree", &trn::ExprModuleData::_expr_tree);

  // NormalizeModule — explicit [min,max]->[out_lo,out_hi] rescale (float plugs).
  py::class_<trn::NormalizeModuleData, dflow::DgModuleData, trn::normalizemoduledata_ptr_t>(trn_module, "NormalizeModule")
      .def_static("createShared", []() -> trn::normalizemoduledata_ptr_t { return trn::NormalizeModuleData::createShared(); });

  py::class_<trn::RemapModuleData, dflow::DgModuleData, trn::remapmoduledata_ptr_t>(trn_module, "RemapModule")
      .def_static("createShared", []() -> trn::remapmoduledata_ptr_t { return trn::RemapModuleData::createShared(); });

  py::class_<trn::ConstModuleData, dflow::DgModuleData, trn::constmoduledata_ptr_t>(trn_module, "ConstModule")
      .def_static("createShared", []() -> trn::constmoduledata_ptr_t { return trn::ConstModuleData::createShared(); });

  py::class_<trn::GradientModuleData, dflow::DgModuleData, trn::gradientmoduledata_ptr_t>(trn_module, "GradientModule")
      .def_static("createShared", []() -> trn::gradientmoduledata_ptr_t { return trn::GradientModuleData::createShared(); });

  py::class_<trn::CombineModuleData, dflow::DgModuleData, trn::combinemoduledata_ptr_t>(trn_module, "CombineModule")
      .def_static("createShared", []() -> trn::combinemoduledata_ptr_t { return trn::CombineModuleData::createShared(); })
      // _op is a reflected enum in C++; expose it int-backed for the DSL (ops.py trades
      // int CODES): 0=add 1=sub 2=mul 3=min 4=max 5=mix.
      .def_property("op",
          [](const trn::CombineModuleData& m) -> int { return int(m._op); },
          [](trn::CombineModuleData& m, int v) { m._op = trn::CombineOp(v); });

  py::class_<trn::TerraceModuleData, dflow::DgModuleData, trn::terracemoduledata_ptr_t>(trn_module, "TerraceModule")
      .def_static("createShared", []() -> trn::terracemoduledata_ptr_t { return trn::TerraceModuleData::createShared(); });

  py::class_<trn::SlopeModuleData, dflow::DgModuleData, trn::slopemoduledata_ptr_t>(trn_module, "SlopeModule")
      .def_static("createShared", []() -> trn::slopemoduledata_ptr_t { return trn::SlopeModuleData::createShared(); })
      .def_readwrite("radius_m", &trn::SlopeModuleData::_radius_m); // baked: pre-blur / scale (meters)

  py::class_<trn::CurvatureModuleData, dflow::DgModuleData, trn::curvaturemoduledata_ptr_t>(trn_module, "CurvatureModule")
      .def_static("createShared", []() -> trn::curvaturemoduledata_ptr_t { return trn::CurvatureModuleData::createShared(); })
      // _mode is a reflected enum in C++; expose it int-backed for the DSL (ops.py trades
      // int CODES): 0=convex 1=concave 2=magnitude.
      .def_property("mode",
          [](const trn::CurvatureModuleData& m) -> int { return int(m._mode); },
          [](trn::CurvatureModuleData& m, int v) { m._mode = trn::CurvatureMode(v); })
      .def_readwrite("radius_m", &trn::CurvatureModuleData::_radius_m); // baked: pre-blur / scale (meters)

  py::class_<trn::RelaxUvModuleData, dflow::DgModuleData, trn::relaxuvmoduledata_ptr_t>(trn_module, "RelaxUvModule")
      .def_static("createShared", []() -> trn::relaxuvmoduledata_ptr_t { return trn::RelaxUvModuleData::createShared(); })
      .def_readwrite("strength", &trn::RelaxUvModuleData::_strength)     // warp gain (0 = planar)
      .def_readwrite("iterations", &trn::RelaxUvModuleData::_iterations); // Poisson Jacobi sweeps (0 = auto)

  py::class_<trn::MaskBlendModuleData, dflow::DgModuleData, trn::maskblendmoduledata_ptr_t>(trn_module, "MaskBlendModule")
      .def_static("createShared", []() -> trn::maskblendmoduledata_ptr_t { return trn::MaskBlendModuleData::createShared(); });

  py::class_<trn::ThermalErodeModuleData, dflow::DgModuleData, trn::thermalerodemoduledata_ptr_t>(trn_module, "ThermalErodeModule")
      .def_static("createShared", []() -> trn::thermalerodemoduledata_ptr_t { return trn::ThermalErodeModuleData::createShared(); })
      .def_readwrite("iterations", &trn::ThermalErodeModuleData::_iterations); // baked step count

  // EroxModule — PHYSICAL Mei hydraulic erosion. NO baked scalar (iterations are
  // derived per-bake from sim_time_s + CFL); every tunable is a meters/seconds float
  // plug set generically via m.inputs.<name>.
  py::class_<trn::EroxModuleData, dflow::DgModuleData, trn::eroxmoduledata_ptr_t>(trn_module, "EroxModule")
      .def_static("createShared", []() -> trn::eroxmoduledata_ptr_t { return trn::EroxModuleData::createShared(); });

  // PhaModule — procedural phacelle erosion FILTER (single-pass, RI, speckle-free).
  py::class_<trn::PhaModuleData, dflow::DgModuleData, trn::phamoduledata_ptr_t>(trn_module, "PhaModule")
      .def_static("createShared", []() -> trn::phamoduledata_ptr_t { return trn::PhaModuleData::createShared(); })
      .def_readwrite("octaves", &trn::PhaModuleData::_octaves);

  // LpfModule — separable gaussian low-pass. ONE `cutoff` plug (wavelength); `cutoff_units`
  // reflected enum (int-backed for the DSL: 0=texels 1=meters) selects the interpretation.
  py::class_<trn::LpfModuleData, dflow::DgModuleData, trn::lpfmoduledata_ptr_t>(trn_module, "LpfModule")
      .def_static("createShared", []() -> trn::lpfmoduledata_ptr_t { return trn::LpfModuleData::createShared(); })
      .def_property("cutoff_units",
          [](const trn::LpfModuleData& m) -> int { return int(m._cutoff_units); },
          [](trn::LpfModuleData& m, int v) { m._cutoff_units = trn::CutoffUnits(v); });

  // BasinFillModule — depression/pit fill (priority-flood, CPU). Fills basins to spill level.
  py::class_<trn::BasinFillModuleData, dflow::DgModuleData, trn::basinfillmoduledata_ptr_t>(trn_module, "BasinFillModule")
      .def_static("createShared", []() -> trn::basinfillmoduledata_ptr_t { return trn::BasinFillModuleData::createShared(); });

  // Flow3DModule — continuous flow field: RGBA dir/slope ("Out") + mono discharge ("Discharge").
  py::class_<trn::Flow3DModuleData, dflow::DgModuleData, trn::flow3dmoduledata_ptr_t>(trn_module, "Flow3DModule")
      .def_static("createShared", []() -> trn::flow3dmoduledata_ptr_t { return trn::Flow3DModuleData::createShared(); })
      .def_readwrite("exponent", &trn::Flow3DModuleData::_exponent)
      .def_readwrite("iterations", &trn::Flow3DModuleData::_iterations)
      .def_readwrite("log_compress", &trn::Flow3DModuleData::_log_compress)
      .def_readwrite("slope_scale", &trn::Flow3DModuleData::_slope_scale)
      .def_readwrite("flat_scale", &trn::Flow3DModuleData::_flat_scale)
      .def_readwrite("curv_scale", &trn::Flow3DModuleData::_curv_scale)
      .def_readwrite("twi_scale", &trn::Flow3DModuleData::_twi_scale);

  // FlowErodeModule — continuous flow-map erosion+deposition step (In=z, Discharge=A).
  py::class_<trn::FlowErodeModuleData, dflow::DgModuleData, trn::flowerodemoduledata_ptr_t>(trn_module, "FlowErodeModule")
      .def_static("createShared", []() -> trn::flowerodemoduledata_ptr_t { return trn::FlowErodeModuleData::createShared(); })
      .def_readwrite("niter", &trn::FlowErodeModuleData::_niter)
      .def_readwrite("dt", &trn::FlowErodeModuleData::_dt)
      .def_readwrite("k_erode", &trn::FlowErodeModuleData::_k_erode)
      .def_readwrite("k_deposit", &trn::FlowErodeModuleData::_k_deposit)
      .def_readwrite("m", &trn::FlowErodeModuleData::_m)
      .def_readwrite("n", &trn::FlowErodeModuleData::_n)
      .def_readwrite("dep_m", &trn::FlowErodeModuleData::_dep_m)
      .def_readwrite("flat_k", &trn::FlowErodeModuleData::_flat_k)
      .def_readwrite("clamp_frac", &trn::FlowErodeModuleData::_clamp_frac)
      .def_readwrite("blend", &trn::FlowErodeModuleData::_blend)
      .def_readwrite("disch_log", &trn::FlowErodeModuleData::_disch_log);

  // FillClosedBasinsModule — detect+fill closed basins w/ persistence control (min_depth is a plug).
  py::class_<trn::FillClosedBasinsModuleData, dflow::DgModuleData, trn::fillclosedbasinsmoduledata_ptr_t>(trn_module, "FillClosedBasinsModule")
      .def_static("createShared", []() -> trn::fillclosedbasinsmoduledata_ptr_t { return trn::FillClosedBasinsModuleData::createShared(); });

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
          [](trn::capturemoduledata_ptr_t m, std::string c) { m->_channel = c; })
      // S4 progressive display: "on_complete" (default) / "on_checkpoint" — the
      // editor's sliced re-bake sets on_checkpoint per session so the cook publishes
      // the completed viewable-node height plane at every checkpoint (hfdflow.h).
      .def_property(
          "visual_update_mode",
          [](trn::capturemoduledata_ptr_t m) -> std::string { return m->_visual_update_mode; },
          [](trn::capturemoduledata_ptr_t m, std::string v) { m->_visual_update_mode = v; })
      // per-bake cook-cache opt-out — self.capture(..., cache=False) sets this; any
      // capture with cache=False disables the disk cook cache for the whole bake.
      .def_property(
          "cache",
          [](trn::capturemoduledata_ptr_t m) -> bool { return m->_cache; },
          [](trn::capturemoduledata_ptr_t m, bool c) { m->_cache = c; });

  /////////////////////////////////////////////////////////////////////////////
  // LoopIterFeed — READ-ONLY inspection of one per-iteration index feed (the doc
  // layer's L.i). `values` is the per-iteration TABLE (empty => affine scale/bias).
  // Used by the V-A oracle to assert the feed a loop carries without unrolling.
  /////////////////////////////////////////////////////////////////////////////
  py::class_<dflow::LoopIterFeed, dflow::loopiterfeed_ptr_t>(trn_module, "LoopIterFeed")
      .def_property_readonly("inner_module", [](dflow::loopiterfeed_ptr_t f) -> std::string { return f->_inner_module; })
      .def_property_readonly("inner_plug", [](dflow::loopiterfeed_ptr_t f) -> std::string { return f->_inner_plug; })
      .def_property_readonly("scale", [](dflow::loopiterfeed_ptr_t f) -> float { return f->_scale; })
      .def_property_readonly("bias", [](dflow::loopiterfeed_ptr_t f) -> float { return f->_bias; })
      .def_property_readonly("values", [](dflow::loopiterfeed_ptr_t f) -> std::vector<float> { return f->_values; });

  // nested (subgraph/loop) cook counters from the last bake — (computes, loads).
  // The g2 loop-cache oracle reads these: the per-iteration recompute/load counts
  // live here, NOT on the host [cook] line (which only sees the loop NODE).
  trn_module.def("last_subgraph_cook_counts", []() -> py::tuple {
    auto p = trn::lastSubgraphCookCounts();
    return py::make_tuple(p.first, p.second);
  });

  /////////////////////////////////////////////////////////////////////////////
  // Composite (subgraph / loop) modules — a group / loop that OWNS a nested graph
  // (the Houdini subnet model). The nested body, the NAMED boundary promotions
  // (addressable by name) and (loops) the count + carries round-trip with the graph.
  // Bypass / output-marker / serialization fall out of the ONE module code path.
  /////////////////////////////////////////////////////////////////////////////
  py::class_<trn::TerrainSubGraphModuleData, dflow::DgModuleData, trn::terrainsubgraphmoduledata_ptr_t>(
      trn_module, "SubGraphModule")
      .def_static("createShared", []() -> trn::terrainsubgraphmoduledata_ptr_t {
        return trn::TerrainSubGraphModuleData::createShared();
      })
      .def_property_readonly(
          "subgraph", [](trn::terrainsubgraphmoduledata_ptr_t m) -> dflow::graphdata_ptr_t { return m->_subgraph; })
      .def(
          "promoteInput",
          [](trn::terrainsubgraphmoduledata_ptr_t m, std::string outer, std::string inner_module, std::string inner_plug) {
            auto p           = std::make_shared<dflow::SubGraphPromotion>();
            p->_outer        = outer;
            p->_inner_module = inner_module;
            p->_inner_plug   = inner_plug;
            m->_promoted_inputs.push_back(p);
          })
      .def(
          "promoteOutput",
          [](trn::terrainsubgraphmoduledata_ptr_t m, std::string outer, std::string inner_module, std::string inner_plug) {
            auto p           = std::make_shared<dflow::SubGraphPromotion>();
            p->_outer        = outer;
            p->_inner_module = inner_module;
            p->_inner_plug   = inner_plug;
            m->_promoted_outputs.push_back(p);
          })
      // (re)build the boundary plugs from the promotion tables — call after promoting.
      .def("reshape", [](trn::terrainsubgraphmoduledata_ptr_t m) {
        auto clazz = m->GetClass();
        if (auto r = clazz->annotationTyped<dflow::moduleIOreshape_fn_t>("reshapeIOs"))
          r.value()(m);
      });

  py::class_<trn::TerrainLoopModuleData, dflow::DgModuleData, trn::terrainloopmoduledata_ptr_t>(
      trn_module, "LoopModule")
      .def_static("createShared", []() -> trn::terrainloopmoduledata_ptr_t {
        return trn::TerrainLoopModuleData::createShared();
      })
      .def_property_readonly(
          "subgraph", [](trn::terrainloopmoduledata_ptr_t m) -> dflow::graphdata_ptr_t { return m->_subgraph; })
      // READ-ONLY view of the per-iteration index feeds (the doc layer's L.i, as
      // affine or value-table). The V-A oracle asserts the feed a loop carries.
      .def_property_readonly(
          "iter_feeds",
          [](trn::terrainloopmoduledata_ptr_t m) -> std::vector<dflow::loopiterfeed_ptr_t> { return m->_iter_feeds; })
      .def_property(
          "count", //
          [](trn::terrainloopmoduledata_ptr_t m) -> int { return m->_count; },
          [](trn::terrainloopmoduledata_ptr_t m, int c) { m->_count = c; })
      .def(
          "promoteInput",
          [](trn::terrainloopmoduledata_ptr_t m, std::string outer, std::string inner_module, std::string inner_plug) {
            auto p           = std::make_shared<dflow::SubGraphPromotion>();
            p->_outer        = outer;
            p->_inner_module = inner_module;
            p->_inner_plug   = inner_plug;
            m->_promoted_inputs.push_back(p);
          })
      .def(
          "promoteOutput",
          [](trn::terrainloopmoduledata_ptr_t m, std::string outer, std::string inner_module, std::string inner_plug) {
            auto p           = std::make_shared<dflow::SubGraphPromotion>();
            p->_outer        = outer;
            p->_inner_module = inner_module;
            p->_inner_plug   = inner_plug;
            m->_promoted_outputs.push_back(p);
          })
      .def(
          "addCarry",
          [](trn::terrainloopmoduledata_ptr_t m, std::string name, std::string promoted_input, std::string promoted_output) {
            auto c              = std::make_shared<dflow::LoopCarry>();
            c->_name            = name;
            c->_promoted_input  = promoted_input;
            c->_promoted_output = promoted_output;
            m->_carries.push_back(c);
          })
      .def(
          "addIterFeed",
          [](trn::terrainloopmoduledata_ptr_t m, std::string inner_module, std::string inner_plug, float scale, float bias) {
            auto f           = std::make_shared<dflow::LoopIterFeed>();
            f->_inner_module = inner_module;
            f->_inner_plug   = inner_plug;
            f->_scale        = scale;
            f->_bias         = bias;
            m->_iter_feeds.push_back(f);
          },
          py::arg("inner_module"), py::arg("inner_plug"), py::arg("scale") = 1.0f, py::arg("bias") = 0.0f)
      .def(
          "addIterFeedTable",
          // per-iteration VALUE TABLE (WINS over affine): the doc layer fills `values` by
          // evaluating an arbitrary (e.g. quadratic) L.i expression once per iteration, so a
          // non-affine L.i param survives without unrolling. Re-elaborate regenerates it.
          [](trn::terrainloopmoduledata_ptr_t m, std::string inner_module, std::string inner_plug,
             std::vector<float> values) {
            auto f           = std::make_shared<dflow::LoopIterFeed>();
            f->_inner_module = inner_module;
            f->_inner_plug   = inner_plug;
            f->_values       = values;
            m->_iter_feeds.push_back(f);
          },
          py::arg("inner_module"), py::arg("inner_plug"), py::arg("values"))
      .def("reshape", [](trn::terrainloopmoduledata_ptr_t m) {
        auto clazz = m->GetClass();
        if (auto r = clazz->annotationTyped<dflow::moduleIOreshape_fn_t>("reshapeIOs"))
          r.value()(m);
      });

  /////////////////////////////////////////////////////////////////////////////
  // FieldStats — per-capture min/max/mean returned by the bake driver.
  /////////////////////////////////////////////////////////////////////////////
  py::class_<trn::FieldStats, trn::fieldstats_ptr_t>(trn_module, "FieldStats")
      .def_readonly("min", &trn::FieldStats::_min)
      .def_readonly("max", &trn::FieldStats::_max)
      .def_readonly("mean", &trn::FieldStats::_mean)
      .def("__repr__", [](trn::fieldstats_ptr_t s) -> std::string {
        return FormatString("FieldStats(ch=%s min=%g max=%g mean=%g)", s->_channel.c_str(), s->_min, s->_max, s->_mean);
      })
          .def_property_readonly("channel", [](trn::fieldstats_ptr_t s) -> std::string { return s->_channel; });

  /////////////////////////////////////////////////////////////////////////////
  // bake driver — sort, instantiate, dispatch the compute, flush captures to
  // EXR/PNG (by extension). Returns the per-capture FieldStats list.
  /////////////////////////////////////////////////////////////////////////////
  trn_module.def(
      "bake_heightfield",
      [](dflow::graphdata_ptr_t g, ctx_t ctx, int dim, float extent_m)
          -> std::vector<trn::fieldstats_ptr_t> {
        return trn::bakeHeightfield(g, ctx.get(), dim, extent_m);
      },
      py::arg("graph"), py::arg("ctx"), py::arg("dim"),
      py::arg("extent_m") = 4096.0f);

  /////////////////////////////////////////////////////////////////////////////
  // MT3 — SLICED (anti-hitch) re-bake handle. begin_sliced_bake enqueues a
  // SOFT_DEADLINE microtask on ctx's scheduler that drives the SAME cook one topo
  // node per slice; the caller pre-sets each capture .path (as bake_heightfield's
  // callers do) and polls .done across frames, or pumpToCompletion() headless.
  /////////////////////////////////////////////////////////////////////////////
  py::class_<trn::SlicedBakeHandle, trn::slicedbake_handle_ptr_t>(trn_module, "SlicedBakeHandle")
      .def_property_readonly("done", [](trn::slicedbake_handle_ptr_t h) -> bool { return h->done(); })
      .def_property_readonly("progress", [](trn::slicedbake_handle_ptr_t h) -> float { return h->progress(); })
      .def_property_readonly("frames_driven", [](trn::slicedbake_handle_ptr_t h) -> uint64_t { return h->framesDriven(); })
      .def("pumpToCompletion", [](trn::slicedbake_handle_ptr_t h, ctx_t ctx) { h->pumpToCompletion(ctx.get()); },
           py::arg("ctx"));

  trn_module.def(
      "begin_sliced_bake",
      [](dflow::graphdata_ptr_t g, ctx_t ctx, int dim, float extent_m) -> trn::slicedbake_handle_ptr_t {
        return trn::enqueueSlicedBake(g, ctx.get(), dim, extent_m, nullptr, /*enqueue*/ true);
      },
      py::arg("graph"), py::arg("ctx"), py::arg("dim"), py::arg("extent_m") = 4096.0f);
  // headless variant: NOT enqueued on the scheduler — the caller drives it via
  // handle.pumpToCompletion(ctx) (one node / frame). Used by the T9 byte-identity oracle.
  trn_module.def(
      "build_sliced_bake",
      [](dflow::graphdata_ptr_t g, ctx_t ctx, int dim, float extent_m) -> trn::slicedbake_handle_ptr_t {
        return trn::enqueueSlicedBake(g, ctx.get(), dim, extent_m, nullptr, /*enqueue*/ false);
      },
      py::arg("graph"), py::arg("ctx"), py::arg("dim"), py::arg("extent_m") = 4096.0f);

  /////////////////////////////////////////////////////////////////////////////
  // S4 — LiveFieldBuffer registry access (the named progressive-display artifact).
  // Consumers/gates observe the generation counter + live/final lifecycle; the
  // begin_bake/mark_final producer methods are the SAME engine API the cook driver
  // calls (exposed so the physics hold-last-final gate can drive the lifecycle
  // deterministically). Keys are canonicalized product paths.
  /////////////////////////////////////////////////////////////////////////////
  py::class_<LiveFieldBuffer, live_field_buffer_ptr_t>(trn_module, "LiveFieldBuffer")
      .def_property_readonly("generation", [](live_field_buffer_ptr_t b) -> uint64_t { return b->generation(); })
      .def_property_readonly("is_live", [](live_field_buffer_ptr_t b) -> bool { return b->isLive(); })
      .def_property_readonly("is_final", [](live_field_buffer_ptr_t b) -> bool { return b->isFinal(); })
      .def("beginBake", [](live_field_buffer_ptr_t b) { b->beginBake(); })
      .def("markFinal", [](live_field_buffer_ptr_t b) { b->markFinal(); });
  trn_module.def("live_field_acquire", [](std::string path) -> live_field_buffer_ptr_t {
    return liveFieldAcquire(liveFieldCanonicalKey(path));
  });
  trn_module.def("live_field_find", [](std::string path) -> live_field_buffer_ptr_t {
    return liveFieldFind(liveFieldCanonicalKey(path)); // None when absent
  });
  // #88 v2 — in-place display REVISIT: push a baked height product into the buffer the HELD
  // drawable consumes (its materialize-time key), so an interior->interior revisit morphs the
  // presenting terrain in place (no scene swap). Returns the plane dim (>0) on success, 0 when
  // it declined (buffer unarmed / product unreadable / S4 off) — the caller then full-swaps.
  trn_module.def(
      "publish_height_plane_from_exr",
      [](std::string held_field_key, std::string height_exr_path) -> int {
        return trn::publishHeightPlaneFromExr(held_field_key, height_exr_path);
      },
      py::arg("held_field_key"), py::arg("height_exr_path"));

  /////////////////////////////////////////////////////////////////////////////
  // E.2 — the C++ scatter placer. Runs one ScatterSinkData against baked channel
  // images and writes the ScatterSet .ogeo; returns the placed point count.
  // (Python's _run_scatters calls THIS — one placer implementation; the numpy
  // version survives only as the parity-gate reference.)
  /////////////////////////////////////////////////////////////////////////////
  trn_module.def(
      "scatter_place_ogeo",
      [](scattersink_data_ptr_t sink, std::map<std::string, std::string> channel_paths,
         float extent_m, std::string out_path) -> int {
        auto geo = trn::scatterPlace(*sink, channel_paths, extent_m);
        OrkAssert(geo);
        geo->writeChunkfile(file::Path(out_path.c_str()));
        return geo->numPoints();
      },
      py::arg("sink"), py::arg("channel_paths"), py::arg("extent_m"),
      py::arg("out_path"));

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
  // SubGraphModule gate: round-trip + N->N+k per-iteration cache oracle + composite bypass.
  module_lev2.def("terrain_subgraph_test", [](ctx_t ctx, int dim) -> int {
    return terrain::terrainSubgraphTest(ctx.get(), dim);
  });
  // hypermesh GPU-mesh dataflow foundation gate (bake a PrimitiveModule, readback-assert).
  module_lev2.def("hypermesh_foundation_selftest", [](ctx_t ctx) -> int {
    return hypermesh::hypermeshFoundationSelfTest(ctx.get());
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
