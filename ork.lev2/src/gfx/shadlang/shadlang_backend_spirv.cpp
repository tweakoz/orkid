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
    _id_renames["gl_InstanceID"] = "gl_InstanceIndex";
  }
  _collectLibBlocks();
  _processGlobalRenames();
  _collectUnisets();
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

  _inheritExtensions();
  _inheritLibraries(shader);
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
void SpirvCompiler::_collectUnisets() {
  auto unisets = SHAST::AstNode::collectNodesOfType<SHAST::UniformSet>(_transu);
  for (auto uni_set : unisets) {
    //////////////////////////////////////
    auto uni_name          = uni_set->typedValueForKey<std::string>("object_name").value();
    auto uniset            = std::make_shared<UniformSet>();
    _uniformsets[uni_name] = uniset;
    uniset->_name          = uni_name;
    auto decls             = SHAST::AstNode::collectNodesOfType<SHAST::DataDeclaration>(uni_set);
    //////////////////////////////////////
    for (auto d : decls) {
      auto tid = d->childAs<SHAST::TypedIdentifier>(0);
      OrkAssert(tid);
      auto dt         = tid->typedValueForKey<std::string>("data_type").value();
      auto id         = tid->typedValueForKey<std::string>("identifier_name").value();
      bool is_sampler = (dt.find("sampler") != std::string::npos);
      if (is_sampler) {
        auto samp                     = std::make_shared<UniformSetSampler>();
        samp->_datatype               = dt;
        samp->_identifier             = id;
        uniset->_samplers_by_name[id] = samp;
      } else {
        auto item                  = std::make_shared<UniformSetItem>();
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
void SpirvCompiler::_inheritLibraries(astnode_ptr_t par_node) {
  auto inh_libs = AstNode::collectNodesOfType<SemaInheritLibrary>(par_node);
  for (auto inh_lib : inh_libs) {
    auto INHID = inh_lib->typedValueForKey<std::string>("inherit_id").value();
    printf("Inherited Library<%s>\n", INHID.c_str());
    auto it_lib = _lib_blocks.find(INHID);
    OrkAssert(it_lib != _lib_blocks.end());
    auto lib_block    = it_lib->second;
    auto lib_children = lib_block->_children;
    // inline lib block into shader
    auto libgroup       = _libraries_group->appendTypedChild<MiscGroupNode>();
    libgroup->_children = lib_children;
    // AstNode::treeops::replaceInParent(oldnode,newnode);
    AstNode::treeops::removeFromParent(inh_lib);
    // OrkAssert(false);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
void SpirvCompiler::_inheritUniformSets(astnode_ptr_t par_node) {
  auto inh_semausets = AstNode::collectNodesOfType<SemaInheritUniformSet>(par_node);
  size_t binding_id  = 0;
  for (auto inh_uset : inh_semausets) {
    auto INHID   = inh_uset->typedValueForKey<std::string>("inherit_id").value();
    auto it_uset = _uniformsets.find(INHID);
    OrkAssert(it_uset != _uniformsets.end());
    auto vk_uniset               = it_uset->second;
    _uniformsets[it_uset->first] = vk_uniset;
    if (_vulkan) {
      vk_uniset->_descriptor_set_id = _descriptor_set_counter++;
      /////////////////////
      // loose unis
      /////////////////////
      auto line = FormatString(
          "layout(set=%zu, binding=%zu) uniform %s {", //
          vk_uniset->_descriptor_set_id,               //
          binding_id,                                  //
          INHID.c_str());
      _appendText(_uniforms_group, line.c_str());
      for (auto item : vk_uniset->_items_by_order) {
        auto dt = item->_datatype;
        auto id = item->_identifier;
        _appendText(_uniforms_group, (dt + " " + id + ";").c_str());
      }
      _appendText(_uniforms_group, "};");
      binding_id++;
      /////////////////////
      // samplers
      /////////////////////
      for (auto item : vk_uniset->_samplers_by_name) {
        auto dt   = item.second->_datatype;
        auto id   = item.second->_identifier;
        auto line = FormatString(
            "layout(set=%zu, binding=%zu) uniform %s %s;", //
            vk_uniset->_descriptor_set_id,                 //
            binding_id,                                    //
            dt.c_str(),                                    //
            id.c_str());
        _appendText(_uniforms_group, line.c_str());
        binding_id++;
      }
    } else { // opengl
      OrkAssert(false);
    }
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
void SpirvCompiler::_inheritExtensions() {
  auto inh_exts = AstNode::collectNodesOfType<SemaInheritExtension>(_shader);
  for (auto extension_node : inh_exts) {
    auto ext_name = extension_node->typedValueForKey<std::string>("extension_name").value();
    _appendText(_extension_group, "#extension %s : enable", ext_name.c_str());
  }
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
