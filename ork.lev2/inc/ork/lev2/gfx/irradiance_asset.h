////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/asset/Asset.h>
#include <ork/asset/HybridAssetLoader.h>
#include <ork/lev2/lev2_types.h>
#include <ork/kernel/datablock.h>

namespace ork::lev2 {

////////////////////////////////////////////////////////////////////////////////
// IrradianceMapsAsset - Asset type for pre-filtered environment maps
// 
// Contains both diffuse and specular irradiance maps that have been
// pre-filtered at build time and stored in XIR format
////////////////////////////////////////////////////////////////////////////////

struct IrradianceMapsAsset : public asset::Asset {
  DeclareConcreteX(IrradianceMapsAsset, asset::Asset);
  
  static const char* assetTypeNameStatic() { return "irradiance"; }
  
  pbr::irradiancemaps_ptr_t _irradianceMaps;
};

using irradianceasset_ptr_t = std::shared_ptr<IrradianceMapsAsset>;

////////////////////////////////////////////////////////////////////////////////
// IrradianceMapsLoader - Loader for XIR format irradiance maps
// 
// Supports both file-based and catalog-based loading through HybridAssetLoader
// Performs deferred GPU upload to avoid requiring context during load
////////////////////////////////////////////////////////////////////////////////

struct IrradianceMapsLoader : public asset::HybridAssetLoader {
  IrradianceMapsLoader();
    
protected:
  asset::asset_ptr_t _doLoadFromDatablock(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t xir_data) final;
        
private:
  asset::asset_ptr_t _loadFromXIR(
    asset::loadrequest_ptr_t loadreq,
    datablock_ptr_t xir_data);
};

} // namespace ork::lev2