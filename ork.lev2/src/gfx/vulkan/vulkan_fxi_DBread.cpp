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

#if defined(__APPLE__)
// #include <MoltenVK/mvk_vulkan.h>
#endif

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::vulkan {
constexpr size_t MAX_PUSH_CONSTANT_SIZE = 4096; // Vulkan spec limit
///////////////////////////////////////////////////////////////////////////////
using namespace shadlang;
///////////////////////////////////////////////////////////////////////////////

template <typename if_type_t>
std::shared_ptr<if_type_t> //
read_interface(chunkfile::InputStream* input_stream, chunkfile::Reader& chunkreader) {
  auto str_interface = input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_interface == "interface");
  auto interface   = std::make_shared<if_type_t>();
  interface->_name = input_stream->ReadIndexedString(chunkreader);
  // printf(" vtx_interface<%s>\n", interface->_name.c_str());
  /////////////////
  auto str_inputgroups = input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_inputgroups == "inputgroups");
  /////////////////
  auto num_input_groups = input_stream->ReadItem<size_t>();
  for (size_t ig = 0; ig < num_input_groups; ig++) {
    auto num_inputs = input_stream->ReadItem<size_t>();
    for (size_t ii = 0; ii < num_inputs; ii++) {
      auto str_item_type = input_stream->ReadIndexedString(chunkreader);

      if (str_item_type == "input") {
        auto input         = std::make_shared<typename if_type_t::input_t>();
        input->_datatype   = input_stream->ReadIndexedString(chunkreader);
        input->_identifier = input_stream->ReadIndexedString(chunkreader);
        input->_semantic   = input_stream->ReadIndexedString(chunkreader);
        input->_datasize   = input_stream->ReadItem<size_t>();
        interface->_inputs.push_back(input);
      } else if (str_item_type == "layout") {

      } else {
        OrkAssert(false);
      }
    }
  }
  /////////////////
  auto str_outputgroups = input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_outputgroups == "outputgroups");
  auto num_output_groups = input_stream->ReadItem<size_t>();
  for (size_t og = 0; og < num_output_groups; og++) {
    auto num_outputs = input_stream->ReadItem<size_t>();
    for (size_t oi = 0; oi < num_outputs; oi++) {
      auto str_item_type = input_stream->ReadIndexedString(chunkreader);
      if (str_item_type == "output") {
        auto str_output_datatype   = input_stream->ReadIndexedString(chunkreader);
        auto str_output_identifier = input_stream->ReadIndexedString(chunkreader);
        // printf("  output<%s %s>\n", str_output_datatype.c_str(), str_output_identifier.c_str());
        auto dsize = input_stream->ReadItem<size_t>();
        if (str_output_identifier.find("gl_") != 0) {
          // _appendText(_interface_group, "layout(location=%zu) out %s %s;", _output_index, dt.c_str(), id.c_str());
          // o_output_index += dsize;
        }
      } else if (str_item_type == "layout") {

      } else {
        OrkAssert(false);
      }
    }
  }
  /////////////////
  return interface;
}

///////////////////////////////////////////////////////////////////////////////

template <typename if_type_t>
void readInterfaces(
    chunkfile::InputStream* input_stream,                                   //
    chunkfile::Reader& chunkreader,                                         //
    std::unordered_map<std::string, std::shared_ptr<if_type_t>>& the_map) { //
  auto if_count = input_stream->ReadItem<size_t>();
  for (size_t i = 0; i < if_count; i++) {
    auto interface            = read_interface<if_type_t>(input_stream, chunkreader);
    the_map[interface->_name] = interface;
  }
}

///////////////////////////////////////////////////////////////////////////////

template <typename if_ptr_t>
void //
read_interface_inheritances(
    chunkfile::InputStream* input_stream,                 //
    chunkfile::Reader& chunkreader,                       //
    std::unordered_map<std::string, if_ptr_t>& the_map) { //

  auto str_if_inheritances = input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_if_inheritances == "interface_inheritances");
  auto num_inheritances = input_stream->ReadItem<size_t>();
  for (size_t i = 0; i < num_inheritances; i++) {
    auto str_if_name = input_stream->ReadIndexedString(chunkreader);
    auto num_inh     = input_stream->ReadItem<size_t>();
    if (num_inh > 0) {
      auto str_inh_name = input_stream->ReadIndexedString(chunkreader);
      // printf(" interface<%s> INHERITS interface<%s>\n", str_if_name.c_str(), str_inh_name.c_str());

      auto if_child = the_map.find(str_if_name);
      OrkAssert(if_child != the_map.end());
      auto if_parent = the_map.find(str_inh_name);
      OrkAssert(if_parent != the_map.end());
      if_child->second->_parent = if_parent->second;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void read_stateblocks(VkFxShaderFile* vulkan_shaderfile, chunkfile::InputStream* inp_stream, chunkfile::Reader& chunkreader) {

  auto str_stateblocks = inp_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_stateblocks == "stateblocks");

  size_t num_stateblocks = inp_stream->ReadItem<size_t>();

  // First pass: read all state block definitions
  struct StateBlockData {
    std::string name;
    std::string parent_name;
    std::map<std::string, std::string> items;
  };
  std::vector<StateBlockData> stateblock_data;

  for (size_t i = 0; i < num_stateblocks; i++) {
    StateBlockData sb_data;

    // Read state block name
    sb_data.name = inp_stream->ReadIndexedString(chunkreader);

    // Read parent name (for inheritance)
    sb_data.parent_name = inp_stream->ReadIndexedString(chunkreader);

    // Read state block items
    size_t num_items = inp_stream->ReadItem<size_t>();

    for (size_t j = 0; j < num_items; j++) {
      auto key           = inp_stream->ReadIndexedString(chunkreader);
      auto value         = inp_stream->ReadIndexedString(chunkreader);
      sb_data.items[key] = value;
    }

    stateblock_data.push_back(sb_data);
  }

  // Helper to resolve state block with inheritance
  std::function<rasterstate_ptr_t(const std::string&)> resolve_stateblock;
  resolve_stateblock = [&](const std::string& name) -> rasterstate_ptr_t {
    // Check if already resolved
    auto it = vulkan_shaderfile->_stateblock_rasterstates.find(name);
    if (it != vulkan_shaderfile->_stateblock_rasterstates.end()) {
      return it->second;
    }

    // Find the state block data
    auto data_it =
        std::find_if(stateblock_data.begin(), stateblock_data.end(), [&](const StateBlockData& d) { return d.name == name; });

    if (data_it == stateblock_data.end()) {
      // Special case for "default" base state block
      if (name == "default") {
        auto rstate = std::make_shared<RasterState>();
        rstate->_name = name;
        // Set engine defaults
        rstate->setDepthTest(EDepthTest::LESS);
        rstate->setCullTest(ECullTest::PASS_FRONT);
        rstate->setWriteMaskZ(true);
        rstate->setWriteMaskRGB(true);
        rstate->setWriteMaskA(true);
        rstate->setBlendingMacro(BlendingMacro::OFF);
        vulkan_shaderfile->_stateblock_rasterstates[name] = rstate;
        return rstate;
      }
      return nullptr;
    }

    // Start with parent state or default
    rasterstate_ptr_t rstate;
    if (!data_it->parent_name.empty()) {
      auto parent = resolve_stateblock(data_it->parent_name);
      rstate      = parent ? parent->clone() : std::make_shared<RasterState>();
      rstate->_name = name;
    } else {
      rstate = std::make_shared<RasterState>();
    }

    // Apply this state block's settings
    for (const auto& [key, value] : data_it->items) {
      if (key == "BlendMode") {
        if (value == "OFF") {
          rstate->setBlendingMacro(BlendingMacro::OFF);
        } else if (value == "PREMA") {
          rstate->setBlendingMacro(BlendingMacro::PREMA);
        } else if (value == "ALPHA") {
          rstate->setBlendingMacro(BlendingMacro::ALPHA);
        } else if (value == "DSTALPHA") {
          rstate->setBlendingMacro(BlendingMacro::DSTALPHA);
        } else if (value == "ADDITIVE") {
          rstate->setBlendingMacro(BlendingMacro::ADDITIVE);
        } else if (value == "ALPHA_ADDITIVE") {
          rstate->setBlendingMacro(BlendingMacro::ALPHA_ADDITIVE);
        } else if (value == "SUBTRACTIVE") {
          rstate->setBlendingMacro(BlendingMacro::SUBTRACTIVE);
        } else if (value == "ALPHA_SUBTRACTIVE") {
          rstate->setBlendingMacro(BlendingMacro::ALPHA_SUBTRACTIVE);
        } else if (value == "MODULATE") {
          rstate->setBlendingMacro(BlendingMacro::MODULATE);
        }
      } else if (key == "DepthTest") {
        if (value == "OFF") {
          rstate->setDepthTest(EDepthTest::OFF);
        } else if (value == "LESS") {
          rstate->setDepthTest(EDepthTest::LESS);
        } else if (value == "LEQUALS") {
          rstate->setDepthTest(EDepthTest::LEQUALS);
        } else if (value == "GREATER") {
          rstate->setDepthTest(EDepthTest::GREATER);
        } else if (value == "GEQUALS") {
          rstate->setDepthTest(EDepthTest::GEQUALS);
        } else if (value == "EQUALS") {
          rstate->setDepthTest(EDepthTest::EQUALS);
        } else if (value == "ALWAYS") {
          rstate->setDepthTest(EDepthTest::ALWAYS);
        }
      } else if (key == "CullTest") {
        if (value == "OFF") {
          rstate->setCullTest(ECullTest::OFF);
        } else if (value == "PASS_FRONT") {
          rstate->setCullTest(ECullTest::PASS_FRONT);
        } else if (value == "PASS_BACK") {
          rstate->setCullTest(ECullTest::PASS_BACK);
        }
      } else if (key == "DepthMask") {
        rstate->setWriteMaskZ(value == "ON" || value == "true");
      } else if (key == "ColorMask") {
        // Could parse RGB/A separately if needed
        bool enabled = (value == "ON" || value == "true");
        rstate->setWriteMaskRGB(enabled);
        rstate->setWriteMaskA(enabled);
      }
      // Add more state items as needed
    }

    // Cache and return
    vulkan_shaderfile->_stateblock_rasterstates[name] = rstate;
    return rstate;
  };

  // Second pass: resolve all state blocks (handles inheritance)
  for (const auto& sb_data : stateblock_data) {
    resolve_stateblock(sb_data.name);
  }
}

///////////////////////////////////////////////////////////////////////////////

vkfxsfile_ptr_t VkFxInterface::_readFromDataBlock(datablock_ptr_t vkfx_datablock, FxShader* ork_shader) {

  if(0)printf(
      "VkFxInterface::_readFromDataBlock datablock<%p> ork_shader<%s>\n", (void*)vkfx_datablock.get(), ork_shader->mName.c_str());
  ////////////////////////////////////////////////////////
  // parse datablock
  ////////////////////////////////////////////////////////

  auto vulkan_shaderfile = std::make_shared<VkFxShaderFile>();
  ork_shader->_internalHandle.setShared<VkFxShaderFile>(vulkan_shaderfile);

  chunkfile::DefaultLoadAllocator load_alloc;
  chunkfile::Reader chunkreader(vkfx_datablock, load_alloc);
  auto header_input_stream     = chunkreader.GetStream("header");
  auto shader_input_stream     = chunkreader.GetStream("shaders");
  auto uniforms_input_stream   = chunkreader.GetStream("uniforms");
  auto interfaces_input_stream = chunkreader.GetStream("interfaces");
  auto tecniq_input_stream     = chunkreader.GetStream("techniques");

  OrkAssert(shader_input_stream != nullptr);
  OrkAssert(tecniq_input_stream != nullptr);

  // header_input_stream->dump();

  auto str_shader_counts = header_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_shader_counts == "shader_counts");
  size_t num_vtx_shaders    = header_input_stream->ReadItem<size_t>();
  size_t num_vtx_interfaces = header_input_stream->ReadItem<size_t>();
  size_t num_geo_shaders    = header_input_stream->ReadItem<size_t>();
  size_t num_geo_interfaces = header_input_stream->ReadItem<size_t>();
  size_t num_frg_shaders    = header_input_stream->ReadItem<size_t>();
  size_t num_frg_interfaces = header_input_stream->ReadItem<size_t>();
  size_t num_cu_shaders     = header_input_stream->ReadItem<size_t>();
  size_t num_smpsets        = header_input_stream->ReadItem<size_t>();
  size_t num_unisets        = header_input_stream->ReadItem<size_t>();
  size_t num_uniblks        = header_input_stream->ReadItem<size_t>();
  size_t num_techniques     = header_input_stream->ReadItem<size_t>();
  size_t num_imports        = header_input_stream->ReadItem<size_t>();
  /////////////////////////////////
  // read sampler sets
  /////////////////////////////////
  auto str_smpsets = uniforms_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_smpsets == "smpsets");
  auto num_smpsets_cnt = uniforms_input_stream->ReadItem<size_t>();
  OrkAssert(num_smpsets_cnt == num_smpsets);
  for (size_t i = 0; i < num_smpsets; i++) {
    auto str_uniset = uniforms_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_uniset == "smpset");
    auto str_smpset_name          = uniforms_input_stream->ReadIndexedString(chunkreader);
    auto vk_smpset                = std::make_shared<VkFxShaderSamplerSet>();
    auto ork_smpset               = vk_smpset->_impl.makeShared<FxSamplerSet>();
    auto dset_id                  = uniforms_input_stream->ReadItem<size_t>();
    vk_smpset->_descriptor_set_id = dset_id;
    // printf( "shader<%p:%s> GOT SAMPLERSET<%s>\n", (void*) ork_shader, ork_shader->mName.c_str(), str_smpset_name.c_str() );
    vulkan_shaderfile->_vk_samplersets[str_smpset_name] = vk_smpset;
    ork_shader->_samplerSets[str_smpset_name]           = ork_smpset.get();
    ///////////////////////////////////////////////
    auto str_samplers = uniforms_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_samplers == "samplers");
    auto num_samplers = uniforms_input_stream->ReadItem<size_t>();
    for (size_t j = 0; j < num_samplers; j++) {
      auto str_sampler_datatype   = uniforms_input_stream->ReadIndexedString(chunkreader);
      auto str_sampler_identifier = uniforms_input_stream->ReadIndexedString(chunkreader);
      auto vk_samp                = std::make_shared<VkFxShaderUniformSampler>();
      vk_samp->_datatype          = str_sampler_datatype;
      vk_samp->_identifier        = str_sampler_identifier;
      auto ork_param              = std::make_shared<FxShaderParam>();
      vk_samp->_orkparam          = ork_param;
      vk_samp->_orkparam->_name   = str_sampler_identifier;
      vk_samp->_orkparam->_impl.set<VkFxShaderUniformSampler*>(vk_samp.get());
      vk_smpset->_samplers_by_name[str_sampler_identifier] = vk_samp;
      ork_param->_impl.set<VkFxShaderUniformSampler*>(vk_samp.get());
      ork_smpset->_parametersByName[str_sampler_identifier] = ork_param.get();
      if (0)
        printf("uniset<%s> ADDING Sampler PARAM<%s>\n", str_smpset_name.c_str(), str_sampler_identifier.c_str());
    }
  }
  /////////////////////////////////
  // read unisets
  /////////////////////////////////
  auto str_unisets = uniforms_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_unisets == "unisets");
  auto num_unisets_cnt = uniforms_input_stream->ReadItem<size_t>();
  OrkAssert(num_unisets_cnt == num_unisets);
  for (size_t i = 0; i < num_unisets; i++) {
    auto str_uniset = uniforms_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_uniset == "uniset");
    auto str_uniset_name = uniforms_input_stream->ReadIndexedString(chunkreader);
    auto vk_uniset       = std::make_shared<VkFxShaderUniformSet>();
    vk_uniset->_name     = str_uniset_name;
    // printf( "GOT UNIFORMSET<%s>\n", str_uniset_name.c_str() );
    vulkan_shaderfile->_vk_uniformsets[str_uniset_name] = vk_uniset;
    ///////////////////////////////////////////////
    auto str_params = uniforms_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_params == "params");
    auto num_params = uniforms_input_stream->ReadItem<size_t>();
    for (size_t j = 0; j < num_params; j++) {
      auto str_param_datatype    = uniforms_input_stream->ReadIndexedString(chunkreader);
      auto str_param_identifier  = uniforms_input_stream->ReadIndexedString(chunkreader);
      auto vk_param              = std::make_shared<VkFxShaderUniformSetItem>();
      vk_param->_datatype        = str_param_datatype;
      vk_param->_identifier      = str_param_identifier;
      vk_param->_offset          = uniforms_input_stream->ReadItem<size_t>();
      vk_param->_orkparam        = std::make_shared<FxShaderParam>();
      vk_param->_orkparam->_name = str_param_identifier;
      vk_param->_orkparam->_impl.set<VkFxShaderUniformSetItem*>(vk_param.get());
      vk_uniset->_items_by_name[str_param_identifier] = vk_param;
      vk_uniset->_items_by_order.push_back(vk_param);
      if (0)
        printf("uniset<%s> ADDING Item PARAM<%s>\n", str_uniset_name.c_str(), str_param_identifier.c_str());
    }
  }
  /////////////////////////////////
  // read uniblks
  /////////////////////////////////
  auto str_uniblks = uniforms_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_uniblks == "uniblks");
  auto num_uniblks_count = uniforms_input_stream->ReadItem<size_t>();
  OrkAssert(num_uniblks_count == num_uniblks);
  for (size_t i = 0; i < num_uniblks; i++) {
    auto str_uniblk = uniforms_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_uniblk == "uniblk");
    auto str_uniblk_name = uniforms_input_stream->ReadIndexedString(chunkreader);
    auto dset_id         = uniforms_input_stream->ReadItem<size_t>();

    // Check if uniform block already exists in this shader file
    vkfxsuniblk_ptr_t vk_uniblk;
    auto existing_it = vulkan_shaderfile->_vk_uniformblks.find(str_uniblk_name);
    bool is_reused   = false;

    if (existing_it != vulkan_shaderfile->_vk_uniformblks.end()) {
      // REUSE EXISTING BLOCK - prevents duplicate shadow buffers and GPU allocations
      vk_uniblk = existing_it->second;
      is_reused = true;

      // Verify descriptor set ID matches
      if (vk_uniblk->_descriptor_set_id != dset_id) {
        printf(
            "WARNING: UBO<%s> reused but descriptor set mismatch: existing<%zu> new<%zu>\n",
            str_uniblk_name.c_str(),
            vk_uniblk->_descriptor_set_id,
            dset_id);
      }

      if(0)printf("VK_UBO: REUSING UBO<%s> ptr<%p> for shader file\n", str_uniblk_name.c_str(), vk_uniblk.get());
    } else {
      // CREATE NEW BLOCK (first occurrence in this shader file)
      vk_uniblk                                           = std::make_shared<VkFxShaderUniformBlk>();
      vk_uniblk->_orkparamblock                           = std::make_shared<FxUniformBlock>();
      vk_uniblk->_orkparamblock->_impl.set<vkfxsuniblk_wkptr_t>(vk_uniblk);
      vk_uniblk->_orkparamblock->_name                    = str_uniblk_name; // SET THE NAME!
      vk_uniblk->_descriptor_set_id                       = dset_id;
      vulkan_shaderfile->_vk_uniformblks[str_uniblk_name] = vk_uniblk;

      if(0)printf("VK_UBO: CREATING UBO<%s> ptr<%p> DSID<%zu>\n", str_uniblk_name.c_str(), vk_uniblk.get(), dset_id);
    }

    // Only register with ork_shader if not already registered
    auto it = ork_shader->_uniformBlocks.find(str_uniblk_name);
    if (it == ork_shader->_uniformBlocks.end()) {
      ork_shader->_uniformBlocks[str_uniblk_name] = vk_uniblk->_orkparamblock.get();
    }

    if (0)
      printf("str_uniblk_name<%s>\n", str_uniblk_name.c_str());
    ///////////////////////////////////////////////
    auto str_params = uniforms_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_params == "items");
    auto num_params = uniforms_input_stream->ReadItem<size_t>();

    if (!is_reused) {
      // Process parameters only for new blocks
      for (size_t j = 0; j < num_params; j++) {
        auto str_param_datatype    = uniforms_input_stream->ReadIndexedString(chunkreader);
        auto str_param_identifier  = uniforms_input_stream->ReadIndexedString(chunkreader);
        auto vk_param              = std::make_shared<VkFxShaderUniformBlkItem>();
        vk_param->_datatype        = str_param_datatype;
        vk_param->_identifier      = str_param_identifier;
        vk_param->_offset          = uniforms_input_stream->ReadItem<size_t>();
        vk_param->_is_array        = uniforms_input_stream->ReadItem<bool>();
        vk_param->_array_length    = uniforms_input_stream->ReadItem<size_t>();
        vk_param->_parent_block    = vk_uniblk.get();
        vk_param->_orkparam        = std::make_shared<FxShaderParam>();
        vk_param->_orkparam->_name = str_param_identifier;
        vk_param->_orkparam->_impl.set<VkFxShaderUniformBlkItem*>(vk_param.get());
        // Set _blockinfo to point to parent UBO (matching GL behavior)
        vk_param->_orkparam->_blockinfo = new FxShaderParamInBlockInfo;
        vk_param->_orkparam->_blockinfo->_parent = vk_uniblk->_orkparamblock.get();
        vk_uniblk->_items_by_name[str_param_identifier] = vk_param;
        vk_uniblk->_items_by_order.push_back(vk_param);
        vk_uniblk->_orkparamblock->_subparams[str_param_identifier] = vk_param->_orkparam.get();
      }
    } else {
      // Skip reading parameters for reused blocks - they were already processed
      for (size_t j = 0; j < num_params; j++) {
        uniforms_input_stream->ReadIndexedString(chunkreader); // datatype
        uniforms_input_stream->ReadIndexedString(chunkreader); // identifier
        uniforms_input_stream->ReadItem<size_t>();             // offset
        uniforms_input_stream->ReadItem<bool>();               // is_array
        uniforms_input_stream->ReadItem<size_t>();             // array_length
      }
      if(0)printf("VK_UBO: SKIPPED reading %zu items for reused UBO<%s>\n", num_params, str_uniblk_name.c_str());
    }

    // Calculate uniform block size and initialize shadow buffer (only for new blocks)
    if (!is_reused) {
      size_t max_offset = 0;
      size_t last_size  = 0;
      for (auto& item : vk_uniblk->_items_by_order) {
        if (item->_offset >= max_offset) {
          max_offset = item->_offset;
          // Calculate base element size based on datatype
          size_t element_size = 0;
          if (item->_datatype == "float" || item->_datatype == "int" || item->_datatype == "bool" || item->_datatype == "uint") {
            element_size = 4;
          } else if (item->_datatype == "vec2") {
            element_size = 8;
          } else if (item->_datatype == "vec3") {
            element_size = 16; // vec3 is padded to vec4 in std140
          } else if (item->_datatype == "vec4") {
            element_size = 16;
          } else if (item->_datatype == "mat3") {
            element_size = 48; // 3 columns of vec4
          } else if (item->_datatype == "mat4") {
            element_size = 64;
          } else {
            // Default/unknown type - assume vec4 size
            element_size = 16;
          }
          
          // Account for arrays
          if (item->_is_array && item->_array_length > 0) {
            // In std140, array elements are aligned to vec4 boundaries (16 bytes)
            size_t aligned_element_size = ((element_size + 15) / 16) * 16;
            last_size = aligned_element_size * item->_array_length;
          } else {
            last_size = element_size;
          }
        }
      }

      // Round up to 16-byte alignment
      vk_uniblk->_buffer_size = ((max_offset + last_size + 15) / 16) * 16;
      vk_uniblk->_shadow_buffer.resize(vk_uniblk->_buffer_size, 0);
      
      // Debug names are already set in VulkanBuffer constructor
    } // end if (!is_reused)
  }
  /////////////////////////////////
  // read SSBOs
  /////////////////////////////////
  auto str_ssbos = uniforms_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_ssbos == "ssbos");
  auto num_ssbos = uniforms_input_stream->ReadItem<size_t>();
  for (size_t i = 0; i < num_ssbos; i++) {
    auto str_ssbo = uniforms_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_ssbo == "ssbo");
    auto str_ssbo_name = uniforms_input_stream->ReadIndexedString(chunkreader);
    auto dset_id = uniforms_input_stream->ReadItem<size_t>();
    auto str_buffer_name = uniforms_input_stream->ReadIndexedString(chunkreader);
    auto buffer_size = uniforms_input_stream->ReadItem<size_t>();

    // Create VkFxShaderStorageBlock
    auto vk_ssbo = std::make_shared<VkFxShaderStorageBlock>();
    vk_ssbo->_name = str_ssbo_name;
    vk_ssbo->_buffer_name = str_buffer_name;
    vk_ssbo->_descriptor_set_id = dset_id;
    vk_ssbo->_buffer_size = buffer_size;

    // Create FxShaderStorageBlock
    auto ork_ssbo = std::make_shared<FxShaderStorageBlock>();
    ork_ssbo->_name = str_ssbo_name;
    ork_ssbo->_buffer_size = buffer_size;
    ork_ssbo->_impl.makeShared<VkFxShaderStorageBlock*>(vk_ssbo.get());
    vk_ssbo->_orkstorageblock = ork_ssbo;

    // Read members
    auto str_members = uniforms_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_members == "members");
    auto num_members = uniforms_input_stream->ReadItem<size_t>();

    for (size_t j = 0; j < num_members; j++) {
      auto member_datatype = uniforms_input_stream->ReadIndexedString(chunkreader);
      auto member_identifier = uniforms_input_stream->ReadIndexedString(chunkreader);
      auto member_offset = uniforms_input_stream->ReadItem<size_t>();
      auto member_size = uniforms_input_stream->ReadItem<size_t>();
      auto member_stride = uniforms_input_stream->ReadItem<size_t>();
      auto member_is_array = uniforms_input_stream->ReadItem<bool>();
      auto member_array_length = uniforms_input_stream->ReadItem<size_t>();

      auto fxmember = std::make_shared<FxBufferMember>();
      fxmember->_name = member_identifier;
      fxmember->_datatype = member_datatype;
      fxmember->_offset = member_offset;
      fxmember->_size = member_size;
      fxmember->_stride = member_stride;
      fxmember->_is_array = member_is_array;
      fxmember->_array_length = member_array_length;

      vk_ssbo->_members_by_name[member_identifier] = fxmember;
      vk_ssbo->_members_by_order.push_back(fxmember);
      ork_ssbo->_members[member_identifier] = fxmember;
    }

    // Store in shader file
    vulkan_shaderfile->_vk_ssbo_blocks[str_ssbo_name] = vk_ssbo;
    ork_shader->_storageBlockByName[str_ssbo_name] = ork_ssbo.get();

    if(0)printf("VK_SSBO: Loaded SSBO<%s> buffer<%s> size<%zu> members<%zu> dset<%zu>\n",
           str_ssbo_name.c_str(), str_buffer_name.c_str(), buffer_size, num_members, dset_id);
  }
  // TODO - read VIFS, GIFS
  /////////////////////////////////
  // read vertex interfaces / inheritances
  /////////////////////////////////
  auto str_interfaces = interfaces_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_interfaces == "vertex-interfaces");
  readInterfaces(
      interfaces_input_stream, //
      chunkreader,             //
      vulkan_shaderfile->_vk_vtxinterfaces);
  read_interface_inheritances(
      interfaces_input_stream, //
      chunkreader,             //
      vulkan_shaderfile->_vk_vtxinterfaces);
  auto str_ifaces_done = interfaces_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_ifaces_done == "vertex-interfaces-done");
  /////////////////////////////////
  // read geometry interfaces / inheritances
  /////////////////////////////////
  str_interfaces = interfaces_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_interfaces == "geometry-interfaces");
  readInterfaces(
      interfaces_input_stream, //
      chunkreader,             //
      vulkan_shaderfile->_vk_geointerfaces);
  read_interface_inheritances(
      interfaces_input_stream, //
      chunkreader,             //
      vulkan_shaderfile->_vk_geointerfaces);
  str_ifaces_done = interfaces_input_stream->ReadIndexedString(chunkreader);
  OrkAssert(str_ifaces_done == "geometry-interfaces-done");
  /////////////////////////////////
  // read shader
  /////////////////////////////////
  auto read_shader_from_stream = [&]() -> vkfxsobj_ptr_t {
    /////////////////////////////////
    // read shader
    /////////////////////////////////
    auto str_shader = shader_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_shader == "shader");
    auto str_shader_type = shader_input_stream->ReadIndexedString(chunkreader);
    auto str_shader_name = shader_input_stream->ReadIndexedString(chunkreader);
    auto sh_bytlen       = shader_input_stream->ReadItem<size_t>();
    auto sh_data         = shader_input_stream->readData(sh_bytlen);
    /////////////////////////////////
    // shader spirv binary
    /////////////////////////////////
    vkfxshader_bin_t shader_bin;
    shader_bin.resize(sh_bytlen / sizeof(uint32_t));
    memcpy(shader_bin.data(), sh_data.data(), sh_bytlen);
    auto vulkan_shobj                                     = std::make_shared<VulkanFxShaderObject>(_contextVK, shader_bin);
    vulkan_shaderfile->_vk_shaderobjects[str_shader_name] = vulkan_shobj;
    vulkan_shobj->_name                                   = str_shader_name;
    /////////////////////////////////
    auto num_ismpsets = shader_input_stream->ReadItem<size_t>();
    if (num_ismpsets) {
      auto refs                  = std::make_shared<VkFxShaderSamplerSetsReference>();
      vulkan_shobj->_smpset_refs = refs;
      for (size_t i = 0; i < num_ismpsets; i++) {
        auto str_smpset = shader_input_stream->ReadIndexedString(chunkreader);
        // printf( "REF SAMPLERSET str_smpset<%s>\n", str_smpset.c_str() );
        auto it = vulkan_shaderfile->_vk_samplersets.find(str_smpset);
        OrkAssert(it != vulkan_shaderfile->_vk_samplersets.end());
        vkfxssmpset_ptr_t vk_smpset = it->second;
        refs->_smpsets[str_smpset]  = vk_smpset;
      }
      if (refs->_smpsets.size() > 1) {
        // print out sampler set names
        // printf("Shader<%s> has multiple sampler sets:\n", str_shader_name.c_str());
        for (const auto& smp_it : refs->_smpsets) {
          // printf("  %s\n", smp_it.first.c_str());
        }
        // Multiple sampler sets are now handled by the merged resources system
        // No longer asserting - merged resources will handle the binding conflicts
        // printf("  Note: Multiple sampler sets will be handled by merged resources system\n");
      }
    }
    /////////////////////////////////
    auto num_iunisets = shader_input_stream->ReadItem<size_t>();
    if (num_iunisets) {
      auto refs                  = std::make_shared<VkFxShaderUniformSetsReference>();
      vulkan_shobj->_uniset_refs = refs;
      for (size_t i = 0; i < num_iunisets; i++) {
        auto str_uniset = shader_input_stream->ReadIndexedString(chunkreader);
        // printf( "REF UNIFORMSET str_smpset<%s>\n", str_uniset.c_str() );
        auto it = vulkan_shaderfile->_vk_uniformsets.find(str_uniset);
        OrkAssert(it != vulkan_shaderfile->_vk_uniformsets.end());
        vkfxsuniset_ptr_t vk_uniset = it->second;
        refs->_unisets[str_uniset]  = vk_uniset;
      }
      if (refs->_unisets.size() > 1) {
        // print out uniform set names
        // printf("Shader<%s> has multiple uniform sets:\n", str_shader_name.c_str());
        for (const auto& uni_it : refs->_unisets) {
          if (0)
            printf("  %s\n", uni_it.first.c_str());
        }
        // Multiple uniform sets are now handled by the merged resources system
        // No longer asserting - merged resources will handle the binding conflicts
        if (0)
          printf("  Note: Multiple uniform sets will be handled by merged resources system\n");
      }
    }
    /////////////////////////////////
    auto num_iuniblks = shader_input_stream->ReadItem<size_t>();
    if (num_iuniblks) {
      auto refs                  = std::make_shared<VkFxShaderUniformBlksReference>();
      vulkan_shobj->_uniblk_refs = refs;
      // printf("SHADER<%s> REFERENCES %zu UBOS:\n", str_shader_name.c_str(), num_iuniblks);
      for (size_t i = 0; i < num_iuniblks; i++) {
        auto str_uniblk = shader_input_stream->ReadIndexedString(chunkreader);
        auto it         = vulkan_shaderfile->_vk_uniformblks.find(str_uniblk);
        OrkAssert(it != vulkan_shaderfile->_vk_uniformblks.end());
        vkfxsuniblk_ptr_t vk_uniblk = it->second;
        refs->_uniblks[str_uniblk]  = vk_uniblk;
        // printf("  -> UBO<%s> IN DESCRIPTOR_SET<%zu>\n", str_uniblk.c_str(), vk_uniblk->_descriptor_set_id);
      }
      OrkAssert(refs->_uniblks.size() <= 8);
    }
    /////////////////////////////////
    auto num_issbos = shader_input_stream->ReadItem<size_t>();
    if (num_issbos) {
      auto refs = std::make_shared<VkFxShaderStorageBlocksReference>();
      vulkan_shobj->_ssbo_refs = refs;
      // printf("SHADER<%s> REFERENCES %zu SSBOs:\n", str_shader_name.c_str(), num_issbos);
      for (size_t i = 0; i < num_issbos; i++) {
        auto str_ssbo = shader_input_stream->ReadIndexedString(chunkreader);
        auto it = vulkan_shaderfile->_vk_ssbo_blocks.find(str_ssbo);
        OrkAssert(it != vulkan_shaderfile->_vk_ssbo_blocks.end());
        vkfxssbo_ptr_t vk_ssbo = it->second;
        refs->_ssbo_blocks[str_ssbo] = vk_ssbo;
        // printf("  -> SSBO<%s> IN DESCRIPTOR_SET<%zu>\n", str_ssbo.c_str(), vk_ssbo->_descriptor_set_id);
      }
    }
    /////////////////////////////////
    auto num_ifaces = shader_input_stream->ReadItem<size_t>();
    if (num_ifaces) {
      for (size_t i = 0; i < num_ifaces; i++) {
        auto str_iface = shader_input_stream->ReadIndexedString(chunkreader);
        // printf( "REF IFACE<%s>\n", str_iface.c_str() );
        vulkan_shobj->_vk_interfaces.push_back(str_iface);
      }
    }
    /////////////////////////////////
    return vulkan_shobj;
  };

  //////////////////

  for (size_t i = 0; i < num_vtx_shaders; i++) {
    auto vtx_shader    = read_shader_from_stream();
    vtx_shader->_STAGE = "vertex"_crcu;
    auto& STGIV        = vtx_shader->_shaderstageinfo;
    initializeVkStruct(STGIV, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    STGIV.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    STGIV.module = vtx_shader->_vk_shadermodule;
    STGIV.pName  = "main";
  }

  //////////////////

  for (size_t i = 0; i < num_geo_shaders; i++) {
    auto geo_shader    = read_shader_from_stream();
    geo_shader->_STAGE = "geometry"_crcu;
    auto& STGIG        = geo_shader->_shaderstageinfo;
    initializeVkStruct(STGIG, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    STGIG.stage  = VK_SHADER_STAGE_GEOMETRY_BIT;
    STGIG.module = geo_shader->_vk_shadermodule;
    STGIG.pName  = "main";
  }

  //////////////////

  for (size_t i = 0; i < num_frg_shaders; i++) {
    auto frg_shader    = read_shader_from_stream();
    frg_shader->_STAGE = "fragment"_crcu;
    auto& STGIF        = frg_shader->_shaderstageinfo;
    initializeVkStruct(STGIF, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    STGIF.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    STGIF.module = frg_shader->_vk_shadermodule;
    STGIF.pName  = "main";
  }

  //////////////////

  for (size_t i = 0; i < num_cu_shaders; i++) {
    auto cu_shader    = read_shader_from_stream();
    cu_shader->_STAGE = "compute"_crcu;
    auto& STCIF       = cu_shader->_shaderstageinfo;
    initializeVkStruct(STCIF, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    STCIF.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    STCIF.module = cu_shader->_vk_shadermodule;
    STCIF.pName  = "main";
  }

  //////////////////
  // techniques (always VTG for now)
  //////////////////

  auto read_technique_from_stream = [&]() -> vkfxstek_ptr_t {
    auto str_tek = tecniq_input_stream->ReadIndexedString(chunkreader);
    OrkAssert(str_tek == "technique");
    auto str_tek_name = tecniq_input_stream->ReadIndexedString(chunkreader);

    auto vk_tek                                     = std::make_shared<VkFxShaderTechnique>();
    vulkan_shaderfile->_vk_techniques[str_tek_name] = vk_tek;

    size_t num_passes = tecniq_input_stream->ReadItem<size_t>();
    OrkAssert(num_passes<=1); // only single pass supported for now

    auto ork_tek                          = vk_tek->_orktechnique;
    ork_tek->_techniqueName               = str_tek_name;
    ork_tek->_shader                      = ork_shader;
    ork_shader->_techniques[str_tek_name] = ork_tek.get();
    ork_tek->_validated                   = true;

    for (size_t i = 0; i < num_passes; i++) {

      // vulkan side
      auto str_pass = tecniq_input_stream->ReadIndexedString(chunkreader);
      OrkAssert(str_pass == "pass");
      auto str_stages = tecniq_input_stream->ReadIndexedString(chunkreader);
      OrkAssert(str_stages == "VF" or str_stages == "VGF");

      auto vk_pass         = std::make_shared<VkFxShaderPass>();
      auto vk_program      = std::make_shared<VkFxShaderProgram>(vulkan_shaderfile.get());
      vk_program->_tek_name = str_tek_name;
      vk_pass->_vk_program = vk_program;
      vk_tek->_vk_passes.push_back(vk_pass);
      static int prog_index          = 0;
      vk_program->_pipeline_bits_prg = prog_index;
      prog_index++;
      OrkAssert(prog_index < 128);

      if (str_stages.find("V") != std::string::npos) {
        auto str_vtx_name = tecniq_input_stream->ReadIndexedString(chunkreader);
        auto vtx_obj      = vulkan_shaderfile->_vk_shaderobjects[str_vtx_name];
        if (vtx_obj == nullptr) {
          printf("vtx_obj<%s> not found\n", str_vtx_name.c_str());
          OrkAssert(false);
        }
        vk_program->_vtxshader = vtx_obj;
      }

      if (str_stages.find("G") != std::string::npos) {
        auto str_geo_name = tecniq_input_stream->ReadIndexedString(chunkreader);
        auto geo_obj      = vulkan_shaderfile->_vk_shaderobjects[str_geo_name];
        if (geo_obj == nullptr) {
          printf("geo_obj<%s> not found\n", str_geo_name.c_str());
          OrkAssert(false);
        }
        vk_program->_geoshader = geo_obj;
      }

      if (str_stages.find("F") != std::string::npos) {
        auto str_frg_name = tecniq_input_stream->ReadIndexedString(chunkreader);
        auto frg_obj      = vulkan_shaderfile->_vk_shaderobjects[str_frg_name];
        if (frg_obj == nullptr) {
          printf("frg_obj<%s> not found\n", str_frg_name.c_str());
          OrkAssert(false);
        }
        vk_program->_frgshader = frg_obj;
      }

      ////////////////////////////////////////////////////////////
      // Populate program's UBO map from all shader stages
      // This is critical for descriptor set updates to find UBOs
      ////////////////////////////////////////////////////////////
      vk_program->_vk_uniformblks.clear();

      // Collect UBOs from vertex shader
      if (vk_program->_vtxshader && vk_program->_vtxshader->_uniblk_refs) {
        for (const auto& [name, ubo] : vk_program->_vtxshader->_uniblk_refs->_uniblks) {
          vk_program->_vk_uniformblks[name] = ubo;
          // printf("UBO_POPULATE: Program collected UBO<%s> from vertex shader\n", name.c_str());
        }
      }

      // Collect UBOs from geometry shader (if present)
      if (vk_program->_geoshader && vk_program->_geoshader->_uniblk_refs) {
        for (const auto& [name, ubo] : vk_program->_geoshader->_uniblk_refs->_uniblks) {
          vk_program->_vk_uniformblks[name] = ubo;
          // printf("UBO_POPULATE: Program collected UBO<%s> from geometry shader\n", name.c_str());
        }
      }

      // Collect UBOs from fragment shader
      if (vk_program->_frgshader && vk_program->_frgshader->_uniblk_refs) {
        for (const auto& [name, ubo] : vk_program->_frgshader->_uniblk_refs->_uniblks) {
          vk_program->_vk_uniformblks[name] = ubo;
          // printf("UBO_POPULATE: Program collected UBO<%s> from fragment shader\n", name.c_str());
        }
      }

      // printf("UBO_POPULATE: Program<%p> total UBOs: %zu\n",
      //        (void*)vk_program.get(), vk_program->_vk_uniformblks.size());

      ////////////////////////////////////////////////////////////
      // Collect SSBOs from vertex shader
      if (vk_program->_vtxshader && vk_program->_vtxshader->_ssbo_refs) {
        for (const auto& [name, ssbo] : vk_program->_vtxshader->_ssbo_refs->_ssbo_blocks) {
          vk_program->_vk_ssbo_blocks[name] = ssbo;
          // printf("SSBO_POPULATE: Program collected SSBO<%s> from vertex shader\n", name.c_str());
        }
      }

      // Collect SSBOs from geometry shader (if present)
      if (vk_program->_geoshader && vk_program->_geoshader->_ssbo_refs) {
        for (const auto& [name, ssbo] : vk_program->_geoshader->_ssbo_refs->_ssbo_blocks) {
          vk_program->_vk_ssbo_blocks[name] = ssbo;
          // printf("SSBO_POPULATE: Program collected SSBO<%s> from geometry shader\n", name.c_str());
        }
      }

      // Collect SSBOs from fragment shader
      if (vk_program->_frgshader && vk_program->_frgshader->_ssbo_refs) {
        for (const auto& [name, ssbo] : vk_program->_frgshader->_ssbo_refs->_ssbo_blocks) {
          vk_program->_vk_ssbo_blocks[name] = ssbo;
          // printf("SSBO_POPULATE: Program collected SSBO<%s> from fragment shader\n", name.c_str());
        }
      }

      // Collect SSBOs from compute shader (if present)
      if (vk_program->_comshader && vk_program->_comshader->_ssbo_refs) {
        for (const auto& [name, ssbo] : vk_program->_comshader->_ssbo_refs->_ssbo_blocks) {
          vk_program->_vk_ssbo_blocks[name] = ssbo;
          // printf("SSBO_POPULATE: Program collected SSBO<%s> from compute shader\n", name.c_str());
        }
      }

      ////////////////////////////////////////////////////////////
      auto sblk_name = tecniq_input_stream->ReadIndexedString(chunkreader);
      // printf("stateblock name<%s>\n", sblk_name.c_str());

      // PRE-RESOLVE the state block to a rasterstate at load time!
      auto it = vulkan_shaderfile->_stateblock_rasterstates.find(sblk_name);
      if (it != vulkan_shaderfile->_stateblock_rasterstates.end()) {
        vk_pass->_stateblock_rasterstate = it->second; // Store pre-resolved rasterstate
        vk_pass->_stateblock_rasterstate->_name = sblk_name;
      } else {
        printf("Warning: State block '%s' not found for pass\n", sblk_name.c_str());
        // Use a default rasterstate or nullptr
        vk_pass->_stateblock_rasterstate = nullptr;
      }
      if(0)printf("TEK<%s> RASTERSTATE<%p:%s>\n",  //
             str_tek_name.c_str(),        //
             (void*)vk_pass->_stateblock_rasterstate.get(),  //
             sblk_name.c_str());

      //////////////////////////////////////////////////////////////
      // push constants
      //////////////////////////////////////////////////////////////

      auto push_constants = std::make_shared<VkFxShaderPushConstantBlock>();
      push_constants->_ranges.reserve(16);

      auto pc_layout = std::make_shared<VkBufferLayout>();

      std::set<vkfxsuniset_ptr_t> unisets_set;

      // Track the layout cursor position for each shader stage
      std::map<VkShaderStageFlags, size_t> stage_start_offsets;
      std::map<VkShaderStageFlags, size_t> stage_end_offsets;

      auto uniset_to_pushconstants = [&](vkfxsobj_ptr_t shobj, vkbufferlayout_ptr_t dest_layout) {
        if (nullptr == shobj->_uniset_refs) {
          return;
        }

        // Determine shader stage and range index
        VkShaderStageFlags shader_stage = 0;
        size_t range_index              = 0;

        if (shobj == vk_program->_vtxshader) {
          shader_stage = VK_SHADER_STAGE_VERTEX_BIT;
          range_index  = 0;
        } else if (shobj == vk_program->_frgshader) {
          shader_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
          range_index  = 0;  // Fragment and vertex share the same push constant range
        } else if (shobj == vk_program->_geoshader) {
          shader_stage = VK_SHADER_STAGE_GEOMETRY_BIT;
          range_index  = 0; // All stages share the same push constant range for now
        }
        // TODO: Add tessellation shader support with appropriate range indices

        // Track where this stage's uniforms start in the layout
        if (stage_start_offsets.find(shader_stage) == stage_start_offsets.end()) {
          size_t start_cursor               = dest_layout->cursor();
          stage_start_offsets[shader_stage] = start_cursor;
          const char* stage_name            = "UNKNOWN";
          if (shader_stage == VK_SHADER_STAGE_VERTEX_BIT)
            stage_name = "VERTEX";
          else if (shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT)
            stage_name = "FRAGMENT";
          else if (shader_stage == VK_SHADER_STAGE_GEOMETRY_BIT)
            stage_name = "GEOMETRY";
          if (0)
            printf("DEBUG: Stage %s starting at cursor position %zu\n", stage_name, start_cursor);
        }

        for (auto uset_item : shobj->_uniset_refs->_unisets) {
          auto uset_name = uset_item.first;
          auto uset      = uset_item.second;
          auto it        = unisets_set.find(uset);
          if (it == unisets_set.end()) {
            unisets_set.insert(uset);
            for (auto item_ptr : uset->_items_by_order) {
              auto item_name = item_ptr->_identifier;
              auto datatype  = item_ptr->_datatype;
              size_t cursor  = 0xffffffff;
              auto orkparam  = item_ptr->_orkparam.get();

              // Set shader stage info for this item
              item_ptr->_shader_stage = shader_stage;
              item_ptr->_range_index  = range_index;

              if (datatype == "bool") {
                cursor = dest_layout->layoutItem<bool>(orkparam);
              } else if (datatype == "float") {
                cursor = dest_layout->layoutItem<float>(orkparam);
              } else if (datatype == "int") {
                cursor = dest_layout->layoutItem<int>(orkparam);
              } else if (datatype == "uint") {
                cursor = dest_layout->layoutItem<uint32_t>(orkparam);
              } else if (datatype == "uint32") {
                cursor = dest_layout->layoutItem<uint32_t>(orkparam);
              } else if (datatype == "vec2") {
                cursor = dest_layout->layoutItem<fvec2>(orkparam);
              } else if (datatype == "vec3") {
                cursor = dest_layout->layoutItem<fvec3>(orkparam);
              } else if (datatype == "vec4") {
                cursor = dest_layout->layoutItem<fvec4>(orkparam);
              } else if (datatype == "mat4") {
                cursor = dest_layout->layoutItem<fmtx4>(orkparam);
              } else if (datatype == "mat3") {
                cursor = dest_layout->layoutItem<fmtx3>(orkparam);
              } else {
                printf("VKFXI: unknown datatype<%s>\n", datatype.c_str());
                OrkAssert(false);
              }
              // Check if offsets match
              if (0) { //cursor != item_ptr->_offset) {
                printf(
                    "VKFXI: OFFSET MISMATCH param<%s> datatype<%s> cursor<%zu> shader_offset<%zu> stage<0x%x> range<%zu>\n",
                    item_name.c_str(),
                    datatype.c_str(),
                    cursor,
                    item_ptr->_offset,
                    shader_stage,
                    range_index);
              }
              //item_ptr->_offset = cursor; // Use the actual layout offset
            }
          }
        }

        // Track where this stage's uniforms end in the layout
        size_t end_cursor               = dest_layout->cursor();
        stage_end_offsets[shader_stage] = end_cursor;
        const char* stage_name          = "UNKNOWN";
        if (shader_stage == VK_SHADER_STAGE_VERTEX_BIT)
          stage_name = "VERTEX";
        else if (shader_stage == VK_SHADER_STAGE_FRAGMENT_BIT)
          stage_name = "FRAGMENT";
        else if (shader_stage == VK_SHADER_STAGE_GEOMETRY_BIT)
          stage_name = "GEOMETRY";
        if (0)
          printf("DEBUG: Stage %s ending at cursor position %zu\n", stage_name, end_cursor);
      };

      if (vk_program->_vtxshader) {
        uniset_to_pushconstants(vk_program->_vtxshader, pc_layout);
      }
      if (vk_program->_geoshader) {
        uniset_to_pushconstants(vk_program->_geoshader, pc_layout);
      }
      if (vk_program->_frgshader) {
        uniset_to_pushconstants(vk_program->_frgshader, pc_layout);
      }

      push_constants->_data_layout = pc_layout;

      size_t pc_size = alignUp(pc_layout->cursor(), 16);
      // TODO: Wire this up to the actual Vulkan device property
      constexpr size_t kDefaultMaxPushConstantSize = MAX_PUSH_CONSTANT_SIZE;
      if (pc_size > kDefaultMaxPushConstantSize) {
        printf(
            "ERROR: tek<%s> Push constant block size %zu exceeds default max %zu\n", //
            str_tek_name.c_str(),                                                    //
            pc_size,                                                                 //
            kDefaultMaxPushConstantSize);                                            //

        // print out all uniform set name that contributed to the push constant block
        printf("  Contributing uniform sets:\n");
        for (const auto& uset : unisets_set) {
          printf("    %s (num items: %zu)\n", uset->_name.c_str(), uset->_items_by_order.size());
          for (const auto& item : uset->_items_by_order) {
            printf("    %s (datatype: %s, offset: %zu)\n", item->_identifier.c_str(), item->_datatype.c_str(), item->_offset);
          }
        }
        printf("         This will cause issues on some hardware\n");
        OrkAssert(false);
      }
      push_constants->_ranges.reserve(8);

      // Create a single shared range for all stages since uniform sets are shared
      auto& pc_range = push_constants->_ranges.emplace_back();
      initializeVkStruct(pc_range);
      pc_range.offset     = 0;
      pc_range.size       = pc_size;
      pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
      if (0)
        printf("Push constant range: SHARED [0-%zu]\n", pc_size);

      // TODO: Handle additional shader stages:
      // - Geometry shader (VK_SHADER_STAGE_GEOMETRY_BIT)
      // - Tessellation shaders (VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT)
      // - Compute shader (VK_SHADER_STAGE_COMPUTE_BIT)

      push_constants->_blockSize = pc_size;
      vk_program->_pushdatabuffer.clear();
      vk_program->_pushdatabuffer.resize(pc_size);
      memset(vk_program->_pushdatabuffer.data(), 0, pc_size);
      vk_program->_pushConstantBlock = push_constants;

      ////////////////////////////////////////////////////////////
      // Read merged resources for this pass
      ////////////////////////////////////////////////////////////
      auto next_token = tecniq_input_stream->ReadIndexedString(chunkreader);
      if (next_token == "merged_resources") {
        auto merged_resources = std::make_shared<VkMergedResources>();

        size_t num_descriptor_sets = tecniq_input_stream->ReadItem<size_t>();

        for (size_t ds_idx = 0; ds_idx < num_descriptor_sets; ds_idx++) {
          auto descriptor_set_token = tecniq_input_stream->ReadIndexedString(chunkreader);
          OrkAssert(descriptor_set_token == "descriptor_set");

          int descriptor_set_id = tecniq_input_stream->ReadItem<int>();

          size_t num_sources = tecniq_input_stream->ReadItem<size_t>();

          // printf("MERGED RESOURCES: DESCRIPTOR_SET<%d> HAS %zu SOURCES\n", descriptor_set_id, num_sources);

          for (size_t src_idx = 0; src_idx < num_sources; src_idx++) {
            auto source_token = tecniq_input_stream->ReadIndexedString(chunkreader);
            OrkAssert(source_token == "source");

            auto source_name = tecniq_input_stream->ReadIndexedString(chunkreader);
            auto source_type = tecniq_input_stream->ReadIndexedString(chunkreader);

            // printf("  SOURCE: NAME<%s> TYPE<%s>\n", source_name.c_str(), source_type.c_str());

            auto descriptor_set_source         = std::make_shared<VkDescriptorSetSource>();
            descriptor_set_source->source_name = source_name;
            descriptor_set_source->source_type = source_type;

            size_t num_bindings = tecniq_input_stream->ReadItem<size_t>();

            for (size_t bind_idx = 0; bind_idx < num_bindings; bind_idx++) {
              auto binding_token = tecniq_input_stream->ReadIndexedString(chunkreader);
              OrkAssert(binding_token == "binding");

              auto binding             = std::make_shared<VkMergedResourceBinding>();
              binding->binding_id      = tecniq_input_stream->ReadItem<uint32_t>();
              binding->name            = tecniq_input_stream->ReadIndexedString(chunkreader);
              binding->datatype        = tecniq_input_stream->ReadIndexedString(chunkreader);
              binding->original_source = tecniq_input_stream->ReadIndexedString(chunkreader);
              binding->type            = static_cast<VkMergedResourceBinding::Type>(tecniq_input_stream->ReadItem<uint32_t>());

              const char* type_str = (binding->type == VkMergedResourceBinding::Type::UniformBlock)    ? "UBO"
                                     : (binding->type == VkMergedResourceBinding::Type::Sampler)       ? "SAMPLER"
                                     : (binding->type == VkMergedResourceBinding::Type::StorageBuffer) ? "SSBO"
                                                                                                       : "UNKNOWN";
              if (0)
                printf(
                    "XXXX<dbread> tekname<%s> BINDING[%u]: NAME<%s> TYPE<%s> DATATYPE<%s> ORIG_SOURCE<%s>\n",
                    str_tek_name.c_str(),
                    binding->binding_id,
                    binding->name.c_str(),
                    type_str,
                    binding->datatype.c_str(),
                    binding->original_source.c_str());

              descriptor_set_source->bindings.push_back(binding);
            }

            merged_resources->descriptor_sets[descriptor_set_id].push_back(descriptor_set_source);
          }
        }

        vk_pass->_merged_resources = merged_resources;
        
        // Auto-register UBOs in _merged_resource_bindings at load time
        // This ensures descriptor sets can be created for UBOs even when only
        // individual members are bound (not the entire UBO via bindUniformBuffer)
        if (merged_resources) {
          for (const auto& [set_id, sources] : merged_resources->descriptor_sets) {
            for (const auto& source : sources) {
              for (const auto& binding : source->bindings) {
                if (binding->type == VkMergedResourceBinding::Type::UniformBlock) {
                  // Find the VkFxShaderUniformBlk for this UBO
                  auto ubo_it = vulkan_shaderfile->_vk_uniformblks.find(binding->name);
                  if (ubo_it != vulkan_shaderfile->_vk_uniformblks.end()) {
                    auto vk_ubo = ubo_it->second;
                    if (vk_ubo && !vk_ubo->_items_by_order.empty()) {
                      // Use the first member's FxShaderParam as a representative key
                      // When any UBO member is bound, we can find its parent via _blockinfo
                      auto first_item = vk_ubo->_items_by_order[0];
                      if (first_item && first_item->_orkparam) {
                        auto fxparam = first_item->_orkparam.get();
                        vk_program->_incr_crc64.accumulateItem(fxparam);
                        vk_program->_merged_resource_bindings[fxparam] = DescBinding{uint32_t(set_id), binding->binding_id};
                        vk_program->_incr_crc64.accumulateItem(uint32_t(set_id));
                        vk_program->_incr_crc64.accumulateItem(binding->binding_id);
                        //printf("AUTO-REGISTERED UBO<%s> at set<%d> binding<%d>\n", 
                        //       binding->name.c_str(), set_id, binding->binding_id);
                      }
                    }
                  }
                }
              }
            }
          }
        }
      } else if (next_token == "no_merged_resources") {
        // No merged resources for this pass
        vk_pass->_merged_resources = std::make_shared<VkMergedResources>();
      } else {
        // Backward compatibility - assume no merged resources
        vk_pass->_merged_resources = std::make_shared<VkMergedResources>();
      }
      ////////////////////////////////////////////////////////////
      OrkAssert(vk_pass->_merged_resources!=nullptr);
      ////////////////////////////////////////////////////////////

      // orkid side
      auto ork_pass   = new FxShaderPass;
      ork_pass->_name = FormatString("pass-%zu", i);
      ork_pass->_impl.setShared<VkFxShaderPass>(vk_pass);

      ork_tek->_passes.push_back(ork_pass);
    } //for (size_t i = 0; i < num_passes; i++) {

    return vk_tek;
  };

  // Read state blocks before techniques (so they can be pre-resolved)
  read_stateblocks(vulkan_shaderfile.get(), tecniq_input_stream, chunkreader);

  for (size_t i = 0; i < num_techniques; i++) {
    auto tecnik = read_technique_from_stream();
  }

  return vulkan_shaderfile;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
