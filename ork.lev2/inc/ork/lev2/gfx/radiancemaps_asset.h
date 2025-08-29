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
// RadianceMapsAsset - Asset type for pre-filtered environment maps
// 
// Contains both diffuse and specular Radiance maps that have been
// pre-filtered at build time and stored in XIR format
////////////////////////////////////////////////////////////////////////////////

struct RadianceMapsAsset : public asset::Asset {
  DeclareConcreteX(RadianceMapsAsset, asset::Asset);
  
  static const char* assetTypeNameStatic() { return "Radiance"; }
  
  pbr::radiancemaps_ptr_t _radiance_maps;
};

using Radianceasset_ptr_t = std::shared_ptr<RadianceMapsAsset>;

////////////////////////////////////////////////////////////////////////////////
// RadianceMapsLoader - Loader for XIR format Radiance maps
// 
// Supports both file-based and catalog-based loading through HybridAssetLoader
// Performs deferred GPU upload to avoid requiring context during load
////////////////////////////////////////////////////////////////////////////////

struct RadianceMapsLoader : public asset::HybridAssetLoader {
  RadianceMapsLoader();
    
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