////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "vulkan_ctx.h"
#include "vulkan_ub_layout.inl"
#include "../shadlang/shadlang_backend_spirv.h"
#include <ork/file/chunkfile.inl>

#if defined(__APPLE__)
// #include <MoltenVK/mvk_vulkan.h>
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
using namespace shadlang;
///////////////////////////////////////////////////////////////////////////////

VulkanFxShaderObject::VulkanFxShaderObject(vkcontext_rawptr_t ctx, vkfxshader_bin_t bin) //
    : _contextVK(ctx)                                                                    //
    , _spirv_binary(bin) {                                                               //

  _vk_shadermoduleinfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  _vk_shadermoduleinfo.codeSize = bin.size() * sizeof(uint32_t);
  _vk_shadermoduleinfo.pCode    = bin.data();
  _vk_shadermoduleinfo.pNext    = nullptr;
  _vk_shadermoduleinfo.flags    = 0;
  VkResult result               = vkCreateShaderModule( //
      _contextVK->_vkdevice,              //
      &_vk_shadermoduleinfo,              //
      nullptr,                            //
      &_vk_shadermodule);                 //
  OrkAssert(result == VK_SUCCESS);
}

VulkanFxShaderObject::~VulkanFxShaderObject() {
  vkDestroyShaderModule(_contextVK->_vkdevice, _vk_shadermodule, nullptr);
}

///////////////////////////////////////////////////////////////////////////////

bool VkFxInterface::LoadFxShader(const AssetPath& input_path, FxShader* pshader) {
    
  auto it = _fxshaderfiles.find(input_path);
  vkfxsfile_ptr_t vulkan_shaderfile;
  ////////////////////////////////////////////
  // if not yet loaded, load...
  ////////////////////////////////////////////
  if (it != _fxshaderfiles.end()) { // shader already loaded...
    vulkan_shaderfile = it->second;
  } else { // load
    auto str_read = ork::File::readAsString(input_path);
    OrkAssert(str_read != nullptr);
    if(input_path=="orkshader://pbr.fxv2"){
      printf("yo\n");
    }
    vulkan_shaderfile          = _loadShaderFromShaderText(pshader, input_path.c_str(), str_read->_data);
    _fxshaderfiles[input_path] = vulkan_shaderfile;
  }
  bool OK = (vulkan_shaderfile != nullptr);
  if (OK) {
    vulkan_shaderfile->_shader_name = input_path.c_str();
    pshader->_internalHandle.set<vkfxsfile_ptr_t>(vulkan_shaderfile);
  }
  return OK;
}

///////////////////////////////////////////////////////////////////////////////

FxShader* VkFxInterface::shaderFromShaderText(const std::string& name, const std::string& shadertext) {
  FxShader* shader                  = new FxShader;
  vkfxsfile_ptr_t vulkan_shaderfile = _loadShaderFromShaderText(shader, name, shadertext);
  if (vulkan_shaderfile) {
    shader->_internalHandle.set<vkfxsfile_ptr_t>(vulkan_shaderfile);
    _fxshaderfiles[name]            = vulkan_shaderfile;
    vulkan_shaderfile->_shader_name = name;
  } else {
    delete shader;
    shader = nullptr;
  }
  return shader;
};


///////////////////////////////////////////////////////////////////////////////

vkfxsfile_ptr_t VkFxInterface::_loadShaderFromShaderText(
    FxShader* shader,                //
    const std::string& parser_name,  //
    const std::string& shadertext) { //
    
  auto basehasher = DataBlock::createHasher();
  basehasher->accumulateString("vkfxshader-1.0");
  basehasher->accumulateString(shadertext);
  uint64_t hashkey               = basehasher->result();
  datablock_ptr_t vkfx_datablock = DataBlockCache::findDataBlock(hashkey);
  vkfxsfile_ptr_t vulkan_shaderfile;
    ////////////////////////////////////////////
    if(parser_name=="orkshader://pbr.fxv2"){
        printf("yo\n");
    }
  ////////////////////////////////////////////
  // shader binary already cached
  // first check precompiled shader cache
  ////////////////////////////////////////////
  if (vkfx_datablock) {

  }
  ////////////////////////////////////////////
  // shader binary not cached, compile and cache
  ////////////////////////////////////////////
  else {
    auto temp_cache = std::make_shared<ShadLangParserCache>();
    auto transunit  = shadlang::parseFromString(temp_cache, parser_name, shadertext);
    vkfx_datablock  = _writeIntermediateToDataBlock(transunit);
    DataBlockCache::setDataBlock(hashkey, vkfx_datablock);
  } // shader binary not cached, compile and cache..
  ////////////////////////////////////////////////////////
  // vkfx_datablock->dump();
  vulkan_shaderfile = _readFromDataBlock(vkfx_datablock, shader);
    vulkan_shaderfile->_shader_name = parser_name;
  ////////////////////////////////////////////////////////
  return vulkan_shaderfile;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
