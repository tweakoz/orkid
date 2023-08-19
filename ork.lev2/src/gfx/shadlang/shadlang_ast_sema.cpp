////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////
using namespace SHAST;

template <typename T> 
void _semaShouldIdent(impl::ShadLangParser* slp, astnode_ptr_t top){
  auto nodes         = slp-> template collectNodesOfType<T>(top);
  for (auto n : nodes) {
    n->_should_indent = true;
  }
}


void _procdatatype(
    impl::ShadLangParser* slp, //
    astnode_ptr_t dt_node,
    std::string dt_name,
    bool built_in) {
  // printf( "dt_node: name<%s>\n", dt_node->_name.c_str() );
  dt_node->_name = FormatString("DataType: %s", dt_name.c_str());
  dt_node->setValueForKey<std::string>("data_type", dt_name);
  dt_node->setValueForKey<bool>("is_builtin", built_in);
  dt_node->setValueForKey<bool>("is_user", not built_in);
}

std::string _dt_extract_type(match_ptr_t dt_node) {
  auto seq = dt_node->asShared<Sequence>();
  auto sel = seq->itemAsShared<OneOf>(2)->_selected;
  auto cm  = sel->asShared<ClassMatch>();
  return cm->_token->text;
}
/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNormalizeDtUserTypes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto matcher_dtype = slp->findMatcherByName("DataType");
  auto matcher_ident = slp->findMatcherByName("IDENTIFIER");
  auto nodes         = slp->collectNodesOfType<DataTypeWithUserTypes>(top);
  for (auto dtu_node : nodes) {
    auto dtu_match = slp->matchForAstNode(dtu_node);
    auto sel_match = dtu_match->asShared<OneOf>()->_selected;
    //////////////////////////////////////////
    // builtin datatype ?
    //////////////////////////////////////////
    if (sel_match->_matcher == matcher_dtype) {
      auto sel_ast                        = slp->astNodeForMatch(sel_match);
      auto sel_as_dt                      = std::dynamic_pointer_cast<DataType>(sel_ast);
      OrkAssert(sel_as_dt) auto type_name = _dt_extract_type(sel_match);
      _procdatatype(slp, sel_as_dt, type_name, true);
      slp->replaceInParent(dtu_node, sel_as_dt);
    }
    //////////////////////////////////////////
    // user datatype ?
    //////////////////////////////////////////
    else if (sel_match->_matcher == matcher_ident) {
      // sel->dump1(0);
      auto classmatch    = sel_match->asShared<ClassMatch>();
      auto dt_name       = classmatch->_token->text;
      auto new_dt_node   = std::make_shared<DataType>();
      new_dt_node->_name = "USER";
      _procdatatype(slp, new_dt_node, dt_name, false);
      slp->replaceInParent(dtu_node, new_dt_node);
    } else {
      printf("sel<%p>\n", sel_match.get());
      sel_match->dump1(0);
      OrkAssert(false);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameBuiltInDataTypes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<DataType>(top);
  for (auto dt_node : nodes) {
    auto dt_match = slp->matchForAstNode(dt_node);
    auto type_name = _dt_extract_type(dt_match);
    _procdatatype(slp, dt_node, type_name, true);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameTypedIdentifers(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<TypedIdentifier>(top);
  for (auto tid_node : nodes) {

    tid_node->_name = "TID: ";

    auto match = slp->matchForAstNode(tid_node);

    auto seq = match->asShared<Sequence>();

    ////////////////////
    // item 0 (type)
    ////////////////////

    auto sel = seq->itemAsShared<OneOf>(0)->_selected;
    if (auto as_cm = sel->tryAsShared<ClassMatch>()) {
      auto name = as_cm.value()->_token->text;
      tid_node->_name += name;
      tid_node->setValueForKey<std::string>("data_type", name);
    } else { // its a DataTypeNode
      auto seq  = sel->asShared<Sequence>();
      auto sel2 = seq->itemAsShared<OneOf>(2)->_selected;
      auto cm   = sel2->asShared<ClassMatch>();
      auto name = cm->_token->text;
      tid_node->_name += name;
      tid_node->setValueForKey<std::string>("data_type", name);
    }

    ////////////////////
    // item 1 (identifier)
    ////////////////////

    auto cm1 = seq->itemAsShared<ClassMatch>(1);
    tid_node->_name += " " + cm1->_token->text;
    tid_node->setValueForKey<std::string>("identifier_name", cm1->_token->text);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _mangleFunctionDef2(
    impl::ShadLangParser* slp,              //
    std::shared_ptr<FunctionDef2> fn2_node, //
    std::string named) {                    //

  ORK_CONFIG_OPENGL(fn2_node);
  auto dt_node = fn2_node->childAs<DataType>(0);
  if (dt_node) {
    auto return_type = dt_node->typedValueForKey<std::string>("data_type").value();
    printf("mangle function<%s> return type: %s \n", named.c_str(), return_type.c_str());
    auto decl_args = fn2_node->findFirstChildOfType<DeclArgumentList>();
    OrkAssert(decl_args);
    auto mangled_name = named + "<" + return_type + ">(";
    AstNode::walkDownAST(decl_args, [&](astnode_ptr_t node) -> bool {
      if (decl_args != node) {
        auto tid_node = std::dynamic_pointer_cast<TypedIdentifier>(node);
        OrkAssert(tid_node);
        auto arg_dt = tid_node->typedValueForKey<std::string>("data_type").value();
        mangled_name += arg_dt + ",";
        // printf( "mangle walkdown - argsnode : %s\n", node->_name.c_str() );
      }
      return true;
    });
    mangled_name += ")";
    printf("mangled_name<%s>\n", mangled_name.c_str());
    fn2_node->setValueForKey<std::string>("function_name", named);
    fn2_node->setValueForKey<std::string>("mangled_name", mangled_name);
    fn2_node->setValueForKey<std::string>("unmangled_name", named);
  } else {
    AstNode::walkDownAST(fn2_node, [&](astnode_ptr_t node) -> bool {
      printf("mangle walkdown - argsnode : %s - no dt_node\n", node->_name.c_str());
      return true;
    });
    OrkAssert(false);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

template <typename node_t> //
void _semaCollectNamedOfType(
    impl::ShadLangParser* slp, //
    astnode_ptr_t top,         //
    astnode_map_t& outmap) {   //

  auto nodes = slp->collectNodesOfType<node_t>(top);
  for (auto n : nodes) {
    astnode_ptr_t obj_name;
    AstNode::walkDownAST(n, [&](astnode_ptr_t node) -> bool {
      auto as_objname = std::dynamic_pointer_cast<ObjectName>(node);
      // printf( "walkdown<%s> as_objname<%s>\n", node->_name.c_str(), as_objname ? "true" : "false" );
      if (as_objname) {
        obj_name = as_objname;
        return false;
      }
      return true;
    });
    if (obj_name) {
      auto the_name = obj_name->_name;

      ////////////////////////////////////////////////////////////
      // name mangling?
      ////////////////////////////////////////////////////////////

      if constexpr (std::is_same<node_t, FunctionDef2>::value) {
        auto as_fn2 = std::dynamic_pointer_cast<FunctionDef2>(n);
        _mangleFunctionDef2(slp, as_fn2, the_name);
      } else if constexpr (std::is_same<node_t, FunctionDef1>::value) {
        OrkAssert(false);
      }

      std::string mangled_name;

      if (n->hasKey("mangled_name")) {
        mangled_name = n->template typedValueForKey<std::string>("mangled_name").value();
      }

      ////////////////////////////////////////////////////////////

      auto it = outmap.find(the_name);
      if (it != outmap.end()) {
        if (mangled_name != "") {
          the_name = mangled_name;
          it       = outmap.find(the_name);
        } else {
          logerrchannel()->log("duplicate named object<%s> mangled_name<%s>", the_name.c_str(), mangled_name.c_str());
          OrkAssert(false);
        }
      }
      outmap[the_name] = n;
      if constexpr (std::is_same<node_t, FragmentShader>::value) {
        // printf("found fragshader<%s>\n", the_name.c_str());
        //  OrkAssert(false);
      } else if constexpr (std::is_same<node_t, LibraryBlock>::value) {
        // printf("found libblock<%s>\n", the_name.c_str());
        // OrkAssert(false);
      } else if constexpr (std::is_same<node_t, UniformSet>::value) {
        // printf("found uniset<%s>\n", the_name.c_str());
      }
      n->template setValueForKey<std::string>("object_name", the_name);
    } else {
      // OrkAssert(false);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

template <typename node_t> //
void _semaProcNamedOfType(
    impl::ShadLangParser* slp, //
    astnode_ptr_t top) {       //

  auto nodes = slp->collectNodesOfType<node_t>(top);
  for (auto n : nodes) {
    astnode_ptr_t obj_name;
    AstNode::walkDownAST(n, [&](astnode_ptr_t node) -> bool {
      auto as_objname = std::dynamic_pointer_cast<ObjectName>(node);
      // printf( "walkdown<%s> as_objname<%s>\n", node->_name.c_str(), as_objname ? "true" : "false" );
      if (as_objname) {
        obj_name = as_objname;
        return false;
      }
      return true;
    });
    if (obj_name) {
      auto the_name = obj_name->_name;
      n->template setValueForKey<std::string>("object_name", the_name);
    } else {
      // OrkAssert(false);
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaPerformImports(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<ImportDirective>(top);
  import_map_t import_map;
  for (auto n : nodes) {
    //
    file::Path::NameType a, b;
    file::Path proc_import_path;
    //
    auto raw_import_path = n->template typedValueForKey<std::string>("object_name").value();
    printf("Import RawPath<%s>\n", raw_import_path.c_str());

    ////////////////////////////////////////////////////
    // if string has enclosing quotes, remove them
    ////////////////////////////////////////////////////

    if (raw_import_path.front() == '"')
      raw_import_path.erase(0, 1);
    if (raw_import_path.back() == '"')
      raw_import_path.pop_back();

    ////////////////////////////////////////////////////

    auto rpath = file::Path(raw_import_path);
    rpath.split(a, b, ':');
    if (b.length() != 0) { // use from import
      proc_import_path = rpath;
      printf("Import ProcPath1<%s>\n", proc_import_path.c_str());
    } else { // infer from container
      proc_import_path = slp->_shader_path;
      printf("Import ProcPath2<%s>\n", proc_import_path.c_str());
      proc_import_path.split(a, b, ':');
      ork::FixedString<256> fxs;
      fxs.format("%s://%s", a.c_str(), rpath.c_str());
      proc_import_path = fxs.c_str();
      printf("Import ProcPath3<%s>\n", proc_import_path.c_str());
      // OrkAssert(false);
    }

    auto sub_tunit = shadlang::parseFromFile(proc_import_path);
    OrkAssert(sub_tunit);
    import_map[proc_import_path.c_str()] = sub_tunit;
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNamePrimaryIdentifers(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<PrimaryIdentifier>(top);
  for (auto prim_node : nodes) {
    auto match       = slp->matchForAstNode(prim_node);
    auto seq         = match->asShared<Sequence>();
    auto cm          = seq->itemAsShared<ClassMatch>(0);
    auto name        = cm->_token->text;
    prim_node->_name = FormatString("PID: %s", name.c_str());
    prim_node->setValueForKey<std::string>("identifier_name", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameMemberAccessOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<MemberAccessOperator>(top);
  for (auto mao_node : nodes) {
    auto match      = slp->matchForAstNode(mao_node);
    auto seq        = match->asShared<Sequence>();
    auto cm         = seq->itemAsShared<ClassMatch>(1);
    auto name       = cm->_token->text;
    mao_node->_name = FormatString("MemberAccess: %s", name.c_str());
    mao_node->setValueForKey<std::string>("member_name", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameAdditiveOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<AdditiveOperator>(top);
  for (auto ao_node : nodes) {
    auto match     = slp->matchForAstNode(ao_node);
    auto sel       = match->asShared<OneOf>()->_selected;
    auto cm        = sel->asShared<ClassMatch>();
    auto name      = cm->_token->text;
    ao_node->_name = FormatString("AdditiveOperator: %s", name.c_str());
    ao_node->setValueForKey<std::string>("operator", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameMultiplicativeOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<MultiplicativeOperator>(top);
  for (auto ao_node : nodes) {
    auto match     = slp->matchForAstNode(ao_node);
    auto sel       = match->asShared<OneOf>()->_selected;
    auto cm        = sel->asShared<ClassMatch>();
    auto name      = cm->_token->text;
    ao_node->_name = FormatString("MultiplicativeOperator: %s", name.c_str());
    ao_node->setValueForKey<std::string>("operator", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameRelationalOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<RelationalOperator>(top);
  for (auto ao_node : nodes) {
    auto match     = slp->matchForAstNode(ao_node);
    auto sel       = match->asShared<OneOf>()->_selected;
    auto cm        = sel->asShared<ClassMatch>();
    auto name      = cm->_token->text;
    ao_node->_name = FormatString("RelationalOperator: %s", name.c_str());
    ao_node->setValueForKey<std::string>("operator", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameAssignmentOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<AssignmentOperator>(top);
  for (auto ao_node : nodes) {
    auto match     = slp->matchForAstNode(ao_node);
    auto sel       = match->asShared<OneOf>()->_selected;
    auto cm        = sel->asShared<ClassMatch>();
    auto name      = cm->_token->text;
    ao_node->_name = FormatString("AssignmentOperator: %s", name.c_str());
    ao_node->setValueForKey<std::string>("operator", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameInheritListItems(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<InheritListItem>(top);
  for (auto ili_node : nodes) {
    auto match      = slp->matchForAstNode(ili_node);
    auto seq        = match->asShared<Sequence>();
    auto cm         = seq->itemAsShared<ClassMatch>(1);
    auto name       = cm->_token->text;
    ili_node->_name = FormatString("Inherits: %s", name.c_str());
    ili_node->setValueForKey<std::string>("inherited_object", name);
    // printf("inh_name set<%s>\n", name.c_str());
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaResolvePostfixExpressions(impl::ShadLangParser* slp, astnode_ptr_t top) {
  //////////////////////////////////////////
  struct PfxToResolve {
    match_ptr_t _pfx;
    match_ptr_t _pid;
    match_ptr_t _parens;
    match_ptr_t _memberacc;
  };
  //////////////////////////////////////////
  auto LITERAL           = slp->findMatcherByName("Literal");
  auto RVC_EXPRESSION    = slp->findMatcherByName("RValueConstructor");
  auto PARENS_EXPRESSION = slp->findMatcherByName("ParensExpression");
  auto IDENTIFIER        = slp->findMatcherByName("IDENTIFIER");
  auto PRIMARY_IDENTIFER = slp->findMatcherByName("PrimaryIdentifier");
  auto MEMBER_ACCESS     = slp->findMatcherByName("MemberAccess");
  auto DOT               = slp->findMatcherByName("MemberDot");

  auto PRVC = slp->findMatcherByName("PrimaryRvalueConstructor");
  auto PPAR = slp->findMatcherByName("PrimaryParensExpression");
  auto PLIT = slp->findMatcherByName("PrimaryLiteral");

  auto nodes = slp->collectNodesOfType<PostfixExpression>(top);
  std::vector<PfxToResolve> resolve_list;
  //////////////////////////////////////////
  for (auto pfx_node : nodes) {
    auto match = slp->matchForAstNode(pfx_node);

    auto seq         = match->asShared<Sequence>();
    auto match_tails = seq->itemAsShared<NOrMore>(1);
    if (match_tails->_items.size() == 1) {

      auto match_primary = Match::followThroughProxy(seq->_items[0]);
      match_primary      = match_primary->asShared<OneOf>()->_selected;

      auto child_literal = match_primary->findFirstDescendanttWithMatcher(LITERAL);
      auto child_rvc     = match_primary->findFirstDescendanttWithMatcher(RVC_EXPRESSION);
      auto child_parens  = match_primary->findFirstDescendanttWithMatcher(PARENS_EXPRESSION);
      auto child_ident   = match_primary->findFirstDescendanttWithMatcher(IDENTIFIER);

      /////////////////////////////////////////////////////////////
      // descend into PostfixExpressionTail.[ ParensExpression ]
      /////////////////////////////////////////////////////////////

      auto match_pfx_tail    = Match::followThroughProxy(match_tails->_items[0]);
      match_pfx_tail         = match_pfx_tail->asShared<OneOf>()->_selected;
      auto match_pfxtail_seq = match_pfx_tail->asShared<Sequence>();

      size_t index = resolve_list.size();

      printf("XXX<%zu:%s>\n", index, match_pfxtail_seq->_items[0]->_matcher->_name.c_str());

      if (match_primary->_matcher == PRVC) {
        OrkAssert(false);
      } else if (match_pfxtail_seq->_items[0]->_matcher == RVC_EXPRESSION) {
        PfxToResolve res;
        res._pfx    = match;
        res._pid    = match_primary;
        res._parens = match_pfxtail_seq->_items[0];
        resolve_list.push_back(res);
        OrkAssert(false);
      } else if (match_pfxtail_seq->_items[0]->_matcher == PARENS_EXPRESSION) {
        PfxToResolve res;
        res._pfx    = match;
        res._pid    = match_primary;
        res._parens = match_pfxtail_seq->_items[0];
        resolve_list.push_back(res);
      } else if (match_pfxtail_seq->_items[0]->_matcher == DOT) {
        PfxToResolve res;
        res._pfx       = match;
        res._pid       = match_primary;
        res._memberacc = match_pfx_tail;
        resolve_list.push_back(res);
      } else {
        printf("// CANNOT RESOLVE ///////////////////\n");
        match_pfx_tail->dump1(0);
        printf("/////////////////////////////////////\n");
      }

      /////////////////////////////////////////////////////////////
    }
  }
  //////////////////////////////////////////
  for (size_t index = 0; index < resolve_list.size(); index++) {
    auto item = resolve_list[index];
    // printf( "// RESOLVE ///////////////////\n");
    // item._pfx->dump1(0);
    // printf( "//////////////////////////////\n");
    // item._pid->dump1(0);
    // printf( "// branchDistance<%zx>\n", Match::implDistance(item._pfx, item._pid) );
    // printf( "//////////////////////////////\n");
    auto ast_pfx = slp->astNodeForMatch(item._pfx);
    auto ast_pid = slp->astNodeForMatch(item._pid);
    if (nullptr == ast_pfx) {
      OrkAssert(false);
      continue;
    }
    if (nullptr == ast_pid) {
      printf("no AST for match %zu\n", index);
      OrkAssert(false);
      continue;
    }

    if (item._parens) {
      auto pid_name      = ast_pid->typedValueForKey<std::string>("identifier_name").value();
      auto ast_parens    = slp->astNodeForMatch(item._parens);
      auto match_primary = slp->matchForAstNode(ast_pid);
      if (match_primary->_matcher == PRIMARY_IDENTIFER) {
        auto func_inv      = std::make_shared<SemaFunctionInvokation>();
        auto func_inv_args = std::make_shared<SemaFunctionArguments>();
        auto func_name     = std::make_shared<SemaFunctionName>();
        func_name->_name   = FormatString("SemaFunctionName: %s", pid_name.c_str());
        func_inv->_children.push_back(func_name);
        func_inv->_children.push_back(func_inv_args);
        //////////////////////////////////////////////////
        auto expr_list = std::dynamic_pointer_cast<ExpressionList>(ast_parens->_children[0]);
        func_inv_args->_children.push_back(expr_list ? expr_list : ast_parens);
        //////////////////////////////////////////////////
        slp->replaceInParent(ast_pfx, func_inv);
      }
    } else if (item._memberacc) {
      item._pid->dump1(0);
      auto pid_name    = ast_pid->typedValueForKey<std::string>("identifier_name").value();
      auto ast_membacc = slp->astNodeForMatch(item._memberacc);
      auto member_name = ast_membacc->typedValueForKey<std::string>("member_name").value();
      // item._memberacc->dump1(0);
      auto maexp   = std::make_shared<SemaMemberAccess>();
      maexp->_name = FormatString("SemaMemberAccess: %s.%s", pid_name.c_str(), member_name.c_str());
      slp->replaceInParent(ast_pfx, maexp);
    } else {
      OrkAssert(false);
    }
  }
  //////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaResolveSemaFunctionArguments(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<SemaFunctionArguments>(top);
  for (auto n : nodes) {
    auto expr_list = std::dynamic_pointer_cast<ExpressionList>(n->_children[0]);
    if (expr_list) {
      n->_children.clear();
      n->_children = expr_list->_children;
      expr_list->_children.clear();
    } else {
      OrkAssert(n->_children[0]->_children.size() == 1);
      n->_children = n->_children[0]->_children;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaResolveConstructors(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<RValueConstructor>(top);
  for (auto n : nodes) {
    //
    auto dtype_node = std::dynamic_pointer_cast<DataType>(n->_children[0]);
    auto parens     = std::dynamic_pointer_cast<ParensExpression>(n->_children[1]);
    OrkAssert(dtype_node);
    OrkAssert(parens);
    auto dtype_name = dtype_node->typedValueForKey<std::string>("data_type").value();
    //
    auto constructor_call = std::make_shared<SemaConstructorInvokation>();
    auto cons_type        = std::make_shared<SemaConstructorType>();
    auto cons_args        = std::make_shared<SemaConstructorArguments>();
    constructor_call->_children.push_back(cons_type);
    constructor_call->_children.push_back(cons_args);
    //
    cons_type->_name = FormatString("SemaConstructorType: %s", dtype_name.c_str());
    constructor_call->setValueForKey<std::string>("data_type", dtype_name);
    //
    auto expr_list = std::dynamic_pointer_cast<ExpressionList>(parens->_children[0]);
    if (expr_list) {
      cons_args->_children = expr_list->_children;
    } else {
      cons_args->_children = parens->_children[0]->_children;
    }
    slp->replaceInParent(n, constructor_call);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

template <typename node_t> //
int _semaProcessInheritances(
    impl::ShadLangParser* slp, //
    astnode_ptr_t top) {       //
  int count  = 0;
  auto nodes = slp->collectNodesOfType<node_t>(top);
  for (auto n : nodes) {
    astnode_ptr_t inh_item;
    auto objname = n->template typedValueForKey<std::string>("object_name").value();
    /////////////////////////////////
    auto check_inheritance = [](std::string inh_name, SHAST::astnode_map_t& in_map) -> bool { //
      auto it = in_map.find(inh_name);
      return (it != in_map.end());
    };
    /////////////////////////////////
    AstNode::walkDownAST(n, [&](astnode_ptr_t node) -> bool {
      auto as_inh_item = std::dynamic_pointer_cast<InheritListItem>(node);
      if (as_inh_item) {
        inh_item      = as_inh_item;
        auto inh_name = inh_item->typedValueForKey<std::string>("inherited_object").value();
        // printf("%s<%s> inh_name<%s>\n", n->_name.c_str(), objname.c_str(), inh_name.c_str());
        /////////////////////////////////
        // check if extension
        /////////////////////////////////
        if (inh_name == "extension") {
          auto inh_match = slp->matchForAstNode(inh_item);
          // inh_match->dump1(0);
          auto ext_id    = inh_match->traverseDownPath("InheritListItem.sub2.sub/IDENTIFIER");
          auto id_name   = ext_id->tryAsShared<ClassMatch>().value()->_token->text;
          auto semalib   = std::make_shared<SemaInheritExtension>();
          semalib->_name = FormatString("SemaInheritExtension\n%s", id_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
          return false;
        }
        /////////////////////////////////
        bool check_lib_blocks  = false;
        bool check_uni_sets    = false;
        bool check_uni_blks    = false;
        bool check_vtx_iface   = false;
        bool check_geo_iface   = false;
        bool check_frg_iface   = false;
        bool check_com_iface   = false;
        bool check_stateblocks = false;
        /////////////////////////////////
        // LibraryBlocks
        /////////////////////////////////
        if constexpr (std::is_same<node_t, LibraryBlock>::value) {
          check_lib_blocks = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
        }
        /////////////////////////////////
        // VertexShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, VertexShader>::value) {
          check_lib_blocks = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_vtx_iface  = true;
        }
        /////////////////////////////////
        // GeometryShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, GeometryShader>::value) {
          check_lib_blocks = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_vtx_iface  = true;
          check_geo_iface  = true;
        }
        /////////////////////////////////
        // FragmentShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, FragmentShader>::value) {
          check_lib_blocks = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_vtx_iface  = true;
          check_geo_iface  = true;
          check_frg_iface  = true;
        }
        /////////////////////////////////
        // FragmentShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, ComputeShader>::value) {
          check_lib_blocks = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_com_iface  = true;
        }
        /////////////////////////////////
        // PipelineInterfaces
        /////////////////////////////////
        else if constexpr (std::is_base_of<PipelineInterface, node_t>::value) {
          check_uni_sets  = true;
          check_uni_blks  = true;
          check_vtx_iface = true;
          check_geo_iface = true;
          check_frg_iface = true;
          check_com_iface = true;
        }
        /////////////////////////////////
        // StateBlocks
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, StateBlock>::value) {
          check_stateblocks = true;
        }
        /////////////////////////////////
        if (check_lib_blocks and check_inheritance(inh_name, slp->_library_blocks)) {
          auto semalib   = std::make_shared<SemaInheritLibrary>();
          semalib->_name = FormatString("SemaInheritLibrary: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
        } else if (check_uni_sets and check_inheritance(inh_name, slp->_uniform_sets)) {
          auto semalib   = std::make_shared<SemaInheritUniformSet>();
          semalib->_name = FormatString("SemaInheritUniformSet: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
        } else if (check_uni_blks and check_inheritance(inh_name, slp->_uniform_blocks)) {
          auto semalib   = std::make_shared<SemaInheritUniformBlock>();
          semalib->_name = FormatString("SemaInheritUniformBlock: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
        } else if (check_vtx_iface and check_inheritance(inh_name, slp->_vertex_interfaces)) {
          auto semalib   = std::make_shared<SemaInheritVertexInterface>();
          semalib->_name = FormatString("SemaInheritVertexInterface: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
        } else if (check_geo_iface and check_inheritance(inh_name, slp->_geometry_interfaces)) {
          auto semalib   = std::make_shared<SemaInheritGeometryInterface>();
          semalib->_name = FormatString("SemaInheritGeometryInterface: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
        } else if (check_frg_iface and check_inheritance(inh_name, slp->_fragment_interfaces)) {
          auto semalib   = std::make_shared<SemaInheritFragmentInterface>();
          semalib->_name = FormatString("SemaInheritFragmentInterface: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
        } else if (check_com_iface and check_inheritance(inh_name, slp->_compute_interfaces)) {
          auto semalib   = std::make_shared<SemaInheritComputeInterface>();
          semalib->_name = FormatString("SemaInheritComputeInterface: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
        } else if (check_stateblocks and check_inheritance(inh_name, slp->_stateblocks)) {
          auto semalib   = std::make_shared<SemaInheritStateBlock>();
          semalib->_name = FormatString("SemaInheritStateBlock: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semalib);
          count++;
        }
      } // if (as_inh_item) {
      return true;
    });
  }
  return count;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

template <typename node_t> void _semaMoveNames(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<node_t>(top);
  for (auto tnode : nodes) {

    if constexpr (std::is_same<node_t, ImportDirective>::value) {
      // OrkAssert(false);
    }

    if (auto as_objname = tnode->template typedValueForKey<std::string>("object_name")) {
      auto objname = as_objname.value();
      if (not objname.empty()) {
        auto child = tnode->template findFirstChildOfType<ObjectName>();
        if (child) {
          slp->removeFromParent(child);
        }
        tnode->_name += "\n" + objname;
      }
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaIntegerLiterals(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<IntegerLiteral>(top);
  for (auto node : nodes) {
    auto match = slp->matchForAstNode(node);
    auto cm    = match->asShared<ClassMatch>();
    auto literal_value  = cm->_token->text;
    auto sema_node = std::make_shared<SemaIntegerLiteral>();
    sema_node->setValueForKey<std::string>("literal_value", literal_value);
    slp->replaceInParent(node, sema_node);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaFloatLiterals(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<FloatLiteral>(top);
  for (auto node : nodes) {
    auto match = slp->matchForAstNode(node);
    auto cm    = match->asShared<ClassMatch>();
    auto literal_value  = cm->_token->text;
    auto sema_node = std::make_shared<SemaFloatLiteral>();
    sema_node->setValueForKey<std::string>("literal_value", literal_value);
    slp->replaceInParent(node, sema_node);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::semaAST(astnode_ptr_t top) {

  //////////////////////////////////

  _semaNameBuiltInDataTypes(this, top);
  _semaNormalizeDtUserTypes(this, top);
  _semaNameTypedIdentifers(this, top);

  //////////////////////////////////

  _semaIntegerLiterals(this, top);
  _semaFloatLiterals(this, top);

  //////////////////////////////////
  // Pass 1 : Build Symbol Tables
  //////////////////////////////////

  _semaCollectNamedOfType<VertexInterface>(this, top, _vertex_interfaces);
  _semaCollectNamedOfType<FragmentInterface>(this, top, _fragment_interfaces);
  _semaCollectNamedOfType<GeometryInterface>(this, top, _geometry_interfaces);
  _semaCollectNamedOfType<ComputeInterface>(this, top, _compute_interfaces);

  _semaCollectNamedOfType<VertexShader>(this, top, _vertex_shaders);
  _semaCollectNamedOfType<FragmentShader>(this, top, _fragment_shaders);
  _semaCollectNamedOfType<GeometryShader>(this, top, _geometry_shaders);
  _semaCollectNamedOfType<ComputeShader>(this, top, _compute_shaders);

  _semaCollectNamedOfType<UniformSet>(this, top, _uniform_sets);
  _semaCollectNamedOfType<UniformBlk>(this, top, _uniform_blocks);
  _semaCollectNamedOfType<LibraryBlock>(this, top, _library_blocks);

  _semaCollectNamedOfType<StructDecl>(this, top, _structs);
  _semaCollectNamedOfType<StateBlock>(this, top, _stateblocks);
  _semaCollectNamedOfType<Technique>(this, top, _techniques);

  _semaCollectNamedOfType<FunctionDef1>(this, top, _fndef1s);
  _semaCollectNamedOfType<FunctionDef2>(this, top, _fndef2s);

  _semaCollectNamedOfType<FxConfigDecl>(this, top, _fxconfig_decls);
  _semaCollectNamedOfType<ImportDirective>(this, top, _import_directives);

  _semaProcNamedOfType<FxConfigRef>(this, top);
  _semaProcNamedOfType<Pass>(this, top);

  _semaMoveNames<Translatable>(this, top);
  _semaMoveNames<FxConfigRef>(this, top);
  _semaMoveNames<Pass>(this, top);
  _semaMoveNames<ImportDirective>(this, top);

  //////////////////////////////////
  // Pass 2 - Imports
  //////////////////////////////////

  _semaPerformImports(this, top);

  //////////////////////////////////
  // Pass 3
  //////////////////////////////////

  _semaNamePrimaryIdentifers(this, top);
  _semaNameMemberAccessOperators(this, top);
  _semaNameAdditiveOperators(this, top);
  _semaNameMultiplicativeOperators(this, top);
  _semaNameRelationalOperators(this, top);
  _semaNameAssignmentOperators(this, top);
  _semaNameInheritListItems(this, top);
  //_semaResolvePostfixExpressions(this, top);
  _semaResolveSemaFunctionArguments(this, top);
  _semaResolveConstructors(this, top);

  //////////////////////////////////
  // Pass 4..
  //////////////////////////////////

  bool keep_going = false;
  while (keep_going) {
    int count = 0;
    count += _semaProcessInheritances<LibraryBlock>(this, top);

    count += _semaProcessInheritances<VertexInterface>(this, top);
    count += _semaProcessInheritances<GeometryInterface>(this, top);
    count += _semaProcessInheritances<FragmentInterface>(this, top);
    count += _semaProcessInheritances<ComputeInterface>(this, top);

    count += _semaProcessInheritances<VertexShader>(this, top);
    count += _semaProcessInheritances<FragmentShader>(this, top);
    count += _semaProcessInheritances<GeometryShader>(this, top);
    count += _semaProcessInheritances<ComputeShader>(this, top);

    count += _semaProcessInheritances<StateBlock>(this, top);
    keep_going = (count > 0);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif
