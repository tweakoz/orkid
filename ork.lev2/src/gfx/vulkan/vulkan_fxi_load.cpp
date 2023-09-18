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

datablock_ptr_t VkFxInterface::_writeIntermediateToDataBlock(shadlang::SHAST::transunit_ptr_t transunit){
    chunkfile::Writer chunkwriter("xfx");
    auto header_stream = chunkwriter.AddStream("header");
    auto shader_stream = chunkwriter.AddStream("shaders");
    auto uniforms_stream = chunkwriter.AddStream("uniforms");
    auto tecniq_stream = chunkwriter.AddStream("techniques");

    /////////////////////////////////////////////////////////////////////////////
    // compile all shaders from translation unit
    /////////////////////////////////////////////////////////////////////////////

    // TODO - before hoisting cache, implement namespaces..

    auto vtx_shaders = SHAST::AstNode::collectNodesOfType<SHAST::VertexShader>(transunit);
    auto frg_shaders = SHAST::AstNode::collectNodesOfType<SHAST::FragmentShader>(transunit);
    auto cu_shaders  = SHAST::AstNode::collectNodesOfType<SHAST::ComputeShader>(transunit);
    auto techniques  = SHAST::AstNode::collectNodesOfType<SHAST::Technique>(transunit);
    auto unisets     = SHAST::AstNode::collectNodesOfType<SHAST::UniformSet>(transunit);
    auto uniblks     = SHAST::AstNode::collectNodesOfType<SHAST::UniformBlk>(transunit);
    auto imports     = SHAST::AstNode::collectNodesOfType<SHAST::ImportDirective>(transunit);

    size_t num_vtx_shaders = vtx_shaders.size();
    size_t num_frg_shaders = frg_shaders.size();
    size_t num_cu_shaders  = cu_shaders.size();
    size_t num_techniques  = techniques.size();
    size_t num_unisets     = unisets.size();
    size_t num_uniblks     = uniblks.size();
    size_t num_imports     = imports.size();

    printf("num_vtx_shaders<%zu>\n", num_vtx_shaders);
    printf("num_frg_shaders<%zu>\n", num_frg_shaders);
    printf("num_cu_shaders<%zu>\n", num_cu_shaders);
    printf("num_techniques<%zu>\n", num_techniques);
    printf("num_unisets<%zu>\n", num_unisets);
    printf("num_uniblks<%zu>\n", num_uniblks);
    printf("num_imports<%zu>\n", num_imports);
    //////////////////
    auto SPC = std::make_shared<spirv::SpirvCompiler>(transunit, true);
    for (auto spirvuniset : SPC->_spirvuniformsets) {
      printf("spirvuniset<%s>\n", spirvuniset.first.c_str());
    }
    for (auto spirvuniblk : SPC->_spirvuniformblks) {
      printf("spirvuniblk<%s>\n", spirvuniblk.first.c_str());
    }
    //////////////////
    // begin shader stream
    //////////////////
    header_stream->addIndexedString("shader_counts", chunkwriter);
    header_stream->addItem<uint64_t>(num_vtx_shaders);
    header_stream->addItem<uint64_t>(num_frg_shaders);
    header_stream->addItem<uint64_t>(num_cu_shaders);
    header_stream->addItem<uint64_t>(num_unisets);
    header_stream->addItem<uint64_t>(num_uniblks);
    header_stream->addItem<uint64_t>(num_techniques);
    header_stream->addItem<uint64_t>(num_imports);
    //////////////////
    // uniformsets
    //////////////////

    auto write_unisets_to_stream = [&](std::unordered_map<std::string, spirv::spirvuniset_ptr_t>& spirv_unisets) { //
      uniforms_stream->addIndexedString("unisets", chunkwriter);
      uniforms_stream->addItem<size_t>(spirv_unisets.size());

      for (auto spirv_item : spirv_unisets) {
        std::string name  = spirv_item.first;
        auto spirv_uniset = spirv_item.second;
        /////////////////////////////////////////////
        // vk_uniset->_descriptor_set_id = spirv_uniset->_descriptor_set_id;
        /////////////////////////////////////////////
        // rebuild _samplers_by_name
        /////////////////////////////////////////////
        uniforms_stream->addIndexedString("uniset", chunkwriter);
        uniforms_stream->addIndexedString(name, chunkwriter);
        uniforms_stream->addIndexedString("samplers", chunkwriter);
        uniforms_stream->addItem<size_t>(spirv_uniset->_samplers_by_name.size());
        for (auto item : spirv_uniset->_samplers_by_name) {
          uniforms_stream->addIndexedString(item.second->_datatype, chunkwriter);
          uniforms_stream->addIndexedString(item.second->_identifier, chunkwriter);
        }
        /////////////////////////////////////////////
        // rebuild _items_by_name
        /////////////////////////////////////////////
        uniforms_stream->addIndexedString("params", chunkwriter);
        uniforms_stream->addItem<size_t>(spirv_uniset->_items_by_order.size());
        for (auto item : spirv_uniset->_items_by_order) {
          uniforms_stream->addIndexedString(item->_datatype, chunkwriter);
          uniforms_stream->addIndexedString(item->_identifier, chunkwriter);
        }
      }
    };

    //////////////////
    // uniformblks
    //////////////////

    auto write_uniblks_to_stream = [&](std::unordered_map<std::string, spirv::spirvuniblk_ptr_t>& spirv_uniblks) { //
      uniforms_stream->addIndexedString("uniblks", chunkwriter);
      uniforms_stream->addItem<size_t>(spirv_uniblks.size());

      // std::unordered_map<std::string, vkfxsuniblk_ptr_t> rval;
      for (auto spirv_item : spirv_uniblks) {
        std::string name  = spirv_item.first;
        auto spirv_uniblk = spirv_item.second;
        uniforms_stream->addIndexedString("uniblk", chunkwriter);
        uniforms_stream->addIndexedString(name, chunkwriter);
        /////////////////////////////////////////////
        // rebuild _items_by_name
        /////////////////////////////////////////////
        uniforms_stream->addIndexedString("items", chunkwriter);
        uniforms_stream->addItem<size_t>(spirv_uniblk->_items_by_name.size());
        for (auto item : spirv_uniblk->_items_by_order) {
          auto vk_item = std::make_shared<VkFxShaderUniformBlkItem>();
          uniforms_stream->addIndexedString(item->_datatype, chunkwriter);
          uniforms_stream->addIndexedString(item->_identifier, chunkwriter);
        }
      }
    };

    write_unisets_to_stream(SPC->_spirvuniformsets);
    write_uniblks_to_stream(SPC->_spirvuniformblks);

    ////////////////////////////////////////////////////////////////

    size_t num_shaders_written = 0;

    auto write_shader_to_stream = [&](SHAST::astnode_ptr_t shader_node, std::string shader_type) {
      auto sh_name  = shader_node->typedValueForKey<std::string>("object_name").value();
      auto sh_data  = (uint8_t*)SPC->_spirv_binary.data();
      size_t sh_len = SPC->_spirv_binary.size() * sizeof(uint32_t);
      shader_stream->addIndexedString("shader", chunkwriter);
      shader_stream->addIndexedString(shader_type, chunkwriter);
      shader_stream->addIndexedString(sh_name, chunkwriter);
      shader_stream->addItem<size_t>(sh_len);
      shader_stream->addData(sh_data, sh_len);

      shadlang::spirv::InheritanceTracker tracker;
      SPC->fetchInheritances(tracker,shader_node);

      shader_stream->addItem<size_t>(tracker._inherited_unisets.size());
      for (auto INHID : tracker._inherited_unisets ) {
        shader_stream->addIndexedString(INHID, chunkwriter);
        printf("INHID<%s>\n", INHID.c_str());
      }

      num_shaders_written++;
    };

    //////////////////
    // vertex shaders
    //////////////////

    for (auto vshader : vtx_shaders) {
      SPC->processShader(vshader);
      write_shader_to_stream(vshader, "vertex");
    }

    //////////////////
    // fragment shaders
    //////////////////

    for (auto fshader : frg_shaders) {
      SPC->processShader(fshader);
      write_shader_to_stream(fshader, "fragment");
    }

    //////////////////
    // compute shaders
    //////////////////

    for (auto cshader : cu_shaders) {
      SPC->processShader(cshader); //
      write_shader_to_stream(cshader, "compute");
    }

    //////////////////
    // techniques (always VTG for now)
    //////////////////

    for (auto tek : techniques) {
      auto passes   = SHAST::AstNode::collectNodesOfType<SHAST::Pass>(tek);
      auto tek_name = tek->typedValueForKey<std::string>("object_name").value();
      tecniq_stream->addIndexedString("technique", chunkwriter);
      tecniq_stream->addIndexedString(tek_name, chunkwriter);
      tecniq_stream->addItem<size_t>(passes.size());
      for (auto p : passes) {
        auto vtx_shader_ref = p->findFirstChildOfType<SHAST::VertexShaderRef>();
        auto frg_shader_ref = p->findFirstChildOfType<SHAST::FragmentShaderRef>();
        auto stateblock_ref = p->findFirstChildOfType<SHAST::StateBlockRef>();
        OrkAssert(vtx_shader_ref);
        OrkAssert(frg_shader_ref);
        auto vtx_sema_id = vtx_shader_ref->findFirstChildOfType<SHAST::SemaIdentifier>();
        auto frg_sema_id = frg_shader_ref->findFirstChildOfType<SHAST::SemaIdentifier>();
        OrkAssert(vtx_sema_id);
        OrkAssert(frg_sema_id);
        auto vtx_name = vtx_sema_id->typedValueForKey<std::string>("identifier_name").value();
        auto frg_name = frg_sema_id->typedValueForKey<std::string>("identifier_name").value();
        tecniq_stream->addIndexedString("pass", chunkwriter);
        tecniq_stream->addIndexedString(vtx_name, chunkwriter);
        tecniq_stream->addIndexedString(frg_name, chunkwriter);
      }
    } // for (auto tek : techniques) {

    auto out_datablock = std::make_shared<DataBlock>();
    chunkwriter.writeToDataBlock(out_datablock);
    return out_datablock;
}

///////////////////////////////////////////////////////////////////////////////

vkfxsfile_ptr_t VkFxInterface::_readFromDataBlock(datablock_ptr_t vkfx_datablock,
                                                  FxShader* ork_shader ){
  ////////////////////////////////////////////////////////
  // parse datablock
  ////////////////////////////////////////////////////////

  auto vulkan_shaderfile = std::make_shared<VkFxShaderFile>();
  ork_shader->_internalHandle.setShared<VkFxShaderFile>(vulkan_shaderfile);

  chunkfile::DefaultLoadAllocator load_alloc;
  chunkfile::Reader chunkreader(vkfx_datablock, load_alloc);
  auto header_input_stream = chunkreader.GetStream("header");
  auto shader_input_stream = chunkreader.GetStream("shaders");
  auto uniforms_input_stream = chunkreader.GetStream("uniforms");
  auto tecniq_input_stream = chunkreader.GetStream("techniques");

  OrkAssert(shader_input_stream != nullptr);
  OrkAssert(tecniq_input_stream != nullptr);

  header_input_stream->dump();

  auto str_shader_counts = header_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_shader_counts == "shader_counts");
  size_t num_vtx_shaders = header_input_stream->readItem<size_t>();
  size_t num_frg_shaders = header_input_stream->readItem<size_t>();
  size_t num_cu_shaders  = header_input_stream->readItem<size_t>();
  size_t num_unisets     = header_input_stream->readItem<size_t>();
  size_t num_uniblks     = header_input_stream->readItem<size_t>();
  size_t num_techniques  = header_input_stream->readItem<size_t>();
  size_t num_imports     = header_input_stream->readItem<size_t>();
  /////////////////////////////////
  // read unisets
  /////////////////////////////////
  auto str_unisets = uniforms_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_unisets == "unisets");
  auto num_unisets_cnt = uniforms_input_stream->readItem<size_t>();
  OrkAssert(num_unisets_cnt==num_unisets);
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
      vk_samp->_orkparam->_impl.set<VkFxShaderUniformSetSampler*>(vk_samp.get());
      vk_uniset->_samplers_by_name[str_sampler_identifier] = vk_samp;
      if(0)printf("uniset<%s> ADDING Sampler PARAM<%s>\n", str_uniset_name.c_str(), str_sampler_identifier.c_str());
    }
    ///////////////////////////////////////////////
    auto str_params = uniforms_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_params == "params");
    auto num_params = uniforms_input_stream->readItem<size_t>();
    for (size_t j = 0; j < num_params; j++) {
      auto str_param_datatype   = uniforms_input_stream->readIndexedString(chunkreader);
      auto str_param_identifier = uniforms_input_stream->readIndexedString(chunkreader);
      auto vk_param             = std::make_shared<VkFxShaderUniformSetItem>();
      vk_param->_datatype       = str_param_datatype;
      vk_param->_identifier     = str_param_identifier;
      vk_param->_orkparam       = std::make_shared<FxShaderParam>();
      vk_param->_orkparam->_impl.set<VkFxShaderUniformSetItem*>(vk_param.get());
      vk_uniset->_items_by_name[str_param_identifier] = vk_param;
      vk_uniset->_items_by_order.push_back(vk_param);
      if(0)printf("uniset<%s> ADDING Item PARAM<%s>\n", str_uniset_name.c_str(), str_param_identifier.c_str());
    }
  }
  /////////////////////////////////
  // read uniblks
  /////////////////////////////////
  auto str_uniblks = uniforms_input_stream->readIndexedString(chunkreader);
  OrkAssert(str_uniblks == "uniblks");
  auto num_uniblks_count = uniforms_input_stream->readItem<size_t>();
  OrkAssert(num_uniblks_count==num_uniblks);
  for (size_t i = 0; i < num_uniblks; i++) {
    auto str_uniblk = uniforms_input_stream->readIndexedString(chunkreader);
    OrkAssert(str_uniblk == "uniblk");
    auto str_uniblk_name                                = uniforms_input_stream->readIndexedString(chunkreader);
    auto vk_uniblk                                      = std::make_shared<VkFxShaderUniformBlk>();
    vk_uniblk->_orkparamblock = std::make_shared<FxShaderParamBlock>();
    vulkan_shaderfile->_vk_uniformblks[str_uniblk_name] = vk_uniblk;
    if(0)printf("str_uniblk_name<%s>\n", str_uniblk_name.c_str());
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
      vk_param->_orkparam       = std::make_shared<FxShaderParam>();
      vk_param->_orkparam->_impl.set<VkFxShaderUniformBlkItem*>(vk_param.get());
      vk_uniblk->_items_by_name[str_param_identifier] = vk_param;
      vk_uniblk->_items_by_order.push_back(vk_param);
      vk_uniblk->_orkparamblock->_subparams[str_param_identifier] = vk_param->_orkparam.get();
      if(0)printf("uniblk<%s> ADDING Item PARAM<%s>\n", str_uniblk_name.c_str(), str_param_identifier.c_str());
    }
  }
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
    if(num_iunisets){
      auto refs = std::make_shared<VkFxShaderUniformSetsReference>();
      vulkan_shobj->_uniset_refs = refs;
      for (size_t i = 0; i < num_iunisets; i++) {
        auto str_uniset = shader_input_stream->readIndexedString(chunkreader);
        auto it = vulkan_shaderfile->_vk_uniformsets.find(str_uniset);
        OrkAssert(it!=vulkan_shaderfile->_vk_uniformsets.end());
        vkfxsuniset_ptr_t vk_uniset = it->second;
        refs->_unisets[str_uniset] = vk_uniset;
      }
      OrkAssert(refs->_unisets.size()<2);
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
    STGIV.stage        = VK_SHADER_STAGE_VERTEX_BIT;
    STGIV.module       = vtx_shader->_vk_shadermodule;
    STGIV.pName        = "main";
  }

  //////////////////

  for (size_t i = 0; i < num_frg_shaders; i++) {
    auto frg_shader    = read_shader_from_stream();
    frg_shader->_STAGE = "fragment"_crcu;
    auto& STGIF        = frg_shader->_shaderstageinfo;
    initializeVkStruct(STGIF, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    STGIF.stage        = VK_SHADER_STAGE_FRAGMENT_BIT;
    STGIF.module       = frg_shader->_vk_shadermodule;
    STGIF.pName        = "main";
  }

  //////////////////

  for (size_t i = 0; i < num_cu_shaders; i++) {
    auto cu_shader    = read_shader_from_stream();
    cu_shader->_STAGE = "compute"_crcu;
    auto& STCIF        = cu_shader->_shaderstageinfo;
    initializeVkStruct(STCIF, VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    STCIF.stage       = VK_SHADER_STAGE_COMPUTE_BIT;
    STCIF.module      = cu_shader->_vk_shadermodule;
    STCIF.pName       = "main";
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
    ork_tek->_impl.setShared<VkFxShaderTechnique>(vk_tek);
    ork_tek->_techniqueName = str_tek_name;
    ork_tek->_shader        = ork_shader;
    ork_shader->_techniques[str_tek_name]=ork_tek;

    for (size_t i = 0; i < num_passes; i++) {
      
      // vulkan side
      auto str_pass = tecniq_input_stream->readIndexedString(chunkreader);
      OrkAssert(str_pass == "pass");
      auto str_vtx_name = tecniq_input_stream->readIndexedString(chunkreader);
      auto str_frg_name = tecniq_input_stream->readIndexedString(chunkreader);
      auto vk_pass      = std::make_shared<VkFxShaderPass>();
      auto vk_program   = std::make_shared<VkFxShaderProgram>();
      auto vtx_obj      = vulkan_shaderfile->_vk_shaderobjects[str_vtx_name];
      auto frg_obj      = vulkan_shaderfile->_vk_shaderobjects[str_frg_name];
      OrkAssert(vtx_obj);
      OrkAssert(frg_obj);
      vk_program->_vtxshader = vtx_obj;
      vk_program->_frgshader = frg_obj;
      vk_pass->_vk_program   = vk_program;
      vk_tek->_vk_passes.push_back(vk_pass);
      static int prog_index = 0;
      vk_program->_pipeline_bits = prog_index;
      OrkAssert(prog_index<256);
    
      ////////////////////////////////////////////////////////////

      auto push_constants = std::make_shared<VkFxShaderPushConstantBlock>();
      vk_program->_pushConstantBlock = push_constants;
      push_constants->_ranges.reserve(16);

      size_t push_constant_offset = 0;

      auto do_unisets = [&](vkfxsobj_ptr_t shobj,
                            uniset_map_t& dest_usetmap,
                            uniset_item_map_t& dest_usetitemmap,
                            vkbufferlayout_ptr_t dest_layout,
                            VkPushConstantRange& dest_range,
                            vkdescriptors_ptr_t desc_set,
                            uint32_t stage_bits ){
        if( shobj->_uniset_refs ){
          std::set<vkfxsuniset_ptr_t> unisets_set;
          for( auto uset_item : shobj->_uniset_refs->_unisets ){
            auto uset_name = uset_item.first;
            auto uset = uset_item.second;
            auto it = unisets_set.find( uset );
            if(it==unisets_set.end()){
              unisets_set.insert( uset );
              dest_usetmap[uset_name] = uset;
              ///////////////////////////////////////////
              // loose uniform items (not samplers)
              ///////////////////////////////////////////
              for( auto item : uset->_items_by_name ){
                auto item_name = item.first;
                auto item_ptr  = item.second;
                auto it = dest_usetitemmap.find(item_name);
                printf( "merging uset<%s> itemname<%s>\n", uset_name.c_str(), item_name.c_str());
                OrkAssert(it==dest_usetitemmap.end());
                dest_usetitemmap[item_name] = item_ptr;
              }
              ///////////////////////////////////////////
              // loose samplers
              ///////////////////////////////////////////
              for( auto item : uset->_samplers_by_name ){
                auto item_name = item.first;
                auto item_ptr  = item.second;

                VkSamplerCreateInfo SI;
                initializeVkStruct(SI, VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
                SI.magFilter = VK_FILTER_NEAREST;
                SI.minFilter = VK_FILTER_NEAREST;
                SI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
                SI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                SI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                SI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
                SI.mipLodBias = 0.0f;
                SI.anisotropyEnable = VK_FALSE;
                SI.maxAnisotropy = 1.0f;
                SI.compareEnable = VK_FALSE;
                SI.compareOp = VK_COMPARE_OP_NEVER;
                SI.minLod = 0.0f;
                SI.maxLod = 0.0f;
                SI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
                SI.unnormalizedCoordinates = VK_FALSE;

                VkSampler& immutableSampler = desc_set->_vksamplers.emplace_back();
                vkCreateSampler(_contextVK->_vkdevice, 
                                &SI, 
                                nullptr, 
                                &immutableSampler);

                size_t binding_index = desc_set->_vksamplers.size();
                desc_set->_vksamplers.push_back( immutableSampler );

                auto& vkb = desc_set->_vkbindings.emplace_back();
                initializeVkStruct( vkb );
                vkb.binding = binding_index; // TODO : query from shader
                vkb.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER; // Type of the descriptor (e.g., uniform buffer, combined image sampler, etc.)
                vkb.descriptorCount = 1; // Number of descriptors in this binding (useful for arrays of descriptors)
                vkb.stageFlags = stage_bits; // Shader stage to bind this descriptor to
                vkb.pImmutableSamplers = &immutableSampler; // Only relevant for samplers and combined image samplers                
              }
            }
          }

          for( auto item : dest_usetitemmap ){
            auto item_name = item.first;
            auto item_ptr  = item.second;
            auto datatype = item_ptr->_datatype;
            size_t cursor = 0xffffffff;
            if( datatype == "float"){
              cursor = dest_layout->layoutItem<float>();
            }
            else if( datatype == "int"){
              cursor = dest_layout->layoutItem<int>();
            }
            else if( datatype == "vec4"){
              cursor = dest_layout->layoutItem<fvec4>();
            }
            else if( datatype == "mat4"){
              cursor = dest_layout->layoutItem<fmtx4>();
            }
            else{
              printf( "unknown datatype<%s>\n", datatype.c_str() );
              OrkAssert(false);
            }
            printf( "datatype<%s> cursor<%zu>\n", datatype.c_str(), cursor );
          }
          initializeVkStruct( dest_range );

          size_t pc_size = dest_layout->cursor();

          dest_range.offset = push_constant_offset;
          dest_range.size = pc_size;
          dest_range.stageFlags = stage_bits;

          push_constant_offset += alignUp(pc_size,16);
        }
      };

      push_constants->_vtx_layout = std::make_shared<VkBufferLayout>();
      push_constants->_frg_layout = std::make_shared<VkBufferLayout>();
      push_constants->_ranges.reserve(8);
      auto& vtx_range = push_constants->_ranges.emplace_back();
      auto& frg_range = push_constants->_ranges.emplace_back();
      auto descriptors = std::make_shared<VkDescriptorSetBindings>();
      descriptors->_vksamplers.reserve( 32 );
      descriptors->_vkbindings.reserve( 32 );              
  
      do_unisets(vtx_obj,
                 push_constants->_vtx_unisets,
                 push_constants->_vtx_items_by_name,
                 push_constants->_vtx_layout,
                 vtx_range,
                 descriptors,
                 VK_SHADER_STAGE_VERTEX_BIT );
      do_unisets(frg_obj,
                 push_constants->_frg_unisets,
                 push_constants->_frg_items_by_name,
                 push_constants->_frg_layout,
                 frg_range,
                 descriptors,
                 VK_SHADER_STAGE_FRAGMENT_BIT );

      push_constants->_blockSize = push_constant_offset;

      ////////////////////////////////////////////////////////////

      // orkid side
      auto ork_pass = new FxShaderPass;
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

vkfxsfile_ptr_t VkFxInterface::_loadShaderFromShaderText(
    FxShader* shader,                //
    const std::string& parser_name,  //
    const std::string& shadertext) { //
  auto basehasher = DataBlock::createHasher();
  basehasher->accumulateString("vkfxshader-1.0");
  basehasher->accumulateString(shadertext);
  uint64_t hashkey    = basehasher->result();
  datablock_ptr_t vkfx_datablock = DataBlockCache::findDataBlock(hashkey);
  vkfxsfile_ptr_t vulkan_shaderfile;
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
    vkfx_datablock = _writeIntermediateToDataBlock(transunit);
    DataBlockCache::setDataBlock(hashkey, vkfx_datablock);
  } // shader binary not cached, compile and cache..
  ////////////////////////////////////////////////////////
  //vkfx_datablock->dump();
  vulkan_shaderfile = _readFromDataBlock(vkfx_datablock,shader);
  ////////////////////////////////////////////////////////
  return vulkan_shaderfile;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
