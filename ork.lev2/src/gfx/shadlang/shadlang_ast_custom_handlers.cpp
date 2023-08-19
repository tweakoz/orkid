////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////
//  Scanner/Parser
//  this replaces CgFx for OpenGL 3.x and OpenGL ES 2.x
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include <ork/file/file.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <ork/util/crc.h>
#include <regex>
#include <stdlib.h>
#include <peglib.h>
#include <ork/util/logger.h>
#include <ork/kernel/string/string.h>
#include <ork/util/parser.inl>
#include "shadlang_impl.h"

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::shadlang::impl {
/////////////////////////////////////////////////////////////////////////////////////////////////

void ShadLangParser::defineAstHandlers() {
  ///////////////////////////////////////////////////////////
  onPost("TranslationUnit", [=](match_ptr_t match) {
    auto translation_unit = ast_create<SHAST::TranslationUnit>(match);
  });
  ///////////////////////////////////////////////////////////
  onPost("decl_arglist", [=](match_ptr_t match) {
    auto arg_list = ast_create<SHAST::DeclArgumentList>(match);
  });
  ///////////////////////////////////////////////////////////
  //onPost("exec_arg", [=](match_ptr_t match) { auto fn_arg = ast_create<SHAST::SemaFunctionArgument>(match); });
  ///////////////////////////////////////////////////////////
  onLink("Expression", [=](match_ptr_t match) { //
    auto expression = ast_get<SHAST::Expression>(match);
  });
  ///////////////////////////////////////////////////////////
  onPost("FunctionDef1", [=](match_ptr_t match) {
    auto seq     = match->asShared<Sequence>();
    auto fn_def  = ast_create<SHAST::FunctionDef1>(match);
    auto objname = ast_get<SHAST::ObjectName>(seq->_items[1]);
    seq->dump("fn_def");
    fn_def->_name = objname->_name;
  });
  ///////////////////////////////////////////////////////////
  onPost("pass_item", [=](match_ptr_t match) {
    auto seq = match->followImplAsShared<Sequence>();
    match->dump1(0);
    seq->dump("pass_item");
    auto pass_binding_key = seq->_items[0]->followImplAsShared<OneOf>()->_selected;
    auto pass_binding_val = seq->_items[2];
    auto pbk = pass_binding_key->asShared<ClassMatch>()->_token->text;
    auto pbv = pass_binding_val->asShared<ClassMatch>()->_token->text;
    if(pbk=="vertex_shader"){
      auto ast_node = ast_create<SHAST::VertexShaderRef>(match);
      ast_node->_name += "\n"+pbv;
      ast_node->setValueForKey<std::string>("ref_id",pbv);
    }
    else if(pbk=="fragment_shader"){
      auto ast_node = ast_create<SHAST::FragmentShaderRef>(match);
      ast_node->_name += "\n"+pbv;
      ast_node->setValueForKey<std::string>("ref_id",pbv);
    }
    else if(pbk=="geometry_shader"){
      auto ast_node = ast_create<SHAST::GeometryShaderRef>(match);
      ast_node->_name += "\n"+pbv;
      ast_node->setValueForKey<std::string>("ref_id",pbv);
    }
    else if(pbk=="state_block"){
      auto ast_node = ast_create<SHAST::StateBlockRef>(match);
      ast_node->_name += "\n"+pbv;
      ast_node->setValueForKey<std::string>("ref_id",pbv);
    }
    else{
      printf("pass_item pbk<%s> pbv<%s>\n",pbk.c_str(), pbv.c_str());
      pass_binding_key->dump1(0);
      OrkAssert(false);
    }
  });
  ///////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::shadlang::impl {
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif