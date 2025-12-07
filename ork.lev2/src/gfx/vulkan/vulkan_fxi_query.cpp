////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include <ork/lev2/gfx/shadman.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

size_t VkFxInterface::numDescriptorSetBindPoints(fxtechnique_constptr_t tek) {
  OrkAssert(false);
  return 0;
}

fxdescriptorsetbindpoint_constptr_t VkFxInterface::descriptorSetBindPoint(fxtechnique_constptr_t tek, int slot_index) {
  OrkAssert(false);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderTechnique* VkFxInterface::technique(FxShader* pshader, const std::string& name) {
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  auto it_tek   = vkshfile->_vk_techniques.find(name);
  if (it_tek != vkshfile->_vk_techniques.end()) {
    vkfxstek_ptr_t tek = it_tek->second;
    return tek->_orktechnique.get();
  } else {
    auto shader_name = vkshfile->_shader_name;
    if (0)
      printf(
          "VkFxInterface shader<%s> technique<%s> not found\n", //
          shader_name.c_str(),                                  //
          name.c_str());
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const FxShaderParam* VkFxInterface::parameter(FxShader* pshader, const std::string& name) {
  const FxShaderParam* rval = nullptr;
  auto vkshfile             = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  auto shader_name          = vkshfile->_shader_name;

  size_t num_smpsets = vkshfile->_vk_samplersets.size();
  size_t num_unisets = vkshfile->_vk_uniformsets.size();
  size_t num_uniblks = vkshfile->_vk_uniformblks.size();

  //////////////////////////////////////////////////////
  // search sampler sets
  //////////////////////////////////////////////////////

  for (auto item : vkshfile->_vk_samplersets) {
    auto smpset_name = item.first;
    auto smpset      = item.second;
    auto it_samp     = smpset->_samplers_by_name.find(name);
    if (it_samp != smpset->_samplers_by_name.end()) {
      auto samp = it_samp->second;
      rval      = samp->_orkparam.get();
      if (0)
        printf(
            "VkFxInterface shader<%s> sampler<%s> found>\n", //
            shader_name.c_str(),                             //
            name.c_str());                                   //
      break;
    }
  }
  if (rval != nullptr) {
    return rval;
  }

  //////////////////////////////////////////////////////
  // search uniform sets
  //////////////////////////////////////////////////////

  for (auto item : vkshfile->_vk_uniformsets) {
    auto uniset_name = item.first;
    // printf( "search uniset<%s>\n", uniset_name.c_str() );
    auto uniset  = item.second;
    auto it_item = uniset->_items_by_name.find(name);
    if (it_item != uniset->_items_by_name.end()) {
      auto item = it_item->second;
      rval      = item->_orkparam.get();
      if (0)
        printf(
            "VkFxInterface shader<%s> parameter<%s> found>\n", //
            shader_name.c_str(),                               //
            name.c_str());                                     //
      break;
    }
  }
  if (rval != nullptr) {
    return rval;
  }

  //////////////////////////////////////////////////////
  // search uniform blocks
  //////////////////////////////////////////////////////

  if (0){
    printf(
        "VkFxInterface shader<%s> parameter<%s> not found in unisets numunisets<%zu>\n", //
        shader_name.c_str(),                                                             //
        name.c_str(),                                                                    //
        num_unisets);
    }

  // search uniform blocks

  for (auto item : vkshfile->_vk_uniformblks) {
    auto uniblk_name = item.first;
    // printf( "search uniset<%s>\n", uniset_name.c_str() );
    auto uniblk = item.second;

    auto it_item = uniblk->_items_by_name.find(name);
    if (it_item != uniblk->_items_by_name.end()) {
      auto item = it_item->second;
      rval      = item->_orkparam.get();
      if (0)
        printf(
            "VkFxInterface shader<%s> parameter<%s> found>\n", //
            shader_name.c_str(),                               //
            name.c_str());                                     //
      break;
    }
  }
  if (rval != nullptr) {
    return rval;
  }
  if (0){
    printf(
        "VkFxInterface shader<%s> parameter<%s> not found in uniblks numuniblks<%zu>\n", //
        shader_name.c_str(),                                                             //
        name.c_str(),                                                                    //
        num_uniblks);
    }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const FxUniformBlock* VkFxInterface::uniformBlock( FxShader* shader, //
                                                   const std::string& name) { //
  OrkAssert(shader != nullptr);
  auto& blockmap = shader->_uniformBlockByName;
  auto it        = blockmap.find(name);
  auto fxsblock  = (it != blockmap.end()) ? it->second : nullptr;
  auto vkshfile  = shader->_internalHandle.get<vkfxsfile_ptr_t>();
  
  auto it2 = vkshfile->_vk_uniformblks.find(name);
  if (it2 != vkshfile->_vk_uniformblks.end()) {
    auto vkblock = it2->second;
    if (vkblock != nullptr and fxsblock == nullptr) {
      // Create FxUniformBlock from Vulkan uniform block
      auto ork_uniblk = vkblock->_orkparamblock;
      fxsblock = ork_uniblk.get();
      if (fxsblock) {
        shader->_uniformBlockByName[name] = fxsblock;
      }
    }
  }
  return fxsblock; // Return nullptr if not found, matching GL behavior
}

fxsamplerset_constptr_t VkFxInterface::samplerSet(FxShader* hfx, const std::string& name) {
  auto& sampsets = hfx->_samplerSets;
  auto it        = sampsets.find(name);
  auto fxsampset  = (FxSamplerSet*)((it != sampsets.end()) ? it->second : nullptr);
  printf( "shader<%p:%s> FIND SAMPLERSET<%s> fxsampset<%p>\n", (void*) hfx, hfx->mName.c_str(), name.c_str(), fxsampset );
  return fxsampset;
}

///////////////////////////////////////////////////////////////////////////////
const FxComputeShader* VkFxInterface::computeShader(FxShader* pshader, const std::string& name) {
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  auto it       = vkshfile->_vk_shaderobjects.find(name);
  OrkAssert(it != vkshfile->_vk_shaderobjects.end());
  auto sh_obj = it->second;
  OrkAssert(sh_obj->_STAGE == "compute"_crcu);

  // Create compute pipeline object
  auto compute_pipeline = std::make_shared<VkComputePipelineObject>(_contextVK);
  bool success = compute_pipeline->createPipeline(sh_obj);
  OrkAssert(success && "Failed to create compute pipeline");

  // Create FxComputeShader and store the pipeline
  auto cushader = new FxComputeShader;
  cushader->_name = name;
  cushader->_impl.set<vkcompute_pipeline_ptr_t>(compute_pipeline);

  // Register with shader
  pshader->addComputeShader(cushader);

  return cushader;
}
const FxShaderStorageBlock* VkFxInterface::storageBlock(FxShader* pshader, const std::string& name) {
  OrkAssert(pshader != nullptr);
  auto& blockmap = pshader->_storageBlockByName;
  auto it = blockmap.find(name);
  auto fxsblock = (it != blockmap.end()) ? it->second : nullptr;
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();

  auto it2 = vkshfile->_vk_ssbo_blocks.find(name);
  if (it2 != vkshfile->_vk_ssbo_blocks.end()) {
    auto vkssbo = it2->second;
    if (vkssbo != nullptr && fxsblock == nullptr) {
      // Create FxShaderStorageBlock from Vulkan storage block
      fxsblock = vkssbo->_orkstorageblock.get();
      if (fxsblock) {
        pshader->_storageBlockByName[name] = fxsblock;
      }
    }
  }
  return fxsblock; // Return nullptr if not found, matching GL behavior
}
///////////////////////////////////////////////////////////////////////////////
fxbuffer_member_constptr_t VkFxInterface::findStorageMember(
    fxparamstorageblock_constptr_t block,
    const std::string& member_name) {
  if (!block) {
    return nullptr;
  }
  return block->findMember(member_name);
}
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
