////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/radiancemaps_processor.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/radiancemaps_asset.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/python/pycodec.inl>
#include <ork/asset/Asset.inl>
#include <ork/asset/AssetManager.inl>

namespace py = pybind11;
using namespace ork::lev2;

namespace ork::lev2 {

void pyinit_radiance_maps_processor(py::module& module_lev2) {
  
  // XIRProcessFuture bindings
  py::class_<XIRProcessFuture, xirprocessfuture_ptr_t>(module_lev2, "XIRProcessFuture")
      .def("get", &XIRProcessFuture::get)
      .def("isReady", &XIRProcessFuture::isReady)
      .def_property_readonly("specular_images", 
        [](xirprocessfuture_ptr_t self) -> image_list_t {
          return self->_specular_images;
        })
      .def_property_readonly("diffuse_images",
        [](xirprocessfuture_ptr_t self) -> image_list_t {
          return self->_diffuse_images;
        });
  
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
        });
        
  // RadianceMapsAsset bindings
  auto type_codec = py::class_<RadianceMapsAsset, 
                                asset::Asset,
                                Radianceasset_ptr_t>(
      module_lev2, "RadianceMapsAsset")
      .def_static("load", [](const std::string& path) {
        auto loadreq = std::make_shared<asset::LoadRequest>(path);
        return asset::AssetManager<RadianceMapsAsset>::load(loadreq);
      })
      .def_property_readonly("Radiance_maps", 
        [](Radianceasset_ptr_t self) -> pbr::radiancemaps_ptr_t {
          return self->_radiance_maps;
        });
}

} // namespace ork::lev2