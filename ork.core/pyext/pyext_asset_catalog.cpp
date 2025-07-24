////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/asset/catalog/asset_manifest.h>
#include <ork/asset/catalog/asset_config.h>
#include <ork/asset/catalog/asset_fetcher.h>

namespace ork::asset::catalog {

void pyinit_asset_catalog(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // AssetManifest
  /////////////////////////////////////////////////////////////////////////////////
  auto manifest_type = py::class_<AssetManifest, assetmanifest_ptr_t>(module_core, "AssetManifest")
    .def_readonly("namespace", &AssetManifest::_namespace)
    .def_readonly("version", &AssetManifest::_version)
    .def_readonly("assets", &AssetManifest::_assets)
    .def_static("load_from_file", &AssetManifest::loadFromFile)
    .def_static("parse_from_string", &AssetManifest::parseFromString)
    .def("__repr__", [](assetmanifest_ptr_t manifest) -> std::string {
      return FormatString("AssetManifest(namespace='%s', version='%s', assets=%zu)", 
        manifest->_namespace.c_str(), manifest->_version.c_str(), manifest->_assets.size());
    });
  type_codec->registerStdCodec<assetmanifest_ptr_t>(manifest_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetRequest
  /////////////////////////////////////////////////////////////////////////////////
  auto assetreq_type = py::class_<AssetRequest, assetreq_ptr_t>(module_core, "AssetRequest")
    .def(py::init<>())
    .def(py::init<const std::string&>(), py::arg("namespace"))
    .def(py::init<const std::string&, const std::string&>(), py::arg("namespace"), py::arg("asset_id"))
    .def_readwrite("namespace", &AssetRequest::_namespace)
    .def_readwrite("asset_id", &AssetRequest::_asset_id)
    .def("is_valid", &AssetRequest::isValid)
    .def("set_progress_callback", [](assetreq_ptr_t req, py::function fn) {
      if (!fn.is_none()) {
        req->_progress_callback._data.makeShared<py::function>(fn);
        req->_progress_callback._item = [req](size_t downloaded, size_t total) {
          auto fn = req->_progress_callback._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(downloaded, total);
        };
      } else {
        req->_progress_callback._item = nullptr;
      }
    })
    .def("__repr__", [](assetreq_ptr_t req) -> std::string {
      return FormatString("AssetRequest(namespace='%s', asset_id='%s')", 
        req->_namespace.c_str(), req->_asset_id.c_str());
    });
  type_codec->registerStdCodec<assetreq_ptr_t>(assetreq_type);

  // AssetEntry nested struct
  py::class_<AssetManifest::AssetEntry>(module_core, "AssetEntry")
    .def_readonly("type", &AssetManifest::AssetEntry::_type)
    .def_readonly("priority", &AssetManifest::AssetEntry::_priority)
    .def_readonly("merge", &AssetManifest::AssetEntry::_merge)
    .def_readonly("dst_loc", &AssetManifest::AssetEntry::_dst_loc)
    .def_readonly("src_loc", &AssetManifest::AssetEntry::_src_loc)
    .def_readonly("filename", &AssetManifest::AssetEntry::_filename)
    .def_readonly("md5", &AssetManifest::AssetEntry::_md5)
    .def_readonly("dependencies", &AssetManifest::AssetEntry::_dependencies)
    .def_readonly("namespace", &AssetManifest::AssetEntry::_namespace)
    .def_readonly("manifest_source", &AssetManifest::AssetEntry::_manifest_source)
    .def("__repr__", [](const AssetManifest::AssetEntry& entry) -> std::string {
      return FormatString("AssetEntry(type='%s', filename='%s', priority=%d)", 
        entry._type.c_str(), entry._filename.c_str(), entry._priority);
    });

  /////////////////////////////////////////////////////////////////////////////////
  // AssetConfig
  /////////////////////////////////////////////////////////////////////////////////
  auto config_type = py::class_<AssetConfig, assetconfig_ptr_t>(module_core, "AssetConfig")
    .def(py::init<>())
    .def_readonly("namespace_keys", &AssetConfig::_namespace_keys)
    .def_readonly("locations", &AssetConfig::_locations)
    .def_readonly("destinations", &AssetConfig::_destinations)
    .def_static("load_from_directory", &AssetConfig::loadFromDirectory)
    .def_static("load_from_file", &AssetConfig::loadFromFile)
    .def("merge", &AssetConfig::merge)
    .def("resolve_path", &AssetConfig::resolvePath)
    .def("resolve_url", &AssetConfig::resolveURL)
    .def("__repr__", [](assetconfig_ptr_t config) -> std::string {
      return FormatString("AssetConfig(namespace_keys=%zu, locations=%zu, destinations=%zu)", 
        config->_namespace_keys.size(), config->_locations.size(), config->_destinations.size());
    });
  type_codec->registerStdCodec<assetconfig_ptr_t>(config_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetFetcher
  /////////////////////////////////////////////////////////////////////////////////
  auto fetcher_type = py::class_<AssetFetcher, assetfetcher_ptr_t>(module_core, "AssetFetcher")
    .def(py::init<>())
    .def(py::init<downloadmanager_ptr_t>())
    .def("fetch_pak", [](assetfetcher_ptr_t fetcher, const std::string& pack_identifier) {
      py::gil_scoped_release release;
      return fetcher->fetchPak(pack_identifier);
    }, py::arg("pack_identifier"))
    .def("set_manifest_directories", &AssetFetcher::setManifestDirectories)
    .def("reload", &AssetFetcher::reload)
    .def("get_assets", &AssetFetcher::getAssets, py::return_value_policy::reference_internal)
    .def("get_config", &AssetFetcher::getConfig, py::return_value_policy::reference_internal)
    .def("on_asset_progress", [](assetfetcher_ptr_t fetcher, py::function fn) {
      if (!fn.is_none()) {
        fetcher->_on_asset_progress._data.makeShared<py::function>(fn);
        fetcher->_on_asset_progress._item = [fetcher](const std::string& asset_id, size_t downloaded, size_t total) {
          auto fn = fetcher->_on_asset_progress._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(asset_id, downloaded, total);
        };
      } else {
        fetcher->_on_asset_progress._item = nullptr;
      }
    })
    .def("on_asset_complete", [](assetfetcher_ptr_t fetcher, py::function fn) {
      if (!fn.is_none()) {
        fetcher->_on_asset_complete._data.makeShared<py::function>(fn);
        fetcher->_on_asset_complete._item = [fetcher](const std::string& asset_id, bool success) {
          auto fn = fetcher->_on_asset_complete._data.getShared<py::function>();
          py::gil_scoped_acquire acquire;
          fn->operator()(asset_id, success);
        };
      } else {
        fetcher->_on_asset_complete._item = nullptr;
      }
    })
    .def_property_readonly("assets", [](assetfetcher_ptr_t fetcher) -> py::dict {
      py::dict result;
      for (const auto& [key, value] : fetcher->getAssets()) {
        result[py::str(key)] = py::str(value._filename);
      }
      return result;
    })
    .def("__repr__", [](assetfetcher_ptr_t fetcher) -> std::string {
      return FormatString("AssetFetcher(assets=%zu)", fetcher->getAssets().size());
    });
  type_codec->registerStdCodec<assetfetcher_ptr_t>(fetcher_type);
}

} // namespace ork::asset::catalog