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

///////////////////////////////////////////////////////////////////////////////

vkfxsfile_ptr_t VkFxInterface::_readFromDataBlock(datablock_ptr_t vkfx_datablock, FxShader* ork_shader) {
  ////////////////////////////////////////////////////////
  // parse datablock
  ////////////////////////////////////////////////////////

  auto vulkan_shaderfile = std::make_shared<VkFxShaderFile>();
  ork_shader->_internalHandle.setShared<VkFxShaderFile>(vulkan_shaderfile);

  chunkfile::DefaultLoadAllocator load_alloc;
  chunkfile::Reader chunkreader(vkfx_datablock, load_alloc);
  auto header_input_stream   = chunkreader.GetStream("header");
  auto shader_input_stream   = chunkreader.GetStream("shaders");
  auto uniforms_input_stream = chunkreader.GetStream("uniforms");
  auto interfaces_input_stream = chunkreader.GetStream("interfaces");
  auto tecniq_input_stream   = chunkreader.GetStream("techniques");

  OrkAssert(shader_input_stream != nullptr);
  OrkAssert(tecniq_input_stream != nullptr);

  header_input_stream->dump();

  auto str_shader_counts = header_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_shader_counts == "shader_counts");
  size_t num_vtx_shaders    = header_input_stream->readItem<size_t>();
  size_t num_vtx_interfaces = header_input_stream->readItem<size_t>();
  size_t num_geo_shaders    = header_input_stream->readItem<size_t>();
  size_t num_geo_interfaces = header_input_stream->readItem<size_t>();
  size_t num_frg_shaders    = header_input_stream->readItem<size_t>();
  size_t num_frg_interfaces = header_input_stream->readItem<size_t>();
  size_t num_cu_shaders     = header_input_stream->readItem<size_t>();
  size_t num_unisets        = header_input_stream->readItem<size_t>();
  size_t num_uniblks        = header_input_stream->readItem<size_t>();
  size_t num_techniques     = header_input_stream->readItem<size_t>();
  size_t num_imports        = header_input_stream->readItem<size_t>();
  /////////////////////////////////
  // read unisets
  /////////////////////////////////
  auto str_unisets = uniforms_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_unisets == "unisets");
  auto num_unisets_cnt = uniforms_input_stream->readItem<size_t>();
  OrkAssert(num_unisets_cnt == num_unisets);
  for (size_t i = 0; i < num_unisets; i++) {
    auto str_uniset = uniforms_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_uniset == "uniset");
    auto str_uniset_name                                = uniforms_input_stream->readIndexedString(chunkreader);
    auto vk_uniset                                      = std::make_shared<VkFxShaderUniformSet>();
    vulkan_shaderfile->_vk_uniformsets[str_uniset_name] = vk_uniset;
    ///////////////////////////////////////////////
    auto str_samplers = uniforms_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_samplers == "samplers");
    auto num_samplers = uniforms_input_stream->readItem<size_t>();
    for (size_t j = 0; j < num_samplers; j++) {
      auto str_sampler_datatype   = uniforms_input_stream->readIndexedString(chunkreader);
      auto str_sampler_identifier = uniforms_input_stream->readIndexedString(chunkreader);
      auto vk_samp                = std::make_shared<VkFxShaderUniformSetSampler>();
      vk_samp->_datatype          = str_sampler_datatype;
      vk_samp->_identifier        = str_sampler_identifier;
      vk_samp->_orkparam          = std::make_shared<FxShaderParam>();
      vk_samp->_orkparam->_name   = str_sampler_identifier;
      vk_samp->_orkparam->_impl.set<VkFxShaderUniformSetSampler*>(vk_samp.get());
      vk_uniset->_samplers_by_name[str_sampler_identifier] = vk_samp;
      if (0)
        printf("uniset<%s> ADDING Sampler PARAM<%s>\n", str_uniset_name.c_str(), str_sampler_identifier.c_str());
    }
    ///////////////////////////////////////////////
    auto str_params = uniforms_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_params == "params");
    auto num_params = uniforms_input_stream->readItem<size_t>();
    for (size_t j = 0; j < num_params; j++) {
      auto str_param_datatype    = uniforms_input_stream->readIndexedString(chunkreader);
      auto str_param_identifier  = uniforms_input_stream->readIndexedString(chunkreader);
      auto vk_param              = std::make_shared<VkFxShaderUniformSetItem>();
      vk_param->_datatype        = str_param_datatype;
      vk_param->_identifier      = str_param_identifier;
      vk_param->_offset = uniforms_input_stream->readItem<size_t>();
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
  auto str_uniblks = uniforms_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_uniblks == "uniblks");
  auto num_uniblks_count = uniforms_input_stream->readItem<size_t>();
  OrkAssert(num_uniblks_count == num_uniblks);
  for (size_t i = 0; i < num_uniblks; i++) {
    auto str_uniblk = uniforms_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_uniblk == "uniblk");
    auto str_uniblk_name                                = uniforms_input_stream->readIndexedString(chunkreader);
    auto vk_uniblk                                      = std::make_shared<VkFxShaderUniformBlk>();
    vk_uniblk->_orkparamblock                           = std::make_shared<FxShaderParamBlock>();
    vulkan_shaderfile->_vk_uniformblks[str_uniblk_name] = vk_uniblk;
    if (0)
      printf("str_uniblk_name<%s>\n", str_uniblk_name.c_str());
    ///////////////////////////////////////////////
    auto str_params = uniforms_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_params == "items");
    auto num_params = uniforms_input_stream->readItem<size_t>();
    for (size_t j = 0; j < num_params; j++) {
      auto str_param_datatype   = uniforms_input_stream->readIndexedString(chunkreader);
      auto str_param_identifier = uniforms_input_stream->readIndexedString(chunkreader);
      auto vk_param             = std::make_shared<VkFxShaderUniformBlkItem>();
      vk_param->_datatype       = str_param_datatype;
      vk_param->_identifier     = str_param_identifier;
      vk_param->_offset = uniforms_input_stream->readItem<size_t>();
      vk_param->_orkparam       = std::make_shared<FxShaderParam>();
      vk_param->_orkparam->_impl.set<VkFxShaderUniformBlkItem*>(vk_param.get());
      vk_uniblk->_items_by_name[str_param_identifier] = vk_param;
      vk_uniblk->_items_by_order.push_back(vk_param);
      vk_uniblk->_orkparamblock->_subparams[str_param_identifier] = vk_param->_orkparam.get();
      if (0)
        printf("uniblk<%s> ADDING Item PARAM<%s>\n", str_uniblk_name.c_str(), str_param_identifier.c_str());
    }
  }
  /////////////////////////////////
  // read vertex interfaces
  /////////////////////////////////
  auto read_vertex_interface = [&](){
    auto str_interface = interfaces_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_interface == "interface");
    auto VKVTXIF = std::make_shared<VulkanVertexInterface>();
    VKVTXIF->_name = interfaces_input_stream->readIndexedString(chunkreader);
    vulkan_shaderfile->_vk_vtxinterfaces[VKVTXIF->_name] = VKVTXIF;
    printf(" vtx_interface<%s>\n", VKVTXIF->_name.c_str());
    /////////////////
    auto str_inputgroups = interfaces_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_inputgroups == "inputgroups");
    /////////////////
    auto num_input_groups = interfaces_input_stream->readItem<size_t>();
    for( size_t ig=0; ig<num_input_groups; ig++ ){
      auto num_inputs = interfaces_input_stream->readItem<size_t>();
      for( size_t ii=0; ii<num_inputs; ii++ ){
        auto str_input = interfaces_input_stream->readIndexedString(chunkreader);
        OrkAssert(str_input == "input");
        auto VKVTXIFINP = std::make_shared<VulkanVertexInterfaceInput>();
        VKVTXIFINP->_datatype = interfaces_input_stream->readIndexedString(chunkreader);
        VKVTXIFINP->_identifier = interfaces_input_stream->readIndexedString(chunkreader);
        VKVTXIFINP->_semantic = interfaces_input_stream->readIndexedString(chunkreader);
        VKVTXIFINP->_datasize = interfaces_input_stream->readItem<size_t>();
        VKVTXIF->_inputs.push_back(VKVTXIFINP);
      }
    }
    /////////////////
    auto str_outputgroups = interfaces_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_outputgroups == "outputgroups");
    auto num_output_groups = interfaces_input_stream->readItem<size_t>();
    for( size_t og=0; og<num_output_groups; og++ ){
      auto num_outputs = interfaces_input_stream->readItem<size_t>();
      for( size_t oi=0; oi<num_outputs; oi++ ){
        auto str_output = interfaces_input_stream->readIndexedString(chunkreader);
        OrkAssert(str_output == "output");
        auto str_output_datatype = interfaces_input_stream->readIndexedString(chunkreader);
        auto str_output_identifier = interfaces_input_stream->readIndexedString(chunkreader);
        printf("  output<%s %s>\n", str_output_datatype.c_str(), str_output_identifier.c_str());
        auto dsize = interfaces_input_stream->readItem<size_t>();
        if( str_output_identifier.find("gl_") != 0 ){
          // _appendText(_interface_group, "layout(location=%zu) out %s %s;", _output_index, dt.c_str(), id.c_str());
          //o_output_index += dsize;
        }
      }
    }
    /////////////////
  };
  auto str_vtx_interfaces = interfaces_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_vtx_interfaces == "vertex-interfaces");
  auto vif_count = interfaces_input_stream->readItem<size_t>();
  OrkAssert(vif_count == num_vtx_interfaces);
  for (size_t i = 0; i < vif_count; i++) {
    read_vertex_interface();
  }
  /////////////////////////////////
  // read vtx interface inheritances
  /////////////////////////////////
  auto str_vif_inheritances = interfaces_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_vif_inheritances == "vertex_interface_inheritances");
  auto num_vif_inheritances = interfaces_input_stream->readItem<size_t>();
  for (size_t i = 0; i < num_vif_inheritances; i++) {
    auto str_vif_name = interfaces_input_stream->readIndexedString(chunkreader);
    auto num_inh       = interfaces_input_stream->readItem<size_t>();
    if(num_inh>0){
      auto str_inh_name = interfaces_input_stream->readIndexedString(chunkreader);
      printf(" vtx_interface<%s> INHERITS vtx_interface<%s>\n", str_vif_name.c_str(), str_inh_name.c_str());

      auto if_child = vulkan_shaderfile->_vk_vtxinterfaces.find(str_vif_name);
      OrkAssert(if_child != vulkan_shaderfile->_vk_vtxinterfaces.end());
      auto if_parent = vulkan_shaderfile->_vk_vtxinterfaces.find(str_inh_name);
      OrkAssert(if_parent != vulkan_shaderfile->_vk_vtxinterfaces.end());
      if_child->second->_parent = if_parent->second;
    }
  }
  auto str_ifaces_done = interfaces_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_ifaces_done == "interfaces-done");
  /////////////////////////////////
  // read shader
  /////////////////////////////////
  auto read_shader_from_stream = [&]() -> vkfxsobj_ptr_t {
    /////////////////////////////////
    // read shader
    /////////////////////////////////
    auto str_shader = shader_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_shader == "shader");
    auto str_shader_type = shader_input_stream->readIndexedString(chunkreader);
    auto str_shader_name = shader_input_stream->readIndexedString(chunkreader);
    auto sh_bytlen       = shader_input_stream->readItem<size_t>();
    auto sh_data         = shader_input_stream->readData(sh_bytlen);
    /////////////////////////////////
    // shader spirv binary
    /////////////////////////////////
    vkfxshader_bin_t shader_bin;
    shader_bin.resize(sh_bytlen / sizeof(uint32_t));
    memcpy(shader_bin.data(), sh_data.data(), sh_bytlen);
    auto vulkan_shobj                                     = std::make_shared<VulkanFxShaderObject>(_contextVK, shader_bin);
    vulkan_shaderfile->_vk_shaderobjects[str_shader_name] = vulkan_shobj;
    /////////////////////////////////
    auto num_iunisets = shader_input_stream->readItem<size_t>();
    if (num_iunisets) {
      auto refs                  = std::make_shared<VkFxShaderUniformSetsReference>();
      vulkan_shobj->_uniset_refs = refs;
      for (size_t i = 0; i < num_iunisets; i++) {
        auto str_uniset = shader_input_stream->readIndexedString(chunkreader);
        auto it         = vulkan_shaderfile->_vk_uniformsets.find(str_uniset);
        OrkAssert(it != vulkan_shaderfile->_vk_uniformsets.end());
        vkfxsuniset_ptr_t vk_uniset = it->second;
        refs->_unisets[str_uniset]  = vk_uniset;
      }
      OrkAssert(refs->_unisets.size() < 2);
    }
    /////////////////////////////////
    auto num_ifaces = shader_input_stream->readItem<size_t>();
    if (num_ifaces) {
      for (size_t i = 0; i < num_ifaces; i++) {
        auto str_iface = shader_input_stream->readIndexedString(chunkreader);
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
    auto str_tek = tecniq_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_tek == "technique");
    auto str_tek_name = tecniq_input_stream->readIndexedString(chunkreader);

    auto vk_tek                                     = std::make_shared<VkFxShaderTechnique>();
    vulkan_shaderfile->_vk_techniques[str_tek_name] = vk_tek;

    size_t num_passes = tecniq_input_stream->readItem<size_t>();

    auto ork_tek = new FxShaderTechnique;
    ork_tek->_impl.set<VkFxShaderTechnique*>(vk_tek.get());
    ork_tek->_techniqueName               = str_tek_name;
    ork_tek->_shader                      = ork_shader;
    ork_shader->_techniques[str_tek_name] = ork_tek;

    for (size_t i = 0; i < num_passes; i++) {

      // vulkan side
      auto str_pass = tecniq_input_stream->readIndexedString(chunkreader);
      OrkAssert(str_pass == "pass");
      auto str_vtx_name = tecniq_input_stream->readIndexedString(chunkreader);
      auto str_frg_name = tecniq_input_stream->readIndexedString(chunkreader);
      auto vk_pass      = std::make_shared<VkFxShaderPass>();
      auto vk_program   = std::make_shared<VkFxShaderProgram>(vulkan_shaderfile.get());
      auto vtx_obj      = vulkan_shaderfile->_vk_shaderobjects[str_vtx_name];
      auto frg_obj      = vulkan_shaderfile->_vk_shaderobjects[str_frg_name];
      if(vtx_obj==nullptr){
        printf("vtx_obj<%s> not found\n", str_vtx_name.c_str());
        OrkAssert(false);
      }
      if(frg_obj==nullptr){
        printf("frg_obj<%s> not found\n", str_frg_name.c_str());
        OrkAssert(false);
      }
      vk_program->_vtxshader = vtx_obj;
      vk_program->_frgshader = frg_obj;
      vk_pass->_vk_program   = vk_program;
      vk_tek->_vk_passes.push_back(vk_pass);
      static int prog_index      = 0;
      vk_program->_pipeline_bits_prg = prog_index;
      prog_index++;
      OrkAssert(prog_index < 128);

      ////////////////////////////////////////////////////////////

      auto unisets_to_descriptors = [&](vkfxsobj_ptr_t shobj, vkdescriptors_ptr_t desc_set, uint32_t stage_bits) {
        if (shobj->_uniset_refs) {
          std::set<vkfxsuniset_ptr_t> unisets_set;
          for (auto uset_item : shobj->_uniset_refs->_unisets) {
            auto uset_name = uset_item.first;
            auto uset      = uset_item.second;
            auto it        = unisets_set.find(uset);
            if (it == unisets_set.end()) {
              unisets_set.insert(uset);
              ///////////////////////////////////////////
              // loose samplers
              ///////////////////////////////////////////
              for (auto item : uset->_samplers_by_name) {
                auto item_name       = item.first;
                auto item_ptr        = item.second;
                auto orkparam        = item_ptr->_orkparam.get();
                size_t binding_index = desc_set->_sampler_count++;
                auto it              = vk_program->_samplers_by_orkparam.find(orkparam);
                OrkAssert(it == vk_program->_samplers_by_orkparam.end());
                vk_program->_samplers_by_orkparam[orkparam] = binding_index;

                auto& vkb = desc_set->_vkbindings.emplace_back();
                initializeVkStruct(vkb);
                vkb.binding        = binding_index;                             // TODO : query from shader
                vkb.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; // Type of the descriptor (e.g., uniform buffer,
                                                                                // combined image sampler, etc.)
                vkb.descriptorCount = 1;          // Number of descriptors in this binding (useful for arrays of descriptors)
                vkb.stageFlags      = stage_bits; // Shader stage to bind this descriptor to
                // vkb.pImmutableSamplers = &immutableSampler; // Only relevant for samplers and combined image samplers
              }
            }
          }
        }
      };
      //////////////////////////////////////////////////////////////
      std::set<vkfxsuniset_ptr_t> unisets_set;
      auto uniset_to_pushconstants = [&](vkfxsobj_ptr_t shobj, vkbufferlayout_ptr_t dest_layout) {
        if (nullptr == shobj->_uniset_refs) {
          return;
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
              if (datatype == "float") {
                cursor = dest_layout->layoutItem<float>(orkparam);
              } else if (datatype == "int") {
                cursor = dest_layout->layoutItem<int>(orkparam);
              } else if (datatype == "vec2") {
                cursor = dest_layout->layoutItem<fvec2>(orkparam);
              } else if (datatype == "vec3") {
                cursor = dest_layout->layoutItem<fvec3>(orkparam);
              } else if (datatype == "vec4") {
                cursor = dest_layout->layoutItem<fvec4>(orkparam);
              } else if (datatype == "mat4") {
                cursor = dest_layout->layoutItem<fmtx4>(orkparam);
              } else {
                printf("VKFXI: unknown datatype<%s>\n", datatype.c_str());
                OrkAssert(false);
              }
              //OrkAssert(cursor==item_ptr->_offset);
              printf("VKFXI: datatype<%s> cursor<%zu>\n", datatype.c_str(), cursor);
            }
          }
        }
      };

      //////////////////////////////////////////////////////////////
      // descriptors
      //////////////////////////////////////////////////////////////

      auto descriptors = std::make_shared<VkDescriptorSetBindings>();
      // descriptors->_vksamplers.reserve( 32 );
      descriptors->_vkbindings.reserve(32);

      // unisets_to_descriptors(vtx_obj, descriptors, VK_SHADER_STAGE_VERTEX_BIT );
      unisets_to_descriptors(frg_obj, descriptors, VK_SHADER_STAGE_FRAGMENT_BIT);
      vk_program->_descriptors = descriptors;
      //////////////////////////////////////////////////////////////
      // push constants
      //////////////////////////////////////////////////////////////

      auto push_constants = std::make_shared<VkFxShaderPushConstantBlock>();
      push_constants->_ranges.reserve(16);

      auto pc_layout = std::make_shared<VkBufferLayout>();
      uniset_to_pushconstants(vtx_obj, pc_layout);
      uniset_to_pushconstants(frg_obj, pc_layout);
      push_constants->_data_layout = pc_layout;

      size_t pc_size = alignUp(pc_layout->cursor(), 16);
      push_constants->_ranges.reserve(8);
      auto& pc_range = push_constants->_ranges.emplace_back();
      initializeVkStruct(pc_range);
      pc_range.offset     = 0;
      pc_range.size       = pc_size;
      pc_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

      push_constants->_blockSize = pc_size;
      vk_program->_pushdatabuffer.clear();
      vk_program->_pushdatabuffer.resize(pc_size);
      memset(vk_program->_pushdatabuffer.data(), 0, pc_size);
      vk_program->_pushConstantBlock = push_constants;

      ////////////////////////////////////////////////////////////

      // orkid side
      auto ork_pass   = new FxShaderPass;
      ork_pass->_name = FormatString("pass-%zu", i);
      ork_pass->_impl.setShared<VkFxShaderPass>(vk_pass);

      ork_tek->_passes.push_back(ork_pass);
    }

    return vk_tek;
  };

  for (size_t i = 0; i < num_techniques; i++) {
    auto tecnik = read_technique_from_stream();
  }

  return vulkan_shaderfile;
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
