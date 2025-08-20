////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "headers/vulkan_ctx.h"
#include "vulkan_ub_layout.inl"
#include "../shadlang/shadlang_backend_spirv.h"
#include <ork/file/chunkfile.inl>
#include <regex>
#include <set>

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
    printf("load shader from path<%s>\n", input_path.c_str());
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

// Helper function to resolve import paths similar to how the parser does it
static file::Path resolveImportPath(
    const std::string& parent_path,
    const std::string& import_path) {
  
  file::Path::NameType a, b;
  file::Path proc_import_path;
  
  // Remove quotes if present
  std::string clean_path = import_path;
  if (!clean_path.empty() && clean_path.front() == '"')
    clean_path.erase(0, 1);
  if (!clean_path.empty() && clean_path.back() == '"')
    clean_path.pop_back();
  
  auto rpath = file::Path(clean_path);
  rpath.split(a, b, ':');
  
  if (b.length() != 0) { 
    // Already has protocol (like orkshader://)
    proc_import_path = rpath;
  } else { 
    // Infer protocol from parent
    file::Path parent(parent_path);
    parent.split(a, b, ':');
    ork::FixedString<256> fxs;
    fxs.format("%s://%s", a.c_str(), clean_path.c_str());
    proc_import_path = fxs.c_str();
  }
  
  return proc_import_path;
}

///////////////////////////////////////////////////////////////////////////////

// Recursively concatenate shader text with all imports in deterministic order
static std::string concatenateShaderWithImports(
    const std::string& shader_name,
    const std::string& shader_text,
    std::set<std::string>& visited) {
    
  std::string result;
  result.reserve(shader_text.size() * 2); // Pre-allocate for efficiency
  
  // Regex to match import statements
  std::regex import_regex("import\\s+\"([^\"]+)\"");
  
  auto search_start = shader_text.cbegin();
  std::smatch match;
  while (std::regex_search(search_start, shader_text.cend(), match, import_regex)) {
    // Add text before the import statement
    result.append(search_start, match[0].first);
    
    std::string import_path = match[1];
    auto resolved_path = resolveImportPath(shader_name, import_path);
    std::string resolved_str = resolved_path.c_str();
    
    // Check for circular imports
    if (visited.insert(resolved_str).second) {
      // Add import marker for debugging/determinism
      result.append("\n//[[IMPORT_BEGIN:" + resolved_str + "]]\n");
      
      // Read and recursively process the imported file
      auto import_data = ork::File::readAsString(resolved_path);
      if (import_data != nullptr) {
        std::string expanded_import = concatenateShaderWithImports(
          resolved_str,
          import_data->_data,
          visited);
        result.append(expanded_import);
      } else {
        result.append("//[[IMPORT_ERROR: Could not read " + resolved_str + "]]\n");
      }
      
      result.append("//[[IMPORT_END:" + resolved_str + "]]\n");
    } else {
      // Circular import detected, skip it
      result.append("//[[CIRCULAR_IMPORT_SKIPPED:" + resolved_str + "]]\n");
    }
    
    // Move past this import statement
    search_start = match.suffix().first;
  }
  
  // Add any remaining text after the last import
  result.append(search_start, shader_text.cend());
  
  return result;
}

///////////////////////////////////////////////////////////////////////////////

// Public wrapper for expanding shader text with all imports
static std::string expandShaderText(
    const std::string& shader_name,
    const std::string& shader_text) {
  std::set<std::string> visited;
  visited.insert(shader_name); // Mark the main file as visited
  return concatenateShaderWithImports(shader_name, shader_text, visited);
}

///////////////////////////////////////////////////////////////////////////////

vkfxsfile_ptr_t VkFxInterface::_loadShaderFromShaderText(
    FxShader* shader,                //
    const std::string& parser_name,  //
    const std::string& shadertext) { //
    
  // Expand shader text to include all imports for proper cache invalidation
  std::string expanded_text = expandShaderText(parser_name, shadertext);
  
  auto basehasher = DataBlock::createHasher();
  basehasher->accumulateString("vkfxshader-1.1"); // Bump version for new hashing scheme
  basehasher->accumulateString(expanded_text);
  basehasher->finish();
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
