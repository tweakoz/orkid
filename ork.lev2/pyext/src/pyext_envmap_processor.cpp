////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/gfx/radiancemaps_processor.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/radiancemaps_asset.h>
#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/python/pycodec.inl>
#include <ork/asset/Asset.inl>
#include <ork/asset/AssetManager.inl>
#include <ork/file/file.h>

namespace py = pybind11;
using namespace ork::lev2;

namespace ork::lev2 {

// Helper: package a list of images as an XTX mipchain datablock
static datablock_ptr_t _imagesToXTXMipchainDataBlock(const std::vector<image_ptr_t>& images) {
  CompressedImageMipChain mipchain;
  for (size_t i = 0; i < images.size(); i++) {
    auto& img = images[i];
    CompressedImage level;
    level._width = img->_width;
    level._height = img->_height;
    level._depth = 1;
    level._format = img->_format;
    level._numcomponents = img->_numcomponents;
    level._bytesPerChannel = img->_bytesPerChannel;
    level._data = img->_data;
    mipchain._levels.push_back(level);
    if (i == 0) {
      mipchain._width = img->_width;
      mipchain._height = img->_height;
      mipchain._depth = 1;
      mipchain._format = img->_format;
      mipchain._numcomponents = img->_numcomponents;
    }
  }
  auto datablock = std::make_shared<DataBlock>();
  mipchain.writeXTX(datablock);
  return datablock;
}

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
        })
      .def_static("processToXIRViaMicrotask",  // MT2 §3 gate b byte-identity harness
        [](const std::string& input_path, const std::string& output_path) -> bool {
          auto future   = EnvMapProcessor::processToXIRDataBlockAsyncViaMicrotask(file::Path(input_path));
          auto xir_data = future->get(); // block for the sliced bake to complete
          if (!xir_data)
            return false;
          return (File::saveDatablock(file::Path(output_path), xir_data) == EFEC_FILE_OK);
        })
      .def_static("processToXIRDataBlockAsyncViaMicrotask",
        [](const std::string& input_path) -> xirprocessfuture_ptr_t {
          return EnvMapProcessor::processToXIRDataBlockAsyncViaMicrotask(
            file::Path(input_path));
        })
      .def_static("writeXIR",
        [](py::list specular_images,
           py::list roughness_values,
           py::list diffuse_images,
           const std::string& output_path) -> bool {
          // Package each specular roughness level as a single-level XTX mipchain
          std::vector<datablock_ptr_t> specular_datablocks;
          for (size_t i = 0; i < specular_images.size(); i++) {
            auto img = specular_images[i].cast<image_ptr_t>();
            specular_datablocks.push_back(_imagesToXTXMipchainDataBlock({img}));
          }
          // Package all diffuse mip levels as one multi-level XTX mipchain (if any)
          datablock_ptr_t diffuse_datablock;
          if (diffuse_images.size() > 0) {
            std::vector<image_ptr_t> diff_imgs;
            for (auto& item : diffuse_images) {
              diff_imgs.push_back(item.cast<image_ptr_t>());
            }
            diffuse_datablock = _imagesToXTXMipchainDataBlock(diff_imgs);
          }
          // Collect roughness floats
          std::vector<float> roughness_vals;
          for (auto& item : roughness_values) {
            roughness_vals.push_back(item.cast<float>());
          }
          // Write XIR
          auto xir_data = xir::XIRWriter::writeXirDatablocksWithArray(
            diffuse_datablock, specular_datablocks, roughness_vals);
          // Save to file
          return (File::saveDatablock(file::Path(output_path), xir_data) == EFEC_FILE_OK);
        })
      .def_static("readXIR",
        [](const std::string& path) -> py::dict {
          // Load file into datablock
          auto datablock = File::loadDatablock(file::Path(path));
          if (!datablock) {
            throw std::runtime_error("Failed to load XIR file: " + path);
          }
          // Parse XIR structure
          auto xir_data = xir::XIRReader::readXirDatablocks(datablock);
          if (!xir_data._valid) {
            throw std::runtime_error("Invalid XIR data in file: " + path);
          }
          // Extract specular images (one per roughness level)
          py::list specular_images;
          int num_roughness_levels = xir_data._num_roughness_levels;
          for (int i = 0; i < num_roughness_levels; i++) {
            CompressedImageMipChain mipchain;
            mipchain.readXTX(xir_data._specular_datablocks[i]);
            auto image = std::make_shared<Image>();
            mipchain._levels[0].convertToImage(*image);
            specular_images.append(image);
          }
          // Collect roughness values
          py::list roughness_values;
          for (auto& r : xir_data._roughness_values) {
            roughness_values.append(r);
          }
          // Extract diffuse images (all mip levels) if present
          py::list diffuse_images;
          if (xir_data._diffuse_data && xir_data._diffuse_data->length() > 0) {
            CompressedImageMipChain diffuse_mipchain;
            diffuse_mipchain.readXTX(xir_data._diffuse_data);
            for (size_t i = 0; i < diffuse_mipchain._levels.size(); i++) {
              auto image = std::make_shared<Image>();
              diffuse_mipchain._levels[i].convertToImage(*image);
              diffuse_images.append(image);
            }
          }
          // Build result dict
          py::dict result;
          result["specular_images"] = specular_images;
          result["roughness_values"] = roughness_values;
          result["diffuse_images"] = diffuse_images;
          result["is_array_format"] = xir_data._is_array_format;
          return result;
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