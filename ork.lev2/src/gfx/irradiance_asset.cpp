////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/irradiance_asset.h>
#include <ork/lev2/gfx/xir_format.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/txi.h>
#include <ork/lev2/gfx/image.h>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/asset/catalog/catalog.h>
#include <ork/kernel/datacache.h>
#include <ork/asset/AssetManager.h>
#include <ork/asset/Asset.inl>
#include <ork/rtti/RTTIX.inl>

ImplementReflectionX(ork::lev2::IrradianceMapsAsset, "IrradianceMapsAsset");

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void IrradianceMapsAsset::describeX(class_t* clazz) {
  // reflection setup if needed
}

///////////////////////////////////////////////////////////////////////////////

IrradianceMapsLoader::IrradianceMapsLoader() {
  // Register for .xir extension
  // Note: The registration needs to happen when the loader is created,
  // typically during static initialization
}

///////////////////////////////////////////////////////////////////////////////

asset::asset_ptr_t IrradianceMapsLoader::_doLoadFromDatablock(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t dblock) {
  return _loadFromXIR(loadreq, dblock);
}

///////////////////////////////////////////////////////////////////////////////

asset::asset_ptr_t IrradianceMapsLoader::_loadFromXIR(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t xir_data) {
  
  // Parse XIR format
  chunkfile::DefaultLoadAllocator allocator;
  auto xir_reader = std::make_shared<chunkfile::Reader>(xir_data, allocator);
  
  // Verify XIR format
  if (xir_reader->_chunkfiletype != "xir-1.0") {
    return nullptr;
  }
  
  // Get streams
  auto diffuse_stream = xir_reader->GetStream("diffuse");
  auto specular_stream = xir_reader->GetStream("specular");
  
  if (!diffuse_stream || !specular_stream) {
    return nullptr;
  }
  
  // Create asset
  auto asset = std::make_shared<IrradianceMapsAsset>();
  asset->_irradianceMaps = std::make_shared<pbr::IrradianceMaps>();
  
  // Create textures (CPU only, no GPU resources yet)
  auto diffuse_tex = std::make_shared<Texture>();
  auto specular_tex = std::make_shared<Texture>();
  
  // For now, we'll deserialize the compressed image data
  // The actual deserialization depends on how textures are stored in the datablock
  // This is a simplified version - actual implementation may need adjustment
  // based on how CompressedImageMipChain serialization works
  
  // Create texture load requests for deferred GPU upload
  auto diffuse_data = diffuse_stream->readData(diffuse_stream->GetLength());
  auto diffuse_dblock = std::make_shared<DataBlock>(diffuse_data.data(), diffuse_data.size());
  
  auto specular_data = specular_stream->readData(specular_stream->GetLength());
  auto specular_dblock = std::make_shared<DataBlock>(specular_data.data(), specular_data.size());
  
  // Extract base name from asset path for texture naming
  std::string base_name = loadreq->_asset_path.getName();
  
  // Parse XTX data into CompressedImageMipChains
  auto diffuse_cmipchain = std::make_shared<CompressedImageMipChain>();
  diffuse_cmipchain->readXTX(diffuse_dblock);
  
  auto specular_cmipchain = std::make_shared<CompressedImageMipChain>();
  specular_cmipchain->readXTX(specular_dblock);
  
  auto diffuse_loadreq = std::make_shared<TexLoadReq>();
  diffuse_loadreq->ptex = diffuse_tex;
  diffuse_loadreq->_cmipchain = diffuse_cmipchain;
  diffuse_loadreq->_texname = base_name + ".irrdiff";
  
  auto specular_loadreq = std::make_shared<TexLoadReq>();
  specular_loadreq->ptex = specular_tex;
  specular_loadreq->_cmipchain = specular_cmipchain;
  specular_loadreq->_texname = base_name + ".irrspec";
  
  // Queue deferred GPU upload operations to GfxEnv
  // These will be executed at the beginning of the next frame with a context
  GfxEnv::GetRef().enqueueDeferredContextOp(
    [diffuse_loadreq](Context* ctx) {
      auto txi = ctx->TXI();
      txi->_createFromLoadReq(diffuse_loadreq);
    });
    
  GfxEnv::GetRef().enqueueDeferredContextOp(
    [specular_loadreq](Context* ctx) {
      auto txi = ctx->TXI();
      txi->_createFromLoadReq(specular_loadreq);
    });
  
  // Store textures in asset
  asset->_irradianceMaps->_filtenvDiffuseMap = diffuse_tex;
  asset->_irradianceMaps->_filtenvSpecularMap = specular_tex;
  
  // Set asset path
  asset->_name = loadreq->_asset_path.toStdString();
  
  return asset;
}

///////////////////////////////////////////////////////////////////////////////

// Static loader instance - will be created during static initialization
static auto _irradiance_loader = std::make_shared<IrradianceMapsLoader>();

// Registration function to be called during initialization
void registerIrradianceLoader() {
  asset::registerLoader<IrradianceMapsAsset>(_irradiance_loader);
  asset::AssetLoader::registerLoaderForExtension("xir", _irradiance_loader);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2