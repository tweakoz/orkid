////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/radiancemaps_asset.h>
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

ImplementReflectionX(ork::lev2::RadianceMapsAsset, "RadianceMapsAsset");

namespace ork::lev2 {
extern context_ptr_t gloadercontext;

///////////////////////////////////////////////////////////////////////////////

// Forward declaration
void registerRadianceLoader();

void RadianceMapsAsset::describeX(class_t* clazz) {
  // Register the loader for XIR files
  registerRadianceLoader();
}

///////////////////////////////////////////////////////////////////////////////

RadianceMapsLoader::RadianceMapsLoader() {
  // Register for .xir extension
  // Note: The registration needs to happen when the loader is created,
  // typically during static initialization
}

///////////////////////////////////////////////////////////////////////////////

asset::asset_ptr_t RadianceMapsLoader::_doLoadFromDatablock(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t dblock) {
  return _loadFromXIR(loadreq, dblock);
}

///////////////////////////////////////////////////////////////////////////////

asset::asset_ptr_t RadianceMapsLoader::_loadFromXIR(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t xir_data) {
  // Use XIRReader to get raw datablocks
  auto xir_data_result = xir::XIRReader::readXirDatablocks(xir_data);
  
  if (!xir_data_result._valid) {
    printf("XIR data invalid\n");
    return nullptr;
  }
  
  if (!xir_data_result._is_array_format) {
    printf("ERROR: XIR v1 legacy format no longer supported. Please regenerate radiance maps.\n");
    return nullptr;
  }
  
  // Create asset
  auto asset = std::make_shared<RadianceMapsAsset>();
  auto irrmaps = std::make_shared<pbr::RadianceMaps>();
  asset->_radiance_maps = irrmaps;
  
  printf("XIR v2 array format: diffuse size: %zu, %d roughness levels\n", 
         xir_data_result._diffuse_data->length(),
         xir_data_result._num_roughness_levels);
  
  // Extract base name from asset path for texture naming
  std::string base_name = loadreq->_asset_path.getName();
  
  // Parse diffuse data
  auto diffuse_cmipchain = std::make_shared<CompressedImageMipChain>();
  diffuse_cmipchain->readXTX(xir_data_result._diffuse_data);
  
  auto diffuse_tex = std::make_shared<Texture>();
  diffuse_tex->_debugName = base_name + ".ibldiff";
  
  // Parse specular roughness array
  std::vector<image_ptr_t> specular_images;
  int num_roughness_levels = xir_data_result._num_roughness_levels;
  
  for(int i = 0; i < num_roughness_levels; i++) {
    // Read single-level XTX for each roughness
    auto cmipchain = std::make_shared<CompressedImageMipChain>();
    cmipchain->readXTX(xir_data_result._specular_datablocks[i]);
    
    // Convert to Image for texture array
    auto image = std::make_shared<Image>();
    cmipchain->_levels[0].convertToImage(*image);
    specular_images.push_back(image);
  }
  
  // Create texture array for specular
  auto specular_texarray = std::make_shared<TextureArray>();
  specular_texarray->_tex->_debugName = base_name + ".iblspec_array";
  
  // Set radiance map properties
  irrmaps->_numRoughnessLevels = num_roughness_levels;
  irrmaps->_specularRoughnessValues = xir_data_result._roughness_values;
  
  // Queue GPU operations
  // Diffuse upload
  auto diffuse_loadreq = std::make_shared<TexLoadReq>();
  diffuse_loadreq->ptex = diffuse_tex;
  diffuse_loadreq->_cmipchain = diffuse_cmipchain;
  diffuse_loadreq->_texname = base_name + ".irrdiff";
  
  GfxEnv::GetRef().enqueueDeferredContextOp(
    [diffuse_loadreq](Context* ctx) {
      auto txi = ctx->TXI();
      txi->_createFromLoadReq(diffuse_loadreq);
    });
    
  // Specular array upload (using existing method like PBRMaterial)
  GfxEnv::GetRef().enqueueDeferredContextOp(
    [specular_texarray, specular_images, num_roughness_levels](Context* ctx) {
      
      // Use EXISTING initTextureArray2DFromData (same as PBRMaterial)
      TextureArrayInitData TID;
      TID._slices.resize(num_roughness_levels);
      
      for(int i = 0; i < num_roughness_levels; i++) {
        // Use CRC for usage ID, similar to PBRMaterial
        uint32_t usage_id = CrcString(FormatString("roughness_%d", i).c_str()).hashed();
        TID._slices[i] = TextureArrayInitSubItem{usage_id, specular_images[i]};
      }
      
      auto txi = ctx->TXI();
      txi->initTextureArray2DFromData(specular_texarray.get(), TID);
    });
  
  // Set asset path
  asset->_name = loadreq->_asset_path.toStdString();
  
  auto brdfIntegrationMapGGX = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"GGX");
  auto brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"GGXVELVET");
  auto brdfIntegrationMapRim = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"GGXRIM");
  auto brdfIntegrationMapBlinn = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"BLINN");
  auto brdfIntegrationMapPhong = PBRMaterial::brdfIntegrationMap(gloadercontext.get(),"PHONG");

  irrmaps->_filtenvDiffuseMap = diffuse_tex;
  irrmaps->_filtenvSpecularMapArray = specular_texarray;  // Use array instead of single texture
  irrmaps->_brdfIntegrationMapGGX = brdfIntegrationMapGGX;
  irrmaps->_brdfIntegrationMapVelvet = brdfIntegrationMapVelvet;
  irrmaps->_brdfIntegrationMapGGXRIM = brdfIntegrationMapRim;
  irrmaps->_brdfIntegrationMapBlinn = brdfIntegrationMapBlinn;
  irrmaps->_brdfIntegrationMapPhong = brdfIntegrationMapPhong;

  printf("XIR asset<%p> irrmaps<%p> dtex<%p> stexarray<%p> roughness_levels<%d>\n", 
         (void*) asset.get(), (void*) irrmaps.get(), diffuse_tex.get(), 
         specular_texarray.get(), num_roughness_levels);

  return asset;
}

///////////////////////////////////////////////////////////////////////////////

// Static loader instance - will be created during static initialization

// Registration function to be called during initialization
void registerRadianceLoader() {
  static auto _Radiance_loader = std::make_shared<RadianceMapsLoader>();
  asset::registerLoader<RadianceMapsAsset>(_Radiance_loader);
  asset::AssetLoader::registerLoaderForExtension("xir", _Radiance_loader);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2

template struct ork::asset::AssetManager<ork::lev2::RadianceMapsAsset>;
