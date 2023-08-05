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
  onPost("FLOATING_POINT", [=](match_ptr_t match) { //
    auto ast_node = ast_create<SHAST::FloatLiteral>(match);
    auto impl     = match->asShared<ClassMatch>();
    // ast_node->_value  = std::stof(impl->_token->text);
    // ast_node->_strval = impl->_token->text;
  });
  ///////////////////////////////////////////////////////////
  onPost("INTEGER", [=](match_ptr_t match) { //
    auto integer = ast_create<SHAST::IntegerLiteral>(match);
    // auto impl       = match->asShared<ClassMatch>();
    // integer->_value = std::stoi(impl->_token->text);
  });
  ///////////////////////////////////////////////////////////
  onPost("exec_arg", [=](match_ptr_t match) { auto fn_arg = ast_create<SHAST::FunctionInvokationArgument>(match); });
  ///////////////////////////////////////////////////////////
  // onPost("exec_arglist", [=](match_ptr_t match) { auto fn_args = ast_create<SHAST::FunctionInvokationArguments>(match); });
  ///////////////////////////////////////////////////////////
  onLink("Expression", [=](match_ptr_t match) { //
    auto expression = ast_get<SHAST::Expression>(match);
    /*
    /////////////////////////
    // try to collapse expr->sum->product->primary chains
    /////////////////////////
    SHAST::astnode_ptr_t collapse_node;
    auto sum = std::dynamic_pointer_cast<SHAST::Sum>(expression->_children[0]);
    if (sum->_op == '_') {
      auto product_node = std::dynamic_pointer_cast<SHAST::Product>(sum->_left);
      if (product_node) {
        if (product_node->_primaries.size() == 1) {
          auto primary_node = product_node->_primaries[0];
          collapse_node     = primary_node->_children[0];
        } else {
          collapse_node = product_node;
        }
      }
    }
    /////////////////////////
    if (collapse_node) {
      auto expr_par = expression->_parent;
      size_t index  = 0;
      size_t numc   = expr_par->_children.size();
      for (size_t ic = 0; ic < numc; ++ic) {
        if (expr_par->_children[ic] == expression) {
          expr_par->_children[ic] = collapse_node;
          break;
        }
      }
    }
    /////////////////////////
    */
  });
  ///////////////////////////////////////////////////////////
  onPost("decl_arglist", [=](match_ptr_t match) {
    auto arg_list = ast_create<SHAST::DeclArgumentList>(match);
    auto nom      = match->asShared<NOrMore>();
    for (auto item : nom->_items) {
      auto seq = item->asShared<Sequence>();
      // auto tid = ast_get<SHAST::TypedIdentifier>(seq->_items[0]);
      // arg_list->_arguments.push_back(tid);
    }
  });
  ///////////////////////////////////////////////////////////
  onPost("FunctionDef1", [=](match_ptr_t match) {
    auto seq     = match->asShared<Sequence>();
    auto fn_def  = ast_create<SHAST::FunctionDef1>(match);
    auto objname = ast_get<SHAST::ObjectName>(seq->_items[1]);
    seq->dump("fn_def");
    fn_def->_name = objname->_name;

    /*auto args    = seq->itemAsShared<NOrMore>(3);
    auto stas    = seq->itemAsShared<NOrMore>(6);

    args->dump("args");

    for (auto arg : args->_items) {
      auto argseq   = arg->asShared<Sequence>();
      auto arg_decl = ast_get<SHAST::ArgumentDeclaration>(arg);
      funcdef->_arguments.push_back(arg_decl);
      auto argtype = arg_decl->_datatype->_name;
      auto argname = arg_decl->_variable_name;
    }

    stas->dump("stas");

    int i = 0;
    for (auto sta : stas->_items) {
      auto stasel = sta->followImplAsShared<OneOf>()->_selected;
      if (auto as_seq = stasel->tryAsShared<Sequence>()) {
        auto staseq0         = as_seq.value()->_items[0];
        auto staseq0_matcher = staseq0->_matcher;

        printf(
            "staseq0 <%p> matcher<%p:%s>\n", //
            (void*)staseq0.get(),
            staseq0_matcher.get(),
            staseq0_matcher->_name.c_str());

        if (staseq0_matcher == assignment_statement) {
          // GOT ASSIGNMENT STATEMENT
        } else {
          auto statype = staseq0->_impl.typestr();
          printf("unknown staseq0 subtype<%s>\n", statype.c_str());
          OrkAssert(false);
        }
      } else if (auto as_semi = stasel->tryAsShared<Sequence>()) {

      } else {
        auto statype = stasel->_impl.typestr();
        printf("unknown statement item type<%s>\n", statype.c_str());
        OrkAssert(false);
      }
      i++;
    }
    */
  });
  ///////////////////////////////////////////////////////////
  if (0)
    onPost("output_decl", [=](match_ptr_t match) {
      auto ast_output_decl = ast_create<SHAST::InterfaceOutput>(match);
      /*
      auto seq                     = match->asShared<Sequence>();
      auto ddecl_layout            = seq->_items[0]->followImplAsShared<Optional>();
      auto ddecl_seq               = seq->_items[1]->followImplAsShared<Sequence>();
      auto tid                     = ast_get<SHAST::TypedIdentifier>(ddecl_seq->_items[0]->asShared<Proxy>()->_selected);
      ast_output_decl->_identifier = tid->_identifier;
      ast_output_decl->_datatype   = tid->_datatype;
      */
    });
  ///////////////////////////////////////////////////////////
  onPost("TranslationUnit", [=](match_ptr_t match) {
    auto translation_unit = ast_create<SHAST::TranslationUnit>(match);
    /*auto the_nom          = match->asShared<NOrMore>();
    for (auto item : the_nom->_items) {

      SHAST::translatable_ptr_t translatable;

      std::string ast_name;

      if (auto as_fndef = try_ast_get<SHAST::Translatable>(item)) {
        translatable = as_fndef;
      }

      if (translatable) {
        auto it = translation_unit->_translatables_by_name.find(translatable->_name);
        OrkAssert(it == translation_unit->_translatables_by_name.end());
        translation_unit->_translatables_by_name[translatable->_name] = translatable;
        translation_unit->_translatable_by_order.push_back(translatable);
      }
    }*/
  });
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::shadlang::impl {
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif