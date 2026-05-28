////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/ecs/AssetSystem.h>

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
          });
}

} // namespace ork::ecs
