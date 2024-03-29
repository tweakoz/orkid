////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
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

  if (1)
    printf(
        "VkFxInterface shader<%s> parameter<%s> not found in unisets numunisets<%zu>\n", //
        shader_name.c_str(),                                                             //
        name.c_str(),                                                                    //
        num_unisets);

  // search uniform blocks

  for (auto item : vkshfile->_vk_uniformblks) {
    auto uniblk_name = item.first;
    // printf( "search uniset<%s>\n", uniset_name.c_str() );
    auto uniblk = item.second;

    auto it_item = uniblk->_items_by_name.find(name);
    if (it_item != uniblk->_items_by_name.end()) {
      auto item = it_item->second;
      rval      = item->_orkparam.get();
      if (1)
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
  if (1)
    printf(
        "VkFxInterface shader<%s> parameter<%s> not found in uniblks numuniblks<%zu>\n", //
        shader_name.c_str(),                                                             //
        name.c_str(),                                                                    //
        num_uniblks);
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

const FxUniformBlock* VkFxInterface::uniformBlock(FxShader* pshader, const std::string& name) {
  const FxUniformBlock* rval = nullptr;
  auto vkshfile                  = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  auto it                        = vkshfile->_vk_uniformblks.find(name);
  if (it != vkshfile->_vk_uniformblks.end()) {
    if (0)
      printf("found uniblk<%s>\n", name.c_str());
    auto vk_uniblk  = it->second;
    auto ork_uniblk = vk_uniblk->_orkparamblock;
    rval            = ork_uniblk.get();
  } else {
    auto vkshfile      = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
    auto shader_name   = vkshfile->_shader_name;
    size_t num_uniblks = vkshfile->_vk_uniformblks.size();
    if (0)
      printf(
          "VkFxInterface shader<%s> uniblock<%s> not found numuniblks<%zu>\n", //
          shader_name.c_str(),                                                 //
          name.c_str(),                                                        //
          num_uniblks);
    OrkAssert(false);
  }
  return rval;
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
  auto vk_program        = std::make_shared<VkFxShaderProgram>(vkshfile.get());
  vk_program->_comshader = sh_obj;
  auto cushader          = new FxComputeShader;
  cushader->_impl.set<vkfxsprg_ptr_t>(vk_program);
  static int prog_index          = 128;
  vk_program->_pipeline_bits_prg = prog_index;
  prog_index++;
  OrkAssert(prog_index < 256);
  return cushader;
}
const FxShaderStorageBlock* VkFxInterface::storageBlock(FxShader* pshader, const std::string& name) {
  auto vkshfile = pshader->_internalHandle.get<vkfxsfile_ptr_t>();
  OrkAssert(false);
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
