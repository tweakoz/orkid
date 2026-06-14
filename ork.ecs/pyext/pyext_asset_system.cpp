////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/AssetSystem.h>
#include <ork/lev2/gfx/material_pbr.inl>    // materializeAll returns live materials
#include <ork/lev2/gfx/hypermesh/hmdflow.h> // ... and LiveHypermesh artifacts (D.3)

// Pyext for HYPERECS M2b.4 AssetSystemData.
//
// Exposes the SystemData type with a declareAssetGen(gen) method and
// a `gens` read accessor so the Python Scene-side asset registry can
// (a) attach reflected gen-data instances onto the SystemData living
// in scene_data.systemDatas, and (b) iterate them after deserialize
// (M2b.5 acid).

namespace ork::ecs {

void pyinit_asset_system(py::module& module_ecs) {
  auto type_codec = python::pb11_typecodec_t::instance();

  py::class_<AssetSystemData, SystemData, asset_system_data_ptr_t>(module_ecs, "AssetSystemData")
      .def(py::init<>([]() { return std::make_shared<AssetSystemData>(); }))
      .def("__repr__", [](asset_system_data_ptr_t d) -> std::string {
        fxstring<128> fxs;
        fxs.format("ecs::AssetSystemData(%p, ngens=%zu)", d.get(), d->_gens.size());
        return fxs.c_str();
      })
      .def(
          "declareAssetGen",
          [](asset_system_data_ptr_t d, lev2::assetgendata_ptr_t gen) {
            d->declareAssetGen(gen);
          },
          R"doc(
        Append a reflected AssetGenData to the system's ordered gen
        list. Name uniqueness is enforced by the Scene-side wrapper
        (Scene.asset namespace) before the gen reaches here.
     )doc")
      .def_property_readonly(
          "gens",
          [](asset_system_data_ptr_t d) -> std::vector<lev2::assetgendata_ptr_t> {
            return d->_gens;
          })
      .def( // D.1 stage 3: the C++ WIRE STEP — materialize every C++-supported gen in declaration
            // order; returns {asset_name: artifact} (live PBRMaterial / heightfield manifest path).
            // Gens still on the Python wire path are skipped (with a notice) during the transition.
          "materializeAll",
          [](asset_system_data_ptr_t d, ork::python::unmanaged_ptr<::ork::lev2::Context> ctx) -> py::dict {
            varmap::VarMap artifacts;
            d->materializeAll(ctx.get(), artifacts);
            py::dict out;
            for (const auto& item : artifacts._themap) {
              if (auto as_mtl = item.second.tryAs<lev2::pbrmaterial_ptr_t>())
                out[py::str(item.first)] = py::cast(as_mtl.value());
              else if (auto as_str = item.second.tryAs<std::string>())
                out[py::str(item.first)] = py::str(as_str.value());
              else if (auto as_live = item.second.tryAs<lev2::hypermesh::livehypermesh_ptr_t>())
                out[py::str(item.first)] = py::cast(as_live.value());
            }
            return out;
          });
}

} // namespace ork::ecs
