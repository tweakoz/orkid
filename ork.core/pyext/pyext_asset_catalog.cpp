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

void pyinit_asset_catalog(py::module& module_core) {
  auto type_codec = python::pb11_typecodec_t::instance();

  /////////////////////////////////////////////////////////////////////////////////
  // AssetManifest
  /////////////////////////////////////////////////////////////////////////////////
  auto manifest_type =
      py::class_<AssetManifest, assetmanifest_ptr_t>(module_core, "AssetManifest")
          .def_property_readonly("namespace", &AssetManifest::getNamespace)
          .def_property_readonly("version", &AssetManifest::getVersion)
          .def_property_readonly("assets", &AssetManifest::getAssets)
          .def_static("load_from_file", &AssetManifest::loadFromFile)
          .def_static("parse_from_string", &AssetManifest::parseFromString)
          .def(
              "createAsset",
              [](assetmanifest_ptr_t self,
                 const assetid_t& id,
                 int priority,
                 const std::string& type,
                 const std::string& remote,
                 const std::string& local,
                 const std::string& filename,
                 const platform_list_t& platforms,
                 const assetid_list_t& dependencies) -> assetentry_ptr_t {
                return AssetManifest::createAsset(self, id, priority, type, remote, local, filename, platforms, dependencies);
              },
              py::arg("id"),
              py::arg("priority"),
              py::arg("type"),
              py::arg("remote"),
              py::arg("local"),
              py::arg("filename"),
              py::arg("platforms"),
              py::arg("dependencies"))
          .def("toJson", &AssetManifest::toJson)
          .def_static("fromJson", &AssetManifest::fromJson)
          .def("getCodec", &AssetManifest::getCodec)
          .def("repackage", &AssetManifest::repackage)
          .def("upload", &AssetManifest::upload, 
               py::arg("config"), 
               py::arg("destination_id"))
          .def("__repr__", [](assetmanifest_ptr_t manifest) -> std::string {
            return FormatString(
                "AssetManifest(namespace='%s', version='%s', assets=%zu)",
                manifest->getNamespace().c_str(),
                manifest->getVersion().c_str(),
                manifest->getAssets().size());
          });
  type_codec->registerStdCodec<assetmanifest_ptr_t>(manifest_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetRequest
  /////////////////////////////////////////////////////////////////////////////////
  auto assetreq_type =
      py::class_<AssetRequest, assetreq_ptr_t>(module_core, "AssetRequest")
          .def(py::init<>())
          .def(py::init<const std::string&>(), py::arg("namespace"))
          .def(py::init<const std::string&, const std::string&>(), py::arg("namespace"), py::arg("asset_id"))
          .def_readwrite("namespace", &AssetRequest::_namespace)
          .def_readwrite("asset_id", &AssetRequest::_asset_id)
          .def("is_valid", &AssetRequest::isValid)
          .def(
              "set_progress_callback",
              [](assetreq_ptr_t req, py::function fn) {
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
            return FormatString("AssetRequest(namespace='%s', asset_id='%s')", req->_namespace.c_str(), req->_asset_id.c_str());
          });
  type_codec->registerStdCodec<assetreq_ptr_t>(assetreq_type);



  /////////////////////////////////////////////////////////////////////////////////
  // AssetState enum
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<AssetState>(module_core, "AssetState")
      .value("NOT_AVAILABLE", AssetState::NOT_AVAILABLE)
      .value("QUEUED", AssetState::QUEUED)
      .value("DOWNLOADING", AssetState::DOWNLOADING)
      .value("ASSEMBLING", AssetState::ASSEMBLING)
      .value("VERIFYING", AssetState::VERIFYING)
      .value("CACHED_MEMORY", AssetState::CACHED_MEMORY)
      .value("CACHED_DISK", AssetState::CACHED_DISK)
      .value("FAILED", AssetState::FAILED)
      .value("CORRUPTED", AssetState::CORRUPTED)
      .export_values();

  /////////////////////////////////////////////////////////////////////////////////
  // AssetStatus enum
  /////////////////////////////////////////////////////////////////////////////////
  py::enum_<AssetStatus>(module_core, "AssetStatus")
      .value("OK", AssetStatus::OK)
      .value("NETWORK", AssetStatus::NETWORK)
      .value("DOWNLOAD_FAILED", AssetStatus::DOWNLOAD_FAILED)
      .value("UPLOAD_FAILED", AssetStatus::UPLOAD_FAILED)
      .value("FILE_NOT_FOUND", AssetStatus::FILE_NOT_FOUND)
      .value("PERMISSION", AssetStatus::PERMISSION)
      .value("TIMEOUT", AssetStatus::TIMEOUT)
      .value("CANCELLED", AssetStatus::CANCELLED)
      .value("NOT_FOUND", AssetStatus::NOT_FOUND)
      .value("CHECKSUM", AssetStatus::CHECKSUM)
      .value("DECRYPT_FAILED", AssetStatus::DECRYPT_FAILED)
      .value("DECOMPRESS_FAILED", AssetStatus::DECOMPRESS_FAILED)
      .value("UNSUPPORTED", AssetStatus::UNSUPPORTED)
      .export_values();

  /////////////////////////////////////////////////////////////////////////////////
  // AssetResult
  /////////////////////////////////////////////////////////////////////////////////
  auto result_type = py::class_<AssetResult, assetresult_ptr_t>(module_core, "AssetResult")
                         .def_readonly("data", &AssetResult::_data)
                         .def_readonly("status", &AssetResult::_status)
                         .def_readonly("error_detail", &AssetResult::_error_detail)
                         .def_readonly("download_time", &AssetResult::_download_time)
                         .def_readonly("processing_time", &AssetResult::_processing_time)
                         .def_readonly("bytes_downloaded", &AssetResult::_bytes_downloaded)
                         .def("is_success", &AssetResult::isSuccess)
                         .def("__bool__", &AssetResult::operator bool)
                         .def("__repr__", [](assetresult_ptr_t result) -> std::string {
                           return FormatString("AssetResult(status=%d, bytes=%zu)", (int)result->_status, result->_bytes_downloaded);
                         });
  type_codec->registerStdCodec<assetresult_ptr_t>(result_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ChunkMeta
  /////////////////////////////////////////////////////////////////////////////////
  auto chunk_meta_type = py::class_<ChunkMeta>(module_core, "ChunkMeta")
                             .def(py::init<>())
                             .def_readonly("offset", &ChunkMeta::_offset)
                             .def_readonly("size", &ChunkMeta::_size)
                             .def_readonly("compressed_size", &ChunkMeta::_compressed_size)
                             .def_readonly("hash", &ChunkMeta::_hash);
  type_codec->registerStdCodec<ChunkMeta>(chunk_meta_type);

  /////////////////////////////////////////////////////////////////////////////////
  // ChunkManifest
  /////////////////////////////////////////////////////////////////////////////////
  auto chunk_manifest_type = py::class_<ChunkManifest, chunkmanifest_ptr_t>(module_core, "ChunkManifest")
                                 .def(py::init<>())
                                 .def_readonly_static("chunk_size", &ChunkManifest::chunk_size)
                                 .def_readonly_static("chunk_threshold", &ChunkManifest::chunk_threshold)
                                 .def_readonly("total_size", &ChunkManifest::_total_size)
                                 .def_readonly("file_hash", &ChunkManifest::_file_hash)
                                 .def_readonly("chunks", &ChunkManifest::_chunks)
                                 .def_readonly("compression", &ChunkManifest::_compression)
                                 .def_readonly("is_encrypted", &ChunkManifest::_is_encrypted);
  type_codec->registerStdCodec<chunkmanifest_ptr_t>(chunk_manifest_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetEntry (merged from ManifestEntry)
  /////////////////////////////////////////////////////////////////////////////////
  auto asset_entry_type = py::class_<AssetEntry, assetentry_ptr_t>(module_core, "AssetEntry")
                              .def_readonly("id", &AssetEntry::_id)
                              .def_property_readonly(
                                  "namespace",
                                  [](assetentry_ptr_t e) -> assetnamespace_ptr_t {
                                    return e->_namespace_ptr.lock(); // Convert weak pointer to shared pointer
                                  })
                              .def_property_readonly("fqid", &AssetEntry::buildFullyQualifiedId)
                              .def_readonly("type", &AssetEntry::_type)
                              .def_readonly("priority", &AssetEntry::_priority)
                              .def_readonly("merge", &AssetEntry::_merge)
                              .def_readonly("local_loc", &AssetEntry::_local_loc)
                              .def_readonly("remote_loc", &AssetEntry::_remote_loc)
                              .def_readonly("filename", &AssetEntry::_filename)
                              .def_readonly("relative_path", &AssetEntry::_relative_path)
                              .def_readonly("size", &AssetEntry::_size)
                              .def_readonly("storage_hash", &AssetEntry::_storage_hash)
                              .def_readonly("content_hash", &AssetEntry::_content_hash)
                              .def_readonly("hash_algorithm", &AssetEntry::_hash_algorithm)
                              .def_readonly("modification_time", &AssetEntry::_modification_time)
                              .def_readonly("is_compressed", &AssetEntry::_is_compressed)
                              .def_readonly("is_encrypted", &AssetEntry::_is_encrypted)
                              .def_readonly("compressed_size", &AssetEntry::_compressed_size)
                              .def_readonly("compression_type", &AssetEntry::_compression_type)
                              .def_readonly("platforms", &AssetEntry::_platforms)
                              .def_readonly("namespace_id", &AssetEntry::_namespace)
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
                              .def("is_valid", &AssetEntry::isValid)
                              .def("get_validation_error", &AssetEntry::getValidationError)
                              .def("supports_current_platform", &AssetEntry::supportsCurrentPlatform)
                              .def("is_chunked", &AssetEntry::isChunked)
                              .def("toJson", &AssetEntry::toJson)
                              .def_static("fromJson", &AssetEntry::fromJson)
                              .def("repackage", &AssetEntry::repackage)
                              .def("upload", &AssetEntry::upload, 
                                   py::arg("config"), 
                                   py::arg("destination_id"))
                              .def("__repr__", [](assetentry_ptr_t entry) -> std::string {
                                return FormatString("AssetEntry(filename='%s', size=%zu)", entry->_filename.c_str(), entry->_size);
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
          .def_property_readonly("space", [](assetcatalog_ptr_t self) -> assetconfigspace_ptr_t {
            return self->getConfigSpace();
          })
          .def("register_namespace", &AssetCatalog::registerNamespace)
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
          .def(
              "load_manifest",
              [](assetcatalog_ptr_t self, py::object path) {
                auto as_pystr = py::cast<py::str>(path);
                auto as_str   = as_pystr.cast<std::string>();
                AssetCatalog::loadManifestFromPath(self,as_str);
              })
          .def("add_manifest", &AssetCatalog::addManifest)
          .def("get_manifest", &AssetCatalog::getManifest)
          .def_static("loadFromGlobalManifests", &AssetCatalog::loadFromGlobalManifests)
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
          .def("registerCodec", &AssetCatalog::registerCodec, py::arg("namespace_id"), py::arg("codec"))
          .def("registerCodecWithPassword", &AssetCatalog::registerCodecWithPassword, py::arg("namespace_id"), py::arg("password"))
          .def("codecForNamespace", &AssetCatalog::codecForNamespace, py::arg("namespace_id"))
          .def("clearCodecs", &AssetCatalog::clearCodecs)

          // Asset Retrieval
          .def(
              "get",
              [](assetcatalog_ptr_t catalog, const std::string& asset_id, bool decrypt) -> assetresult_ptr_t {
                py::gil_scoped_release release;
                return catalog->get(asset_id, decrypt);
              },
              py::arg("asset_id"),
              py::arg("decrypt") = true)
          .def("has_asset", &AssetCatalog::hasAsset)
          .def("get_asset_info", &AssetCatalog::getAssetInfo)

          // Asset Queries
          .def("list_assets", &AssetCatalog::listAssets, py::arg("pattern") = "*")
          .def("list_assets_in_namespace", &AssetCatalog::listAssetsInNamespace)
          .def_property_readonly("all_fqids", &AssetCatalog::dumpAllAssetFQIDs)

          // Serialization
          .def("toJson", &AssetCatalog::toJson)
          
          // Repackaging
          .def("repackage", &AssetCatalog::repackage)
          
          // Upload Operations
          .def("upload", &AssetCatalog::upload, py::arg("namespace_id"))
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
          .def_property_readonly("encrypted_dir", [](const assetcatalog_ptr_t& catalog) -> std::string {
            return catalog->getEncryptedDir().toStdString();
          })
          .def_property_readonly("chunks_dir", [](const assetcatalog_ptr_t& catalog) -> std::string {
            return catalog->getChunksDir().toStdString();
          })
          .def_property_readonly("receipts_dir", [](const assetcatalog_ptr_t& catalog) -> std::string {
            return catalog->getReceiptsDir().toStdString();
          })
          .def_property_readonly("temp_dir", [](const assetcatalog_ptr_t& catalog) -> std::string {
            return catalog->getTempDir().toStdString();
          })

          .def("__repr__", [](assetcatalog_ptr_t catalog) -> std::string { return "AssetCatalog()"; });
  type_codec->registerStdCodec<assetcatalog_ptr_t>(catalog_type);

  /////////////////////////////////////////////////////////////////////////////////
  // AssetNamespace
  /////////////////////////////////////////////////////////////////////////////////
  auto namespace_type = py::class_<AssetNamespace, assetnamespace_ptr_t>(module_core, "AssetNamespace")
                            .def(py::init<const std::string&>(), py::arg("id"))
                            .def_property_readonly("id", [](assetnamespace_ptr_t ns) -> std::string {
                              return ns->_id; // Accessing the private _id directly
                            })
                            .def("has_codec", &AssetNamespace::hasCodec)
                            .def("__repr__", [](assetnamespace_ptr_t ns) -> std::string {
                              return FormatString("AssetNamespace(id='%s')", ns->_id.c_str());
                            });
  type_codec->registerStdCodec<assetnamespace_ptr_t>(namespace_type);
} // void pyinit_asset_catalog(py::module& module_core) {
} // namespace ork::asset::catalog