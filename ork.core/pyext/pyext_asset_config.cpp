////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/uploader.h>

namespace ork::asset::catalog {

void pyinit_asset_config(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // LocationInfo
  /////////////////////////////////////////////////////////////////////////////////
  auto location_type =
      py::class_<LocationInfo, locationinfo_ptr_t>(module_core, "LocationInfo")
          .def(py::init<>())
          .def_property_readonly(
              "download_url",
              [](const LocationInfo& loc) -> py::object {
                return py::cast(loc._download_url);
              })
          .def_property_readonly(
              "upload_url",
              [](const LocationInfo& loc) -> py::object {
                  return py::cast(loc._upload_url);
              })
          .def_readonly("api_key_read", &LocationInfo::_api_key_read)
          .def_readonly("api_key_write", &LocationInfo::_api_key_write)
          .def_readwrite("disable_cert_check", &LocationInfo::_disable_cert_check)
          .def_property_readonly(
              "scp_destination",
              [](const LocationInfo& loc) -> py::object {
                if (loc._scp_destination.has_value()) {
                  return py::str(loc._scp_destination.value());
                }
                return py::none();
              })
          .def("get_effective_read_api_key", &LocationInfo::getEffectiveReadApiKey)
          .def("get_effective_write_api_key", &LocationInfo::getEffectiveWriteApiKey)
          .def("__repr__", [](locationinfo_ptr_t loc) -> std::string {
            std::string scp_dest = loc->_scp_destination.has_value() ? loc->_scp_destination.value() : "None";
            auto dl_url = loc->_download_url.toString();
            auto ul_url = loc->_upload_url.toString();
            return FormatString("LocationInfo(dl_url='%s', ul_url='%s', scp_destination='%s')", dl_url.c_str(), ul_url.c_str(), scp_dest.c_str());
          });
  type_codec->registerStdCodec<locationinfo_ptr_t>(location_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetConfig
  /////////////////////////////////////////////////////////////////////////////////
  auto config_type = py::class_<AssetConfig, assetconfig_ptr_t>(module_core, "AssetConfig")
                         .def(py::init<>())
                         .def("merge", &AssetConfig::merge, py::arg("other"))
                         .def(
                             "resolveLocalPath",
                             [](assetconfig_ptr_t self, const std::string& path) -> std::string {
                               return self->resolveLocalPath(path).toAbsolute().c_str();
                             },
                             py::arg("path"))
                         .def(
                             "resolveRemoteLocation",
                             [](assetconfig_ptr_t self, const std::string& location) -> locationinfo_ptr_t {
                               return self->resolveRemoteLocation(location);
                             },
                             py::arg("location"))
                         .def("getEncryptionKeyForNamespace", &AssetConfig::getEncryptionKeyForNamespace, py::arg("namespace_id"))
                         .def("getRemoteLocationForNamespace", &AssetConfig::getRemoteLocationForNamespace, py::arg("namespace_id"))
                         .def(
                             "addNamespace",
                             &AssetConfig::addNamespace,
                             py::arg("namespace_id"),
                             py::arg("encryption_key"),
                             py::arg("remote_location"))
                         .def("addRemoteLocation", &AssetConfig::addRemoteLocation, py::arg("id"), py::arg("loc"))
                         .def("addLocalLocation", &AssetConfig::addLocalLocation, py::arg("id"), py::arg("loc"))
                         .def_property_readonly(
                             "destinations",
                             [](assetconfig_ptr_t self) -> py::dict {
                               py::dict destinations;
                               for (const auto& [key, path] : self->_local_locations) {
                                 destinations[py::str(key)] = py::str(path.c_str());
                               }
                               return destinations;
                             })
                         .def_static(
                             "loadFromDirectory",
                             [](py::object path) -> assetconfig_ptr_t {
                               auto as_pystr = py::cast<py::str>(path);
                               auto as_str   = as_pystr.cast<std::string>();
                               return AssetConfig::loadFromDirectory(file::Path(as_str));
                             },
                             py::arg("dir"))
                         .def_static(
                             "loadFromFile",
                             [](py::object path) -> assetconfig_ptr_t {
                               auto as_pystr = py::cast<py::str>(path);
                               auto as_str   = as_pystr.cast<std::string>();
                               return AssetConfig::loadFromFile(file::Path(as_str));
                             },
                             py::arg("file"))
                         .def("__repr__", [](assetconfig_ptr_t config) -> std::string {
                           return FormatString(
                               "AssetConfig(namespaces=%zu, locations=%zu, destinations=%zu)",
                               config->_namespaces.size(),
                               config->_remote_locations.size(),
                               config->_local_locations.size());
                         });
  type_codec->registerStdCodec<assetconfig_ptr_t>(config_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetConfigSpace
  /////////////////////////////////////////////////////////////////////////////////
  auto configspace_type =
      py::class_<AssetConfigSpace, assetconfigspace_ptr_t>(module_core, "AssetConfigSpace")
          .def(py::init<>())
          .def(
              "createConfig",
              [](AssetConfigSpace* self, const std::string& id, py::object path) -> assetconfig_ptr_t {
                auto as_pystr = py::cast<py::str>(path);
                auto as_str   = as_pystr.cast<std::string>();
                return self->createConfig(id, file::Path(as_str));
              },
              py::arg("id"),
              py::arg("file"))
          .def("getConfig", &AssetConfigSpace::getConfig, py::arg("id"))
          .def_property_readonly("merged_config", [](assetconfigspace_ptr_t self) -> assetconfig_ptr_t { return self->merged(); })
          .def("getNamespaceRemoteLocation", &AssetConfigSpace::getNamespaceRemoteLocation, py::arg("namespace_id"))
          .def("writeToDisk", &AssetConfigSpace::writeToDisk)
          .def(
              "loadConfigFromDisk",
              [](assetconfigspace_ptr_t self, py::object path) -> assetconfig_ptr_t {
                auto as_pystr = py::cast<py::str>(path);
                auto as_str   = as_pystr.cast<std::string>();
                return AssetConfigSpace::loadConfigFromDisk(self, as_str);
              },
              py::arg("path"))
          .def_static(
              "loadFromDisk",
              [](py::list paths) -> assetconfigspace_ptr_t {
                path_list_t path_list;
                for (auto item : paths) {
                  auto as_pystr = py::cast<py::str>(item);
                  auto as_str   = as_pystr.cast<std::string>();
                  path_list.push_back(file::Path(as_str));
                }
                return AssetConfigSpace::loadFromDisk(path_list);
              },
              py::arg("paths"))
          .def_static("loadGlobalConfigs", &AssetConfigSpace::loadGlobalConfigs)
          .def("__repr__", [](assetconfigspace_ptr_t space) -> std::string {
            return FormatString("AssetConfigSpace(%p)", (void*)space.get());
          });
  type_codec->registerStdCodec<assetconfigspace_ptr_t>(configspace_type);
}

} // namespace ork::asset::catalog