////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/namespace.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/config.h>
#include <ork/asset/catalog/uploader.h>
#include <ork/asset/catalog/request.h>
#include <ork/kernel/string/deco.inl>
#include <ork/file/file.h>
#include <ork/object/ObjectClass.h>
#include <ork/util/md5.h>
#include <ork/util/xxhash.inl>
#include <ork/util/upload.h>
#include <ork/util/logger.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/writer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/filesystem.hpp>
#include <sys/stat.h>
#include <ctime>
#include "catalog_impl.h"

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////////////////////
// AssetManifestImpl - Pimpl implementation
////////////////////////////////////////////////////////////////////////////////

struct AssetManifestImpl {
  std::string _manifest_id;         // Unique identifier for this manifest
  namespaceid_t _namespace;
  std::string _version;
  asset_entry_map_t _assets;
  
  // Optional metadata
  std::string _description;
  time_t _creation_time = 0;
  std::string _creator;             // Tool/person that created this manifest
  asset_metadata_map_t _meta_data;  // Custom metadata
  file::Path _source_file;           // Path to JSON file this manifest was loaded from
  
  // Parent catalog reference (for accessing ConfigSpace)
};

////////////////////////////////////////////////////////////////////////////////

AssetManifest::AssetManifest() {
  _impl.makeShared<AssetManifestImpl>();
  auto impl = _impl.getShared<AssetManifestImpl>();
  // Generate unique manifest ID
  boost::uuids::uuid uuid = object::ObjectClass::genUUID();
  impl->_manifest_id = boost::uuids::to_string(uuid);
}

AssetManifest::AssetManifest(assetcatalog_wkptr_t parent_catalog) {
  _impl.makeShared<AssetManifestImpl>();
  auto impl = _impl.getShared<AssetManifestImpl>();
  // Generate unique manifest ID
  boost::uuids::uuid uuid = object::ObjectClass::genUUID();
  impl->_manifest_id = boost::uuids::to_string(uuid);
  // Store parent catalog reference
  _parent_catalog = parent_catalog;
}

AssetManifest::~AssetManifest() {
}

////////////////////////////////////////////////////////////////////////////////
// AssetManifest::upload() - Upload all assets in manifest
////////////////////////////////////////////////////////////////////////////////

uploadreceipt_ptr_t AssetManifest::upload(
    const AssetConfig& config,
    locationinfo_ptr_t location_info) const {
  
  logchan_catalog->log("Starting manifest upload - ID: %s, namespace: %s, destination: %s, assets: %zu",
                       getManifestId().c_str(), getNamespace().c_str(), location_info->_upload_url.toString().c_str(), getAssets().size());
  
  // Create a combined receipt for all assets
  auto manifest_receipt = std::make_shared<UploadReceipt>();
  manifest_receipt->upload_id = getManifestId() + "_manifest";
  manifest_receipt->destination = location_info->_download_url.toString();
  manifest_receipt->timestamp = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
  manifest_receipt->success = true;  // Start optimistic
  manifest_receipt->total_files = 0;
  manifest_receipt->bytes_uploaded = 0;
  
  std::vector<std::string> failed_assets;
  std::vector<std::string> successful_assets;
  
  // Upload each asset in the manifest
  for (const auto& [_asset_id, asset_entry] : getAssets()) {
    logchan_catalog->log("Uploading asset: %s", _asset_id.c_str());
    
    try {
      auto asset_receipt = asset_entry->upload(config, location_info);
      
      if (asset_receipt && asset_receipt->success) {
        // Asset upload successful
        successful_assets.push_back(_asset_id);
        manifest_receipt->total_files += asset_receipt->total_files;
        manifest_receipt->bytes_uploaded += asset_receipt->bytes_uploaded;
      } else {
        logchan_catalog->log("ERROR: Asset upload failed: %s%s", _asset_id.c_str(),
                             asset_receipt ? (" - " + asset_receipt->status_message).c_str() : "");
        failed_assets.push_back(_asset_id);
        manifest_receipt->success = false;
      }
    } catch (const std::exception& e) {
      logchan_catalog->log("ERROR: Asset upload exception: %s - %s", _asset_id.c_str(), e.what());
      failed_assets.push_back(_asset_id);
      manifest_receipt->success = false;
    }
  }
  
  // Set final status
  if (manifest_receipt->success) {
    manifest_receipt->status_message = FormatString(
      "All %zu assets uploaded successfully", successful_assets.size());
    logchan_catalog->log("Manifest upload SUCCESS - uploaded %zu assets (%zu bytes total)", 
                         successful_assets.size(), manifest_receipt->bytes_uploaded);
  } else {
    manifest_receipt->status_message = FormatString(
      "%zu/%zu assets failed to upload", 
      failed_assets.size(), getAssets().size());
    logchan_catalog->log("ERROR: Manifest upload PARTIAL FAILURE - successful: %zu, failed: %zu", 
                         successful_assets.size(), failed_assets.size());
    for (const auto& failed_id : failed_assets) {
      logchan_catalog->log("  Failed asset: %s", failed_id.c_str());
    }
  }
  
  logchan_catalog->log("Completed manifest upload for ID: %s", getManifestId().c_str());
  return manifest_receipt;
}

////////////////////////////////////////////////////////////////////////////////

// Accessor implementations
const std::string& AssetManifest::getManifestId() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  return impl->_manifest_id;
}

const namespaceid_t& AssetManifest::getNamespace() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  return impl->_namespace;
}

encryptioncodec_ptr_t AssetManifest::getCodec() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  auto catalog = _parent_catalog.lock();
  if (catalog) {
    return catalog->codecForNamespace(impl->_namespace);
  }
  return nullptr;
}

const std::string& AssetManifest::getVersion() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  return impl->_version;
}

const asset_entry_map_t& AssetManifest::getAssets() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  return impl->_assets;
}

const asset_metadata_map_t& AssetManifest::getMetadata() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  return impl->_meta_data;
}

const file::Path& AssetManifest::getSourceFile() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  return impl->_source_file;
}

void AssetManifest::setNamespace(const namespaceid_t& ns) {
  auto impl = _impl.getShared<AssetManifestImpl>();
  impl->_namespace = ns;
}

void AssetManifest::setVersion(const std::string& version) {
  auto impl = _impl.getShared<AssetManifestImpl>();
  impl->_version = version;
}

void AssetManifest::setDescription(const std::string& desc) {
  auto impl = _impl.getShared<AssetManifestImpl>();
  impl->_description = desc;
}

void AssetManifest::setCreator(const std::string& creator) {
  auto impl = _impl.getShared<AssetManifestImpl>();
  impl->_creator = creator;
}

void AssetManifest::setCreationTime(time_t time) {
  auto impl = _impl.getShared<AssetManifestImpl>();
  impl->_creation_time = time;
}

void AssetManifest::addAsset(const assetid_t& id, assetentry_ptr_t entry) {
  auto impl = _impl.getShared<AssetManifestImpl>();
  impl->_assets[id] = entry;
}

////////////////////////////////////////////////////////////////////////////////

assetconfig_ptr_t AssetManifest::getConfig() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  assetconfig_ptr_t rval = nullptr;
  auto parent_catalog = _parent_catalog.lock();
  if (parent_catalog) {
    auto config_space = parent_catalog->getConfigSpace();
    if (config_space) {
      rval = config_space->getConfig(impl->_namespace);
      if (!rval) {
        rval = config_space->getConfig("default");
      }
    }
  }
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

assetentry_ptr_t AssetManifest::createAsset(
    assetmanifest_ptr_t self,
    const assetid_t& id,
    int priority,
    const std::string& type,
    const std::string& local,
    const platform_list_t& platforms,
    const assetid_list_t& dependencies,
    const std::string& tar_root,
    const std::vector<std::string>& filters) {
  
  auto impl = self->_impl.getShared<AssetManifestImpl>();
  auto catalog = self->_parent_catalog.lock();
  OrkAssert(catalog != nullptr);
  namespaceid_t namespace_id = self->getNamespace();
  auto fqid_str = AssetCatalog::buildAssetId(namespace_id, id);
  // Create new asset entry
  auto index_entry = catalog->findAssetIndexEntry(fqid_str);
  if(nullptr==index_entry){
    index_entry = catalog->_addNewAssetIndexEntry(fqid_str);
  }
  index_entry->_manifest = self;
  auto asset_info = index_entry->_entry;
  OrkAssert(asset_info != nullptr);
  // Set basic properties
  asset_info->_parent_manifest = self;
  asset_info->_id = id;
  asset_info->_priority = priority;
  asset_info->_type = type;
  asset_info->_local_loc = local;
  asset_info->_tar_root = tar_root;  // Set tar_root from parameter
  asset_info->_filters = filters;     // Set filters from parameter
  asset_info->_platforms = platforms;
  // Convert dependency list to map format
  // Store each dependency with itself as the key for now
  for (const auto& dep : dependencies) {
    asset_info->_dependencies[dep] = dep;
  }
  
  // Set namespace from manifest
  asset_info->_namespace = impl->_namespace;
  
  // Generate UUID
  boost::uuids::uuid uuid = object::ObjectClass::genUUID();
  // TODO: Add UUID field to AssetEntry if needed
  
  // Resolve local path to find the actual file
  // The 'local' parameter contains the template path like "<cache>/my_local_asset_dir"
  // The 'filename' parameter contains just the filename like "my_asset_file.txt"
  // We need to resolve the template and combine with filename
  
  // Resolve template paths using AssetConfigSpace
  std::string resolved_local = local;
  if( auto cfg = self->getConfig() ) {
    resolved_local = cfg->resolveLocalPath(local).toStdString();
  }
    
  // If no resolution happened, do basic template resolution as fallback
  if (resolved_local.find("<stage>") == 0) {
    file::Path stage_path = file::Path::stage_dir();
    resolved_local = stage_path.toAbsolute().toStdString() + resolved_local.substr(7);
  } else if (resolved_local.find("<cache>") == 0 || resolved_local.find("<assetcache>") == 0) {
    file::Path cache_path = file::Path::stage_dir() / "assetcache";
    size_t template_len = resolved_local.find("<cache>") == 0 ? 7 : 12;
    resolved_local = cache_path.toAbsolute().toStdString() + resolved_local.substr(template_len);
  }
  
  file::Path local_dir(resolved_local);
  
  // Only asset_pak type is supported
  if (type != "asset_pak") {
    logchan_catalog->log("ERROR: Only asset_pak type is supported. Asset type: %s", type.c_str());
    OrkAssert(false);
  }
  
  // Determine source directory using tar_root field
  file::Path source_dir;
  if (tar_root.empty()) {
    // No tar_root - use local_dir directly
    source_dir = local_dir;
  } else {
    // Use explicit tar_root
    source_dir = local_dir / tar_root;
  }
  
  if (source_dir.doesPathExist()) {
    // Directory exists - the TAR will be created during repackage
    asset_info->_hash_algorithm = "md5";
    
    // Call repackage which will create the TAR and compute hashes
    asset_info->repackage();
  } else {
    // Directory doesn't exist - this is an error
    logchan_catalog->log("ERROR: Asset pak directory does not exist! Looking for: %s (local_dir: %s, tar_root: %s)",
                         source_dir.c_str(), local_dir.c_str(), tar_root.c_str());
    OrkAssert(false);
  }
  
  // Add to manifest
  self->addAsset(id, asset_info);
  
  return asset_info;
}

////////////////////////////////////////////////////////////////////////////////

assetmanifest_ptr_t AssetManifest::loadFromFile(const file::Path& path) {
  // Read file contents
  std::ifstream file(path.c_str());
  if (!file.is_open()) {
    return nullptr;
  }
  
  std::stringstream buffer;
  buffer << file.rdbuf();
  file.close();
  
  return parseFromString(buffer.str(), path);
}

////////////////////////////////////////////////////////////////////////////////

assetmanifest_ptr_t AssetManifest::parseFromString(const std::string& json_str, const file::Path& source_file) {
  auto manifest = std::make_shared<AssetManifest>();
  manifest->parseFromJsonInternal(json_str, source_file);
  auto impl = manifest->_impl.getShared<AssetManifestImpl>();
  for( auto & [id, entry] : impl->_assets ) {
    entry->_parent_manifest = manifest; // Set parent manifest for each asset
  }
  return manifest;
}

////////////////////////////////////////////////////////////////////////////////

void AssetManifest::parseFromJsonInternal(const std::string& json_str, const file::Path& source_file) {
  auto impl = _impl.getShared<AssetManifestImpl>();
  impl->_source_file = source_file;  // Store the source file path
  rapidjson::Document doc;
  
  // Parse JSON
  doc.Parse(json_str.c_str());
  
  if (doc.HasParseError()) {
    auto error_code = doc.GetParseError();
    auto error_offset = doc.GetErrorOffset();
    logchan_catalog->log("ERROR: JSON parse error at offset %zu: %s", error_offset, rapidjson::GetParseError_En(error_code));
    return;
  }
  
  ///////////////////////////////////////////////////////////
  // Check for new format (has namespace field)
  ///////////////////////////////////////////////////////////
  if (doc.HasMember("namespace") && doc["namespace"].IsString()) {
    // New format
    impl->_namespace = doc["namespace"].GetString();
    
    // Parse manifest_id if present, otherwise keep the gene_rated one
    if (doc.HasMember("manifest_id") && doc["manifest_id"].IsString()) {
      impl->_manifest_id = doc["manifest_id"].GetString();
    }
    
    if (doc.HasMember("version") && doc["version"].IsString()) {
      impl->_version = doc["version"].GetString();
    }
    
    if (doc.HasMember("assets") && doc["assets"].IsObject()) {
      const auto& assets = doc["assets"];
      
      for (auto it = assets.MemberBegin(); it != assets.MemberEnd(); ++it) {
        std::string _asset_id = it->name.GetString();
        const auto& asset_data = it->value;
        
        if (!asset_data.IsObject()) continue;
        
        AssetEntry entry;
        entry._id = _asset_id;                        // Set the asset ID
        entry._namespace = impl->_namespace;
        entry._manifest_source = source_file.c_str();

        ///////////////////////////////////////////////////////////
        // Parse asset fields
        ///////////////////////////////////////////////////////////
        if (asset_data.HasMember("type") && asset_data["type"].IsString()) {
          entry._type = asset_data["type"].GetString();
        }
        
        if (asset_data.HasMember("priority") && asset_data["priority"].IsInt()) {
          entry._priority = asset_data["priority"].GetInt();
        }
        
        if (asset_data.HasMember("merge") && asset_data["merge"].IsBool()) {
          entry._merge = asset_data["merge"].GetBool();
        }
        
        if (asset_data.HasMember("local_loc") && asset_data["local_loc"].IsString()) {
          entry._local_loc = asset_data["local_loc"].GetString();
        }
        
        
        // filename field no longer used
        
        if (asset_data.HasMember("tar_root") && asset_data["tar_root"].IsString()) {
          entry._tar_root = asset_data["tar_root"].GetString();
        }
        
        // Handle both "storage_hash" and legacy "md5" fields
        if (asset_data.HasMember("storage_hash") && asset_data["storage_hash"].IsString()) {
          entry._storage_hash = asset_data["storage_hash"].GetString();
          
          // Parse hash algorithm if specified
          if (asset_data.HasMember("hash_algorithm") && asset_data["hash_algorithm"].IsString()) {
            entry._hash_algorithm = asset_data["hash_algorithm"].GetString();
            // Validate hash algorithm - only MD5 supported for now
            OrkAssert(entry._hash_algorithm == "md5" && "Only MD5 hash algorithm is currently supported");
          }
        } else if (asset_data.HasMember("md5") && asset_data["md5"].IsString()) {
          entry._storage_hash = asset_data["md5"].GetString();
          entry._hash_algorithm = "md5";
        }
        
        // Handle content hash
        if (asset_data.HasMember("content_hash") && asset_data["content_hash"].IsString()) {
          entry._content_hash = asset_data["content_hash"].GetString();
        }
        
        // Parse native_size (original uncompressed size)
        entry._archive_size = asset_data["archive_size"].GetUint64();
        entry._encrypted_size = asset_data["encrypted_size"].GetInt64();
        entry._compressed_size = asset_data["compressed_size"].GetInt64();
        
        ///////////////////////////////////////////////////////////
        // Parse platforms
        ///////////////////////////////////////////////////////////
        if (asset_data.HasMember("platforms") && asset_data["platforms"].IsArray()) {
          const auto& platforms = asset_data["platforms"];
          for (rapidjson::SizeType i = 0; i < platforms.Size(); ++i) {
            if (platforms[i].IsString()) {
              entry._platforms.push_back(platforms[i].GetString());
            }
          }
        }
        
        ///////////////////////////////////////////////////////////
        // Parse dependencies
        ///////////////////////////////////////////////////////////
        if (asset_data.HasMember("dependencies") && asset_data["dependencies"].IsObject()) {
          const auto& deps = asset_data["dependencies"];
          for (auto dep_it = deps.MemberBegin(); dep_it != deps.MemberEnd(); ++dep_it) {
            if (dep_it->value.IsString()) {
              entry._dependencies[dep_it->name.GetString()] = dep_it->value.GetString();
            }
          }
        }
        
        ///////////////////////////////////////////////////////////
        // Parse chunk manifest if present
        ///////////////////////////////////////////////////////////
        if (asset_data.HasMember("chunks") && asset_data["chunks"].IsObject()) {
          entry._chunk_manifest = std::make_shared<ChunkManifest>();
          entry._chunk_manifest->fromJson(&asset_data["chunks"]);
        }
        
        impl->_assets[_asset_id] = std::make_shared<AssetEntry>(entry);
      }
    }
  }
}

////////////////////////////////////////////////////////////////////////////////

bool AssetManifest::saveToFile(const file::Path& path) const {
  std::string json = toJson();
  
  std::ofstream file(path.c_str());
  if (!file.is_open()) {
    return false;
  }
  
  file << json;
  file.close();
  
  return true;
}

////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////

void AssetManifest::merge(const AssetManifest& other) {
  auto impl = _impl.getShared<AssetManifestImpl>();
  auto other_impl = other._impl.getShared<AssetManifestImpl>();
  
  // Merge assets with priority resolution
  for (const auto& [_asset_id, asset_entry] : other_impl->_assets) {
    auto it = impl->_assets.find(_asset_id);
    if (it != impl->_assets.end()) {
      // Asset exists - check priority
      if (asset_entry->_priority < it->second->_priority) {
        impl->_assets[_asset_id] = asset_entry;
      }
    } else {
      // New asset
      impl->_assets[_asset_id] = asset_entry;
    }
  }
  
  // Merge metadata
  for (const auto& [key, value] : other_impl->_meta_data) {
    impl->_meta_data[key] = value;
  }
}

////////////////////////////////////////////////////////////////////////////////

size_t AssetManifest::getTotalSize() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  size_t total = 0;
  for (const auto& [id, entry] : impl->_assets) {
    total += entry->_archive_size;
  }
  return total;
}

////////////////////////////////////////////////////////////////////////////////

size_t AssetManifest::getTotalCompressedSize() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  size_t total = 0;
  for (const auto& [id, entry] : impl->_assets) {
    total += entry->_compressed_size;
  }
  return total;
}

////////////////////////////////////////////////////////////////////////////////

asset_type_count_map_t AssetManifest::countAssetsByType() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  asset_type_count_map_t counts;
  for (const auto& [id, entry] : impl->_assets) {
    counts[entry->_type]++;
  }
  return counts;
}

////////////////////////////////////////////////////////////////////////////////

std::string AssetManifest::toJson() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  
  rapidjson::Document doc;
  doc.SetObject();
  auto& allocator = doc.GetAllocator();
  
  // Add id (use namespace as id for backward compatibility)
  doc.AddMember("id", rapidjson::Value(impl->_namespace.c_str(), allocator), allocator);
  
  // Add uuid (use manifest_id as uuid)
  doc.AddMember("uuid", rapidjson::Value(impl->_manifest_id.c_str(), allocator), allocator);
  
  // Add namespace
  doc.AddMember("namespace", rapidjson::Value(impl->_namespace.c_str(), allocator), allocator);
  
  // Add version
  doc.AddMember("version", rapidjson::Value(impl->_version.c_str(), allocator), allocator);
  
  // Add assets object
  rapidjson::Value assets_obj(rapidjson::kObjectType);
  
  for (const auto& [_asset_id, entry] : impl->_assets) {
    rapidjson::Value asset_obj(rapidjson::kObjectType);
    
    // Basic fields
    asset_obj.AddMember("type", rapidjson::Value(entry->_type.c_str(), allocator), allocator);
    asset_obj.AddMember("priority", entry->_priority, allocator);
    asset_obj.AddMember("local_loc", rapidjson::Value(entry->_local_loc.c_str(), allocator), allocator);
    // filename field no longer written
    
    // Add tar_root if not empty (for asset_pak)
    if (!entry->_tar_root.empty()) {
      asset_obj.AddMember("tar_root", rapidjson::Value(entry->_tar_root.c_str(), allocator), allocator);
    }
    
    // Platforms
    rapidjson::Value platforms_array(rapidjson::kArrayType);
    for (const auto& platform : entry->_platforms) {
      platforms_array.PushBack(rapidjson::Value(platform.c_str(), allocator), allocator);
    }
    asset_obj.AddMember("platforms", platforms_array, allocator);
    
    // Dependencies - convert map back to list
    rapidjson::Value deps_array(rapidjson::kArrayType);
    for (const auto& [dep_id, dep_value] : entry->_dependencies) {
      deps_array.PushBack(rapidjson::Value(dep_id.c_str(), allocator), allocator);
    }
    asset_obj.AddMember("dependencies", deps_array, allocator);
    
    // Hash info
    asset_obj.AddMember("content_hash", rapidjson::Value(entry->_content_hash.c_str(), allocator), allocator);
    asset_obj.AddMember("storage_hash", rapidjson::Value(entry->_storage_hash.c_str(), allocator), allocator);
    asset_obj.AddMember("hash_algorithm", rapidjson::Value(entry->_hash_algorithm.c_str(), allocator), allocator);
    
    // Size info
    asset_obj.AddMember("archive_size", static_cast<uint64_t>(entry->_archive_size), allocator);
    asset_obj.AddMember("encrypted_size", static_cast<uint64_t>(entry->_encrypted_size), allocator);
    asset_obj.AddMember("compressed_size", static_cast<uint64_t>(entry->_compressed_size), allocator);
    
    // Chunk info if present - use ChunkManifest's own serialization
    if (entry->_chunk_manifest) {
      rapidjson::Value chunks_obj(rapidjson::kObjectType);
      entry->_chunk_manifest->toJson(&chunks_obj, &allocator);
      asset_obj.AddMember("chunks", chunks_obj, allocator);
    }
    
    assets_obj.AddMember(rapidjson::Value(_asset_id.c_str(), allocator), asset_obj, allocator);
  }
  
  doc.AddMember("assets", assets_obj, allocator);
  
  // Convert to pretty printed string
  rapidjson::StringBuffer buffer;
  rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
  writer.SetIndent(' ', 2);
  doc.Accept(writer);
  
  return buffer.GetString();
}

////////////////////////////////////////////////////////////////////////////////
// AssetManifest::getParentCatalog() - Get parent catalog
////////////////////////////////////////////////////////////////////////////////

assetcatalog_ptr_t AssetManifest::getParentCatalog() const {
  auto impl = _impl.getShared<AssetManifestImpl>();
  return _parent_catalog.lock();
}

////////////////////////////////////////////////////////////////////////////////
// AssetManifest::repackage() - Repackage all assets in manifest
////////////////////////////////////////////////////////////////////////////////

void AssetManifest::repackage() {
  auto impl = _impl.getShared<AssetManifestImpl>();
  if (!impl) {
    return;
  }
  
  logchan_catalog->log("Repackaging manifest '%s' with %zu assets...", 
                       impl->_manifest_id.c_str(), impl->_assets.size());
  
  // Ite_rate through all assets and repackage each one
  for (auto& [_asset_id, entry] : impl->_assets) {
    logchan_catalog->log("  Repackaging asset: %s", _asset_id.c_str());
    
    // Call repackage on the individual asset
    if (entry) {
      entry->repackage();
    }
  }
  
  logchan_catalog->log("Manifest repackaging complete.");
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::getParentManifest() - Get parent manifest
////////////////////////////////////////////////////////////////////////////////

assetmanifest_ptr_t AssetEntry::getParentManifest() const {
  return _parent_manifest.lock();
}

////////////////////////////////////////////////////////////////////////////////
// AssetEntry::getCatalog() - Get catalog from parent manifest
////////////////////////////////////////////////////////////////////////////////

assetcatalog_ptr_t AssetEntry::getCatalog() const {
  auto parent = getParentManifest();
  if (parent) {
    return parent->getParentCatalog();
  }
  return nullptr;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork::asset::catalog
