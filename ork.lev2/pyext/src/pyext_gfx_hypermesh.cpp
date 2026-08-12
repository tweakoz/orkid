////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////
// Python bindings for the hypermesh GPU mesh compute-dataflow: the module factories (composed into
// a dflow.GraphData via addModule/connect), a GpuMesh handle exposing the INDEXED attribute mesh's
// SSBOs (vertex channels + vidx + face_offsets + face attrs), materialize() = bakeMesh, the LIVE
// GraphInst, and setupMeshRender() = install the render-time triangulator + per-frame in-frame hook.
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/grammar/rewrite.h> // family-neutral grammar pass 1 (the vocabulary + derive test seams)
#include <ork/lev2/gfx/hypermesh/hmdflow.h>
#include <ork/lev2/gfx/hypermesh/hm_drawable.h> // D.3: the reflected hypermesh drawable description
#include <ork/lev2/gfx/hypermesh/meshlet.h>     // CPU meshlet partitioner (mesh-shader bridge, build side)
#include <ork/lev2/gfx/sdf/sdfdflow.h>          // E.7: the sdfgrid family
#include <ork/dataflow/module.inl>              // typedOutputNamed<SdfGridPlugTraits> instantiation (read_brick)
#include <ork/dataflow/plug_inst.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <fstream>

namespace ork::lev2 {

namespace dflow = dataflow;
namespace hm    = hypermesh;

// resolved-op -> dict, for the family-neutral `_grammarDerive` seam below.
static py::dict _ropToDict(const ork::grammar::ROp& r) {
  py::dict d;
  d["kind"]  = int(r.kind);
  d["vocab"] = int(r.vocab);
  d["gid"]   = r.gid;
  py::dict params;
  for (auto& kv : r.params)
    params[py::str(kv.first)] = kv.second;
  d["params"] = params;
  py::list branches;
  for (auto& br : r.branches) {
    py::list ops;
    for (auto& sub : br)
      ops.append(_ropToDict(sub));
    branches.append(ops);
  }
  d["branches"] = branches;
  return d;
}

void pyinit_gfx_hypermesh(py::module& module_lev2) {
  auto type_codec = python::pb11_typecodec_t::instance();
  auto hmmod      = module_lev2.def_submodule("hypermesh", "GPU mesh compute-dataflow");
  auto sdfmod     = module_lev2.def_submodule("sdf", "sdfgrid dflow family (E.7)");

  // ---- module factories (DgModuleData subclasses; composed via dflow.GraphData) ----
  py::class_<hm::RipplePrimitiveData, dflow::DgModuleData, hm::rippleprimitivemoduledata_ptr_t>(hmmod, "RipplePrimitive")
      .def_static("createShared", []() -> hm::rippleprimitivemoduledata_ptr_t { return hm::RipplePrimitiveData::createShared(); })
      .def("set_mask", [](hm::rippleprimitivemoduledata_ptr_t d, std::vector<uint32_t> m) { d->_mask = m; })
      .def_readwrite("kind", &hm::RipplePrimitiveData::_kind); // baked: 0=grid (grid resolution is the "grid" int plug)
  py::class_<hm::BoxData, dflow::DgModuleData, hm::boxdata_ptr_t>(hmmod, "Box")
      .def_static("createShared", []() -> hm::boxdata_ptr_t { return hm::BoxData::createShared(); })
      .def("set_mask", [](hm::boxdata_ptr_t d, std::vector<uint32_t> m) { d->_mask = m; });
  // ---- GR1.a: the LRuleSet grammar-as-data schema (6 reflected types). Hand-written bindings
  //      (createShared + kwargs-init + whole-vector .def_property object-array setters) because the
  //      generic reflection proxy is READ-ONLY for object arrays (T3). kind/op stay INT-valued at the
  //      python boundary (the DSL's named codes mirror lruleset.h); C++ side they are reflected enums,
  //      so an int outside the registered set aborts loudly the first time the grammar serializes.
  auto lexpr_type = //
      py::class_<hm::LExpr, ork::Object, hm::lexpr_ptr_t>(hmmod, "LExpr")
          .def_static("createShared", []() -> hm::lexpr_ptr_t { return std::make_shared<hm::LExpr>(); })
          .def(py::init([](py::kwargs kw) {
            auto d = std::make_shared<hm::LExpr>();
            if (kw.contains("kind"))     d->_kind  = hm::LExprKind(kw["kind"].cast<int>());
            if (kw.contains("constant")) d->_const = kw["constant"].cast<float>();
            if (kw.contains("ref"))      d->_ref   = kw["ref"].cast<std::string>();
            if (kw.contains("op"))       d->_op    = hm::LExprOp(kw["op"].cast<int>());
            if (kw.contains("args"))     d->_args  = kw["args"].cast<std::vector<hm::lexpr_ptr_t>>();
            return d;
          }))
          .def_property("kind",     [](hm::lexpr_ptr_t d) { return int(d->_kind); },  [](hm::lexpr_ptr_t d, int v) { d->_kind = hm::LExprKind(v); })
          .def_property("constant", [](hm::lexpr_ptr_t d) { return d->_const; }, [](hm::lexpr_ptr_t d, float v) { d->_const = v; })
          .def_property("ref",      [](hm::lexpr_ptr_t d) { return d->_ref; },   [](hm::lexpr_ptr_t d, std::string v) { d->_ref = std::move(v); })
          .def_property("op",       [](hm::lexpr_ptr_t d) { return int(d->_op); },    [](hm::lexpr_ptr_t d, int v) { d->_op = hm::LExprOp(v); })
          .def_property("args", // whole-vector object-array setter (T3)
                        [](hm::lexpr_ptr_t d) -> std::vector<hm::lexpr_ptr_t> { return d->_args; },
                        [](hm::lexpr_ptr_t d, std::vector<hm::lexpr_ptr_t> v) { d->_args = std::move(v); });
  type_codec->registerStdCodec<hm::lexpr_ptr_t>(lexpr_type);

  auto lsymdef_type = //
      py::class_<hm::LSymbolDef, ork::Object, hm::lsymboldef_ptr_t>(hmmod, "LSymbolDef")
          .def_static("createShared", []() -> hm::lsymboldef_ptr_t { return std::make_shared<hm::LSymbolDef>(); })
          .def(py::init([](py::kwargs kw) {
            auto d = std::make_shared<hm::LSymbolDef>();
            if (kw.contains("name"))     d->_name     = kw["name"].cast<std::string>();
            if (kw.contains("defaults")) d->_defaults = kw["defaults"].cast<std::map<std::string, float>>();
            return d;
          }))
          .def_property("name",     [](hm::lsymboldef_ptr_t d) { return d->_name; }, [](hm::lsymboldef_ptr_t d, std::string v) { d->_name = std::move(v); })
          .def_property("defaults",
                        [](hm::lsymboldef_ptr_t d) -> std::map<std::string, float> { return d->_defaults; },
                        [](hm::lsymboldef_ptr_t d, std::map<std::string, float> v) { d->_defaults = std::move(v); });
  type_codec->registerStdCodec<hm::lsymboldef_ptr_t>(lsymdef_type);

  auto lparam_type = //
      py::class_<hm::LParamBinding, ork::Object, hm::lparam_binding_ptr_t>(hmmod, "LParamBinding")
          .def_static("createShared", []() -> hm::lparam_binding_ptr_t { return std::make_shared<hm::LParamBinding>(); })
          .def(py::init([](py::kwargs kw) {
            auto d = std::make_shared<hm::LParamBinding>();
            if (kw.contains("key"))   d->_key   = kw["key"].cast<std::string>();
            if (kw.contains("value")) d->_value = kw["value"].cast<hm::lexpr_ptr_t>();
            return d;
          }))
          .def_property("key",   [](hm::lparam_binding_ptr_t d) { return d->_key; }, [](hm::lparam_binding_ptr_t d, std::string v) { d->_key = std::move(v); })
          .def_property("value", // nullable single object
                        [](hm::lparam_binding_ptr_t d) -> hm::lexpr_ptr_t { return d->_value; },
                        [](hm::lparam_binding_ptr_t d, hm::lexpr_ptr_t v) { d->_value = std::move(v); });
  type_codec->registerStdCodec<hm::lparam_binding_ptr_t>(lparam_type);

  auto lturtle_type = //
      py::class_<hm::LTurtleOp, ork::Object, hm::lturtleop_ptr_t>(hmmod, "LTurtleOp")
          .def_static("createShared", []() -> hm::lturtleop_ptr_t { return std::make_shared<hm::LTurtleOp>(); })
          .def(py::init([](py::kwargs kw) {
            auto d = std::make_shared<hm::LTurtleOp>();
            if (kw.contains("kind"))     d->_kind     = hm::LOpCode(kw["kind"].cast<int>());
            if (kw.contains("params"))   d->_params   = kw["params"].cast<std::vector<hm::lparam_binding_ptr_t>>();
            if (kw.contains("gid"))      d->_gid      = kw["gid"].cast<uint32_t>();
            if (kw.contains("symbol"))   d->_symbol   = kw["symbol"].cast<std::string>();
            if (kw.contains("children")) d->_children = kw["children"].cast<std::vector<hm::lturtleop_ptr_t>>();
            if (kw.contains("weights"))  d->_weights  = kw["weights"].cast<std::vector<float>>();
            if (kw.contains("guard"))    d->_guard    = kw["guard"].cast<hm::lexpr_ptr_t>();
            return d;
          }))
          .def_property("kind",   [](hm::lturtleop_ptr_t d) { return int(d->_kind); },   [](hm::lturtleop_ptr_t d, int v) { d->_kind = hm::LOpCode(v); })
          .def_property("gid",    [](hm::lturtleop_ptr_t d) { return d->_gid; },    [](hm::lturtleop_ptr_t d, uint32_t v) { d->_gid = v; })
          .def_property("symbol", [](hm::lturtleop_ptr_t d) { return d->_symbol; }, [](hm::lturtleop_ptr_t d, std::string v) { d->_symbol = std::move(v); })
          .def_property("params",
                        [](hm::lturtleop_ptr_t d) -> std::vector<hm::lparam_binding_ptr_t> { return d->_params; },
                        [](hm::lturtleop_ptr_t d, std::vector<hm::lparam_binding_ptr_t> v) { d->_params = std::move(v); })
          .def_property("children",
                        [](hm::lturtleop_ptr_t d) -> std::vector<hm::lturtleop_ptr_t> { return d->_children; },
                        [](hm::lturtleop_ptr_t d, std::vector<hm::lturtleop_ptr_t> v) { d->_children = std::move(v); })
          .def_property("weights",
                        [](hm::lturtleop_ptr_t d) -> std::vector<float> { return d->_weights; },
                        [](hm::lturtleop_ptr_t d, std::vector<float> v) { d->_weights = std::move(v); })
          .def_property("guard", // nullable single object
                        [](hm::lturtleop_ptr_t d) -> hm::lexpr_ptr_t { return d->_guard; },
                        [](hm::lturtleop_ptr_t d, hm::lexpr_ptr_t v) { d->_guard = std::move(v); });
  type_codec->registerStdCodec<hm::lturtleop_ptr_t>(lturtle_type);

  auto lruledef_type = //
      py::class_<hm::LRuleDef, ork::Object, hm::lruledef_ptr_t>(hmmod, "LRuleDef")
          .def_static("createShared", []() -> hm::lruledef_ptr_t { return std::make_shared<hm::LRuleDef>(); })
          .def(py::init([](py::kwargs kw) {
            auto d = std::make_shared<hm::LRuleDef>();
            if (kw.contains("lhs"))    d->_lhs    = kw["lhs"].cast<std::string>();
            if (kw.contains("guard"))  d->_guard  = kw["guard"].cast<hm::lexpr_ptr_t>();
            if (kw.contains("weight")) d->_weight = kw["weight"].cast<float>();
            if (kw.contains("rhs"))    d->_rhs    = kw["rhs"].cast<std::vector<hm::lturtleop_ptr_t>>();
            return d;
          }))
          .def_property("lhs",    [](hm::lruledef_ptr_t d) { return d->_lhs; },    [](hm::lruledef_ptr_t d, std::string v) { d->_lhs = std::move(v); })
          .def_property("weight", [](hm::lruledef_ptr_t d) { return d->_weight; }, [](hm::lruledef_ptr_t d, float v) { d->_weight = v; })
          .def_property("guard", // nullable single object
                        [](hm::lruledef_ptr_t d) -> hm::lexpr_ptr_t { return d->_guard; },
                        [](hm::lruledef_ptr_t d, hm::lexpr_ptr_t v) { d->_guard = std::move(v); })
          .def_property("rhs",
                        [](hm::lruledef_ptr_t d) -> std::vector<hm::lturtleop_ptr_t> { return d->_rhs; },
                        [](hm::lruledef_ptr_t d, std::vector<hm::lturtleop_ptr_t> v) { d->_rhs = std::move(v); });
  type_codec->registerStdCodec<hm::lruledef_ptr_t>(lruledef_type);

  auto lruleset_type = //
      py::class_<hm::LRuleSet, ork::Object, hm::lruleset_ptr_t>(hmmod, "LRuleSet")
          .def_static("createShared", []() -> hm::lruleset_ptr_t { return std::make_shared<hm::LRuleSet>(); })
          .def(py::init([](py::kwargs kw) {
            auto d = std::make_shared<hm::LRuleSet>();
            if (kw.contains("symbols"))        d->_symbols       = kw["symbols"].cast<std::vector<hm::lsymboldef_ptr_t>>();
            if (kw.contains("axiom"))          d->_axiom         = kw["axiom"].cast<std::vector<hm::lturtleop_ptr_t>>();
            if (kw.contains("rules"))          d->_rules         = kw["rules"].cast<std::vector<hm::lruledef_ptr_t>>();
            if (kw.contains("depth"))          d->_depth         = kw["depth"].cast<uint32_t>();
            if (kw.contains("segment_budget")) d->_countedBudget = kw["segment_budget"].cast<uint32_t>();
            if (kw.contains("seed"))           d->_seed          = kw["seed"].cast<uint32_t>();
            return d;
          }))
          .def_property("depth",          [](hm::lruleset_ptr_t d) { return d->_depth; },         [](hm::lruleset_ptr_t d, uint32_t v) { d->_depth = v; })
          .def_property("segment_budget", [](hm::lruleset_ptr_t d) { return d->_countedBudget; }, [](hm::lruleset_ptr_t d, uint32_t v) { d->_countedBudget = v; })
          .def_property("seed",           [](hm::lruleset_ptr_t d) { return d->_seed; },          [](hm::lruleset_ptr_t d, uint32_t v) { d->_seed = v; })
          .def_property("symbols",
                        [](hm::lruleset_ptr_t d) -> std::vector<hm::lsymboldef_ptr_t> { return d->_symbols; },
                        [](hm::lruleset_ptr_t d, std::vector<hm::lsymboldef_ptr_t> v) { d->_symbols = std::move(v); })
          .def_property("axiom",
                        [](hm::lruleset_ptr_t d) -> std::vector<hm::lturtleop_ptr_t> { return d->_axiom; },
                        [](hm::lruleset_ptr_t d, std::vector<hm::lturtleop_ptr_t> v) { d->_axiom = std::move(v); })
          .def_property("rules",
                        [](hm::lruleset_ptr_t d) -> std::vector<hm::lruledef_ptr_t> { return d->_rules; },
                        [](hm::lruleset_ptr_t d, std::vector<hm::lruledef_ptr_t> v) { d->_rules = std::move(v); });
  type_codec->registerStdCodec<hm::lruleset_ptr_t>(lruleset_type);

  py::class_<hm::LSystemModuleData, dflow::DgModuleData, hm::lsystemmoduledata_ptr_t>(hmmod, "LSystemModule")
      .def_static("createShared", []() -> hm::lsystemmoduledata_ptr_t { return hm::LSystemModuleData::createShared(); })
      // the reflected grammar — REQUIRED at activation since GR1.d (species are DATA; the four
      // stock growth models are the Python preset emitters in lsystem/presets.py).
      .def_property("grammar",
                    [](hm::lsystemmoduledata_ptr_t d) -> hm::lruleset_ptr_t { return d->_grammar; },
                    [](hm::lsystemmoduledata_ptr_t d, hm::lruleset_ptr_t g) { d->_grammar = std::move(g); })
      .def_readwrite("depth", &hm::LSystemModuleData::_depth)
      .def_readwrite("budget", &hm::LSystemModuleData::_budget)
      .def_readwrite("children", &hm::LSystemModuleData::_children)
      .def_readwrite("internodes", &hm::LSystemModuleData::_internodes)
      .def_readwrite("seed", &hm::LSystemModuleData::_seed)
      .def_readwrite("seg_len", &hm::LSystemModuleData::_seg_len)
      .def_readwrite("base_radius", &hm::LSystemModuleData::_base_radius)
      .def_readwrite("branch_angle", &hm::LSystemModuleData::_branch_angle)
      .def_readwrite("roll", &hm::LSystemModuleData::_roll)
      .def_readwrite("len_decay", &hm::LSystemModuleData::_len_decay)
      .def_readwrite("rad_decay", &hm::LSystemModuleData::_rad_decay)
      .def_readwrite("taper", &hm::LSystemModuleData::_taper)
      .def_readwrite("tropism", &hm::LSystemModuleData::_tropism)
      .def_readwrite("jitter", &hm::LSystemModuleData::_jitter)
      .def_readwrite("apical", &hm::LSystemModuleData::_apical)
      .def_readwrite("jit_azimuth", &hm::LSystemModuleData::_jit_azimuth)
      .def_readwrite("jit_pitch", &hm::LSystemModuleData::_jit_pitch)
      .def_readwrite("jit_length", &hm::LSystemModuleData::_jit_length)
      .def_readwrite("jit_spacing", &hm::LSystemModuleData::_jit_spacing)
      .def_readwrite("jit_drop", &hm::LSystemModuleData::_jit_drop)
      .def_readwrite("jit_wave", &hm::LSystemModuleData::_jit_wave);
  // ---- GR1.b: the evaluator TEST SEAM (pure CPU — the determinism/hand-count/budget gates drive
  //      derive() headless, no GPU). `_deriveLRuleSet` runs the two-pass rewrite+interpret and returns
  //      the derived XfNode buffer (raw bytes for memcmp determinism + unpacked fields for hand-count);
  //      `_moduleIdentityHash` exposes the cook-identity hash so the same gate asserts BOTH are stable.
  hmmod.def(
      "_deriveLRuleSet",
      [](hm::lruleset_ptr_t grammar, hm::lsystemmoduledata_ptr_t env) -> py::dict {
        OrkAssert(grammar);
        hm::lsystemmoduledata_ptr_t P = env ? env : hm::LSystemModuleData::createShared();
        ork::hyper::xfnode_vect nodes;
        ork::hyper::xfslot_vect slots;
        hm::deriveLRuleSet(grammar.get(), P.get(), nodes, slots);
        py::dict d;
        py::list parents, positions, radii, tags, slotl;
        std::string raw;
        raw.reserve(nodes.size() * 88);
        for (auto& n : nodes) {
          parents.append(n._parent);
          positions.append(n._xform[12]);
          positions.append(n._xform[13]);
          positions.append(n._xform[14]);
          radii.append(n._attrs[0]);
          tags.append(n._tags);
          raw.append(reinterpret_cast<const char*>(n._xform), 16 * sizeof(float));
          raw.append(reinterpret_cast<const char*>(&n._parent), sizeof(uint32_t));
          raw.append(reinterpret_cast<const char*>(n._attrs), 4 * sizeof(float));
          raw.append(reinterpret_cast<const char*>(&n._tags), sizeof(uint32_t));
        }
        for (auto& s : slots)
          slotl.append(py::make_tuple(s._node, s._tag));
        d["count"]     = int(nodes.size());
        d["parents"]   = parents;
        d["positions"] = positions;
        d["radii"]     = radii;
        d["tags"]      = tags;
        d["slots"]     = slotl;
        d["bytes"]     = py::bytes(raw);
        return d;
      },
      py::arg("grammar"), py::arg("env") = hm::lsystemmoduledata_ptr_t());
  // ANY module: the cook-identity hash is content-only over reflected state, so a gate on any
  // family's params (a scatter's placement knobs as much as an L-system's grammar) reads it here.
  hmmod.def("_moduleIdentityHash", [](dflow::dgmoduledata_ptr_t mod) -> uint64_t {
    return hm::hypermeshModuleIdentityHash(mod.get());
  });
  // ---- the FAMILY-NEUTRAL grammar seams (ork::grammar, ork.core). Bound here only because the six
  //      schema classes are bound here; neither function touches a mesh type. `_grammarRegisterVocabulary`
  //      is how a family (or a test standing in for one) hands core its op alphabet;
  //      `_grammarDerive` runs pass 1 ALONE and returns the resolved op stream, so a vocabulary with
  //      no interpreter yet can still be derived and counted.
  hmmod.def(
      "_grammarRegisterVocabulary",
      [](const std::string& name, const std::vector<std::tuple<std::string, int, bool>>& ops) -> uint32_t {
        std::vector<ork::grammar::LOpDesc> descs;
        for (auto& o : ops)
          descs.push_back({int32_t(std::get<1>(o)), std::get<0>(o), std::get<2>(o)});
        return ork::grammar::LVocabularyRegistry::instance().registerVocabulary(name, descs);
      },
      py::arg("name"), py::arg("ops"));
  hmmod.def(
      "_grammarDerive",
      [](hm::lruleset_ptr_t grammar, std::map<std::string, float> host_params) -> py::dict {
        OrkAssert(grammar);
        ork::grammar::preflightAxiomBudget(grammar.get());
        ork::grammar::LRewriter ev(grammar.get(), [host_params](const std::string& n) -> float {
          auto it = host_params.find(n);
          return (it != host_params.end()) ? it->second : 0.0f;
        });
        ork::grammar::rop_vect stream;
        ev.expand(grammar->_axiom, ork::grammar::Env{}, int(grammar->_depth), stream);
        py::list ops;
        for (auto& r : stream)
          ops.append(_ropToDict(r));
        py::dict d;
        d["ops"]        = ops;
        d["counted"]    = ev._countedCount;
        d["budget_hit"] = ev._budgetHit;
        return d;
      },
      py::arg("grammar"), py::arg("params") = std::map<std::string, float>());
  /////////////////////////////////////////////////////////////////////////////
  // R-FAMILY (roads / streets / layout) — Q3: R. DSL over hypermesh-family C++.
  /////////////////////////////////////////////////////////////////////////////
  py::class_<hm::RouteSpineModuleData, dflow::DgModuleData, hm::routespinemoduledata_ptr_t>(hmmod, "RouteSpineModule")
      .def_static("createShared", []() -> hm::routespinemoduledata_ptr_t { return hm::RouteSpineModuleData::createShared(); })
      .def("set_pois", [](hm::routespinemoduledata_ptr_t d, std::vector<float> v) { d->_pois = v; }) // flat world x,z pairs
      .def_readwrite("extent_m", &hm::RouteSpineModuleData::_extent_m)
      .def_readwrite("layout_cell_m", &hm::RouteSpineModuleData::_layout_cell_m)
      .def_readwrite("field_dim", &hm::RouteSpineModuleData::_field_dim)
      .def_readwrite("width_m", &hm::RouteSpineModuleData::_width_m)
      .def_readwrite("export_name", &hm::RouteSpineModuleData::_export_name)
      .def_readwrite("max_grade", &hm::RouteSpineModuleData::_max_grade)
      .def_readwrite("w_slope", &hm::RouteSpineModuleData::_w_slope)
      .def_readwrite("w_curv", &hm::RouteSpineModuleData::_w_curv)
      .def_readwrite("w_water", &hm::RouteSpineModuleData::_w_water)
      .def_readwrite("disch_thresh", &hm::RouteSpineModuleData::_disch_thresh)
      .def_readwrite("grade_weight", &hm::RouteSpineModuleData::_grade_weight)
      .def_readwrite("base_cost", &hm::RouteSpineModuleData::_base_cost)
      .def_readwrite("seed", &hm::RouteSpineModuleData::_seed)
      .def_readwrite("min_radius_m", &hm::RouteSpineModuleData::_min_radius_m)
      .def_readwrite("station_m", &hm::RouteSpineModuleData::_station_m)
      .def_readwrite("vcurve_len_m", &hm::RouteSpineModuleData::_vcurve_len_m)
      .def_readwrite("clearance_m", &hm::RouteSpineModuleData::_clearance_m);
  py::class_<hm::RoadbedMaskModuleData, dflow::DgModuleData, hm::roadbedmaskmoduledata_ptr_t>(hmmod, "RoadbedMaskModule")
      .def_static("createShared", []() -> hm::roadbedmaskmoduledata_ptr_t { return hm::RoadbedMaskModuleData::createShared(); })
      .def_readwrite("width_m", &hm::RoadbedMaskModuleData::_width_m)
      .def_readwrite("shoulder_m", &hm::RoadbedMaskModuleData::_shoulder_m)
      .def_readwrite("v_meters_per_tile", &hm::RoadbedMaskModuleData::_v_meters_per_tile)
      .def_readwrite("extent_m", &hm::RoadbedMaskModuleData::_extent_m)
      .def_readwrite("out_dim", &hm::RoadbedMaskModuleData::_out_dim);
  py::class_<hm::KeepoutMaskModuleData, dflow::DgModuleData, hm::keepoutmaskmoduledata_ptr_t>(hmmod, "KeepoutMaskModule")
      .def_static("createShared", []() -> hm::keepoutmaskmoduledata_ptr_t { return hm::KeepoutMaskModuleData::createShared(); })
      .def_readwrite("keepout_radius_m", &hm::KeepoutMaskModuleData::_keepout_radius_m)
      .def_readwrite("extent_m", &hm::KeepoutMaskModuleData::_extent_m);
  py::class_<hm::ParcelizeModuleData, dflow::DgModuleData, hm::parcelizemoduledata_ptr_t>(hmmod, "ParcelizeModule")
      .def_static("createShared", []() -> hm::parcelizemoduledata_ptr_t { return hm::ParcelizeModuleData::createShared(); })
      .def_readwrite("frontage_m", &hm::ParcelizeModuleData::_frontage_m)
      .def_readwrite("depth_m", &hm::ParcelizeModuleData::_depth_m)
      .def_readwrite("spacing_m", &hm::ParcelizeModuleData::_spacing_m)
      .def_readwrite("jitter", &hm::ParcelizeModuleData::_jitter)
      .def_readwrite("extent_m", &hm::ParcelizeModuleData::_extent_m)
      .def_readwrite("seed", &hm::ParcelizeModuleData::_seed);
  py::class_<hm::BuildingSeedsModuleData, dflow::DgModuleData, hm::buildingseedsmoduledata_ptr_t>(hmmod, "BuildingSeedsModule")
      .def_static("createShared", []() -> hm::buildingseedsmoduledata_ptr_t { return hm::BuildingSeedsModuleData::createShared(); })
      .def("set_type_weights", [](hm::buildingseedsmoduledata_ptr_t d, std::vector<float> v) { d->_type_weights = v; })
      .def_readwrite("emit_adapter", &hm::BuildingSeedsModuleData::_emit_adapter)
      .def_readwrite("seed", &hm::BuildingSeedsModuleData::_seed);
  py::class_<hm::RoadMeshModuleData, dflow::DgModuleData, hm::roadmeshmoduledata_ptr_t>(hmmod, "RoadMeshModule")
      .def_static("createShared", []() -> hm::roadmeshmoduledata_ptr_t { return hm::RoadMeshModuleData::createShared(); })
      .def_readwrite("v_meters_per_tile", &hm::RoadMeshModuleData::_v_meters_per_tile)
      .def_readwrite("junction_setback_scale", &hm::RoadMeshModuleData::_junction_setback_scale)
      .def_readwrite("junction_min_edge_frac", &hm::RoadMeshModuleData::_junction_min_edge_frac)
      .def_readwrite("road_gid", &hm::RoadMeshModuleData::_road_gid)
      .def_readwrite("junction_gid", &hm::RoadMeshModuleData::_junction_gid)
      .def_readwrite("extent_m", &hm::RoadMeshModuleData::_extent_m)
      .def_readwrite("shoulder_m", &hm::RoadMeshModuleData::_shoulder_m)
      .def_readwrite("shoulder_gid", &hm::RoadMeshModuleData::_shoulder_gid)
      .def_readwrite("clearance_m", &hm::RoadMeshModuleData::_clearance_m)
      .def_readwrite("max_bank_rad", &hm::RoadMeshModuleData::_max_bank_rad)
      .def_readwrite("bank_runoff_m", &hm::RoadMeshModuleData::_bank_runoff_m)
      .def_readwrite("bank_ref_radius_m", &hm::RoadMeshModuleData::_bank_ref_radius_m);
  py::class_<hm::LSweepModuleData, dflow::DgModuleData, hm::lsweepmoduledata_ptr_t>(hmmod, "LSweepModule")
      .def_static("createShared", []() -> hm::lsweepmoduledata_ptr_t { return hm::LSweepModuleData::createShared(); })
      .def_readwrite("sides", &hm::LSweepModuleData::_sides)
      .def_readwrite("cap_segments", &hm::LSweepModuleData::_cap_segments)
      .def_readwrite("cap_round", &hm::LSweepModuleData::_cap_round);
  py::class_<hm::LeafScatterModuleData, dflow::DgModuleData, hm::leafscattermoduledata_ptr_t>(hmmod, "LeafScatterModule")
      .def_static("createShared", []() -> hm::leafscattermoduledata_ptr_t { return hm::LeafScatterModuleData::createShared(); })
      .def_readwrite("style", &hm::LeafScatterModuleData::_style)
      .def_readwrite("source", &hm::LeafScatterModuleData::_source)
      .def_readwrite("per_node", &hm::LeafScatterModuleData::_per_node)
      .def_readwrite("min_gen", &hm::LeafScatterModuleData::_min_gen)
      .def_readwrite("max_radius", &hm::LeafScatterModuleData::_max_radius)
      .def_readwrite("size", &hm::LeafScatterModuleData::_size)
      .def_readwrite("aspect", &hm::LeafScatterModuleData::_aspect)
      .def_readwrite("roll", &hm::LeafScatterModuleData::_roll)
      .def_readwrite("pitch", &hm::LeafScatterModuleData::_pitch)
      .def_readwrite("embed", &hm::LeafScatterModuleData::_embed)
      .def_readwrite("twist", &hm::LeafScatterModuleData::_twist)
      .def_readwrite("up_bias", &hm::LeafScatterModuleData::_up_bias)
      .def_readwrite("jitter", &hm::LeafScatterModuleData::_jitter)
      .def_readwrite("jitter_deg", &hm::LeafScatterModuleData::_jitter_deg)
      .def_readwrite("seed", &hm::LeafScatterModuleData::_seed);
  py::class_<hm::MergeMeshData, dflow::DgModuleData, hm::mergemeshdata_ptr_t>(hmmod, "MergeMesh")
      .def_static("createShared", []() -> hm::mergemeshdata_ptr_t { return hm::MergeMeshData::createShared(); })
      .def_readwrite("gid_a", &hm::MergeMeshData::_gid_a)
      .def_readwrite("gid_b", &hm::MergeMeshData::_gid_b);
  py::class_<hm::SortTestData, dflow::DgModuleData, hm::sorttestdata_ptr_t>(hmmod, "SortTest")
      .def_static("createShared", []() -> hm::sorttestdata_ptr_t { return hm::SortTestData::createShared(); })
      .def_readwrite("n", &hm::SortTestData::_n);
  py::class_<hm::EdgeTestData, dflow::DgModuleData, hm::edgetestdata_ptr_t>(hmmod, "EdgeTest")
      .def_static("createShared", []() -> hm::edgetestdata_ptr_t { return hm::EdgeTestData::createShared(); });
  py::class_<hm::VertTestData, dflow::DgModuleData, hm::verttestdata_ptr_t>(hmmod, "VertTest")
      .def_static("createShared", []() -> hm::verttestdata_ptr_t { return hm::VertTestData::createShared(); });
  py::class_<hm::BevelData, dflow::DgModuleData, hm::beveldata_ptr_t>(hmmod, "Bevel")
      .def_static("createShared", []() -> hm::beveldata_ptr_t { return hm::BevelData::createShared(); })
      .def_readwrite("slot", &hm::BevelData::_slot)
      .def_readwrite("precheck", &hm::BevelData::_precheck)    // assert if INPUT mesh isn't good (welded/manifold)
      .def_readwrite("postcheck", &hm::BevelData::_postcheck)  // assert if OUTPUT mesh isn't good (no new holes)
      .def("set_part_masks", [](hm::beveldata_ptr_t d, std::vector<uint32_t> m) { d->_part_masks = m; });
  py::class_<hm::UvSphereData, dflow::DgModuleData, hm::uvspheredata_ptr_t>(hmmod, "UvSphere")
      .def_static("createShared", []() -> hm::uvspheredata_ptr_t { return hm::UvSphereData::createShared(); })
      .def("set_mask", [](hm::uvspheredata_ptr_t d, std::vector<uint32_t> m) { d->_mask = m; });
  py::class_<hm::IcoSphereData, dflow::DgModuleData, hm::icospheredata_ptr_t>(hmmod, "IcoSphere")
      .def_static("createShared", []() -> hm::icospheredata_ptr_t { return hm::IcoSphereData::createShared(); })
      .def("set_mask", [](hm::icospheredata_ptr_t d, std::vector<uint32_t> m) { d->_mask = m; });
  py::class_<hm::ConeData, dflow::DgModuleData, hm::conedata_ptr_t>(hmmod, "Cone")
      .def_static("createShared", []() -> hm::conedata_ptr_t { return hm::ConeData::createShared(); })
      .def("set_mask", [](hm::conedata_ptr_t d, std::vector<uint32_t> m) { d->_mask = m; });
  py::class_<hm::SubdivideModuleData, dflow::DgModuleData, hm::subdividemoduledata_ptr_t>(hmmod, "SubdivideModule")
      .def_static("createShared", []() -> hm::subdividemoduledata_ptr_t { return hm::SubdivideModuleData::createShared(); })
      .def_readwrite("smooth", &hm::SubdivideModuleData::_smooth)    // False=linear midpoint; True=Catmull-Clark
      .def_readwrite("slot", &hm::SubdivideModuleData::_slot);       // -1=whole mesh; >=0 = subdivide only that __tags bit
  py::class_<hm::SelectData, dflow::DgModuleData, hm::selectdata_ptr_t>(hmmod, "Select")  // selection -> __tags
      .def_static("createShared", []() -> hm::selectdata_ptr_t { return hm::SelectData::createShared(); })
      .def_property(
          "predicate", [](hm::selectdata_ptr_t s) { return s->_predicate; },
          [](hm::selectdata_ptr_t s, std::string p) { s->_predicate = p; })
      .def_property(   // E2.5 (Q6): the canonical hypermesh.selexpr ExprIR tree JSON (sibling of predicate)
          "predicate_tree", [](hm::selectdata_ptr_t s) { return s->_predicate_tree; },
          [](hm::selectdata_ptr_t s, std::string p) { s->_predicate_tree = p; })
      .def_readwrite("sel_and", &hm::SelectData::_sel_and)
      .def_readwrite("sel_or", &hm::SelectData::_sel_or)
      .def_readwrite("sel_xor", &hm::SelectData::_sel_xor)
      .def_readwrite("unsel_and", &hm::SelectData::_unsel_and)
      .def_readwrite("unsel_or", &hm::SelectData::_unsel_or)
      .def_readwrite("unsel_xor", &hm::SelectData::_unsel_xor)
      .def_readwrite("domain", &hm::SelectData::_domain);
  py::class_<hm::ExtrudeFacesData, dflow::DgModuleData, hm::extrudefacesdata_ptr_t>(hmmod, "ExtrudeFaces")
      .def_static("createShared", []() -> hm::extrudefacesdata_ptr_t { return hm::ExtrudeFacesData::createShared(); })
      .def_readwrite("slot", &hm::ExtrudeFacesData::_slot)
      .def_readwrite("mode", &hm::ExtrudeFacesData::_mode)   // 0=vertex(region), 1=face(individual); default 1
      .def_readwrite("keep_base", &hm::ExtrudeFacesData::_keep_base)  // re-close the footprint (no see-through on a shell)
      // FACE-mode per-face expression predicates (SelExpr -> GLSL assigning _dist / _inset / _dir). RUNTIME.
      .def_property("dist_predicate",  [](hm::extrudefacesdata_ptr_t e) { return e->_dist_pred; },
                                       [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_dist_pred = p; })
      .def_property("inset_predicate", [](hm::extrudefacesdata_ptr_t e) { return e->_inset_pred; },
                                       [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_inset_pred = p; })
      .def_property("dir_predicate",   [](hm::extrudefacesdata_ptr_t e) { return e->_dir_pred; },
                                       [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_dir_pred = p; })
      // E2.5 (Q6): the canonical hypermesh.selexpr ExprIR tree JSON per field (sibling of the preds above)
      .def_property("dist_tree",  [](hm::extrudefacesdata_ptr_t e) { return e->_dist_tree; },
                                  [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_dist_tree = p; })
      .def_property("inset_tree", [](hm::extrudefacesdata_ptr_t e) { return e->_inset_tree; },
                                  [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_inset_tree = p; })
      .def_property("dir_tree",   [](hm::extrudefacesdata_ptr_t e) { return e->_dir_tree; },
                                  [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_dir_tree = p; })
      // MULTI-SEGMENT: `segments` (int, STRUCTURAL -> rebuild) subdivides the lift into N rings; twist (ABSOLUTE
      // radians about the extrusion axis) + scale (ABSOLUTE in-plane profile scale) are per-t (S.t/S.seg) preds.
      .def_readwrite("segments", &hm::ExtrudeFacesData::_segments)
      .def_property("twist_predicate", [](hm::extrudefacesdata_ptr_t e) { return e->_twist_pred; },
                                       [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_twist_pred = p; })
      .def_property("scale_predicate", [](hm::extrudefacesdata_ptr_t e) { return e->_scale_pred; },
                                       [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_scale_pred = p; })
      .def_property("twist_tree", [](hm::extrudefacesdata_ptr_t e) { return e->_twist_tree; },
                                  [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_twist_tree = p; })
      .def_property("scale_tree", [](hm::extrudefacesdata_ptr_t e) { return e->_scale_tree; },
                                  [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_scale_tree = p; })
      // generic runtime params for the expressions (DSL `param()` -> EXPRP[]). set_expr_params seeds the
      // whole vec4 array (sizes the buffer); set_expr_param4 rebinds one slot live (from the asset).
      // B.4: expression params are REAL vec4 input plugs ("exprp{slot}") — these setters write the PLUG
      // VALUES (serialized with the asset; snapshotted by writeParams). Same call surface as before.
      .def("set_expr_params", [](hm::extrudefacesdata_ptr_t e, std::vector<float> v) {
        int nslots = std::min<int>(hm::kMaxExprParams, int(v.size() / 4));
        for (int k = 0; k < nslots; k++) {
          auto plg = std::dynamic_pointer_cast<dflow::inplugdata<dflow::Vec4fPlugTraits>>(
              e->inputNamed(FormatString("exprp%d", k)));
          if (plg)
            plg->setValue(fvec4(v[k * 4 + 0], v[k * 4 + 1], v[k * 4 + 2], v[k * 4 + 3]));
        }
      })
      .def("set_expr_param4", [](hm::extrudefacesdata_ptr_t e, int slot, float x, float y, float z, float w) {
        if (slot < 0 or slot >= hm::kMaxExprParams)
          return;
        auto plg = std::dynamic_pointer_cast<dflow::inplugdata<dflow::Vec4fPlugTraits>>(
            e->inputNamed(FormatString("exprp%d", slot)));
        if (plg)
          plg->setValue(fvec4(x, y, z, w));
      })
      .def_readwrite("time_slot", &hm::ExtrudeFacesData::_time_slot)  // S.time bridge: EXPRP slot fed by the C++ clock
      .def("set_masks", [](hm::extrudefacesdata_ptr_t e, std::vector<uint32_t> m) { e->_part_masks = m; });
  // GENERIC per-vertex GPU compute: an arbitrary fxv2 compute kernel (shadertext) authored from the DSL,
  // run per input vertex (iP -> oP), with 8 vec4 runtime param plugs + the S.time bridge. No new C++ per op.
  py::class_<hm::GpuComputeModuleData, dflow::DgModuleData, hm::gpucomputemoduledata_ptr_t>(hmmod, "GpuCompute")
      .def_static("createShared", []() -> hm::gpucomputemoduledata_ptr_t { return hm::GpuComputeModuleData::createShared(); })
      // the authored fxv2 compute kernel TEXT + its entry-point — the portable, reflected op identity.
      .def_property("shadertext", [](hm::gpucomputemoduledata_ptr_t d) { return d->_shadertext; },
                                  [](hm::gpucomputemoduledata_ptr_t d, std::string s) { d->_shadertext = s; })
      .def_property("kernel",     [](hm::gpucomputemoduledata_ptr_t d) { return d->_kernel; },
                                  [](hm::gpucomputemoduledata_ptr_t d, std::string s) { d->_kernel = s; })
      .def_readwrite("dispatch_mode", &hm::GpuComputeModuleData::_dispatch_mode)
      .def_readwrite("time_slot",     &hm::GpuComputeModuleData::_time_slot)  // S.time bridge: EXPRP slot fed by the C++ clock
      // B.4: the 8 vec4 runtime params are REAL input plugs ("exprp{slot}") — these write PLUG VALUES
      // (serialized with the asset; snapshotted by writeParams). set_expr_params seeds the array; param4 pokes one.
      .def("set_expr_params", [](hm::gpucomputemoduledata_ptr_t d, std::vector<float> v) {
        int nslots = std::min<int>(hm::kMaxExprParams, int(v.size() / 4));
        for (int k = 0; k < nslots; k++) {
          auto plg = std::dynamic_pointer_cast<dflow::inplugdata<dflow::Vec4fPlugTraits>>(
              d->inputNamed(FormatString("exprp%d", k)));
          if (plg)
            plg->setValue(fvec4(v[k * 4 + 0], v[k * 4 + 1], v[k * 4 + 2], v[k * 4 + 3]));
        }
      })
      .def("set_expr_param4", [](hm::gpucomputemoduledata_ptr_t d, int slot, float x, float y, float z, float w) {
        if (slot < 0 or slot >= hm::kMaxExprParams)
          return;
        auto plg = std::dynamic_pointer_cast<dflow::inplugdata<dflow::Vec4fPlugTraits>>(
            d->inputNamed(FormatString("exprp%d", slot)));
        if (plg)
          plg->setValue(fvec4(x, y, z, w));
      });
  py::class_<hm::InsetData, dflow::DgModuleData, hm::insetdata_ptr_t>(hmmod, "Inset")  // face -> collar + inner poly
      .def_static("createShared", []() -> hm::insetdata_ptr_t { return hm::InsetData::createShared(); })
      .def_readwrite("slot", &hm::InsetData::_slot)
      .def_readwrite("fill", &hm::InsetData::_fill)     // True = inner cap face; False = hole (baked)
      // `sides` (int), `rotate` (float DEGREES) + `amount` (float) are now INPUT PLUGS -> set via m.inputs.* and
      // animatable (rotate/amount runtime; sides triggers a rebuild on change). See _reshapeInsetIOs in the .cpp.
      .def_readwrite("precheck", &hm::InsetData::_precheck)    // inherited from MeshModuleData: assert on bad INPUT
      .def_readwrite("postcheck", &hm::InsetData::_postcheck)  // inherited from MeshModuleData: assert on bad OUTPUT
      .def_readwrite("cpu", &hm::InsetData::_cpu)              // route CPU(reference) vs GPU
      .def("set_masks", [](hm::insetdata_ptr_t e, std::vector<uint32_t> m) { e->_part_masks = m; });
  py::class_<hm::NormalsData, dflow::DgModuleData, hm::normalsdata_ptr_t>(hmmod, "Normals")  // recompute N + B
      .def_static("createShared", []() -> hm::normalsdata_ptr_t { return hm::NormalsData::createShared(); })
      .def_readwrite("smooth", &hm::NormalsData::_smooth)    // True=area-weighted average; False=per-face (flat)
      .def_readwrite("slot", &hm::NormalsData::_slot);       // -1=whole mesh; else __tags bit (seam-creased)
  py::class_<hm::TransformData, dflow::DgModuleData, hm::transformdata_ptr_t>(hmmod, "Transform")  // affine xform of a vert selection
      .def_static("createShared", []() -> hm::transformdata_ptr_t { return hm::TransformData::createShared(); })
      .def_readwrite("matrix", &hm::TransformData::_matrix)  // P' = matrix·P; PARAM (re-set each frame to animate)
      .def_readwrite("slot", &hm::TransformData::_slot);     // -1=whole mesh; else only that __tags bit's verts
  py::class_<hm::DisplaceByFieldData, dflow::DgModuleData, hm::displacebyfielddata_ptr_t>(hmmod, "DisplaceByField")  // E.1 cross-family
      .def_static("createShared", []() -> hm::displacebyfielddata_ptr_t { return hm::DisplaceByFieldData::createShared(); })
      .def_readwrite("mode", &hm::DisplaceByFieldData::_mode)            // 0=along vertex normal, 1=world +Y; amount/extent are plugs
      .def_readwrite("field_dim", &hm::DisplaceByFieldData::_field_dim); // bake resolution of the terrain FIELD subgraph (module-carried)
  py::class_<hm::DisplaceBySdfData, dflow::DgModuleData, hm::displacebysdfdata_ptr_t>(hmmod, "DisplaceBySdf")  // M4b cross-family SDF
      .def_static("createShared", []() -> hm::displacebysdfdata_ptr_t { return hm::DisplaceBySdfData::createShared(); })
      .def_readwrite("mode", &hm::DisplaceBySdfData::_mode)                     // 0=conform, 1=offset, 2=scalar; amount/phi_offset are plugs
      .def_readwrite("conform_steps", &hm::DisplaceBySdfData::_conform_steps)   // Gauss-Newton iterations (conform)
      .def_readwrite("relax_steps", &hm::DisplaceBySdfData::_relax_steps);      // Taubin surface-fairing passes; relax_lambda/relax_passband are plugs
  py::class_<hm::TemporalSmoothData, dflow::DgModuleData, hm::temporalsmoothdata_ptr_t>(hmmod, "TemporalSmooth")  // inter-frame EMA jitter damp
      .def_static("createShared", []() -> hm::temporalsmoothdata_ptr_t { return hm::TemporalSmoothData::createShared(); })
      .def_readwrite("mode", &hm::TemporalSmoothData::_mode);                    // 0=per-frame alpha EMA, 1=time-based tau EMA; alpha/tau are plugs
  py::class_<hm::ScatterSourceData, dflow::DgModuleData, hm::scattersourcedata_ptr_t>(hmmod, "ScatterSource")  // E.2 typed instance edge
      .def_static("createShared", []() -> hm::scattersourcedata_ptr_t { return hm::ScatterSourceData::createShared(); })
      .def_readwrite("scatter_asset", &hm::ScatterSourceData::_scatter_asset) // portable: HF asset name
      .def_readwrite("sink", &hm::ScatterSourceData::_sink)                   // + sink name -> <assetcache> path
      .def_readwrite("ogeo_path", &hm::ScatterSourceData::_ogeo_path)         // direct path override (tools)
      .def_readwrite("type_id", &hm::ScatterSourceData::_type_id);            // -1 = all types
  py::class_<hm::DeleteFacesData, dflow::DgModuleData, hm::deletefacesdata_ptr_t>(hmmod, "DeleteFaces")  // drop tagged faces
      .def_static("createShared", []() -> hm::deletefacesdata_ptr_t { return hm::DeleteFacesData::createShared(); })
      .def_readwrite("slot", &hm::DeleteFacesData::_slot);   // delete faces tagged with this __tags bit
  py::class_<hm::MirrorData, dflow::DgModuleData, hm::mirrordata_ptr_t>(hmmod, "Mirror")  // reflect + reversed copy
      .def_static("createShared", []() -> hm::mirrordata_ptr_t { return hm::MirrorData::createShared(); })
      .def_readwrite("axis", &hm::MirrorData::_axis)         // 0=X 1=Y 2=Z plane
      .def_readwrite("weld", &hm::MirrorData::_weld)         // share the on-plane seam verts
      .def_readwrite("eps", &hm::MirrorData::_eps);
  py::class_<hm::CompactData, dflow::DgModuleData, hm::compactdata_ptr_t>(hmmod, "Compact")  // gc orphan verts + remap
      .def_static("createShared", []() -> hm::compactdata_ptr_t { return hm::CompactData::createShared(); });
  // E.3 — THE ONLY verb writing the LOCKED gid band (__tags [20:32)); both params runtime
  py::class_<hm::GidAssignData, dflow::DgModuleData, hm::gidassigndata_ptr_t>(hmmod, "GidAssign")
      .def_static("createShared", []() -> hm::gidassigndata_ptr_t { return hm::GidAssignData::createShared(); })
      .def_readwrite("gid", &hm::GidAssignData::_gid)
      .def_readwrite("slot", &hm::GidAssignData::_slot);
  // O3 — per-section xatlas unwrap: each gid section gets its OWN 0-1 UV domain + a dense
  // LAYER index in UV0.z (the baked texture-array path). Runs AFTER the gid partition.
  py::class_<hm::SectionUnwrapData, dflow::DgModuleData, hm::sectionunwrapdata_ptr_t>(hmmod, "SectionUnwrap")
      .def_static("createShared", []() -> hm::sectionunwrapdata_ptr_t { return hm::SectionUnwrapData::createShared(); })
      .def_readwrite("padding", &hm::SectionUnwrapData::_padding)       // xatlas chart padding (texels)
      .def_readwrite("max_layers", &hm::SectionUnwrapData::_max_layers); // safety cap; exceeding FAILS LOUD
  // E.6/2.12 — drives a material UBO param BY NAME from the graph (drawable drains
  // the pokeable "value" float plug per-frame; live in every cached pipeline)
  py::class_<hm::MaterialParamSinkData, dflow::DgModuleData, hm::materialparamsinkdata_ptr_t>(hmmod, "MaterialParamSink")
      .def_static("createShared", []() -> hm::materialparamsinkdata_ptr_t { return hm::MaterialParamSinkData::createShared(); })
      .def_readwrite("param_name", &hm::MaterialParamSinkData::_param_name);
  py::class_<hm::BitOpData, dflow::DgModuleData, hm::bitopdata_ptr_t>(hmmod, "BitOp")  // __tags bit-banking
      .def_static("createShared", []() -> hm::bitopdata_ptr_t { return hm::BitOpData::createShared(); })
      .def_readwrite("dst", &hm::BitOpData::_dst)      // all RUNTIME (ctl SSBO) — no recompile on change
      .def_readwrite("a", &hm::BitOpData::_a)
      .def_readwrite("b", &hm::BitOpData::_b)
      .def_readwrite("op", &hm::BitOpData::_op)
      .def_readwrite("width", &hm::BitOpData::_width);

  // ---- GpuMesh handle: the INDEXED attribute mesh. SSBOs returned as non-owning unmanaged_ptr (like
  //      ctx_t for Context) — the GpuMesh (which Python keeps alive) owns the pooled buffers. ----
  auto _ssbo = [](FxShaderStorageBuffer* b) -> fxshaderstoragebuffer_ptr_t {
    return b ? fxshaderstoragebuffer_ptr_t(b) : fxshaderstoragebuffer_ptr_t();
  };
  auto gpumesh_type = //
      py::class_<hm::GpuMesh, hm::gpumesh_ptr_t>(hmmod, "GpuMesh")
          .def_property_readonly("num_verts", [](hm::gpumesh_ptr_t m) -> int { return m->_num_verts; })
          .def_property_readonly("num_corners", [](hm::gpumesh_ptr_t m) -> int { return m->_num_corners; })
          .def_property_readonly("num_faces", [](hm::gpumesh_ptr_t m) -> int { return m->_num_faces; })
          // vertex-channel SSBO by semantic (0=P,1=N,2=B,3=uv,4=color).
          .def(
              "channel_ssbo",
              [_ssbo](hm::gpumesh_ptr_t m, int sem) -> fxshaderstoragebuffer_ptr_t {
                auto ch = m->channel(hm::MeshChannel(sem));
                return ch ? _ssbo(ch->_ssbo) : fxshaderstoragebuffer_ptr_t();
              })
          .def("vidx_ssbo", [_ssbo](hm::gpumesh_ptr_t m) { return m->_vidx ? _ssbo(m->_vidx->_ssbo) : fxshaderstoragebuffer_ptr_t(); })
          .def("face_offsets_ssbo", [_ssbo](hm::gpumesh_ptr_t m) { return m->_face_offsets ? _ssbo(m->_face_offsets->_ssbo) : fxshaderstoragebuffer_ptr_t(); })
          .def("face_ssbo", [_ssbo](hm::gpumesh_ptr_t m, std::string name) -> fxshaderstoragebuffer_ptr_t {
            auto ch = m->face(name);
            return ch ? _ssbo(ch->_ssbo) : fxshaderstoragebuffer_ptr_t();
          })
          .def("header_ssbo", [_ssbo](hm::gpumesh_ptr_t m) { return _ssbo(m->_header); });
  type_codec->registerStdCodec<hm::gpumesh_ptr_t>(gpumesh_type);

  // ---- materialize: sort + instantiate + run the graph (fresh MeshEnv + pow2 pool) -> GpuMesh ----
  hmmod.def(
      "materialize",
      [](dflow::graphdata_ptr_t g, ctx_t ctx, int vtx_budget) -> hm::gpumesh_ptr_t {
        return hm::bakeMesh(g, ctx.get(), vtx_budget);
      },
      py::arg("graph"),
      py::arg("ctx"),
      py::arg("vtx_budget") = (1 << 20));

  // ---- LIVE: persistent GraphInst; recompute() re-evaluates each frame (after a plug change) ----
  auto live_type = //
      py::class_<hm::LiveHypermesh, hm::livehypermesh_ptr_t>(hmmod, "LiveHypermesh")
          .def("recompute", [](hm::livehypermesh_ptr_t l, ctx_t ctx) { l->recompute(ctx.get()); })
          .def_property(
              "paused", // PER-GRAPH clock pause (global = hypermesh.set_clock_paused)
              [](hm::livehypermesh_ptr_t l) -> bool { return l->_paused; },
              [](hm::livehypermesh_ptr_t l, bool p) { l->_paused = p; })
          .def_property_readonly( // accumulated live-clock time (advanced ONLY by the render-path
              "clock_abstime",    // _liveRecompute hook -> observability for the hook-fired gate)
              [](hm::livehypermesh_ptr_t l) -> double { return l->_clock_abstime; })
          .def_property_readonly("mesh", [](hm::livehypermesh_ptr_t l) -> hm::gpumesh_ptr_t { return l->_mesh; })
          .def_property_readonly( // E.2: the graph's InstanceSet count (0 = not an instanced graph)
              "instance_count",
              [](hm::livehypermesh_ptr_t l) -> int { return l->_instances ? l->_instances->_count : 0; })
          .def_property_readonly( // the PUBLISHED meshlet pair (partition + the topology it was built
              "meshlet_partition", // from); None until a build requested by the render hook completes
              [](hm::livehypermesh_ptr_t l) -> hm::meshletpartition_ptr_t {
                return l->_meshlets ? l->_meshlets->partition() : hm::meshletpartition_ptr_t();
              });
  type_codec->registerStdCodec<hm::livehypermesh_ptr_t>(live_type);

  // ---- MESHLETS: the CPU partition of a triangle snapshot into <=256-vert / <=256-prim buckets.
  //      Build side only — no upload, no draw. A partition ALWAYS carries the topology it was built
  //      from (.topology), which is the only way to obtain one: a mismatched pair is unrepresentable.
  // the LIVE bucket caps (platform-derived — see meshlet.h): the codegen must declare exactly
  // these as the mesh stage's output limits, so the gate compares against these, never a literal.
  hmmod.attr("meshlet_max_verts") = int(hm::kMeshletMaxVerts);
  hmmod.attr("meshlet_max_prims") = int(hm::kMeshletMaxPrims);
  py::class_<hm::MeshletStats>(hmmod, "MeshletStats")
      .def_property_readonly("meshlet_count", [](const hm::MeshletStats& s) { return s._meshletCount; })
      .def_property_readonly("tri_count", [](const hm::MeshletStats& s) { return s._triCount; })
      .def_property_readonly("vertex_ref_count", [](const hm::MeshletStats& s) { return s._vertexRefCount; })
      .def_property_readonly("avg_vertex_fill", [](const hm::MeshletStats& s) { return s._avgVertexFill; })
      .def_property_readonly("avg_prim_fill", [](const hm::MeshletStats& s) { return s._avgPrimFill; })
      .def_property_readonly("vertex_reuse", [](const hm::MeshletStats& s) { return s._vertexReuse; })
      .def("report", [](const hm::MeshletStats& s, std::string label) { return s.report(label); },
           py::arg("label") = std::string("mesh"));

  py::class_<hm::MeshletTopology, hm::meshlettopology_ptr_t>(hmmod, "MeshletTopology")
      .def_property_readonly("num_tris", [](hm::meshlettopology_ptr_t t) -> int { return int(t->numTris()); })
      .def_property_readonly("num_verts", [](hm::meshlettopology_ptr_t t) -> int { return t->numVerts(); })
      .def_property_readonly("snapshot_id", [](hm::meshlettopology_ptr_t t) -> uint64_t { return t->snapshotId(); })
      .def_property_readonly("tri_indices", [](hm::meshlettopology_ptr_t t) { return t->_triIndices; })
      .def("isAppendOf", [](hm::meshlettopology_ptr_t t, hm::meshlettopology_ptr_t prior) {
        return t->isAppendOf(*prior);
      });

  py::class_<hm::MeshletPartition, hm::meshletpartition_ptr_t>(hmmod, "MeshletPartition")
      .def_property_readonly("topology", [](hm::meshletpartition_ptr_t p) { return p->topology(); })
      .def_property_readonly("meshlet_count", [](hm::meshletpartition_ptr_t p) -> int { return int(p->_desc.size()); })
      .def_property_readonly( // buckets carried verbatim from the prior partition (append fast path)
          "immutable_count", [](hm::meshletpartition_ptr_t p) -> int { return int(p->_immutableCount); })
      .def_property_readonly( // per bucket: (vertex_offset, vertex_count, prim_offset, prim_count)
          "descriptors",
          [](hm::meshletpartition_ptr_t p) {
            std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> out;
            for (auto& d : p->_desc)
              out.push_back(std::make_tuple(d._vertexOffset, d._vertexCount, d._primOffset, d._primCount));
            return out;
          })
      .def_property_readonly("vertex_list", [](hm::meshletpartition_ptr_t p) { return p->_vertexList; })
      .def_property_readonly( // one uint per triangle: 3 packed 8-bit LOCAL indices
          "prim_indices", [](hm::meshletpartition_ptr_t p) { return p->_primIndices; })
      .def_property_readonly("stats", [](hm::meshletpartition_ptr_t p) { return p->stats(); });

  py::class_<hm::MeshletBuilder, hm::meshletbuilder_ptr_t>(hmmod, "MeshletBuilder")
      .def("step", [](hm::meshletbuilder_ptr_t b, int budget) { return b->step(uint32_t(budget)); },
           py::arg("budget_tris"))
      .def_property_readonly("done", [](hm::meshletbuilder_ptr_t b) { return b->done(); })
      .def_property_readonly("progress", [](hm::meshletbuilder_ptr_t b) { return b->progress(); })
      .def_property_readonly("append_path", [](hm::meshletbuilder_ptr_t b) { return b->isAppendPath(); })
      .def_property_readonly("start_tri", [](hm::meshletbuilder_ptr_t b) -> int { return int(b->startTri()); })
      .def_property_readonly("partition", [](hm::meshletbuilder_ptr_t b) { return b->partition(); });

  // CPU snapshot of a live/baked GpuMesh: READ_ONLY map of vidx + face_offsets, fan-triangulated in
  // face order (deterministic — the GPU triangulator's output order is atomic-cursor dependent).
  hmmod.def(
      "meshletTopologyFromMesh",
      [](hm::gpumesh_ptr_t mesh, ctx_t ctx) { return hm::MeshletTopology::fromMesh(mesh, ctx.get()); },
      py::arg("mesh"),
      py::arg("ctx"));
  hmmod.def(
      "meshletTopologyFromIndices",
      [](std::vector<uint32_t> indices, int num_verts) {
        return hm::MeshletTopology::fromIndices(std::move(indices), num_verts);
      },
      py::arg("indices"),
      py::arg("num_verts"));
  hmmod.def(
      "meshletBuilder",
      [](hm::meshlettopology_ptr_t topo, hm::meshletpartition_ptr_t prior) {
        return hm::MeshletBuilder::create(topo, prior);
      },
      py::arg("topology"),
      py::arg("prior") = hm::meshletpartition_ptr_t());
  hmmod.def(
      "meshletBuild", // create + drive to completion
      [](hm::meshlettopology_ptr_t topo, hm::meshletpartition_ptr_t prior) {
        return hm::MeshletBuilder::buildComplete(topo, prior);
      },
      py::arg("topology"),
      py::arg("prior") = hm::meshletpartition_ptr_t());
  // last 5s-window perf block (the HYPERMESH channel content) — for HUD display (StringDrawable)
  hmmod.def("perfStats", []() -> std::string { return hm::HmPerf::instance().statsText(); });
  // E.6/2.19 — (hits, stores) of the most recent cacheable materialize/bake (cook-cache gates)
  hmmod.def("last_cook_stats", []() -> std::tuple<int, int> {
    return std::make_tuple(hm::hypermeshLastCookHits(), hm::hypermeshLastCookStores());
  });
  // E.5 gate — concave triangulation oracle (C++ asserts; python harness)
  module_lev2.def("hypermesh_triangulation_selftest", [](ctx_t ctx) -> int {
    return hm::hypermeshTriangulationSelfTest(ctx.get());
  });
  // E.6/2.12 gate — material rebind-propagation core (no GPU; failure count)
  module_lev2.def("fxpipeline_rebind_selftest", []() -> int { //
    return fxPipelineRebindSelfTest();
  });
  // E.7/M0 — the sdfgrid family: SdfEval (analytic distance expression -> dense brick)
  // + the M0 oracle gate (analytic readback through the real dispatch machinery).
  py::class_<sdf::SdfEvalData, dflow::DgModuleData, sdf::sdfevaldata_ptr_t>(sdfmod, "SdfEval")
      .def_static("createShared", []() -> sdf::sdfevaldata_ptr_t { return sdf::SdfEvalData::createShared(); })
      .def_readwrite("expression", &sdf::SdfEvalData::_expression);
  // M1 — GPU voxelize (mesh -> dense SDF brick; pseudonormal/winding sign)
  py::class_<sdf::MeshToSdfData, dflow::DgModuleData, sdf::meshtosdfdata_ptr_t>(sdfmod, "MeshToSdf")
      .def_static("createShared", []() -> sdf::meshtosdfdata_ptr_t { return sdf::MeshToSdfData::createShared(); })
      .def_readwrite("sign_mode", &sdf::MeshToSdfData::_sign_mode);
  // M2 — boolean composite of two SDF bricks (union/intersect/subtract/smooth)
  py::class_<sdf::CsgData, dflow::DgModuleData, sdf::csgdata_ptr_t>(sdfmod, "Csg")
      .def_static("createShared", []() -> sdf::csgdata_ptr_t { return sdf::CsgData::createShared(); })
      .def_readwrite("op", &sdf::CsgData::_op);
  // M2 — marching tetrahedra (SDF brick -> indexed GpuMesh)
  py::class_<sdf::SdfToMeshData, dflow::DgModuleData, sdf::sdftomeshdata_ptr_t>(sdfmod, "SdfToMesh")
      .def_static("createShared", []() -> sdf::sdftomeshdata_ptr_t { return sdf::SdfToMeshData::createShared(); })
      .def_readwrite("weld", &sdf::SdfToMeshData::_weld)
      .def_readwrite("blocky", &sdf::SdfToMeshData::_blocky); // CUBERILLE: pure voxel-block surface (forces weld off)
  // M2 — SHAPE-AWARE clean remesh (openvdb adaptive volumeToMesh + optional xatlas UV unwrap)
  py::class_<sdf::SdfToMeshCleanData, dflow::DgModuleData, sdf::sdftomeshcleandata_ptr_t>(sdfmod, "SdfToMeshClean")
      .def_static("createShared", []() -> sdf::sdftomeshcleandata_ptr_t { return sdf::SdfToMeshCleanData::createShared(); })
      .def_readwrite("adaptivity", &sdf::SdfToMeshCleanData::_adaptivity) // 0=max detail .. 1=flattest
      .def_readwrite("isovalue", &sdf::SdfToMeshCleanData::_isovalue)
      .def_readwrite("unwrap", &sdf::SdfToMeshCleanData::_unwrap)          // xatlas UV unwrap (triangulates)
      .def_readwrite("weld_tol", &sdf::SdfToMeshCleanData::_weld_tol);
  // M4a — JFA eikonal redistance (SDF brick -> true |grad|=1 SDF brick)
  py::class_<sdf::RedistanceData, dflow::DgModuleData, sdf::redistancedata_ptr_t>(sdfmod, "Redistance")
      .def_static("createShared", []() -> sdf::redistancedata_ptr_t { return sdf::RedistanceData::createShared(); })
      .def_readwrite("max_iterations", &sdf::RedistanceData::_max_iterations);
  // read the FIRST SdfGrid output of a live graph back to CPU (gates + tooling):
  // returns ((dx,dy,dz), (ox,oy,oz), voxel, [values x-fastest])
  sdfmod.def("read_brick", [](hm::livehypermesh_ptr_t live, ctx_t c) -> py::tuple {
    for (auto inst : live->_ginst->_ordered_module_insts) {
      auto outp = inst->typedOutputNamed<dflowgfx::SdfGridPlugTraits>("Out");
      if (not outp or not outp->_value or not outp->_value->_ssbo)
        continue;
      auto& g  = *outp->_value;
      size_t n = size_t(g._dim[0]) * g._dim[1] * g._dim[2];
      std::vector<float> v(n);
      auto fxi = c.get()->FXI();
      auto m   = fxi->mapStorageBuffer(g._ssbo, 0, n * 4, BufferMapAccess::READ_ONLY);
      std::memcpy(v.data(), m->_mappedaddr, n * 4);
      fxi->unmapStorageBuffer(m.get());
      return py::make_tuple(
          py::make_tuple(g._dim[0], g._dim[1], g._dim[2]),
          py::make_tuple(g._origin[0], g._origin[1], g._origin[2]),
          g._voxel,
          py::cast(v));
    }
    return py::make_tuple(py::none(), py::none(), 0.0f, py::none());
  });
  module_lev2.def("sdfgrid_selftest", [](ctx_t ctx) -> int { //
    return sdf::sdfGridSelfTest(ctx.get());
  });
  module_lev2.def("sdf_voxelize_selftest", [](ctx_t ctx) -> int { //
    return sdf::sdfVoxelizeSelfTest(ctx.get());
  });
  module_lev2.def("sdf_csg_selftest", [](ctx_t ctx) -> int { //
    return sdf::sdfCsgSelfTest(ctx.get());
  });
  module_lev2.def("sdf_mesh_selftest", [](ctx_t ctx) -> int { //
    return sdf::sdfMeshSelfTest(ctx.get());
  });
  module_lev2.def("sdf_redistance_selftest", [](ctx_t ctx) -> int { // M4a
    return sdf::sdfRedistanceSelfTest(ctx.get());
  });
  hmmod.def(
      "materialize_live",
      [](dflow::graphdata_ptr_t g, ctx_t ctx, int vtx_budget) -> hm::livehypermesh_ptr_t {
        return hm::materializeLive(g, ctx.get(), vtx_budget);
      },
      py::arg("graph"),
      py::arg("ctx"),
      py::arg("vtx_budget") = (1 << 20));

  // ---- R-family Tier-B gate seam: read the baked R-graph products (spine / fields /
  //      seed count) back to CPU for the determinism / keepout-zero / flatten-match oracles. ----
  hmmod.def(
      "_roadsReadout",
      [](hm::livehypermesh_ptr_t live, ctx_t ctx) -> py::dict {
        auto R = hm::roadsBakeReadout(live, ctx.get());
        py::dict d;
        d["spine_count"]     = R.spine_count;
        d["spine_positions"] = R.spine_positions;
        py::list parents; for (auto p : R.spine_parents) parents.append(p);
        d["spine_parents"]   = parents;
        d["spine_road_elev"] = R.spine_road_elev;
        d["spine_bytes"]     = py::bytes(R.spine_bytes);
        d["field_dim"]       = R.field_dim;
        d["height_field"]    = R.height_field;
        d["slope_field"]     = R.slope_field;
        d["curv_field"]      = R.curv_field;
        d["disch_field"]     = R.disch_field;
        d["roadbed"]         = R.roadbed;
        d["road_elev_field"] = R.road_elev_field;
        d["keepout"]         = R.keepout;
        d["seed_count"]      = R.seed_count;
        return d;
      },
      py::arg("live"), py::arg("ctx"));

  // ---- DEBUG: read a live GpuMesh back to CPU and write a Wavefront OBJ (positions / uvs / normals +
  //      n-gon faces; binormals as `# b` comment lines). For inspecting actual geometry + recomputed
  //      normals instead of guessing. Map is READ_ONLY; call between frames (e.g. a viewer keypress). ----
  hmmod.def(
      "dump_obj",
      [](hm::gpumesh_ptr_t mesh, ctx_t ctx, std::string path) -> int {
        auto fxi = ctx.get()->FXI();
        int nv = mesh->_num_verts, nf = mesh->_num_faces, nc = mesh->_num_corners;
        // M2.5: for a GPU-resident-count mesh, _num_faces/_num_corners hold the CAPACITY;
        // the LIVE counts are in the header (uint[0]=nv,[1]=nc,[2]=nf). Read them so the
        // dump is exactly the live primitives (not the degenerate capacity tail).
        if (mesh->_gpuResidentCount and mesh->_header) {
          auto hm_ = fxi->mapStorageBuffer(mesh->_header, 0, 12, BufferMapAccess::READ_ONLY);
          uint32_t hdr[3]; std::memcpy(hdr, hm_->_mappedaddr, 12);
          fxi->unmapStorageBuffer(hm_.get());
          nv = int(hdr[0]); nc = int(hdr[1]); nf = int(hdr[2]);
        }
        auto rdf = [&](FxShaderStorageBuffer* b, int n) {
          std::vector<float> v(size_t(std::max(1, n)) * 4, 0.0f);
          if (b) {
            auto m = fxi->mapStorageBuffer(b, 0, size_t(std::max(1, n)) * 16, BufferMapAccess::READ_ONLY);
            std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 16);
            fxi->unmapStorageBuffer(m.get());
          }
          return v;
        };
        auto rdu = [&](FxShaderStorageBuffer* b, int n) {
          std::vector<uint32_t> v(std::max(1, n), 0u);
          if (b) {
            auto m = fxi->mapStorageBuffer(b, 0, size_t(std::max(1, n)) * 4, BufferMapAccess::READ_ONLY);
            std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, n)) * 4);
            fxi->unmapStorageBuffer(m.get());
          }
          return v;
        };
        auto chb  = [&](hm::MeshChannel c) -> FxShaderStorageBuffer* { auto ch = mesh->channel(c); return ch ? ch->_ssbo : nullptr; };
        auto P    = rdf(chb(hm::MeshChannel::POSITION), nv);
        auto N    = rdf(chb(hm::MeshChannel::NORMAL),   nv);
        auto Bn   = rdf(chb(hm::MeshChannel::BINORMAL), nv);
        auto UV   = rdf(chb(hm::MeshChannel::UV0),      nv);
        auto vidx = rdu(mesh->_vidx ? mesh->_vidx->_ssbo : nullptr, nc);
        auto fo   = rdu(mesh->_face_offsets ? mesh->_face_offsets->_ssbo : nullptr, nf + 1);
        std::ofstream out(path);
        out.precision(9); // full float precision so a position-weld / trimesh watertight check on the
                          // dumped OBJ is meaningful (6 sig-figs splits bit-identical shared-edge verts)
        out << "# hypermesh dump  nv=" << nv << " nf=" << nf << " nc=" << nc << "\n";
        for (int i = 0; i < nv; i++) out << "v "  << P[4 * i] << " " << P[4 * i + 1] << " " << P[4 * i + 2] << "\n";
        for (int i = 0; i < nv; i++) out << "vt " << UV[4 * i] << " " << UV[4 * i + 1] << "\n";
        for (int i = 0; i < nv; i++) out << "vn " << N[4 * i] << " " << N[4 * i + 1] << " " << N[4 * i + 2] << "\n";
        for (int i = 0; i < nv; i++) out << "# b " << Bn[4 * i] << " " << Bn[4 * i + 1] << " " << Bn[4 * i + 2] << "\n";
        for (int f = 0; f < nf; f++) {
          out << "f";
          for (uint32_t c = fo[f]; c < fo[f + 1]; c++) { uint32_t vi = vidx[c] + 1u; out << " " << vi << "/" << vi << "/" << vi; }
          out << "\n";
        }
        out.close();
        return nv;
      },
      py::arg("mesh"), py::arg("ctx"), py::arg("path"));

  // ---- E.3 gate probe: read the face __tags channel back (gid = tags>>20 & 0xFFF) ----
  hmmod.def(
      "read_face_tags",
      [](hm::gpumesh_ptr_t mesh, ctx_t ctx) -> std::vector<uint32_t> {
        int nf    = mesh->_num_faces;
        auto tags = mesh->face("__tags");
        std::vector<uint32_t> v(std::max(1, nf), 0u);
        if (tags) {
          auto fxi = ctx.get()->FXI();
          auto m   = fxi->mapStorageBuffer(tags->_ssbo, 0, size_t(std::max(1, nf)) * 4, BufferMapAccess::READ_ONLY);
          std::memcpy(v.data(), m->_mappedaddr, size_t(std::max(1, nf)) * 4);
          fxi->unmapStorageBuffer(m.get());
        }
        v.resize(size_t(std::max(0, nf)));
        return v;
      },
      py::arg("mesh"), py::arg("ctx"));

  // ---- install the render-time triangulator + per-frame IN-FRAME hook (onPreRender) on a
  //      ComputeDrawableData. animated=True re-evaluates the live graph each frame; either way the
  //      mesh is fan-triangulated on-GPU -> DrawIndexedIndirect, all in one in-frame dispatch phase. ----
  // GLOBAL dflow-clock pause: freezes time for EVERY live hypermesh graph (viewer [SPACE]).
  hmmod.def("set_clock_paused", [](bool p) { hm::setClockPaused(p); });
  hmmod.def("clock_paused", []() -> bool { return hm::clockPaused(); });

  hmmod.def(
      "setupMeshRender",
      [_ssbo](computedrawabledata_ptr_t cdd, hm::livehypermesh_ptr_t live, ctx_t ctx, bool animated,
              bool face_viz, bool tag_viz, bool wireframe, int instance_count,
              std::vector<float> instance_matrices, std::vector<int> bound_gids,
              bool cull, fvec4 cull_bound, bool use_graph_instances)
          -> std::tuple<fxshaderstoragebuffer_ptr_t, fxshaderstoragebuffer_ptr_t, fxshaderstoragebuffer_ptr_t> {
        // EXPLICIT INSTANCE SCOPING (make_drawable instance_from): a graph-carried InstanceSet
        // (ScatterSource) is discovered graph-wide during materialize (live->_instances). When the
        // caller has NOT opted THIS drawable into it, hide it for the duration of the build so the
        // mesh renders un-instanced — a road ribbon that shares its graph with an unrelated
        // building-seed set must not tile by the lot transforms (never a graph-wide any-set sniff).
        auto _saved_inst = live->_instances;
        if (not use_graph_instances)
          live->_instances = nullptr;
        auto handles = hm::setupMeshRender(cdd.get(), live, ctx.get(), animated, face_viz, tag_viz, wireframe,
                                           instance_count, instance_matrices, bound_gids, cull, cull_bound);
        live->_instances = _saved_inst;
        // (faceid for the face/tag-viz FS, matrices for storage_inst_mtx, attrs for
        //  storage_inst_attr — the E.2 typed per-instance data) — any may be null.
        return {_ssbo(handles._faceid), _ssbo(handles._instMtx), _ssbo(handles._instAttr)};
      },
      py::arg("cdd"),
      py::arg("live"),
      py::arg("ctx"),
      py::arg("animated") = false,
      py::arg("face_viz") = false,
      py::arg("tag_viz")  = false,
      py::arg("wireframe") = false,
      py::arg("instance_count") = 1,
      py::arg("instance_matrices") = std::vector<float>(),
      py::arg("bound_gids") = std::vector<int>(),    // E.3: gids that get their own per-gid bucket draw
      py::arg("cull") = false,                       // E.4: per-view GPU frustum cull (instanced only)
      py::arg("cull_bound") = fvec4(0, 0, 0, 0),     // object-space sphere; w<=0 = auto (mesh-readback bound)
      py::arg("use_graph_instances") = true);        // false -> ignore the graph's ScatterSource InstanceSet

  // E.3 — build the per-gid BUCKET draws (the make_drawable port of hm_drawable.cpp's bucket loop):
  // one extra indexed-indirect draw per (gid -> material), args offset = gid*20, with that material's
  // OWN storage-block list (the 5 vertex channels + the instance blocks). Call AFTER setupMeshRender
  // (whose bound_gids built the per-gid args slots) — pass its instmtx/instattr handles back in.
  hmmod.def(
      "addGidBuckets",
      [](computedrawabledata_ptr_t cdd, hm::livehypermesh_ptr_t live,
         std::map<int, pbrmaterial_ptr_t> gid_materials, bool instanced,
         fxshaderstoragebuffer_ptr_t instmtx, fxshaderstoragebuffer_ptr_t instattr) {
        static const char* kChanBlocks[5] = {"sif_ptex_vtx", "sif_N", "sif_B", "sif_uv", "sif_clr"};
        for (auto& [gid, gm] : gid_materials) {
          auto gfs = gm->_as_freestyle;
          if (not gfs)
            continue;
          ComputeDrawable::BucketDraw bucket;
          bucket._material   = gm;
          bucket._argsOffset = size_t(gid) * 20;
          for (int i = 0; i < 5; i++) {
            auto block = gfs->storageBlock(kChanBlocks[i]);
            auto chan  = live->_mesh->channel(hypermesh::MeshChannel(i));
            if (block and chan)
              bucket._graphicsStorage.push_back({block, chan->_ssbo});
          }
          if (instanced and instmtx) {
            if (auto blk = gfs->storageBlock("storage_inst_mtx"))
              bucket._graphicsStorage.push_back({blk, instmtx.get()});
            if (instattr)
              if (auto blk = gfs->storageBlock("storage_inst_attr"))
                bucket._graphicsStorage.push_back({blk, instattr.get()});
          }
          cdd->_bucketDraws.push_back(bucket);
        }
      },
      py::arg("cdd"), py::arg("live"), py::arg("gid_materials"), py::arg("instanced") = false,
      py::arg("instmtx") = fxshaderstoragebuffer_ptr_t(), py::arg("instattr") = fxshaderstoragebuffer_ptr_t());

  // O3 stage 2 — the per-section texture-array GPU material bake driver. sectionLayerGids reads the
  // SectionUnwrap layer->gid table (A8; derived from the mesh, cook-load safe); prepareSectionBake registers
  // the in-frame bake one-shot (mirror of the impostor bake) and returns a pollable job whose per-layer host
  // CaptureBuffers the section_bake cache assembles into ONE sampler2DArray layer per section.
  hmmod.def(
      "sectionLayerGids",
      [](hm::livehypermesh_ptr_t live, ctx_t ctx) -> std::vector<int> {
        return hm::sectionUnwrapLayerGids(live, ctx.get());
      },
      py::arg("live"), py::arg("ctx"));
  auto secbake_type = //
      py::class_<hm::SectionBakeJob, hm::sectionbakejob_ptr_t>(hmmod, "SectionBakeJob")
          .def_property_readonly("is_ready", [](hm::sectionbakejob_ptr_t j) -> bool { return j->isReady(); })
          .def_property_readonly("num_layers", [](hm::sectionbakejob_ptr_t j) -> int { return j->numLayers(); })
          .def_property_readonly("num_targets", [](hm::sectionbakejob_ptr_t j) -> int { return j->numTargets(); })
          .def(
              "layerCapture",
              [](hm::sectionbakejob_ptr_t j, int layer, int target) -> capturebuffer_ptr_t {
                return j->layerCapture(layer, target);
              },
              py::arg("layer"), py::arg("target") = 0);
  type_codec->registerStdCodec<hm::sectionbakejob_ptr_t>(secbake_type);
  hmmod.def(
      "prepareSectionBake",
      [](computedrawabledata_ptr_t cdd, hm::livehypermesh_ptr_t live, pbrmaterial_ptr_t material,
         std::vector<int> layer_gids, ctx_t ctx, int bake_res, int num_targets) -> hm::sectionbakejob_ptr_t {
        return hm::prepareSectionBake(ctx.get(), cdd.get(), live, material, layer_gids, bake_res, num_targets);
      },
      py::arg("cdd"), py::arg("live"), py::arg("material"), py::arg("layer_gids"), py::arg("ctx"),
      py::arg("bake_res") = 256, py::arg("num_targets") = 1);

  // ---- D.3: HypermeshDrawableData — the reflected (round-trippable) hypermesh render
  //      description; the C++ port of make_drawable. createDrawable() materializes LAZILY
  //      on the first onGpuUpdate; the material resolves by name (resolved_material here =
  //      the in-process / test assignment path; the ECS component wires a registry resolver). ----
  auto hmdd_type = //
      py::class_<hm::HypermeshDrawableData, DrawableData, hm::hypermesh_drawable_data_ptr_t>(
          module_lev2, "HypermeshDrawableData")
          .def(py::init([](py::kwargs kwargs) {
            auto d = std::make_shared<hm::HypermeshDrawableData>();
            if (kwargs.contains("graph"))
              d->_graphdata = kwargs["graph"].cast<dflow::graphdata_ptr_t>();
            if (kwargs.contains("material_asset"))
              d->_material_asset_name = kwargs["material_asset"].cast<std::string>();
            if (kwargs.contains("animated"))
              d->_animated = kwargs["animated"].cast<bool>();
            if (kwargs.contains("face_viz"))
              d->_face_viz = kwargs["face_viz"].cast<bool>();
            if (kwargs.contains("tag_viz"))
              d->_tag_viz = kwargs["tag_viz"].cast<bool>();
            if (kwargs.contains("wireframe"))
              d->_wireframe = kwargs["wireframe"].cast<bool>();
            if (kwargs.contains("vtx_budget"))
              d->_vtx_budget = kwargs["vtx_budget"].cast<int>();
            if (kwargs.contains("instance_matrices"))
              d->_instance_matrices = kwargs["instance_matrices"].cast<std::vector<float>>();
            // instance_source: a bare name (legacy/vestigial) OR a tuple
            // (scatter_asset, sink[, type_id]) for DRAWABLE-LEVEL resolution (LOD/Phase 2 —
            // the drawable resolves the baked ScatterSet itself; one shared set -> N LOD meshes).
            if (kwargs.contains("instance_source")) {
              auto src = kwargs["instance_source"];
              if (py::isinstance<py::str>(src)) {
                d->_instance_source_name = src.cast<std::string>();
              } else {
                auto t                     = src.cast<py::sequence>();
                d->_instance_scatter_asset = t[0].cast<std::string>();
                d->_instance_sink          = t[1].cast<std::string>();
                d->_instance_type_id       = (py::len(t) > 2) ? t[2].cast<int>() : -1;
              }
            }
            // Phase 3c — DISTANCE LOD: parallel arrays of coarser graphs + ascending distance boundaries.
            if (kwargs.contains("lod_graphs"))
              d->_lod_graphs = kwargs["lod_graphs"].cast<std::vector<dflow::graphdata_ptr_t>>();
            if (kwargs.contains("lod_distances"))
              d->_lod_distances = kwargs["lod_distances"].cast<std::vector<float>>();
            if (kwargs.contains("impostor_lods")) // LOD step #3: extra-tier indices that draw billboards
              d->_impostor_lods = kwargs["impostor_lods"].cast<std::vector<int>>();
            if (kwargs.contains("impostor_grid")) // imposter(grid=): hemi-oct atlas view count
              d->_impostor_grid = kwargs["impostor_grid"].cast<int>();
            if (kwargs.contains("impostor_tile")) // imposter(tile=): per-view atlas tile pixels
              d->_impostor_tile = kwargs["impostor_tile"].cast<int>();
            if (kwargs.contains("impostor_ssaa")) // imposter(ssaa=): bake supersample factor
              d->_impostor_ssaa = kwargs["impostor_ssaa"].cast<int>();
            if (kwargs.contains("impostor_msaa")) // imposter(msaa=): bake multisample count
              d->_impostor_msaa = kwargs["impostor_msaa"].cast<int>();
            if (kwargs.contains("lod_materials")) { // {lod_index: material asset name}
              for (auto item : kwargs["lod_materials"].cast<py::dict>())
                d->_lod_material_assets[std::to_string(item.first.cast<int>())] =
                    item.second.cast<std::string>();
            }
            if (kwargs.contains("instance_ogeo_path")) // direct .ogeo (tools/viewers; wins over asset+sink)
              d->_instance_ogeo_path = kwargs["instance_ogeo_path"].cast<std::string>();
            if (kwargs.contains("instance_type_id"))
              d->_instance_type_id = kwargs["instance_type_id"].cast<int>();
            if (kwargs.contains("cull"))      // E.4: per-view GPU instance cull
              d->_cull = kwargs["cull"].cast<bool>();
            if (kwargs.contains("cull_bound")) // object-space sphere; w<=0 = auto
              d->_cull_bound = kwargs["cull_bound"].cast<fvec4>();
            if (kwargs.contains("cull_slabs"))     // occludee decomposition: 1 = AABB, N = vertical slabs
              d->_cull_slabs = kwargs["cull_slabs"].cast<int>();
            if (kwargs.contains("cull_tightness")) // occludee box scale (<1 culls harder)
              d->_cull_tightness = kwargs["cull_tightness"].cast<float>();
            if (kwargs.contains("cull_distance"))  // radial distance cull from the eye (meters; 0 = off)
              d->_cull_distance = kwargs["cull_distance"].cast<float>();
            if (kwargs.contains("gid_materials")) { // E.3: {gid: material asset name}
              for (auto item : kwargs["gid_materials"].cast<py::dict>()) {
                int gid          = item.first.cast<int>();
                std::string mtl  = item.second.cast<std::string>();
                d->_gid_material_assets[std::to_string(gid)] = mtl;
              }
            }
            // O3 stage 3 — STORED-MODE per-section texture-array bake (opt-in). section_bake flips
            // gid_materials into the per-gid BAKE MAP; material_asset is the stored sampler; section_targets
            // are the capture-target (== array-sampler) names in MRT order.
            if (kwargs.contains("section_bake"))
              d->_section_bake = kwargs["section_bake"].cast<bool>();
            if (kwargs.contains("section_bake_res"))
              d->_section_bake_res = kwargs["section_bake_res"].cast<int>();
            if (kwargs.contains("section_targets"))
              d->_section_targets = kwargs["section_targets"].cast<std::vector<std::string>>();
            return d;
          }))
          .def_property(
              "graph",
              [](hm::hypermesh_drawable_data_ptr_t d) -> dflow::graphdata_ptr_t { return d->_graphdata; },
              [](hm::hypermesh_drawable_data_ptr_t d, dflow::graphdata_ptr_t g) { d->_graphdata = g; })
          .def_property(
              "material_asset",
              [](hm::hypermesh_drawable_data_ptr_t d) -> std::string { return d->_material_asset_name; },
              [](hm::hypermesh_drawable_data_ptr_t d, std::string v) { d->_material_asset_name = v; })
          .def_property(
              "gid_materials", // E.3: {gid:int -> material asset name}
              [](hm::hypermesh_drawable_data_ptr_t d) -> py::dict {
                py::dict out;
                for (const auto& [k, v] : d->_gid_material_assets)
                  out[py::int_(atoi(k.c_str()))] = v;
                return out;
              },
              [](hm::hypermesh_drawable_data_ptr_t d, py::dict mm) {
                d->_gid_material_assets.clear();
                for (auto item : mm)
                  d->_gid_material_assets[std::to_string(item.first.cast<int>())] =
                      item.second.cast<std::string>();
              })
          .def_property(
              "animated",
              [](hm::hypermesh_drawable_data_ptr_t d) -> bool { return d->_animated; },
              [](hm::hypermesh_drawable_data_ptr_t d, bool v) { d->_animated = v; })
          .def_property(
              "face_viz",
              [](hm::hypermesh_drawable_data_ptr_t d) -> bool { return d->_face_viz; },
              [](hm::hypermesh_drawable_data_ptr_t d, bool v) { d->_face_viz = v; })
          .def_property(
              "tag_viz",
              [](hm::hypermesh_drawable_data_ptr_t d) -> bool { return d->_tag_viz; },
              [](hm::hypermesh_drawable_data_ptr_t d, bool v) { d->_tag_viz = v; })
          .def_property(
              "wireframe",
              [](hm::hypermesh_drawable_data_ptr_t d) -> bool { return d->_wireframe; },
              [](hm::hypermesh_drawable_data_ptr_t d, bool v) { d->_wireframe = v; })
          .def_property(
              "vtx_budget",
              [](hm::hypermesh_drawable_data_ptr_t d) -> int { return d->_vtx_budget; },
              [](hm::hypermesh_drawable_data_ptr_t d, int v) { d->_vtx_budget = v; })
          .def_property(
              "instance_matrices",
              [](hm::hypermesh_drawable_data_ptr_t d) -> std::vector<float> { return d->_instance_matrices; },
              [](hm::hypermesh_drawable_data_ptr_t d, std::vector<float> v) { d->_instance_matrices = std::move(v); })
          .def_property(
              "instance_source",
              [](hm::hypermesh_drawable_data_ptr_t d) -> std::string { return d->_instance_source_name; },
              [](hm::hypermesh_drawable_data_ptr_t d, std::string v) { d->_instance_source_name = v; })
          // runtime (never reflected): the resolved live material + the live mesh-graph handle
          .def_property(
              "resolved_material",
              [](hm::hypermesh_drawable_data_ptr_t d) -> pbrmaterial_ptr_t { return d->_resolved_material; },
              [](hm::hypermesh_drawable_data_ptr_t d, pbrmaterial_ptr_t m) { d->_resolved_material = m; })
          .def_property_readonly(
              "live", [](hm::hypermesh_drawable_data_ptr_t d) -> hm::livehypermesh_ptr_t { return d->_live; });
  type_codec->registerStdCodec<hm::hypermesh_drawable_data_ptr_t>(hmdd_type);
}

} // namespace ork::lev2
