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
namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////
using namespace SHAST;

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

void _semaNameDataTypes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<DataType>(top);
  for (auto dt_node : nodes) {
    auto match      = slp->matchForAstNode(dt_node);
    auto seq        = match->asShared<Sequence>();
    auto sel        = seq->itemAsShared<OneOf>(2)->_selected;
    auto cm         = sel->asShared<ClassMatch>();
    auto name       = cm->_token->text;
    dt_node->_name = FormatString("DataType: %s", name.c_str());
    dt_node->setValueForKey<std::string>("data_type", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameDataUserTypes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = slp->collectNodesOfType<DataTypeWithUserTypes>(top);
  for (auto dtype_node : nodes) {
    auto match = slp->matchForAstNode(dtype_node);
    auto sel   = match->asShared<OneOf>()->_selected;
    if (auto as_cm = sel->tryAsShared<ClassMatch>()) {
      auto name         = as_cm.value()->_token->text;
      dtype_node->_name = FormatString("DTWU: %s", name.c_str());
    } else { // its a DataTypeNode
      auto seq          = sel->asShared<Sequence>();
      auto sel2         = seq->itemAsShared<OneOf>(2)->_selected;
      auto cm           = sel2->asShared<ClassMatch>();
      auto name         = cm->_token->text;
      dtype_node->_name = FormatString("DTWU: %s", name.c_str());
    }
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
    } else { // its a DataTypeNode
      auto seq  = sel->asShared<Sequence>();
      auto sel2 = seq->itemAsShared<OneOf>(2)->_selected;
      auto cm   = sel2->asShared<ClassMatch>();
      auto name = cm->_token->text;
      tid_node->_name += name;
    }

    ////////////////////
    // item 1 (identifier)
    ////////////////////

    auto cm1 = seq->itemAsShared<ClassMatch>(1);
    tid_node->_name += " " + cm1->_token->text;
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
  auto PARENS_EXPRESSION = slp->findMatcherByName("ParensExpression");
  auto PRIMARY_IDENTIFER = slp->findMatcherByName("PrimaryIdentifier");
  auto MEMBER_ACCESS     = slp->findMatcherByName("MemberAccess");
  auto DOT               = slp->findMatcherByName("DOT");
  auto nodes             = slp->collectNodesOfType<PostfixExpression>(top);
  std::vector<PfxToResolve> resolve_list;
  //////////////////////////////////////////
  for (auto pfx_node : nodes) {
    auto match       = slp->matchForAstNode(pfx_node);
    auto seq         = match->asShared<Sequence>();
    auto match_tails = seq->itemAsShared<NOrMore>(1);
    if (match_tails->_items.size() == 1) {

      auto match_primary = Match::followThroughProxy(seq->_items[0]);
      match_primary      = match_primary->asShared<OneOf>()->_selected;

      /////////////////////////////////////////////////////////////
      // descend into PostfixExpressionTail.[ ParensExpression ]
      /////////////////////////////////////////////////////////////

      auto match_pfx_tail    = Match::followThroughProxy(match_tails->_items[0]);
      match_pfx_tail         = match_pfx_tail->asShared<OneOf>()->_selected;
      auto match_pfxtail_seq = match_pfx_tail->asShared<Sequence>();

      if (match_pfxtail_seq->_items[0]->_matcher == PARENS_EXPRESSION) {
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
        match_pfx_tail->dump1(0);
        //OrkAssert(false);
      }

      /////////////////////////////////////////////////////////////
    }
  }
  //////////////////////////////////////////
  for (auto item : resolve_list) {
    auto ast_pfx  = slp->astNodeForMatch(item._pfx);
    auto ast_pid  = slp->astNodeForMatch(item._pid);
    auto pid_name = ast_pid->typedValueForKey<std::string>("identifier_name");
    if (item._parens) {
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
        AstNode::replaceInParent(ast_pfx, func_inv);
      }
    } else if (item._memberacc) {
      auto ast_membacc = slp->astNodeForMatch(item._memberacc);
      auto member_name = ast_membacc->typedValueForKey<std::string>("member_name");
      item._memberacc->dump1(0);
      auto maexp   = std::make_shared<SemaMemberAccess>();
      maexp->_name = FormatString("SemaMemberAccess: %s.%s", pid_name.c_str(), member_name.c_str());
      AstNode::replaceInParent(ast_pfx, maexp);
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
    auto parens = std::dynamic_pointer_cast<ParensExpression>(n->_children[1]);
    OrkAssert(dtype_node);
    OrkAssert(parens);
    auto dtype_name = dtype_node->typedValueForKey<std::string>("data_type");
    //
    auto constructor_call = std::make_shared<SemaConstructorInvokation>();
    auto cons_type = std::make_shared<SemaConstructorType>();
    auto cons_args = std::make_shared<SemaConstructorArguments>();
    constructor_call->_children.push_back(cons_type);
    constructor_call->_children.push_back(cons_args);
    //
    cons_type->_name = FormatString( "SemaConstructorType: %s", dtype_name.c_str() );
    //
    auto expr_list = std::dynamic_pointer_cast<ExpressionList>(parens->_children[0]);
    if (expr_list) {
       cons_args->_children = expr_list->_children;
    } else {
      cons_args->_children = parens->_children[0]->_children;
    }
    AstNode::replaceInParent(n, constructor_call);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::semaAST(astnode_ptr_t top) {
  _semaNamePrimaryIdentifers(this, top);
  _semaNameMemberAccessOperators(this, top);
  _semaNameDataTypes(this, top);
  _semaNameDataUserTypes(this, top);
  _semaNameTypedIdentifers(this, top);
  _semaNameAdditiveOperators(this, top);
  _semaNameMultiplicativeOperators(this, top);
  _semaNameAssignmentOperators(this, top);
  _semaNameInheritListItems(this, top);
  _semaResolvePostfixExpressions(this, top);
  _semaResolveSemaFunctionArguments(this, top);
  _semaResolveConstructors(this, top);
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif
