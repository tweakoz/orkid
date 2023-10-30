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

  out_stream->addItem<size_t>(interfaces.size());
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
        dumpAstNode(input);
        if( auto tid = input->template childAs<TypedIdentifier>(0) ){
          out_stream->addIndexedString("input", chunkwriter);
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
        else if( auto layout = input->template childAs<InterfaceLayout>(0) ){
          out_stream->addIndexedString("layout", chunkwriter);
        }
        else{
          OrkAssert(false);
        }
      }
    }
    /////////////////////////////////////////
    out_stream->addIndexedString("outputgroups", chunkwriter);
    out_stream->addItem<size_t>(output_groups.size());
    for (auto output_group : output_groups) {
      auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
      out_stream->addItem<size_t>(outputs.size());
      for (auto output : outputs) {
        dumpAstNode(output);
        if( auto as_tid = output->template childAs<TypedIdentifier>(0) ){
          out_stream->addIndexedString("output", chunkwriter);
          auto dt = as_tid->template typedValueForKey<std::string>("data_type").value();
          auto id = as_tid->template typedValueForKey<std::string>("identifier_name").value();
          out_stream->addIndexedString(dt, chunkwriter);
          out_stream->addIndexedString(id, chunkwriter);
          auto it = IO_DATASIZES.find(dt);
          OrkAssert(it != IO_DATASIZES.end());
          out_stream->addItem<size_t>(it->second);
        }
        else if( auto layout = output->template childAs<InterfaceLayout>(0) ){
          out_stream->addIndexedString("layout", chunkwriter);
        }
        else{
          OrkAssert(false);
        }
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

  out_stream->addIndexedString("interface_inheritances", chunkwriter);
  out_stream->addItem<size_t>(interfaces.size());
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

  using namespace shadlang::SHAST;

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

  auto vtx_shaders    = AstNode::collectNodesOfType<VertexShader>(transunit);
  auto vtx_interfaces = AstNode::collectNodesOfType<VertexInterface>(transunit);
  auto geo_shaders    = AstNode::collectNodesOfType<GeometryShader>(transunit);
  auto geo_interfaces = AstNode::collectNodesOfType<GeometryInterface>(transunit);
  auto frg_interfaces = AstNode::collectNodesOfType<FragmentInterface>(transunit);
  auto frg_shaders    = AstNode::collectNodesOfType<FragmentShader>(transunit);
  auto cu_shaders     = AstNode::collectNodesOfType<ComputeShader>(transunit);
  auto techniques     = AstNode::collectNodesOfType<Technique>(transunit);
  auto smpsets        = AstNode::collectNodesOfType<SamplerSet>(transunit);
  auto unisets        = AstNode::collectNodesOfType<UniformSet>(transunit);
  auto uniblks        = AstNode::collectNodesOfType<UniformBlk>(transunit);
  auto imports        = AstNode::collectNodesOfType<ImportDirective>(transunit);

  size_t num_vtx_shaders = vtx_shaders.size();
  size_t num_vtx_ifaces  = vtx_interfaces.size();
  size_t num_geo_shaders = geo_shaders.size();
  size_t num_geo_ifaces  = geo_interfaces.size();
  size_t num_frg_shaders = frg_shaders.size();
  size_t num_frg_ifaces  = frg_interfaces.size();
  size_t num_cu_shaders  = cu_shaders.size();
  size_t num_techniques  = techniques.size();
  size_t num_smpsets     = smpsets.size();
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
  printf("num_smpsets<%zu>\n", num_smpsets);
  printf("num_unisets<%zu>\n", num_unisets);
  printf("num_uniblks<%zu>\n", num_uniblks);
  printf("num_imports<%zu>\n", num_imports);
  //////////////////
  auto SPC = std::make_shared<spirv::SpirvCompiler>(transunit, true);
  for (auto spirvsmpset : SPC->_spirvsamplersets) {
    printf("spirvsmpset<%s>\n", spirvsmpset.first.c_str());
  }
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
  header_stream->addItem<uint64_t>(num_smpsets);
  header_stream->addItem<uint64_t>(num_unisets);
  header_stream->addItem<uint64_t>(num_uniblks);
  header_stream->addItem<uint64_t>(num_techniques);
  header_stream->addItem<uint64_t>(num_imports);

  //////////////////
  // samplersets
  //////////////////

  auto write_smpsets_to_stream = [&](std::unordered_map<std::string, spirv::spirvsmpset_ptr_t>& spirv_smpsets) { //
    uniforms_stream->addIndexedString("smpsets", chunkwriter);
    uniforms_stream->addItem<size_t>(spirv_smpsets.size());

    for (auto smp_item : spirv_smpsets) {
      std::string name  = smp_item.first;
      auto spirv_smpset = smp_item.second;
      /////////////////////////////////////////////
      // rebuild _samplers_by_name
      /////////////////////////////////////////////
      uniforms_stream->addIndexedString("smpset", chunkwriter);
      printf( "WRITE SAMPLERSET<%s>\n", name.c_str() );
      uniforms_stream->addIndexedString(name, chunkwriter);
      uniforms_stream->addItem<size_t>(spirv_smpset->_descriptor_set_id);
      uniforms_stream->addIndexedString("samplers", chunkwriter);
      uniforms_stream->addItem<size_t>(spirv_smpset->_samplers_by_name.size());
      for (auto item : spirv_smpset->_samplers_by_name) {
        uniforms_stream->addIndexedString(item.second->_datatype, chunkwriter);
        uniforms_stream->addIndexedString(item.second->_identifier, chunkwriter);
      }
    }
  };
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
      // rebuild _samplers_by_name
      /////////////////////////////////////////////
      uniforms_stream->addIndexedString("uniset", chunkwriter);
      uniforms_stream->addIndexedString(name, chunkwriter);
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
      uniforms_stream->addItem<size_t>(spirv_uniblk->_descriptor_set_id);
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

  write_smpsets_to_stream(SPC->_spirvsamplersets);
  write_unisets_to_stream(SPC->_spirvuniformsets);
  write_uniblks_to_stream(SPC->_spirvuniformblks);

  ////////////////////////////////////////////////////////////////
  ////////////////////////////////////////////////////////////////
  // write vtx interfaces / inheritances
  ////////////////////////////////////////////////////////////////
  interfaces_stream->addIndexedString("vertex-interfaces", chunkwriter);
  writeInterfaces(interfaces_stream, chunkwriter, vtx_interfaces);
  writeInterfaceInheritances(interfaces_stream, chunkwriter, transunit, vtx_interfaces);
  interfaces_stream->addIndexedString("vertex-interfaces-done", chunkwriter);
  ////////////////////////////////////////////////////////////////
  // write geo interfaces / inheritances
  ////////////////////////////////////////////////////////////////
  interfaces_stream->addIndexedString("geometry-interfaces", chunkwriter);
  writeInterfaces(interfaces_stream, chunkwriter, geo_interfaces);
  writeInterfaceInheritances(interfaces_stream, chunkwriter, transunit, geo_interfaces);
  interfaces_stream->addIndexedString("geometry-interfaces-done", chunkwriter);
  ////////////////////////////////////////////////////////////////

  size_t num_shaders_written = 0;

  auto write_shader_to_stream = [&](astnode_ptr_t shader_node, //
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
    shader_stream->addItem<size_t>(tracker._inherited_ssets.size());
    for (auto uset : tracker._inherited_ssets) {
      auto INHID = uset->typedValueForKey<std::string>("object_name").value();
      shader_stream->addIndexedString(INHID, chunkwriter);
      printf("WRITE SAMPLERSET REF<%s>\n", INHID.c_str());
    }
    //////////////////////////////////////////////////////////////////
    shader_stream->addItem<size_t>(tracker._inherited_usets.size());
    for (auto uset : tracker._inherited_usets) {
      auto INHID = uset->typedValueForKey<std::string>("object_name").value();
      shader_stream->addIndexedString(INHID, chunkwriter);
      printf("WRITE UNIFORMSET REF<%s>\n", INHID.c_str());
    }
    //////////////////////////////////////////////////////////////////
    shader_stream->addItem<size_t>(tracker._inherited_ublks.size());
    for (auto ublk : tracker._inherited_ublks) {
      auto INHID = ublk->typedValueForKey<std::string>("object_name").value();
      shader_stream->addIndexedString(INHID, chunkwriter);
      printf("WRITE UNIFORMBLOCK REF<%s>\n", INHID.c_str());
    }
    //////////////////////////////////////////////////////////////////
    shader_stream->addItem<size_t>(tracker._inherited_ifaces.size());
    for (auto uset : tracker._inherited_ifaces) {
      auto INHID = uset->typedValueForKey<std::string>("object_name").value();
      shader_stream->addIndexedString(INHID, chunkwriter);
      printf("WRITE IFACE REF<%s>\n", INHID.c_str());
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
    auto passes   = AstNode::collectNodesOfType<Pass>(tek);
    auto tek_name = tek->typedValueForKey<std::string>("object_name").value();
    tecniq_stream->addIndexedString("technique", chunkwriter);
    tecniq_stream->addIndexedString(tek_name, chunkwriter);
    tecniq_stream->addItem<size_t>(passes.size());
    for (auto p : passes) {
      auto vtx_shader_ref = p->findFirstChildOfType<VertexShaderRef>();
      auto frg_shader_ref = p->findFirstChildOfType<FragmentShaderRef>();
      auto stateblock_ref = p->findFirstChildOfType<StateBlockRef>();
      OrkAssert(vtx_shader_ref);
      OrkAssert(frg_shader_ref);
      auto vtx_sema_id = vtx_shader_ref->findFirstChildOfType<SemaIdentifier>();
      auto frg_sema_id = frg_shader_ref->findFirstChildOfType<SemaIdentifier>();
      OrkAssert(vtx_sema_id);
      OrkAssert(frg_sema_id);
      auto vtx_name = vtx_sema_id->typedValueForKey<std::string>("identifier_name").value();
      auto frg_name = frg_sema_id->typedValueForKey<std::string>("identifier_name").value();
      tecniq_stream->addIndexedString("pass", chunkwriter);
      std::string stages;
      stages += "V";
      ////////////////////////////////////////////////////////////////
      auto geo_shader_ref = p->findFirstChildOfType<GeometryShaderRef>();
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
        auto geo_sema_id = geo_shader_ref->findFirstChildOfType<SemaIdentifier>();
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
