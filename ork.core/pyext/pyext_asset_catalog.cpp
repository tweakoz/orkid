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
#include <ork/asset/catalog/request.h>

namespace ork::asset::catalog {

void pyinit_asset_catalog(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // CompressionType enum (needed for ChunkManifest.compression return type)
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<CompressionType>(module_core, "CompressionType")
      .value("NONE", CompressionType::NONE)
      .value("LZ4", CompressionType::LZ4)
      .value("LZ4HC", CompressionType::LZ4HC);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetNamespace (needed as return type for find_namespace)
  /////////////////////////////////////////////////////////////////////////////////
  auto namespace_type = py::class_<AssetNamespace, assetnamespace_ptr_t>(module_core, "AssetNamespace");
  type_codec->registerStdCodec<assetnamespace_ptr_t>(namespace_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetManifest
  /////////////////////////////////////////////////////////////////////////////////
  auto manifest_type =
      py::class_<AssetManifest, assetmanifest_ptr_t>(module_core, "AssetManifest")
          .def_property_readonly("namespace", &AssetManifest::getNamespace)
          .def_property_readonly("version", &AssetManifest::getVersion)
          .def_property_readonly("assets", &AssetManifest::getAssets)
          .def_property_readonly("source_file", [](assetmanifest_ptr_t self) -> std::string {
            return self->getSourceFile().toStdString();
          })
          .def_static("loadFromFile", &AssetManifest::loadFromFile)
          .def(
              "createAsset",
              [](assetmanifest_ptr_t self,
                 const assetid_t& id,
                 int priority,
                 const std::string& type,
                 const std::string& local,
                 const platform_list_t& platforms,
                 const assetid_list_t& dependencies,
                 const std::string& tar_root,
                 const std::vector<std::string>& filters) -> assetentry_ptr_t {
                return AssetManifest::createAsset(self, id, priority, type, local, platforms, dependencies, tar_root, filters);
              },
              py::arg("id"),
              py::arg("priority"),
              py::arg("type"),
              py::arg("local"),
              py::arg("platforms"),
              py::arg("dependencies"),
              py::arg("tar_root") = "",
              py::arg("filters") = std::vector<std::string>{})
          .def("toJson", &AssetManifest::toJson)
          .def("__repr__", [](assetmanifest_ptr_t manifest) -> std::string {
            return FormatString(
                "AssetManifest(namespace='%s', version='%s', assets=%zu)",
                manifest->getNamespace().c_str(),
                manifest->getVersion().c_str(),
                manifest->getAssets().size());
          });
  type_codec->registerStdCodec<assetmanifest_ptr_t>(manifest_type);

  /////////////////////////////////////////////////////////////////////////////////
  // FetchRequest
  /////////////////////////////////////////////////////////////////////////////////
  auto request_type = py::class_<FetchRequest, fetchrequest_ptr_t>(module_core, "FetchRequest")
                         .def_readonly("error_detail", &FetchRequest::_error_detail)
                         .def_readonly("bytes_downloaded", &FetchRequest::_bytes_downloaded)
                         .def("wait", [](fetchrequest_ptr_t self) -> bool {
                           py::gil_scoped_release release;
                           return self->wait();
                         })
                         .def_property_readonly("succeeded", [](fetchrequest_ptr_t self) -> bool { return self->isSuccess(); })
                         .def_property_readonly("completed", [](fetchrequest_ptr_t self) -> bool { return self->isComplete(); })
                         .def("__repr__", [](fetchrequest_ptr_t result) -> std::string {
                           return FormatString("FetchRequest(status=%d, bytes=%zu)", (int)result->_status, result->_bytes_downloaded.load());
                         });
  type_codec->registerStdCodec<fetchrequest_ptr_t>(request_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ChunkMeta
  /////////////////////////////////////////////////////////////////////////////////
  auto chunk_meta_type = py::class_<ChunkMeta>(module_core, "ChunkMeta")
                             .def(py::init<>())
                             .def_readonly("offset", &ChunkMeta::_offset)
                             .def_readonly("size", &ChunkMeta::_size)
                             .def_readonly("hash", &ChunkMeta::_hash);
  type_codec->registerStdCodec<ChunkMeta>(chunk_meta_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ChunkManifest
  /////////////////////////////////////////////////////////////////////////////////
  auto chunk_manifest_type = py::class_<ChunkManifest, chunkmanifest_ptr_t>(module_core, "ChunkManifest")
                                 .def(py::init<>())
                                 .def_readonly_static("chunk_size", &ChunkManifest::chunk_size)
                                 .def_readonly("total_size", &ChunkManifest::_total_size)
                                 .def_readonly("file_hash", &ChunkManifest::_file_hash)
                                 .def_readonly("chunks", &ChunkManifest::_chunks);
  type_codec->registerStdCodec<chunkmanifest_ptr_t>(chunk_manifest_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetEntry (merged from ManifestEntry)
  /////////////////////////////////////////////////////////////////////////////////
  auto asset_entry_type = py::class_<AssetEntry, assetentry_ptr_t>(module_core, "AssetEntry")
                              .def_readonly("id", &AssetEntry::_id)
                              .def_readonly("namespace", &AssetEntry::_namespace)
                              .def_readonly("type", &AssetEntry::_type)
                              .def_readonly("priority", &AssetEntry::_priority)
                              .def_readonly("local_loc", &AssetEntry::_local_loc)
                              .def_readonly("archive_size", &AssetEntry::_archive_size)
                              .def_readonly("encrypted_size", &AssetEntry::_encrypted_size)
                              .def_readonly("compressed_size", &AssetEntry::_compressed_size)
                              .def_readonly("storage_hash", &AssetEntry::_storage_hash)
                              .def_readonly("content_hash", &AssetEntry::_content_hash)
                              .def_readonly("hash_algorithm", &AssetEntry::_hash_algorithm)
                              .def_readonly("platforms", &AssetEntry::_platforms)
                              .def_readonly("chunk_manifest", &AssetEntry::_chunk_manifest)
                              .def_property_readonly(
                                  "resolved_local_path",
                                  [](assetentry_ptr_t e) -> py::object {
                                    file::Path resolved_path = e->getResolvedLocalPath();
                                    if (resolved_path.empty()) {
                                      return py::none();
                                    }
                                    return py::str(resolved_path.c_str());
                                  })
                              .def_property_readonly(
                                  "local_encrypted_path",
                                  [](assetentry_ptr_t e) -> py::object {
                                    file::Path encrypted_path = e->getLocalEncryptedPath();
                                    if (encrypted_path.empty()) {
                                      return py::none();
                                    }
                                    return py::str(encrypted_path.c_str());
                                  })
                              .def("supports_current_platform", &AssetEntry::supportsCurrentPlatform)
                              .def("repackage", &AssetEntry::repackage)
                              .def("upload", &AssetEntry::upload, 
                                   py::arg("config"), 
                                   py::arg("destination_id"))
                              .def("__repr__", [](assetentry_ptr_t entry) -> std::string {
                                return FormatString("AssetEntry(id='%s', size=%zu)", entry->_id.c_str(), entry->_archive_size);
                              });
  type_codec->registerStdCodec<assetentry_ptr_t>(asset_entry_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetCatalog - Main Interface
  /////////////////////////////////////////////////////////////////////////////////
  auto catalog_type =
      py::class_<AssetCatalog, assetcatalog_ptr_t>(module_core, "AssetCatalog")
          .def(py::init<>())
          .def(py::init<assetconfigspace_ptr_t>(), py::arg("space") = nullptr)
          // Namespace Management
          .def("find_namespace", &AssetCatalog::findNamespace)
          .def("list_namespaces", &AssetCatalog::listNamespaces, py::arg("pattern") = "*")

          // Manifest Management
          .def(
              "load_manifests_from_path",
              [](assetcatalog_ptr_t self, py::object path) {
                auto as_pystr = py::cast<py::str>(path);
                auto as_str   = as_pystr.cast<std::string>();
                self->loadManifestsFromPath(as_str);
              })
          .def("manifestsForNamespace", &AssetCatalog::manifestsForNamespace)
          .def("findAssetEntry", &AssetCatalog::findAssetEntry)
          .def_static("loadFromGlobalManifests", &AssetCatalog::loadFromGlobalManifests)
          .def_property_readonly_static("instance", [](py::object /* self */) -> assetcatalog_ptr_t { 
              return AssetCatalog::globalInstance(); 
          })
          .def(
              "createManifest",
              [](assetcatalog_ptr_t self,
                 const std::string& id,
                 const std::string& version,
                 const std::string& namespace_id,
                 py::object file) -> assetmanifest_ptr_t {
                auto as_pystr = py::cast<py::str>(file);
                auto as_str   = as_pystr.cast<std::string>();
                return AssetCatalog::createManifest(self, id, version, namespace_id, file::Path(as_str));
              },
              py::arg("id"),
              py::arg("version"),
              py::arg("namespace"),
              py::arg("file"))

          // Codec Management
          .def("registerCodecWithPassword", &AssetCatalog::registerCodecWithPassword, py::arg("namespace_id"), py::arg("password"))

          // Asset Retrieval
          .def(
              "fetch",
              [](assetcatalog_ptr_t catalog, const std::string& asset_id) -> fetchrequest_ptr_t {
                //py::gil_scoped_release release;
                return catalog->fetch(asset_id);
              },
              py::arg("asset_id"))
          .def(
              "fetchAsync",
              [](assetcatalog_ptr_t catalog, const std::string& asset_id) -> fetchrequest_ptr_t {
                py::gil_scoped_release release;
                return catalog->fetchAsync(asset_id);
              },
              py::arg("asset_id"))
          // Asset Queries
          .def("list_assets", &AssetCatalog::listAssets, py::arg("pattern") = "*")
          
          // Upload Operations
          .def("uploadNamespace", &AssetCatalog::uploadNamespace, py::arg("namespace_id"))
          .def("uploadAsset", &AssetCatalog::uploadAsset, py::arg("fq_asset_id"))
          .def("uploadAllNamespaces", &AssetCatalog::uploadAllNamespaces)
          .def_property_readonly("config_space", &AssetCatalog::getConfigSpace)
          .def_property_readonly("merged_config", [](assetcatalog_ptr_t self) -> assetconfig_ptr_t {
            return self->getConfigSpace()->merged();
          })
          // Cache Directory Management
          .def_property("cache_dir",
              [](const assetcatalog_ptr_t& catalog) -> std::string {
                return catalog->getCacheDir().toStdString();
              },
              [](assetcatalog_ptr_t& catalog, const std::string& dir) {
                catalog->setCacheDir(file::Path(dir));
              })

          .def("__repr__", [](assetcatalog_ptr_t catalog) -> std::string { return "AssetCatalog()"; });
  type_codec->registerStdCodec<assetcatalog_ptr_t>(catalog_type);

} // void pyinit_asset_catalog(py::module& module_core) {
} // namespace ork::asset::catalog