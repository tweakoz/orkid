////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/envmap_processor.h>
#include <ork/lev2/gfx/irradiance_asset.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/python/pycodec.inl>
#include <ork/asset/Asset.inl>
#include <ork/asset/AssetManager.inl>

namespace py = pybind11;
using namespace ork::lev2;

namespace ork::lev2 {

void pyinit_envmap_processor(py::module& module_lev2) {
  
  // XIRProcessFuture bindings
  py::class_<XIRProcessFuture, xirprocessfuture_ptr_t>(module_lev2, "XIRProcessFuture")
      .def("get", &XIRProcessFuture::get)
      .def("isReady", &XIRProcessFuture::isReady);
  
  py::class_<EnvMapProcessor>(module_lev2, "EnvMapProcessor")
      .def_static("processToXIR", 
        [](const std::string& input_path, const std::string& output_path) -> bool {
          return EnvMapProcessor::processToXIR(
            file::Path(input_path), 
            file::Path(output_path));
        })
      .def_static("processToXIRDataBlockAsync",
        [](const std::string& input_path) -> xirprocessfuture_ptr_t {
          return EnvMapProcessor::processToXIRDataBlockAsync(
            file::Path(input_path));
        })
      .def_static("processDirectory",
        [](const std::string& source_dir, 
           const std::string& output_dir,
           py::list extensions) -> py::dict {
          
          std::vector<std::string> ext_vec;
          for (auto item : extensions) {
            ext_vec.push_back(py::cast<std::string>(item));
          }
          
          auto result = EnvMapProcessor::processDirectory(
            file::Path(source_dir),
            file::Path(output_dir),
            ext_vec);
          
          py::dict ret;
          ret["success"] = result._success;
          ret["error_message"] = result._error_message;
          ret["output_size"] = result._output_size;
          ret["processing_time"] = result._processing_time;
          return ret;
        });
        
  // IrradianceMapsAsset bindings
  auto type_codec = py::class_<IrradianceMapsAsset, 
                                asset::Asset,
                                irradianceasset_ptr_t>(
      module_lev2, "IrradianceMapsAsset")
      .def_static("load", [](const std::string& path) {
        auto loadreq = std::make_shared<asset::LoadRequest>(path);
        return asset::AssetManager<IrradianceMapsAsset>::load(loadreq);
      })
      .def_property_readonly("irradiance_maps", 
        [](irradianceasset_ptr_t self) -> pbr::irradiancemaps_ptr_t {
          return self->_irradianceMaps;
        });
}

} // namespace ork::lev2