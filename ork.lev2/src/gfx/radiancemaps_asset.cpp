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
  
  // Create asset
  auto asset = std::make_shared<RadianceMapsAsset>();
  auto irrmaps = std::make_shared<pbr::RadianceMaps>();
  asset->_radiance_maps = irrmaps;
  asset->_name = loadreq->_asset_path.toStdString();
  std::string base_name = loadreq->_asset_path.getName();

  auto op = [=](){

      // Use XIRReader to get raw datablocks
    auto xir_data_result = xir::XIRReader::readXirDatablocks(xir_data);
    
    if (!xir_data_result._valid) {
      printf("XIR data invalid\n");
      loadreq->_assetStatus = "NoData"_crcu;
      if(loadreq->_on_load_failed) loadreq->_on_load_failed();
      return;
    }
    
    if (!xir_data_result._is_array_format) {
      printf("ERROR: XIR v1 legacy format no longer supported. Please regenerate radiance maps.\n");
      loadreq->_assetStatus = "InvalidFormat"_crcu;
      if(loadreq->_on_load_failed) loadreq->_on_load_failed();
      return;
    }

    bool has_diffuse = xir_data_result._diffuse_data && xir_data_result._diffuse_data->length() > 0;

    if(0)printf("XIR v2 array format: diffuse size: %zu, %d roughness levels\n",
           has_diffuse ? xir_data_result._diffuse_data->length() : 0,
           xir_data_result._num_roughness_levels);

    ////////////////////////////////////
    // Parse diffuse data (if present)
    ////////////////////////////////////

    std::shared_ptr<CompressedImageMipChain> diffuse_cmipchain;
    std::shared_ptr<Texture> diffuse_tex;
    if (has_diffuse) {
      diffuse_cmipchain = std::make_shared<CompressedImageMipChain>();
      diffuse_cmipchain->readXTX(xir_data_result._diffuse_data);
      diffuse_tex = std::make_shared<Texture>();
      diffuse_tex->_debugName = base_name + ".ibldiff";
    }
    
    ////////////////////////////////////
    // Parse specular roughness array
    ////////////////////////////////////

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
    
    ////////////////////////////////////
    // Create texture array for specular
    ////////////////////////////////////

    auto specular_texarray = std::make_shared<TextureArray>();
    specular_texarray->_tex->_debugName = base_name + ".iblspec_array";
    
    ////////////////////////////////////
    // Set radiance map properties
    ////////////////////////////////////

    irrmaps->_numRoughnessLevels = num_roughness_levels;
    irrmaps->_specularRoughnessValues = xir_data_result._roughness_values;
    
    //////////////////////////////////////////////////////////////
    // Diffuse upload op (deferrable)
    //////////////////////////////////////////////////////////////

    if (has_diffuse) {
      auto diffuseUploadOp = [=](Context* ctx) {
        auto diffuse_loadreq = std::make_shared<TexLoadReq>();
        diffuse_loadreq->ptex = diffuse_tex;
        diffuse_loadreq->_cmipchain = diffuse_cmipchain;
        diffuse_loadreq->_texname = base_name + ".irrdiff";
        ctx->TXI()->_createFromLoadReq(diffuse_loadreq);
        irrmaps->_filtenvDiffuseMap = diffuse_tex;
      };
      GfxEnv::GetRef().enqueueDeferredContextOp(diffuseUploadOp);
    }
    //diffuseUploadOp(gloadercontext.get());
      
    //////////////////////////////////////////////////////////////
    // init texture array op (deferrable)
    //////////////////////////////////////////////////////////////

    auto initTexArrayOp = [=](Context* ctx) {
      auto txi = ctx->TXI();
      TextureArrayInitData TID;
      TID._slices.resize(num_roughness_levels);
      for(int i = 0; i < num_roughness_levels; i++) {
        // Use CRC for usage ID, similar to PBRMaterial
        uint32_t usage_id = CrcString(FormatString("roughness_%d", i).c_str()).hashed();
        TID._slices[i] = TextureArrayInitSubItem{usage_id, specular_images[i]};
      }
      txi->initTextureArray2DFromData(specular_texarray.get(), TID);    
      irrmaps->_filtenvSpecularMapArray = specular_texarray;  // Use array instead of single texture
    };

    GfxEnv::GetRef().enqueueDeferredContextOp(initTexArrayOp);
    //initTexArrayOp(gloadercontext.get());    

    //////////////////////////////////////////////////////////////
    // brdf integration maps (deferrable)
    //////////////////////////////////////////////////////////////
    
    auto brdfSetOp = [=](Context* ctx) {
      irrmaps->_brdfIntegrationMapGGX = PBRMaterial::brdfIntegrationMap(ctx,"GGX");
      irrmaps->_brdfIntegrationMapVelvet = PBRMaterial::brdfIntegrationMap(ctx,"GGXVELVET");
      irrmaps->_brdfIntegrationMapGGXRIM = PBRMaterial::brdfIntegrationMap(ctx,"GGXRIM");
      irrmaps->_brdfIntegrationMapBlinn = PBRMaterial::brdfIntegrationMap(ctx,"BLINN");
      irrmaps->_brdfIntegrationMapPhong = PBRMaterial::brdfIntegrationMap(ctx,"PHONG");
    };

    GfxEnv::GetRef().enqueueDeferredContextOp(brdfSetOp);
    //brdfSetOp(gloadercontext.get());
    //////////////////////////////////////////////////////////////

    if(0)printf("XIR asset<%p> irrmaps<%p> dtex<%p> stexarray<%p> roughness_levels<%d>\n",
           (void*) asset.get(), (void*) irrmaps.get(),
           diffuse_tex ? diffuse_tex.get() : nullptr,
           specular_texarray.get(), num_roughness_levels);
  };
  opq::concurrentQueue()->enqueue(op);
  //op();
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
