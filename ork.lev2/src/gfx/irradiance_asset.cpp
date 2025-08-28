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
#include <ork/util/hexdump.inl>

ImplementReflectionX(ork::lev2::IrradianceMapsAsset, "IrradianceMapsAsset");

namespace ork::lev2 {
extern context_ptr_t gloadercontext;

///////////////////////////////////////////////////////////////////////////////

// Forward declaration
void registerIrradianceLoader();

void IrradianceMapsAsset::describeX(class_t* clazz) {
  // Register the loader for XIR files
  registerIrradianceLoader();
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
  // Use XIRReader to get raw datablocks
  auto xir_data_result = xir::XIRReader::readXirDatablocks(xir_data);
  
  if (!xir_data_result._valid) {
    printf("XIR data invalid\n");
    return nullptr;
  }
  
  printf("XIR data valid, diffuse size: %zu, specular size: %zu\n", 
         xir_data_result._diffuse_data->length(),
         xir_data_result._specular_data->length());
  
  // Debug: Check first few bytes of diffuse data to see format
  if (xir_data_result._diffuse_data->length() > 64) {
    printf("Diffuse data first 64 bytes:\n");
    hexdumpbytes(xir_data_result._diffuse_data->data(), 64);
  }
  
  // Create asset
  auto asset = std::make_shared<IrradianceMapsAsset>();
  auto irrmaps = std::make_shared<pbr::IrradianceMaps>();
  asset->_irradianceMaps = irrmaps;
  
  // Create textures (CPU only, no GPU resources yet)
  auto diffuse_tex = std::make_shared<Texture>();
  auto specular_tex = std::make_shared<Texture>();
  
  // Extract base name from asset path for texture naming
  std::string base_name = loadreq->_asset_path.getName();
  
  // Parse XTX data into CompressedImageMipChains
  auto diffuse_cmipchain = std::make_shared<CompressedImageMipChain>();
  diffuse_cmipchain->readXTX(xir_data_result._diffuse_data);
  
  auto specular_cmipchain = std::make_shared<CompressedImageMipChain>();
  specular_cmipchain->readXTX(xir_data_result._specular_data);
  
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
  
  // Set asset path
  asset->_name = loadreq->_asset_path.toStdString();
  
  auto brdfIntegrationMapGGX = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"GGX");
  auto brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"GGXVELVET");
  auto brdfIntegrationMapRim = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"GGXRIM");
  auto brdfIntegrationMapBlinn = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"BLINN");
  auto brdfIntegrationMapPhong = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"PHONG");

  irrmaps->_filtenvDiffuseMap = diffuse_tex;
  irrmaps->_filtenvSpecularMap = specular_tex;
  irrmaps->_brdfIntegrationMapGGX = brdfIntegrationMapGGX;
  irrmaps->_brdfIntegrationMapVelvet = brdfIntegrationMapVelvet;
  irrmaps->_brdfIntegrationMapGGXRIM = brdfIntegrationMapRim;
  irrmaps->_brdfIntegrationMapBlinn = brdfIntegrationMapBlinn;
  irrmaps->_brdfIntegrationMapPhong = brdfIntegrationMapPhong;

  return asset;
}

///////////////////////////////////////////////////////////////////////////////

// Static loader instance - will be created during static initialization

// Registration function to be called during initialization
void registerIrradianceLoader() {
  static auto _irradiance_loader = std::make_shared<IrradianceMapsLoader>();
  asset::registerLoader<IrradianceMapsAsset>(_irradiance_loader);
  asset::AssetLoader::registerLoaderForExtension("xir", _irradiance_loader);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

template struct ork::asset::AssetManager<ork::lev2::IrradianceMapsAsset>;
