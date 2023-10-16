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

template <typename T>
void writeInterfaces( chunkfile::OutputStream* out_stream,
                      chunkfile::Writer& chunkwriter, 
                      const std::vector<std::shared_ptr<T>>& interfaces) {
  using namespace shadlang::SHAST;
  const auto& IO_DATASIZES = shadlang::spirv::SpirvCompilerGlobals::instance()->_io_data_sizes;
  for (auto IF : interfaces) {
    out_stream->addIndexedString("interface", chunkwriter);
    auto if_name = IF->template typedValueForKey<std::string>("object_name").value();
    out_stream->addIndexedString(if_name, chunkwriter);
    auto input_groups  = AstNode::collectNodesOfType<InterfaceInputs>(IF);
    auto output_groups = AstNode::collectNodesOfType<InterfaceOutputs>(IF);
    out_stream->addIndexedString("inputgroups", chunkwriter);
    out_stream->addItem<size_t>(input_groups.size());
    for (auto input_group : input_groups) {
      auto inputs = AstNode::collectNodesOfType<InterfaceInput>(input_group);
      out_stream->addItem<size_t>(inputs.size());
      for (auto input : inputs) {
        out_stream->addIndexedString("input", chunkwriter);
        auto tid = input->template childAs<TypedIdentifier>(0);
        OrkAssert(tid);
        auto dt = tid->template typedValueForKey<std::string>("data_type").value();
        auto id = tid->template typedValueForKey<std::string>("identifier_name").value();
        std::string semantic;
        if( auto try_sema = input->template typedValueForKey<std::string>("semantic")) {
          semantic = try_sema.value();
          OrkAssert(semantic.length());
        }
        out_stream->addIndexedString(dt, chunkwriter);
        out_stream->addIndexedString(id, chunkwriter);
        out_stream->addIndexedString(semantic, chunkwriter);
        auto it = IO_DATASIZES.find(dt);
        OrkAssert(it != IO_DATASIZES.end());
        out_stream->addItem<size_t>(it->second);
      }
    }
    /////////////////////////////////////////
    out_stream->addIndexedString("outputgroups", chunkwriter);
    out_stream->addItem<size_t>(output_groups.size());
    for (auto output_group : output_groups) {
      auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
      out_stream->addItem<size_t>(outputs.size());
      for (auto output : outputs) {
        out_stream->addIndexedString("output", chunkwriter);
        auto tid = output->template findFirstChildOfType<TypedIdentifier>();
        OrkAssert(tid);
        auto dt = tid->template typedValueForKey<std::string>("data_type").value();
        auto id = tid->template typedValueForKey<std::string>("identifier_name").value();
        out_stream->addIndexedString(dt, chunkwriter);
        out_stream->addIndexedString(id, chunkwriter);
        auto it = IO_DATASIZES.find(dt);
        OrkAssert(it != IO_DATASIZES.end());
        out_stream->addItem<size_t>(it->second);
      }
    }
  } // for (auto VIF : vtx_interfaces) {
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
void writeInterfaceInheritances( chunkfile::OutputStream* out_stream,
                                 chunkfile::Writer& chunkwriter, 
                                 shadlang::SHAST::transunit_ptr_t transunit,
                                 const std::vector<std::shared_ptr<T>>& interfaces) {

  for (auto TOP_IF : interfaces) {
    auto top_if_name = TOP_IF->template typedValueForKey<std::string>("object_name").value();
    out_stream->addIndexedString(top_if_name, chunkwriter);

    shadlang::SHAST::InheritanceTracker if_tracker(transunit);
    if_tracker.fetchInheritances(TOP_IF);
    size_t if_count = if_tracker._inherited_ifaces.size();
    out_stream->addItem<size_t>(if_count);
    if(if_count>0){
      auto last_inh = if_tracker._inherited_ifaces.back();
      auto name = last_inh->template typedValueForKey<std::string>("object_name").value();
     out_stream->addIndexedString(name, chunkwriter);     
    }
  }
}

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
  auto geo_shaders    = SHAST::AstNode::collectNodesOfType<SHAST::GeometryShader>(transunit);
  auto geo_interfaces = SHAST::AstNode::collectNodesOfType<SHAST::GeometryInterface>(transunit);
  auto frg_interfaces = SHAST::AstNode::collectNodesOfType<SHAST::FragmentInterface>(transunit);
  auto frg_shaders    = SHAST::AstNode::collectNodesOfType<SHAST::FragmentShader>(transunit);
  auto cu_shaders     = SHAST::AstNode::collectNodesOfType<SHAST::ComputeShader>(transunit);
  auto techniques     = SHAST::AstNode::collectNodesOfType<SHAST::Technique>(transunit);
  auto unisets        = SHAST::AstNode::collectNodesOfType<SHAST::UniformSet>(transunit);
  auto uniblks        = SHAST::AstNode::collectNodesOfType<SHAST::UniformBlk>(transunit);
  auto imports        = SHAST::AstNode::collectNodesOfType<SHAST::ImportDirective>(transunit);

  size_t num_vtx_shaders = vtx_shaders.size();
  size_t num_vtx_ifaces  = vtx_interfaces.size();
  size_t num_geo_shaders = geo_shaders.size();
  size_t num_geo_ifaces  = geo_interfaces.size();
  size_t num_frg_shaders = frg_shaders.size();
  size_t num_frg_ifaces  = frg_interfaces.size();
  size_t num_cu_shaders  = cu_shaders.size();
  size_t num_techniques  = techniques.size();
  size_t num_unisets     = unisets.size();
  size_t num_uniblks     = uniblks.size();
  size_t num_imports     = imports.size();

  printf("num_vtx_shaders<%zu>\n", num_vtx_shaders);
  printf("num_vtx_interfaces<%zu>\n", num_vtx_ifaces);
  printf("num_geo_shaders<%zu>\n", num_geo_shaders);
  printf("num_geo_interfaces<%zu>\n", num_geo_ifaces);
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
  header_stream->addItem<uint64_t>(num_geo_shaders);
  header_stream->addItem<uint64_t>(num_geo_ifaces);
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
  // write vtx interfaces / inheritances
  ////////////////////////////////////////////////////////////////
  interfaces_stream->addIndexedString("vertex-interfaces", chunkwriter);
  interfaces_stream->addItem<size_t>(vtx_interfaces.size());
  writeInterfaces(interfaces_stream, chunkwriter, vtx_interfaces);
  interfaces_stream->addIndexedString("vertex_interface_inheritances", chunkwriter);
  interfaces_stream->addItem<size_t>(vtx_interfaces.size());
  writeInterfaceInheritances(interfaces_stream, chunkwriter, transunit, vtx_interfaces);
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

  for (auto gshader : geo_shaders) {
    SPC->processShader(gshader);
    write_shader_to_stream(gshader, "geometry");
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
      std::string stages;
      stages += "V";
      ////////////////////////////////////////////////////////////////
      auto geo_shader_ref = p->findFirstChildOfType<SHAST::GeometryShaderRef>();
      if(geo_shader_ref){
        stages += "G";
      }
      stages += "F";
      ////////////////////////////////////////////////////////////////
      tecniq_stream->addIndexedString(stages, chunkwriter);
      ////////////////////////////////////////////////////////////////
      tecniq_stream->addIndexedString(vtx_name, chunkwriter);
      ////////////////////////////////////////////////////////////////
      if(geo_shader_ref){
        auto geo_sema_id = geo_shader_ref->findFirstChildOfType<SHAST::SemaIdentifier>();
        OrkAssert(geo_sema_id);
        auto geo_name = geo_sema_id->typedValueForKey<std::string>("identifier_name").value();
        tecniq_stream->addIndexedString(geo_name, chunkwriter);
      }
      ////////////////////////////////////////////////////////////////
      tecniq_stream->addIndexedString(frg_name, chunkwriter);
      ////////////////////////////////////////////////////////////////

    }
  } // for (auto tek : techniques) {

  auto out_datablock = std::make_shared<DataBlock>();
  chunkwriter.writeToDataBlock(out_datablock);
  return out_datablock;
}


///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::vulkan {
///////////////////////////////////////////////////////////////////////////////
