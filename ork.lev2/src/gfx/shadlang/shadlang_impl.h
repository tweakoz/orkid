////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/shadlang.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/pch.h>
#include <ork/util/crc.h>
#include <regex>
#include <stdlib.h>
#include <ork/util/logger.h>
#include <ork/kernel/string/string.h>
#include <ork/util/parser.inl>

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////

void SHAST::_dumpAstTreeVisitor( //
    SHAST::astnode_ptr_t node,   //
    int indent,                  //
    std::string& out_str);

std::string toASTstring(SHAST::astnode_ptr_t node);

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace impl {
///////////////////////////////////////////////////////////////////////////////
struct ShadLangParser : public Parser {

  ShadLangParser();
  ////////////////////////////////////////////
  SHAST::translationunit_ptr_t parseString(std::string parse_str);
  ////////////////////////////////////////////
  void preDeclareAstNodes();
  void declareAstNodes();
  void defineAstHandlers();
  ////////////////////////////////////////////
  void pruneAST(SHAST::astnode_ptr_t top);
  void reduceAST(std::set<SHAST::astnode_ptr_t> nodes_to_reduce);
  void semaAST(SHAST::astnode_ptr_t top);
  bool walkUpAST(                 //
    SHAST::astnode_ptr_t node,   //
    SHAST::walk_visitor_fn_t visitor);
  ////////////////////////////////////////////
  SHAST::astnode_ptr_t astNodeForMatch(match_ptr_t match) const;
  match_ptr_t matchForAstNode(SHAST::astnode_ptr_t astnode) const;
  ////////////////////////////////////////////
  template <typename T> std::shared_ptr<T> ast_create(match_ptr_t m) {
    auto sh = std::make_shared<T>();
    m->_uservars.set<SHAST::astnode_ptr_t>("astnode", sh);
    _match2astnode[m] = sh;
    _astnode2match[sh] = m;
    return sh;
  }
  ////////////////////////////////////////////
  template <typename T> //
  std::shared_ptr<T> ast_get(match_ptr_t m) {
    auto sh = m->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();
    return std::dynamic_pointer_cast<T>(sh);
  }
  ////////////////////////////////////////////
  template <typename T> //
  std::shared_ptr<T> try_ast_get(match_ptr_t m) {
    auto sh = m->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode");
    if (sh) {
      return std::dynamic_pointer_cast<T>(sh.value());
    }
    return nullptr;
  }
  ////////////////////////////////////////////
  void _buildAstTreeVisitor(match_ptr_t the_match);
  ////////////////////////////////////////////
  void replaceInParent(SHAST::astnode_ptr_t oldnode, SHAST::astnode_ptr_t newnode);
  void removeFromParent(SHAST::astnode_ptr_t node);
  ////////////////////////////////////////////
  template <typename T> void importTranslatable(std::string name, //
                                                SHAST::translatable_ptr_t translatable, //
                                                SHAST::astnode_map_t& the_map){ //
    auto it = the_map.find(name);
    OrkAssert(it==the_map.end());
    the_map[name] = translatable;
    auto it2 = _translatables.find(name);
    OrkAssert(it2==_translatables.end());
    _translatables[name] = translatable;
  }
  ////////////////////////////////////////////
  matcher_ptr_t _tu_matcher;
  std::vector<SHAST::astnode_ptr_t> _astnodestack;
  std::unordered_map<match_ptr_t, SHAST::astnode_ptr_t> _match2astnode;
  std::unordered_map<SHAST::astnode_ptr_t, match_ptr_t> _astnode2match;
  SHAST::astnode_map_t _symboltable;
  SHAST::astnode_map_t _translatables;

  SHAST::astnode_map_t _uniform_sets;
  SHAST::astnode_map_t _uniform_blocks;
  SHAST::astnode_map_t _library_blocks;

  SHAST::astnode_map_t _vertex_interfaces;
  SHAST::astnode_map_t _fragment_interfaces;
  SHAST::astnode_map_t _geometry_interfaces;
  SHAST::astnode_map_t _compute_interfaces;

  SHAST::astnode_map_t _vertex_shaders;
  SHAST::astnode_map_t _fragment_shaders;
  SHAST::astnode_map_t _geometry_shaders;
  SHAST::astnode_map_t _compute_shaders;

  SHAST::astnode_map_t _stateblocks;
  SHAST::astnode_map_t _techniques;

  SHAST::astnode_map_t _fndef1s;
  SHAST::astnode_map_t _fndef2s;

  SHAST::astnode_map_t _fxconfig_decls;
  SHAST::astnode_map_t _import_directives;

  SHAST::astnode_map_t _structs;

  file::Path _shader_path;

}; // struct ShadLangParser

} // namespace impl

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#define DECLARE_STD_AST_NODE(x) onPre(#x, [=](match_ptr_t match) { auto ast_node = ast_create<SHAST::x>(match); });

#define DECLARE_OBJNAME_AST_NODE(x)                                                                                                \
  onPre(x, [=](match_ptr_t match) {                                                                                                \
    auto objname     = ast_create<SHAST::ObjectName>(match);                                                                       \
    auto fn_name_seq = match->asShared<Sequence>();                                                                                \
    auto fn_name     = fn_name_seq->_items[0]->followImplAsShared<ClassMatch>();                                                   \
    objname->_name   = fn_name->_token->text;                                                                                      \
  });

#endif