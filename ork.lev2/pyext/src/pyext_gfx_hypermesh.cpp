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
#include <ork/lev2/gfx/hypermesh/hmdflow.h>
#include <ork/lev2/gfx/hypermesh/hm_drawable.h> // D.3: the reflected hypermesh drawable description
#include <ork/lev2/gfx/sdf/sdfdflow.h>          // E.7: the sdfgrid family
#include <ork/dataflow/module.inl>              // typedOutputNamed<SdfGridPlugTraits> instantiation (read_brick)
#include <ork/dataflow/plug_inst.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/compute_drawable.h>
#include <fstream>

namespace ork::lev2 {

namespace dflow = dataflow;
namespace hm    = hypermesh;

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
      // MULTI-SEGMENT: `segments` (int, STRUCTURAL -> rebuild) subdivides the lift into N rings; twist (ABSOLUTE
      // radians about the extrusion axis) + scale (ABSOLUTE in-plane profile scale) are per-t (S.t/S.seg) preds.
      .def_readwrite("segments", &hm::ExtrudeFacesData::_segments)
      .def_property("twist_predicate", [](hm::extrudefacesdata_ptr_t e) { return e->_twist_pred; },
                                       [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_twist_pred = p; })
      .def_property("scale_predicate", [](hm::extrudefacesdata_ptr_t e) { return e->_scale_pred; },
                                       [](hm::extrudefacesdata_ptr_t e, std::string p) { e->_scale_pred = p; })
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
              [](hm::livehypermesh_ptr_t l) -> int { return l->_instances ? l->_instances->_count : 0; });
  type_codec->registerStdCodec<hm::livehypermesh_ptr_t>(live_type);
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
              std::vector<float> instance_matrices)
          -> std::tuple<fxshaderstoragebuffer_ptr_t, fxshaderstoragebuffer_ptr_t, fxshaderstoragebuffer_ptr_t> {
        auto handles = hm::setupMeshRender(cdd.get(), live, ctx.get(), animated, face_viz, tag_viz, wireframe,
                                           instance_count, instance_matrices);
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
      py::arg("instance_matrices") = std::vector<float>());

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
            if (kwargs.contains("instance_source"))
              d->_instance_source_name = kwargs["instance_source"].cast<std::string>();
            if (kwargs.contains("cull"))      // E.4: per-view GPU instance cull
              d->_cull = kwargs["cull"].cast<bool>();
            if (kwargs.contains("cull_bound")) // object-space sphere; w<=0 = auto
              d->_cull_bound = kwargs["cull_bound"].cast<fvec4>();
            if (kwargs.contains("gid_materials")) { // E.3: {gid: material asset name}
              for (auto item : kwargs["gid_materials"].cast<py::dict>()) {
                int gid          = item.first.cast<int>();
                std::string mtl  = item.second.cast<std::string>();
                d->_gid_material_assets[std::to_string(gid)] = mtl;
              }
            }
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
