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
static logchannel_ptr_t logchan         = logger()->createChannel("ORKSLIMPL", fvec3(1, 1, .9), false);
static logchannel_ptr_t logchan_grammar = logger()->createChannel("ORKSLGRAM", fvec3(1, 1, .8), false);
static logchannel_ptr_t logchan_lexer   = logger()->createChannel("ORKSLLEXR", fvec3(1, 1, .7), false);
/////////////////////////////////////////////////////////////////////////////////////////////////
void SHAST::_dumpAstTreeVisitor(
    SHAST::astnode_ptr_t node, //
    int indent,                //
    std::string& out_str) {    //
  auto indentstr = std::string(indent * 2, ' ');
  out_str += FormatString("%s%s\n", indentstr.c_str(), node->desc().c_str());
  if (node->_descend) {
    for (auto c : node->_children) {
      _dumpAstTreeVisitor(c, indent + 1, out_str);
    }
  }
}
//////////////////////////////
std::string toASTstring(SHAST::astnode_ptr_t node) {
  std::string rval;
  SHAST::_dumpAstTreeVisitor(node, 0, rval);
  return rval;
}
//////////////////////////////
void visitAST(
    SHAST::astnode_ptr_t node,      //
    SHAST::visitor_ptr_t visitor) { //
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
//////////////////////////////
using expression_chain_t = std::vector<SHAST::expression_ptr_t>;
//////////////////////////////
bool reduceExpressionChain(expression_chain_t& chain) {
  for (auto item : chain) {
    // printf( "%s ", item->_name.c_str() );
  }
  // printf("\n");
  if (chain.size() > 2) {
    auto first = *chain.begin();
    auto last  = *chain.rbegin();
    last->_children.clear();
    last->_children.push_back(first);
    first->_parent = last;
    return true;
  }
  return false;
}
//////////////////////////////
void reduceASTexpressions(SHAST::astnode_ptr_t top) {

  std::set<SHAST::astnode_ptr_t> expression_nodes;
  auto collect_expressions     = std::make_shared<SHAST::Visitor>();
  //////////////////////////////////////////////////
  collect_expressions->_on_pre = [&](SHAST::astnode_ptr_t node) {
    if (auto node_as_expression = std::dynamic_pointer_cast<SHAST::Expression>(node)) {
      if (node_as_expression->_children.size() == 1) {
        auto parent               = node_as_expression->_parent;
        if (auto parent_as_expression = std::dynamic_pointer_cast<SHAST::Expression>(parent)) {
          //if (parent_as_expression->_children.size() == 1) {
            expression_nodes.insert(node_as_expression);
          //}
        }
      }
    }
  };
  //////////////////////////////////////////////////
  visitAST(top, collect_expressions);
  //////////////////////////////////////////////////
  printf( "reduceASTexpressions::expression_nodes count<%zu>\n", expression_nodes.size() );
  while (expression_nodes.size()) {
    auto it                   = expression_nodes.begin();
    auto node                 = *it;
    auto parent_as_expression = std::dynamic_pointer_cast<SHAST::Expression>(node->_parent);
    auto node_as_expression   = std::dynamic_pointer_cast<SHAST::Expression>(node);
    OrkAssert(parent_as_expression);
    OrkAssert(node_as_expression);
    OrkAssert(node_as_expression->_children.size() == 1);
    //OrkAssert(parent_as_expression->_children.size() == 1);
    auto new_child                         = node_as_expression->_children[0];
    new_child->_parent = parent_as_expression;
    for( size_t index=0; index<parent_as_expression->_children.size(); index++ ){
      auto prev_child = parent_as_expression->_children[index];
      if( prev_child == node ){
        parent_as_expression->_children[index] = new_child;
      }
    }
    expression_nodes.erase(it);
    printf( "reduceASTexpressions reduce<%s>\n", node->_name.c_str() );
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////
namespace impl {
///////////////////////////////////////////////////////////////////////////////

std::string scanner_spec = "";
std::string parser_spec  = "";

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

#define DECLARE_STD_AST_NODE(x) onPre(#x, [=](match_ptr_t match) { auto ast_node = ast_create<SHAST::x>(match); });
#define DECLARE_OBJNAME_AST_NODE(x)                                                                                                \
  onPre(x, [=](match_ptr_t match) {                                                                                                \
    auto objname     = ast_create<SHAST::ObjectName>(match);                                                                       \
    auto fn_name_seq = match->asShared<Sequence>();                                                                                \
    auto fn_name     = fn_name_seq->_items[0]->followImplAsShared<ClassMatch>();                                                   \
    objname->_name   = fn_name->_token->text;                                                                                      \
  });

///////////////////////////////////////////////////////////////////////////////

struct ShadLangParser : public Parser {

  ShadLangParser() {
    _name = "shadlang";
    //_DEBUG_MATCH       = true;
    //_DEBUG_INFO        = true;

    if (0 == scanner_spec.length()) {
      auto scanner_path = ork::file::Path::data_dir() / "grammars" / "shadlang.scanner";
      auto read_result  = ork::File::readAsString(scanner_path);
      scanner_spec      = read_result->_data;
    }
    if (0 == parser_spec.length()) {
      auto parser_path = ork::file::Path::data_dir() / "grammars" / "shadlang.parser";
      auto read_result = ork::File::readAsString(parser_path);
      parser_spec      = read_result->_data;
    }

    auto scanner_match = this->loadPEGScannerSpec(scanner_spec);
    OrkAssert(scanner_match);

    auto parser_match = this->loadPEGParserSpec(parser_spec);
    OrkAssert(parser_match);
    ///////////////////////////////////////////////////////////
    // parser should be compiled and linked at this point
    ///////////////////////////////////////////////////////////
    DECLARE_OBJNAME_AST_NODE("fn_name");
    DECLARE_OBJNAME_AST_NODE("fn2_name");
    DECLARE_OBJNAME_AST_NODE("vtx_name");
    DECLARE_OBJNAME_AST_NODE("frg_name");
    DECLARE_OBJNAME_AST_NODE("com_name");

    DECLARE_OBJNAME_AST_NODE("uniset_name");
    DECLARE_OBJNAME_AST_NODE("uniblk_name");
    DECLARE_OBJNAME_AST_NODE("vif_name");
    DECLARE_OBJNAME_AST_NODE("gif_name");
    DECLARE_OBJNAME_AST_NODE("fif_name");
    DECLARE_OBJNAME_AST_NODE("lib_name");
    DECLARE_OBJNAME_AST_NODE("sb_name");

    DECLARE_OBJNAME_AST_NODE("pass_name");
    DECLARE_OBJNAME_AST_NODE("fxconfigdecl_name");
    DECLARE_OBJNAME_AST_NODE("fxconfigref_name");
    DECLARE_OBJNAME_AST_NODE("technique_name");
    ///////////////////////////////////////////////////////////
    DECLARE_STD_AST_NODE(DataType);
    DECLARE_STD_AST_NODE(MemberRef);
    DECLARE_STD_AST_NODE(ArrayRef);
    DECLARE_STD_AST_NODE(RValueConstructor);
    DECLARE_STD_AST_NODE(TypedIdentifier);
    DECLARE_STD_AST_NODE(DataDeclaration);
    ///////////////////////////////////////////////////////////
    DECLARE_STD_AST_NODE(PrimaryExpression);
    DECLARE_STD_AST_NODE(MultiplicativeExpression);
    DECLARE_STD_AST_NODE(AdditiveExpression);
    DECLARE_STD_AST_NODE(UnaryExpression);
    DECLARE_STD_AST_NODE(PostfixExpression);
    DECLARE_STD_AST_NODE(PrimaryExpression);
    DECLARE_STD_AST_NODE(ConditionalExpression);
    DECLARE_STD_AST_NODE(AssignmentExpression);
    DECLARE_STD_AST_NODE(ArgumentExpressionList);
    DECLARE_STD_AST_NODE(LogicalAndExpression);
    DECLARE_STD_AST_NODE(LogicalOrExpression);
    DECLARE_STD_AST_NODE(InclusiveOrExpression);
    DECLARE_STD_AST_NODE(ExclusiveOrExpression);
    DECLARE_STD_AST_NODE(AndExpression);
    DECLARE_STD_AST_NODE(EqualityExpression);
    DECLARE_STD_AST_NODE(RelationalExpression);
    DECLARE_STD_AST_NODE(ShiftExpression);
    DECLARE_STD_AST_NODE(Expression);
    DECLARE_STD_AST_NODE(Literal);
    DECLARE_STD_AST_NODE(NumericLiteral);
    ///////////////////////////////////////////////////////////
    DECLARE_STD_AST_NODE(IfStatement);
    DECLARE_STD_AST_NODE(WhileStatement);
    DECLARE_STD_AST_NODE(ReturnStatement);
    DECLARE_STD_AST_NODE(CompoundStatement);
    DECLARE_STD_AST_NODE(ExpressionStatement);
    DECLARE_STD_AST_NODE(DiscardStatement);
    DECLARE_STD_AST_NODE(StatementList);
    ///////////////////////////////////////////////////////////
    DECLARE_STD_AST_NODE(InterfaceLayout);
    DECLARE_STD_AST_NODE(InterfaceOutputs);
    DECLARE_STD_AST_NODE(InterfaceInputs);
    DECLARE_STD_AST_NODE(InterfaceInput);
    DECLARE_STD_AST_NODE(InterfaceOutput);
    DECLARE_STD_AST_NODE(DataDeclarations);
    DECLARE_STD_AST_NODE(DataDeclarations);
    DECLARE_STD_AST_NODE(ImportDirective);
    DECLARE_STD_AST_NODE(InheritList);
    DECLARE_STD_AST_NODE(InheritListItem);
    ///////////////////////////////////////////////////////////
    DECLARE_STD_AST_NODE(VertexInterface);
    DECLARE_STD_AST_NODE(FragmentInterface);
    DECLARE_STD_AST_NODE(StateBlock);
    DECLARE_STD_AST_NODE(StateBlockItem);
    DECLARE_STD_AST_NODE(FxConfigDecl);
    DECLARE_STD_AST_NODE(UniformSet);
    DECLARE_STD_AST_NODE(UniformBlk);
    DECLARE_STD_AST_NODE(LibraryBlock);
    DECLARE_STD_AST_NODE(VertexShader);
    DECLARE_STD_AST_NODE(GeometryShader);
    DECLARE_STD_AST_NODE(FragmentShader);
    DECLARE_STD_AST_NODE(ComputeShader);
    ///////////////////////////////////////////////////////////
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
    // top_view.dump("top_view");
    auto slv    = std::make_shared<ScannerLightView>(top_view);
    _tu_matcher = findMatcherByName("TranslationUnit");
    OrkAssert(_tu_matcher);
    auto match = this->match(_tu_matcher, slv, [this](match_ptr_t m) { _buildAstTreeVisitor(m); });
    OrkAssert(match);
    auto ast_top = match->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();
    ///////////////////////////////////////////
    reduceASTexpressions(ast_top);
    ///////////////////////////////////////////
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

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif