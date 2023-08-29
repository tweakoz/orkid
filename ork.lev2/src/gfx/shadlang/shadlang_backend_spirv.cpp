////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include "shadlang_backend_spirv.h"

namespace ork::lev2::shadlang::spirv {
using namespace SHAST;
/////////////////////////////////////////////////////////////////////////////////////////////////
SpirvCompiler::SpirvCompiler(transunit_ptr_t transu, bool vulkan)
    : _transu(transu)
    , _vulkan(vulkan) {
  _data_sizes["int"]   = 1;
  _data_sizes["float"] = 1;
  _data_sizes["vec2"]  = 1;
  _data_sizes["vec3"]  = 1;
  _data_sizes["vec4"]  = 1;
  _data_sizes["mat3"]  = 4;
  _data_sizes["mat4"]  = 4;

  if (_vulkan) {
    _id_renames["ofx_instanceID"] = "gl_InstanceIndex";
  }
  else{
    _id_renames["ofx_depth"] = "gl_FragDepth";
    _id_renames["ofx_instanceID"] = "gl_InstanceID";
    
  }
  _collectLibBlocks();
  _processGlobalRenames();
  _convertUniformSets();
  _convertUniformBlocks();
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_beginShader(shader_ptr_t shader) {
  _shader          = shader;
  _shader_group    = std::make_shared<MiscGroupNode>();
  _interface_group = std::make_shared<MiscGroupNode>();
  _extension_group = std::make_shared<MiscGroupNode>();
  _uniforms_group  = std::make_shared<MiscGroupNode>();
  _libraries_group = std::make_shared<MiscGroupNode>();
  _input_index     = 0;
  _output_index    = 0;

  _procInheritances(shader);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::processShader(shader_ptr_t sh){
  _beginShader(sh);
  if( auto as_vsh = std::dynamic_pointer_cast<VertexShader>(sh) ){
    _compileShader(shaderc_glsl_vertex_shader);
  }
  else if( auto as_gsh = std::dynamic_pointer_cast<GeometryShader>(sh) ){
    _compileShader(shaderc_glsl_geometry_shader);
  }
  else if( auto as_fsh = std::dynamic_pointer_cast<FragmentShader>(sh) ){
    _compileShader(shaderc_glsl_fragment_shader);
  }
  else if( auto as_csh = std::dynamic_pointer_cast<ComputeShader>(sh) ){
    _compileShader(shaderc_glsl_compute_shader);
  }
  else {
    OrkAssert(false);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_appendText(miscgroupnode_ptr_t grp, const char* formatstring, ...) {
  char formatbuffer[512];
  va_list args;
  va_start(args, formatstring);
  vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
  va_end(args);
  grp->appendTypedChild<InsertLine>(formatbuffer);
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_processGlobalRenames() {
  auto sema_identifiers = SHAST::AstNode::collectNodesOfType<SHAST::SemaIdentifier>(_transu);
  for (auto it : sema_identifiers) {
    auto id     = it->typedValueForKey<std::string>("identifier_name").value();
    auto it_ren = _id_renames.find(id);
    if (it_ren != _id_renames.end()) {
      auto newid = it_ren->second;
      it->setValueForKey<std::string>("identifier_name", newid);
    }
  }
  auto prim_identifiers = SHAST::AstNode::collectNodesOfType<SHAST::PrimaryIdentifier>(_transu);
  for (auto it : prim_identifiers) {
    auto id     = it->typedValueForKey<std::string>("identifier_name").value();
    auto it_ren = _id_renames.find(id);
    if (it_ren != _id_renames.end()) {
      auto newid = it_ren->second;
      it->setValueForKey<std::string>("identifier_name", newid);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_convertUniformSets() {
  auto ast_unisets = SHAST::AstNode::collectNodesOfType<SHAST::UniformSet>(_transu);
  for (auto ast_uniset : ast_unisets) {
    auto decls             = SHAST::AstNode::collectNodesOfType<SHAST::DataDeclaration>(ast_uniset);
    //////////////////////////////////////
    auto uni_name          = ast_uniset->typedValueForKey<std::string>("object_name").value();
    auto uniset            = std::make_shared<SpirvUniformSet>();
    _spirvuniformsets[uni_name] = uniset;
    uniset->_name          = uni_name;
    //////////////////////////////////////
    for (auto d : decls) {
      auto tid = d->childAs<SHAST::TypedIdentifier>(0);
      OrkAssert(tid);
      auto dt         = tid->typedValueForKey<std::string>("data_type").value();
      auto id         = tid->typedValueForKey<std::string>("identifier_name").value();
      bool is_sampler = (dt.find("sampler") != std::string::npos);
      if (is_sampler) {
        auto samp                     = std::make_shared<SpirvUniformSetSampler>();
        samp->_datatype               = dt;
        samp->_identifier             = id;
        uniset->_samplers_by_name[id] = samp;
      } else {
        auto item                  = std::make_shared<SpirvUniformSetItem>();
        item->_datatype            = dt;
        item->_identifier          = id;
        uniset->_items_by_name[id] = item;
        uniset->_items_by_order.push_back(item);
      }
    }
    //////////////////////////////////////
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_convertUniformBlocks() {
  auto ast_uniblks = SHAST::AstNode::collectNodesOfType<SHAST::UniformBlk>(_transu);
  for (auto ast_uniblk : ast_uniblks) {
    auto decls             = SHAST::AstNode::collectNodesOfType<SHAST::DataDeclaration>(ast_uniblk);
    //////////////////////////////////////
    auto uni_name          = ast_uniblk->typedValueForKey<std::string>("object_name").value();
    auto uniblk            = std::make_shared<SpirvUniformBlock>();
    _spirvuniformblks[uni_name] = uniblk;
    uniblk->_name          = uni_name;
    //////////////////////////////////////
    for (auto d : decls) {
      auto tid = d->childAs<SHAST::TypedIdentifier>(0);
      OrkAssert(tid);
      auto dt         = tid->typedValueForKey<std::string>("data_type").value();
      auto id         = tid->typedValueForKey<std::string>("identifier_name").value();
      auto item                  = std::make_shared<SpirvUniformBlockItem>();
      item->_datatype            = dt;
      item->_identifier          = id;
      uniblk->_items_by_name[id] = item;
      uniblk->_items_by_order.push_back(item);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_collectLibBlocks() {
  auto lib_blocks = AstNode::collectNodesOfType<LibraryBlock>(_transu);
  for (auto lib_block : lib_blocks) {
    auto name = lib_block->typedValueForKey<std::string>("object_name").value();
    auto it   = _lib_blocks.find(name);
    if (it == _lib_blocks.end()) {
      _lib_blocks[name] = lib_block;
    } else {
      printf("dupe libblock<%s>\n", name.c_str());
      OrkAssert(false);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_procInheritances(astnode_ptr_t parent_node) {
  _binding_id = 0;
  for (auto c : parent_node->_children) {
    //////////////////////////////////////////////////////////////////////
    if (auto as_lib = std::dynamic_pointer_cast<SemaInheritLibrary>(c)) {
      auto INHID = as_lib->typedValueForKey<std::string>("inherit_id").value();
      auto LIB = _transu->find<LibraryBlock>(INHID);
      _procInheritances(LIB);
      _inheritLibrary(LIB);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_vif = std::dynamic_pointer_cast<SemaInheritVertexInterface>(c)) {
      auto INHID = as_vif->typedValueForKey<std::string>("inherit_id").value();
      auto IFACE = _transu->find<VertexInterface>(INHID);
      _procInheritances(IFACE);
      _inheritIO(IFACE);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_fif = std::dynamic_pointer_cast<SemaInheritFragmentInterface>(c)) {
      auto INHID = as_fif->typedValueForKey<std::string>("inherit_id").value();
      auto IFACE = _transu->find<FragmentInterface>(INHID);
      _procInheritances(IFACE);
      _inheritIO(IFACE);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_gif = std::dynamic_pointer_cast<SemaInheritGeometryInterface>(c)) {
      auto INHID = as_gif->typedValueForKey<std::string>("inherit_id").value();
      auto IFACE = _transu->find<GeometryInterface>(INHID);
      _procInheritances(IFACE);
      _inheritIO(IFACE);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_cif = std::dynamic_pointer_cast<SemaInheritComputeInterface>(c)) {
      auto INHID = as_cif->typedValueForKey<std::string>("inherit_id").value();
      auto IFACE = _transu->find<ComputeInterface>(INHID);
      _procInheritances(IFACE);
      _inheritIO(IFACE);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_uset = std::dynamic_pointer_cast<SemaInheritUniformSet>(c)) {
      auto INHID = as_uset->typedValueForKey<std::string>("inherit_id").value();
      auto ast_uset = _transu->find<SHAST::UniformSet>(INHID);
      auto it_uset = _spirvuniformsets.find(INHID);
      OrkAssert(it_uset != _spirvuniformsets.end());
      auto spirvuniset = it_uset->second;
      _procInheritances(ast_uset);
      _inheritUniformSet(INHID,spirvuniset);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_ublk = std::dynamic_pointer_cast<SemaInheritUniformBlock>(c)) {
      auto INHID = as_ublk->typedValueForKey<std::string>("inherit_id").value();
      auto ast_ublk = _transu->find<SHAST::UniformBlk>(INHID);
      auto it_ublk = _spirvuniformblks.find(INHID);
      OrkAssert(it_ublk != _spirvuniformblks.end());
      auto spirvuniblk = it_ublk->second;
      _procInheritances(ast_ublk);
      _inheritUniformBlk(INHID,spirvuniblk);
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_sb = std::dynamic_pointer_cast<SemaInheritStateBlock>(c)) {
    }
    //////////////////////////////////////////////////////////////////////
    else if (auto as_ext = std::dynamic_pointer_cast<SemaInheritExtension>(c)) {
      _inheritExtension(as_ext);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritLibrary(libblock_ptr_t lib_block) {
    auto lib_children = lib_block->_children;
    auto libgroup       = _libraries_group->appendTypedChild<MiscGroupNode>();
    libgroup->_children = lib_children;  
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritUniformSet(std::string unisetname, //
                                       spirvuniset_ptr_t spirvuset) { //
  if (_vulkan) {
    spirvuset->_descriptor_set_id = _descriptor_set_counter++;
    /////////////////////
    // loose unis
    /////////////////////
    auto line = FormatString(
        "layout(set=%zu, binding=%zu) uniform %s {", //
        spirvuset->_descriptor_set_id,               //
        _binding_id,                                  //
        unisetname.c_str());
    _appendText(_uniforms_group, line.c_str());
    for (auto item : spirvuset->_items_by_order) {
      auto dt = item->_datatype;
      auto id = item->_identifier;
      _appendText(_uniforms_group, (dt + " " + id + ";").c_str());
    }
    _appendText(_uniforms_group, "};");
    _binding_id++;
    /////////////////////
    // samplers
    /////////////////////
    for (auto item : spirvuset->_samplers_by_name) {
      auto dt   = item.second->_datatype;
      auto id   = item.second->_identifier;
      auto line = FormatString(
          "layout(set=%zu, binding=%zu) uniform %s %s;", //
          spirvuset->_descriptor_set_id,                 //
          _binding_id,                                    //
          dt.c_str(),                                    //
          id.c_str());
      _appendText(_uniforms_group, line.c_str());
      _binding_id++;
    }
  } else { // opengl
    OrkAssert(false);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritUniformBlk(std::string uniblkname, //
                                       spirvuniblk_ptr_t spirvublk) { //
  if (_vulkan) {
    spirvublk->_descriptor_set_id = _descriptor_set_counter++;
    /////////////////////
    // loose unis
    /////////////////////
    auto line = FormatString(
        "layout(set=%zu, binding=%zu) uniform %s {", //
        spirvublk->_descriptor_set_id,               //
        _binding_id,                                  //
        uniblkname.c_str());
    _appendText(_uniforms_group, line.c_str());
    for (auto item : spirvublk->_items_by_order) {
      auto dt = item->_datatype;
      auto id = item->_identifier;
      _appendText(_uniforms_group, (dt + " " + id + ";").c_str());
    }
    _appendText(_uniforms_group, "};");
    _binding_id++;
  } else { // opengl
    OrkAssert(false);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritIO(astnode_ptr_t interface_node) {
  //
  // TODO inherited interfaces
  //
  auto input_groups  = AstNode::collectNodesOfType<InterfaceInputs>(interface_node);
  auto output_groups = AstNode::collectNodesOfType<InterfaceOutputs>(interface_node);
  // printf("  num_input_groups<%zu>\n", input_groups.size());
  // printf("  num_output_groups<%zu>\n", output_groups.size());
  /////////////////////////////////////////
  for (auto input_group : input_groups) {
    auto inputs = AstNode::collectNodesOfType<InterfaceInput>(input_group);
    // printf("  num_inputs<%zu>\n", inputs.size());
    for (auto input : inputs) {
      auto tid = input->childAs<TypedIdentifier>(0);
      OrkAssert(tid);
      // dumpAstNode(tid);
      auto dt = tid->typedValueForKey<std::string>("data_type").value();
      auto id = tid->typedValueForKey<std::string>("identifier_name").value();
      _appendText(_interface_group, "layout(location=%zu) in %s %s;", _input_index, dt.c_str(), id.c_str());
      auto it = _data_sizes.find(dt);
      OrkAssert(it != _data_sizes.end());
      _input_index += it->second;
    }
  }
  /////////////////////////////////////////
  for (auto output_group : output_groups) {
    auto outputs = AstNode::collectNodesOfType<InterfaceOutput>(output_group);
    // printf("  num_outputs<%zu>\n", outputs.size());
    for (auto output : outputs) {
      // dumpAstNode(output);
      auto tid = output->findFirstChildOfType<TypedIdentifier>();
      OrkAssert(tid);
      auto dt = tid->typedValueForKey<std::string>("data_type").value();
      auto id = tid->typedValueForKey<std::string>("identifier_name").value();
      if (id.find("gl_") != 0) {
        _appendText(_interface_group, "layout(location=%zu) out %s %s;", _output_index, dt.c_str(), id.c_str());
        auto it = _data_sizes.find(dt);
        OrkAssert(it != _data_sizes.end());
        _output_index += it->second;
      }
    }
  }
  /////////////////////////////////////////
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritExtension(semainhext_ptr_t extension_node) {
  auto ext_name = extension_node->typedValueForKey<std::string>("extension_name").value();
  _appendText(_extension_group, "#extension %s : enable", ext_name.c_str());
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_compileShader(shaderc_shader_kind shader_type) {

  ///////////////////////////////////////////////////////
  // final prep for shaderc
  // build final ast
  ///////////////////////////////////////////////////////

  _shader_group->appendTypedChild<InsertLine>("#version 450");
  _shader_group->appendChild(_extension_group);
  _shader_group->appendChild(_interface_group);
  _shader_group->appendChild(_uniforms_group);
  _shader_group->appendChild(_libraries_group);
  _shader_group->appendTypedChild<InsertLine>("void main()");
  _shader_group->appendChildrenFrom(_shader); // compound statement

  ///////////////////////////////////////////////////////
  // emit
  ///////////////////////////////////////////////////////

  auto as_glsl = shadlang::toGLFX1(_shader_group);
  _shader_name = _shader->typedValueForKey<std::string>("object_name").value();
  printf("// shader<%s>:\n%s\n", _shader_name.c_str(), as_glsl.c_str());

  ///////////////////////////////////////////////////////
  // compile with shaderc
  ///////////////////////////////////////////////////////

  shaderc::Compiler compiler;
  shaderc::CompileOptions options;
  shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv( //
      as_glsl.c_str(),                                              // glsl source (string)
      as_glsl.length(),                                             // glsl source length
      shader_type,                                                  // shader type
      "main",                                                       // entry point name
      options);

  if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    std::cerr << result.GetErrorMessage();
    OrkAssert(false);
  }

  _spirv_binary = shader_bin_t(result.cbegin(), result.cend());
}
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang::spirv
