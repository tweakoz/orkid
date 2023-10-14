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

datablock_ptr_t VkFxInterface::_writeIntermediateToDataBlock(shadlang::SHAST::transunit_ptr_t transunit) {
  chunkfile::Writer chunkwriter("xfx");
  auto header_stream   = chunkwriter.AddStream("header");
  auto shader_stream   = chunkwriter.AddStream("shaders");
  auto uniforms_stream = chunkwriter.AddStream("uniforms");
  auto interfaces_stream = chunkwriter.AddStream("interfaces");
  auto tecniq_stream   = chunkwriter.AddStream("techniques");

  /////////////////////////////////////////////////////////////////////////////
  // compile all shaders from translation unit
  /////////////////////////////////////////////////////////////////////////////

  // TODO - before hoisting cache, implement namespaces..

  auto vtx_shaders    = SHAST::AstNode::collectNodesOfType<SHAST::VertexShader>(transunit);
  auto vtx_interfaces = SHAST::AstNode::collectNodesOfType<SHAST::VertexInterface>(transunit);
  auto frg_interfaces = SHAST::AstNode::collectNodesOfType<SHAST::FragmentInterface>(transunit);
  auto frg_shaders    = SHAST::AstNode::collectNodesOfType<SHAST::FragmentShader>(transunit);
  auto cu_shaders     = SHAST::AstNode::collectNodesOfType<SHAST::ComputeShader>(transunit);
  auto techniques     = SHAST::AstNode::collectNodesOfType<SHAST::Technique>(transunit);
  auto unisets        = SHAST::AstNode::collectNodesOfType<SHAST::UniformSet>(transunit);
  auto uniblks        = SHAST::AstNode::collectNodesOfType<SHAST::UniformBlk>(transunit);
  auto imports        = SHAST::AstNode::collectNodesOfType<SHAST::ImportDirective>(transunit);

  size_t num_vtx_shaders = vtx_shaders.size();
  size_t num_vtx_ifaces  = vtx_interfaces.size();
  size_t num_frg_shaders = frg_shaders.size();
  size_t num_frg_ifaces  = frg_interfaces.size();
  size_t num_cu_shaders  = cu_shaders.size();
  size_t num_techniques  = techniques.size();
  size_t num_unisets     = unisets.size();
  size_t num_uniblks     = uniblks.size();
  size_t num_imports     = imports.size();

  printf("num_vtx_shaders<%zu>\n", num_vtx_shaders);
  printf("num_vtx_interfaces<%zu>\n", num_vtx_ifaces);
  printf("num_frg_shaders<%zu>\n", num_frg_shaders);
  printf("num_frg_interfaces<%zu>\n", num_frg_ifaces);
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
  header_stream->addItem<uint64_t>(num_vtx_ifaces);
  header_stream->addItem<uint64_t>(num_frg_shaders);
  header_stream->addItem<uint64_t>(num_frg_ifaces);
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
        uniforms_stream->addItem<size_t>(item->_offset);
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
        uniforms_stream->addItem<size_t>(item->_offset);
      }
    }
  };

  write_unisets_to_stream(SPC->_spirvuniformsets);
  write_uniblks_to_stream(SPC->_spirvuniformblks);

  ////////////////////////////////////////////////////////////////
  using namespace shadlang::SHAST;
  const auto& IO_DATASIZES = shadlang::spirv::SpirvCompilerGlobals::instance()->_io_data_sizes;
  ////////////////////////////////////////////////////////////////
  // write vtx interfaces
  ////////////////////////////////////////////////////////////////
  interfaces_stream->addIndexedString("vertex-interfaces", chunkwriter);
  interfaces_stream->addItem<size_t>(vtx_interfaces.size());
  for (auto VIF : vtx_interfaces) {
    interfaces_stream->addIndexedString("interface", chunkwriter);
    auto vif_name = VIF->typedValueForKey<std::string>("object_name").value();
    interfaces_stream->addIndexedString(vif_name, chunkwriter);
    auto input_groups  = AstNode::collectNodesOfType<InterfaceInputs>(VIF);
    auto output_groups = AstNode::collectNodesOfType<InterfaceOutputs>(VIF);
    interfaces_stream->addIndexedString("inputgroups", chunkwriter);
    interfaces_stream->addItem<size_t>(input_groups.size());
    for (auto input_group : input_groups) {
      auto inputs = AstNode::collectNodesOfType<InterfaceInput>(input_group);
      interfaces_stream->addItem<size_t>(inputs.size());
      for (auto input : inputs) {
        interfaces_stream->addIndexedString("input", chunkwriter);
        auto tid = input->childAs<TypedIdentifier>(0);
        OrkAssert(tid);
        auto dt = tid->typedValueForKey<std::string>("data_type").value();
        auto id = tid->typedValueForKey<std::string>("identifier_name").value();
        std::string semantic;
        if( auto try_sema = input->typedValueForKey<std::string>("semantic")) {
          semantic = try_sema.value();
          OrkAssert(semantic.length());
        }
        interfaces_stream->addIndexedString(dt, chunkwriter);
        interfaces_stream->addIndexedString(id, chunkwriter);
        interfaces_stream->addIndexedString(semantic, chunkwriter);
        auto it = IO_DATASIZES.find(dt);
        OrkAssert(it != IO_DATASIZES.end());
        interfaces_stream->addItem<size_t>(it->second);
      }
    }
    /////////////////////////////////////////
    interfaces_stream->addIndexedString("outputgroups", chunkwriter);
    interfaces_stream->addItem<size_t>(output_groups.size());
    for (auto output_group : output_groups) {
      auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
      interfaces_stream->addItem<size_t>(outputs.size());
      for (auto output : outputs) {
        interfaces_stream->addIndexedString("output", chunkwriter);
        auto tid = output->findFirstChildOfType<TypedIdentifier>();
        OrkAssert(tid);
        auto dt = tid->typedValueForKey<std::string>("data_type").value();
        auto id = tid->typedValueForKey<std::string>("identifier_name").value();
        interfaces_stream->addIndexedString(dt, chunkwriter);
        interfaces_stream->addIndexedString(id, chunkwriter);
        auto it = IO_DATASIZES.find(dt);
        OrkAssert(it != IO_DATASIZES.end());
        interfaces_stream->addItem<size_t>(it->second);
      }
    }
  } // for (auto VIF : vtx_interfaces) {
  ////////////////////////////////////////////////////////////////
  // write vtx interface inheritances
  ////////////////////////////////////////////////////////////////
  interfaces_stream->addIndexedString("vertex_interface_inheritances", chunkwriter);
  interfaces_stream->addItem<size_t>(vtx_interfaces.size());
  for (auto TOP_VIF : vtx_interfaces) {
    auto top_vif_name = TOP_VIF->typedValueForKey<std::string>("object_name").value();
    interfaces_stream->addIndexedString(top_vif_name, chunkwriter);

    InheritanceTracker if_tracker(transunit);
    if_tracker.fetchInheritances(TOP_VIF);
    size_t if_count = if_tracker._inherited_ifaces.size();
    interfaces_stream->addItem<size_t>(if_count);
    if(if_count>0){
      auto last_inh = if_tracker._inherited_ifaces.back();
      auto name = last_inh->typedValueForKey<std::string>("object_name").value();
     interfaces_stream->addIndexedString(name, chunkwriter);     
    }
  }
  interfaces_stream->addIndexedString("interfaces-done", chunkwriter);
  ////////////////////////////////////////////////////////////////

  size_t num_shaders_written = 0;

  auto write_shader_to_stream = [&](SHAST::astnode_ptr_t shader_node, //
                                    std::string shader_type) { //
    auto sh_name  = shader_node->typedValueForKey<std::string>("object_name").value();
    auto sh_data  = (uint8_t*)SPC->_spirv_binary.data();
    size_t sh_len = SPC->_spirv_binary.size() * sizeof(uint32_t);
    shader_stream->addIndexedString("shader", chunkwriter);
    shader_stream->addIndexedString(shader_type, chunkwriter);
    shader_stream->addIndexedString(sh_name, chunkwriter);
    shader_stream->addItem<size_t>(sh_len);
    shader_stream->addData(sh_data, sh_len);

    shadlang::spirv::InheritanceTracker tracker(transunit);
    tracker.fetchInheritances(shader_node);

    //////////////////////////////////////////////////////////////////
    shader_stream->addItem<size_t>(tracker._inherited_usets.size());
    for (auto uset : tracker._inherited_usets) {
      auto INHID = uset->typedValueForKey<std::string>("object_name").value();
      shader_stream->addIndexedString(INHID, chunkwriter);
      printf("INHID<%s>\n", INHID.c_str());
    }
    //////////////////////////////////////////////////////////////////
    shader_stream->addItem<size_t>(tracker._inherited_ifaces.size());
    for (auto uset : tracker._inherited_ifaces) {
      auto INHID = uset->typedValueForKey<std::string>("object_name").value();
      shader_stream->addIndexedString(INHID, chunkwriter);
      printf("INHID<%s>\n", INHID.c_str());
    }
    //////////////////////////////////////////////////////////////////

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
  // read interfaces
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
  ////////////////////////////////////////////////////////
  return vulkan_shaderfile;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::vulkan
///////////////////////////////////////////////////////////////////////////////
