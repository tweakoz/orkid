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

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////

namespace impl {
static logchannel_ptr_t logchan         = logger()->createChannel("ORKSLIMPL", fvec3(1, 1, .9), false);
static logchannel_ptr_t logchan_grammar = logger()->createChannel("ORKSLGRAM", fvec3(1, 1, .8), false);
static logchannel_ptr_t logchan_lexer   = logger()->createChannel("ORKSLLEXR", fvec3(1, 1, .7), false);

///////////////////////////////////////////////////////////////////////////////

std::string scanner_spec = "";
std::string parser_spec = "";

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::shared_ptr<T> ast_create(match_ptr_t m) {
  auto sh = std::make_shared<T>();
  m->_uservars.set<SHAST::astnode_ptr_t>("astnode", sh);
  return sh;
}

template <typename T> std::shared_ptr<T> ast_get(match_ptr_t m) {
  auto sh = m->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();
  return std::dynamic_pointer_cast<T>(sh);
}
template <typename T> std::shared_ptr<T> try_ast_get(match_ptr_t m) {
  auto sh = m->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode");
  if (sh) {
    return std::dynamic_pointer_cast<T>(sh.value());
  }
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

struct ShadLangParser : public Parser {

  ShadLangParser() {
    _name = "shadlang";
    //_DEBUG_MATCH       = true;
    //_DEBUG_INFO        = true;

    if(0==scanner_spec.length()){
      auto scanner_path = ork::file::Path::data_dir()/"grammars"/"shadlang.scanner";
      auto read_result = ork::File::readAsString(scanner_path);
      scanner_spec = read_result->_data;
    }
    if(0==parser_spec.length()){
      auto parser_path = ork::file::Path::data_dir()/"grammars"/"shadlang.parser";
      auto read_result = ork::File::readAsString(parser_path);
      parser_spec = read_result->_data;
    }

    auto scanner_match = this->loadPEGScannerSpec(scanner_spec);
    OrkAssert(scanner_match);

    auto parser_match = this->loadPEGParserSpec(parser_spec);
    OrkAssert(parser_match);
    ///////////////////////////////////////////////////////////
    // parser should be compiled and linked at this point
    ///////////////////////////////////////////////////////////
    auto objectNameAst = [&](match_ptr_t match) {
      auto objname     = ast_create<SHAST::ObjectName>(match);
      auto fn_name_seq = match->asShared<Sequence>();
      auto fn_name     = fn_name_seq->_items[0]->followImplAsShared<ClassMatch>();
      objname->_name   = fn_name->_token->text;
    };
    ///////////////////////////////////////////////////////////
    onPost("fn_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("fn2_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("fni_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("vtx_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("frg_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("com_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("uniset_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("uniblk_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("vif_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("gif_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("fif_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("lib_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("sb_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("pass_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("fxconfigdecl_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("fxconfigref_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("technique_name", [=](match_ptr_t match) { objectNameAst(match); });
    // onPost("primary_var_ref", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("ImportDirective", [=](match_ptr_t match) { //
      auto member_ref     = ast_create<SHAST::ImportDirective>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("member_ref", [=](match_ptr_t match) { //
      auto member_ref     = ast_create<SHAST::MemberRef>(match);
      auto seq            = match->asShared<Sequence>();
      member_ref->_member = seq->_items[1]->followImplAsShared<ClassMatch>()->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("array_ref", [=](match_ptr_t match) { //
      auto array_ref = ast_create<SHAST::ArrayRef>(match);
      auto seq       = match->asShared<Sequence>();
      // array_ref->_member = seq->_items[1]->followImplAsShared<ClassMatch>()->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("FLOATING_POINT", [=](match_ptr_t match) { //
      auto ast_node     = ast_create<SHAST::FloatLiteral>(match);
      auto impl         = match->asShared<ClassMatch>();
      ast_node->_value  = std::stof(impl->_token->text);
      ast_node->_strval = impl->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("INTEGER", [=](match_ptr_t match) { //
      auto integer    = ast_create<SHAST::IntegerLiteral>(match);
      auto impl       = match->asShared<ClassMatch>();
      integer->_value = std::stoi(impl->_token->text);
    });
    ///////////////////////////////////////////////////////////
    onPost("datatype", [=](match_ptr_t match) { //
      auto selected     = match->asShared<OneOf>()->_selected;
      auto ast_dt       = ast_create<SHAST::DataType>(match);
      ast_dt->_datatype = selected->asShared<ClassMatch>()->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("exec_arg", [=](match_ptr_t match) { auto fn_arg = ast_create<SHAST::FunctionInvokationArgument>(match); });
    ///////////////////////////////////////////////////////////
    //onPost("exec_arglist", [=](match_ptr_t match) { auto fn_args = ast_create<SHAST::FunctionInvokationArguments>(match); });
    ///////////////////////////////////////////////////////////
    onPost("fn_invok", [=](match_ptr_t match) { auto fn_invok = ast_create<SHAST::FunctionInvokation>(match); });
    ///////////////////////////////////////////////////////////
    onPost("PrimaryExpression", [=](match_ptr_t match) {
      auto primary   = ast_create<SHAST::PrimaryExpression>(match);
      primary->_name = "primary";
    });
    ///////////////////////////////////////////////////////////
    onPost("MultiplicativeExpression", [=](match_ptr_t match) {
      auto product      = ast_create<SHAST::MultiplicativeExpression>(match);
      auto seq          = match->asShared<Sequence>();
      auto primary1     = seq->_items[0];
      /*auto primary1_ast = ast_get<SHAST::Primary>(primary1);
      auto opt          = seq->itemAsShared<Optional>(1);
      auto primary2     = opt->_subitem;

      product->_primaries.push_back(primary1_ast);
      if (primary2) {
        auto seq          = primary2->asShared<Sequence>();
        primary2          = seq->_items[1];
        auto primary2_ast = ast_get<SHAST::Primary>(primary2);
        product->_primaries.push_back(primary2_ast);
      } else { // "prI1"
      }*/
    });
    ///////////////////////////////////////////////////////////
    onPost("AdditiveExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::AdditiveExpression>(match);
      /*auto sum      = ast_create<SHAST::Sum>(match);
      auto seq      = match->asShared<Sequence>();
      auto selected = seq->_items[0]->asShared<OneOf>()->_selected;
      auto seq2     = selected->asShared<Sequence>();
      if (selected->_matcher->_name == "pro") {
        sum->_left = ast_get<SHAST::Product>(seq2->_items[1]);
        sum->_op   = '_';
      } else if (selected->_matcher->_name == "add") {
        sum->_left  = ast_get<SHAST::Product>(seq2->_items[0]);
        sum->_right = ast_get<SHAST::Product>(seq2->_items[2]);
        sum->_op    = '+';
      } else if (selected->_matcher->_name == "sub") {
        sum->_left  = ast_get<SHAST::Product>(seq2->_items[0]);
        sum->_right = ast_get<SHAST::Product>(seq2->_items[2]);
        sum->_op    = '-';
      } else {
        OrkAssert(false);
      }*/
    });
    ///////////////////////////////////////////////////////////
    onPost("UnaryExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::UnaryExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("PostfixExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::PostfixExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("PrimaryExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::PrimaryExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("ConditionalExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::ConditionalExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("AssignmentExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::AssignmentExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("ArgumentExpressionList", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::ArgumentExpressionList>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("LogicalAndExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::LogicalAndExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("LogicalOrExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::LogicalOrExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("InclusiveOrExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::InclusiveOrExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("ExclusiveOrExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::ExclusiveOrExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("AndExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::AndExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("EqualityExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::EqualityExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("RelationalExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::RelationalExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("ShiftExpression", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::ShiftExpression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("expression", [=](match_ptr_t match) { //
      auto expression = ast_create<SHAST::Expression>(match);
    });
    onLink("expression", [=](match_ptr_t match) { //
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
    onPost("IfStatement", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::IfStatement>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("WhileStatement", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::WhileStatement>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("ReturnStatement", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::ReturnStatement>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("CompoundStatement", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::CompoundStatement>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("ExpressionStatement", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::ExpressionStatement>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("DiscardStatement", [=](match_ptr_t match) {
      auto unary = ast_create<SHAST::DiscardStatement>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("RValueConstructor", [=](match_ptr_t match) {
      auto ast_rvc       = ast_create<SHAST::RValueConstructor>(match);
      auto seq           = match->asShared<Sequence>();
      //auto ast_dt        = ast_get<SHAST::DataType>(seq->_items[0]);
      //ast_rvc->_datatype = ast_dt->_datatype;
      //ast_dt->_showDOT   = false;
    });
    ///////////////////////////////////////////////////////////
    onPost("typed_identifier", [=](match_ptr_t match) {
      auto tid         = ast_create<SHAST::TypedIdentifier>(match);
      auto seq         = match->asShared<Sequence>();
      auto dt          = ast_get<SHAST::DataType>(seq->_items[0]);
      tid->_datatype   = dt->_datatype;
      auto kwid        = seq->_items[1]->followImplAsShared<ClassMatch>();
      tid->_identifier = kwid->_token->text;
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
    onPost("statement_list", [=](match_ptr_t match) {
      auto statement_list = ast_create<SHAST::StatementList>(match);
      auto nom            = match->asShared<NOrMore>();
      for (auto item : nom->_items) {
        // auto seq = item->asShared<Sequence>();
        //  auto statement = ast_get<SHAST::Stat>(seq->_items[0]);
        //  arg_list->_arguments.push_back(tid);
      }
    });
    ///////////////////////////////////////////////////////////
    onPost("LibraryBlock", [=](match_ptr_t match) {
      auto libblock   = ast_create<SHAST::LibraryBlock>(match);
      auto seq        = match->asShared<Sequence>();
      auto objname    = ast_get<SHAST::ObjectName>(seq->_items[1]);
      libblock->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("fn_def", [=](match_ptr_t match) {
      auto seq     = match->asShared<Sequence>();
      auto fn_def  = ast_create<SHAST::FunctionDef>(match);
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
    onPost("vtx_shader", [=](match_ptr_t match) {
      auto vtx_shader   = ast_create<SHAST::VertexShader>(match);
      auto seq          = match->asShared<Sequence>();
      auto objname      = ast_get<SHAST::ObjectName>(seq->_items[1]);
      vtx_shader->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("geo_shader", [=](match_ptr_t match) {
      auto vtx_shader   = ast_create<SHAST::GeometryShader>(match);
      auto seq          = match->asShared<Sequence>();
      auto objname      = ast_get<SHAST::ObjectName>(seq->_items[1]);
      vtx_shader->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("frg_shader", [=](match_ptr_t match) {
      auto frg_shader   = ast_create<SHAST::FragmentShader>(match);
      auto seq          = match->asShared<Sequence>();
      auto objname      = ast_get<SHAST::ObjectName>(seq->_items[1]);
      frg_shader->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("com_shader", [=](match_ptr_t match) {
      auto com_shader   = ast_create<SHAST::ComputeShader>(match);
      auto seq          = match->asShared<Sequence>();
      auto objname      = ast_get<SHAST::ObjectName>(seq->_items[1]);
      com_shader->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("data_decl", [=](match_ptr_t match) {
      auto data_decl = ast_create<SHAST::DataDeclaration>(match);
      /*auto seq       = match->asShared<Sequence>();
      seq->dump("POST:data_decl");
      auto dtsel       = seq->_items[0]->asShared<Proxy>()->_selected;
      auto dt          = ast_get<SHAST::DataType>(dtsel);
      auto kwid        = seq->_items[1]->followImplAsShared<ClassMatch>();
      data_decl->_name = kwid->_token->text;
      */
    });
    onLink("data_decl", [=](match_ptr_t match) {
      auto data_decl         = ast_get<SHAST::DataDeclaration>(match);
      /*
      auto tid               = data_decl->childAs<SHAST::TypedIdentifier>(0);
      data_decl->_datatype   = tid->_datatype;
      data_decl->_identifier = tid->_identifier;
      tid->_showDOT          = false;
      data_decl->_descend    = false;
      */
    });
    ///////////////////////////////////////////////////////////
    onPost("data_decls", [=](match_ptr_t match) { auto data_decls = ast_create<SHAST::DataDeclarations>(match); });
    ///////////////////////////////////////////////////////////
    onPost("UniformSet", [=](match_ptr_t match) {
      auto uniset   = ast_create<SHAST::UniformSet>(match);
      auto seq      = match->asShared<Sequence>();
      auto objname  = ast_get<SHAST::ObjectName>(seq->_items[1]);
      uniset->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("UniformBlk", [=](match_ptr_t match) {
      auto uniblk   = ast_create<SHAST::UniformBlk>(match);
      /*auto seq      = match->asShared<Sequence>();
      auto objname  = ast_get<SHAST::ObjectName>(seq->_items[1]);
      uniblk->_name = objname->_name;
      */
    });
    ///////////////////////////////////////////////////////////
    onPost("iface_input", [=](match_ptr_t match) {
      auto iface_input = ast_create<SHAST::InterfaceInput>(match);
      /*
      auto seq         = match->asShared<Sequence>();
      seq->dump("iface_input");
      auto tid                 = ast_get<SHAST::TypedIdentifier>(seq->_items[0]->asShared<Proxy>()->_selected);
      iface_input->_identifier = tid->_identifier;
      iface_input->_datatype   = tid->_datatype;
      if (auto try_semantic = seq->_items[1]->asShared<Optional>()->_subitem) {
        auto semantic_seq      = try_semantic->asShared<Sequence>();
        auto semantic          = semantic_seq->_items[1]->followImplAsShared<ClassMatch>();
        iface_input->_semantic = semantic->_token->text;
      }
      */
    });
    ///////////////////////////////////////////////////////////
    onPost("iface_inputs", [=](match_ptr_t match) {
      auto iface_inputs   = ast_create<SHAST::InterfaceInputs>(match);
      //auto seq            = match->asShared<Sequence>();
      //auto fn_name        = seq->_items[1]->followImplAsShared<ClassMatch>();
      //iface_inputs->_name = fn_name->_token->text;
    });
    ///////////////////////////////////////////////////////////
    if(0)onPost("output_decl", [=](match_ptr_t match) {
      auto ast_output_decl         = ast_create<SHAST::InterfaceOutput>(match);
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
    onPost("InterfaceLayout", [=](match_ptr_t match) {
      auto ast_layout   = ast_create<SHAST::InterfaceLayout>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("iface_outputs", [=](match_ptr_t match) {
      auto ast_iface_outputs   = ast_create<SHAST::InterfaceOutputs>(match);
      //auto seq                 = match->asShared<Sequence>();
      //auto fn_name             = seq->_items[1]->followImplAsShared<ClassMatch>();
      //ast_iface_outputs->_name = fn_name->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("VertexInterface", [=](match_ptr_t match) {
      auto ast_vtx_iface   = ast_create<SHAST::VertexInterface>(match);
      //auto seq             = match->asShared<Sequence>();
      //auto objname         = ast_get<SHAST::ObjectName>(seq->_items[1]);
      //ast_vtx_iface->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("FragmentInterface", [=](match_ptr_t match) {
      auto frg_iface   = ast_create<SHAST::FragmentInterface>(match);
      //auto seq         = match->asShared<Sequence>();
      //auto objname     = ast_get<SHAST::ObjectName>(seq->_items[1]);
      //frg_iface->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("StateBlockItem", [=](match_ptr_t match) {
      auto ast_sb_item    = ast_create<SHAST::StateBlockItem>(match);
      //auto seq            = match->asShared<Sequence>();
      //auto key_item       = seq->_items[0]->followImplAsShared<OneOf>()->_selected;
      //ast_sb_item->_key   = key_item->followImplAsShared<ClassMatch>()->_token->text;
      //ast_sb_item->_value = seq->_items[2]->followImplAsShared<ClassMatch>()->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("StateBlock", [=](match_ptr_t match) {
      auto ast_sb   = ast_create<SHAST::StateBlock>(match);
      //auto seq      = match->asShared<Sequence>();
      //auto name_seq = seq->_items[1]->asShared<Sequence>();
      //auto sb_name  = name_seq->_items[0]->followImplAsShared<ClassMatch>();
      //ast_sb->_name = sb_name->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("FxConfigDecl", [=](match_ptr_t match) {
      auto ast_sb   = ast_create<SHAST::FxConfigDecl>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("inh_list_item", [=](match_ptr_t match) {
      auto seq     = match->asShared<Sequence>();
      /*auto fn_name = seq->_items[1]->followImplAsShared<ClassMatch>();
      if (fn_name->_token->text == "extension") {
        auto opt                       = seq->_items[2]->asShared<Optional>();
        auto opt_seq                   = opt->_subitem->asShared<Sequence>();
        auto ext_name                  = opt_seq->_items[1]->followImplAsShared<ClassMatch>()->_token->text;
        auto inh_list_item             = ast_create<SHAST::Extension>(match);
        inh_list_item->_extension_name = ext_name;
        inh_list_item->_name           = fn_name->_token->text;
      } else {
        auto inh_list_item   = ast_create<SHAST::Dependency>(match);
        inh_list_item->_name = fn_name->_token->text;
      }*/
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

    ///////////////////////////////////////////////////////////
  }

  /////////////////////////////////////////////////////////////////

  void _buildAstTreeVisitor(match_ptr_t the_match) {
    bool has_ast = the_match->_uservars.hasKey("astnode");
    if (has_ast) {
      auto ast = the_match->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();

      if (_astnodestack.size() > 0) {
        auto parent = _astnodestack.back();
        parent->_children.push_back(ast);
        ast->_parent = parent;
      }
      _astnodestack.push_back(ast);
    }

    for (auto c : the_match->_children) {
      _buildAstTreeVisitor(c);
    }
    if (has_ast) {
      _astnodestack.pop_back();
    }
  }

  /////////////////////////////////////////////////////////////////

  SHAST::translationunit_ptr_t parseString(std::string parse_str) {

    _scanner->scanString(parse_str);
    _scanner->discardTokensOfClass(uint64_t(TokenClass::WHITESPACE));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::SINGLE_LINE_COMMENT));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::MULTI_LINE_COMMENT));
    _scanner->discardTokensOfClass(uint64_t(TokenClass::NEWLINE));

    auto top_view = _scanner->createTopView();
    //top_view.dump("top_view");
    auto slv    = std::make_shared<ScannerLightView>(top_view);
    _tu_matcher = findMatcherByName("TranslationUnit");
    OrkAssert(_tu_matcher);
    auto match = this->match(_tu_matcher, slv, [this](match_ptr_t m) { _buildAstTreeVisitor(m); });
    OrkAssert(match);

    auto ast_top = match->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();
    printf("///////////////////////////////\n");
    printf("// AST TREE\n");
    printf("///////////////////////////////\n");
    std::string ast_str = toASTstring(ast_top);
    printf("%s\n", ast_str.c_str());
    printf("///////////////////////////////\n");
    return std::dynamic_pointer_cast<SHAST::TranslationUnit>(ast_top);
  }

  /////////////////////////////////////////////////////////////////

  matcher_ptr_t _tu_matcher;
  std::vector<SHAST::astnode_ptr_t> _astnodestack;
}; // struct ShadLangParser

} // namespace impl

SHAST::translationunit_ptr_t parse(const std::string& shader_text) {
  auto parser = std::make_shared<impl::ShadLangParser>();
  auto topast = parser->parseString(shader_text);
  return topast;
}

void SHAST::_dumpAstTreeVisitor(SHAST::astnode_ptr_t node, int indent, std::string& out_str) {
  auto indentstr = std::string(indent * 2, ' ');
  out_str += FormatString("%s%s\n", indentstr.c_str(), node->desc().c_str());
  if (node->_descend) {
    for (auto c : node->_children) {
      _dumpAstTreeVisitor(c, indent + 1, out_str);
    }
  }
}

std::string toASTstring(SHAST::astnode_ptr_t node) {
  std::string rval;
  SHAST::_dumpAstTreeVisitor(node, 0, rval);
  return rval;
}

void visitAST(SHAST::astnode_ptr_t node, SHAST::visitor_ptr_t visitor) {
  if (visitor->_on_pre) {
    visitor->_on_pre(node);
  }
  visitor->_nodestack.push(node);
  for (auto c : node->_children) {
    visitAST(c, visitor);
  }
  visitor->_nodestack.pop();
  if (visitor->_on_post) {
    visitor->_on_post(node);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif