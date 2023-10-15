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

// TODO - flyweighted import

#if defined(USE_ORKSL_LANG)

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::shadlang {
/////////////////////////////////////////////////////////////////////////////////////////////////
using namespace SHAST;

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

/////////////////////////////////////////////////////////////////////////////////////////////////

std::string _dt_extract_type(dt_ptr_t dt_node, match_ptr_t dt_match) {
  auto seq     = dt_match->asShared<Sequence>();
  auto _inp    = seq->itemAsShared<Optional>(0)->_subitem;
  auto _const  = seq->itemAsShared<Optional>(1)->_subitem;
  auto dt_name = seq->itemAsShared<OneOf>(2)->_selected;
  auto dt_cm   = dt_name->asShared<ClassMatch>();

  auto type_name = dt_cm->_token->text;
  dt_node->setValueForKey<bool>("has_attr_inp", (_inp != nullptr));
  dt_node->setValueForKey<bool>("has_attr_const", (_const != nullptr));
  dt_node->setValueForKey<std::string>("base_type", type_name);
  if (_inp) {
    type_name = "in " + type_name;
  }
  if (_const) {
    type_name = "const " + type_name;
  } else {
  }

  return type_name;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNormalizeDtUserTypes(impl::ShadLangParser* slp, astnode_ptr_t top) {

  auto matcher_dtype = slp->findMatcherByName("DataType");
  auto matcher_ident = slp->findMatcherByName("IDENTIFIER");
  auto nodes         = AstNode::collectNodesOfType<DataTypeWithUserTypes>(top);
  for (auto dtu_node : nodes) {
    auto dtu_match = slp->matchForAstNode(dtu_node);
    auto sel_match = dtu_match->asShared<OneOf>()->_selected;
    //////////////////////////////////////////
    // builtin datatype ?
    //////////////////////////////////////////
    if (sel_match->_matcher == matcher_dtype) {
      auto sel_ast                        = slp->astNodeForMatch(sel_match);
      auto sel_as_dt                      = std::dynamic_pointer_cast<DataType>(sel_ast);
      OrkAssert(sel_as_dt) auto type_name = _dt_extract_type(sel_as_dt, sel_match);
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
  auto nodes = AstNode::collectNodesOfType<DataType>(top);
  for (auto dt_node : nodes) {
    auto dt_match  = slp->matchForAstNode(dt_node);
    auto type_name = _dt_extract_type(dt_node, dt_match);
    _procdatatype(slp, dt_node, type_name, true);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameIdentifers(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<IDENTIFIER>(top);
  for (auto id_node : nodes) {
    auto match     = slp->matchForAstNode(id_node);
    auto sema_id   = slp->ast_create<SemaIdentifier>(match);
    sema_id->_name = "SemaId: ";
    auto cm1       = match->asShared<ClassMatch>();
    sema_id->_name += " " + cm1->_token->text;
    sema_id->setValueForKey<std::string>("identifier_name", cm1->_token->text);
    slp->replaceInParent(id_node, sema_id);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameIdentiferCalls(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<IdentifierCall>(top);
  for (auto id_node : nodes) {
    id_node->_name = "IDCALL: ";
    auto match     = slp->matchForAstNode(id_node);
    auto cm1       = match->asShared<ClassMatch>();
    id_node->_name += " " + cm1->_token->text;
    id_node->setValueForKey<std::string>("identifier_name", cm1->_token->text);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameTypedIdentifers(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<TypedIdentifier>(top);
  for (auto tid_node : nodes) {

    tid_node->_name = "TypedIdentifier";

    auto match = slp->matchForAstNode(tid_node);

    auto seq = match->asShared<Sequence>();

    ////////////////////
    // item 0 (type)
    ////////////////////

    auto sel = seq->itemAsShared<OneOf>(0)->_selected;
    std::string type_name;
    if (auto as_cm = sel->tryAsShared<ClassMatch>()) {
      type_name = as_cm.value()->_token->text;
    } else { // its a DataTypeNode
      auto seq  = sel->asShared<Sequence>();
      auto sel2 = seq->itemAsShared<OneOf>(2)->_selected;
      auto cm   = sel2->asShared<ClassMatch>();
      type_name = cm->_token->text;
    }

    // tid_node->_name += FormatString("type: %s\n", type_name.c_str());
    tid_node->setValueForKey<std::string>("data_type", type_name);

    ////////////////////
    // item 1 (identifier)
    ////////////////////

    auto cm1     = seq->itemAsShared<ClassMatch>(1);
    auto id_name = cm1->_token->text;
    // tid_node->_name += FormatString("id: %s", id_name.c_str());
    tid_node->setValueForKey<std::string>("identifier_name", id_name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _mangleFunctionDef2(
    impl::ShadLangParser* slp,              //
    std::shared_ptr<FunctionDef2> fn2_node, //
    std::string named) {                    //

  ORK_CONFIG_OPENGL(fn2_node);
  auto dt_node             = fn2_node->childAs<DataType>(0);
  auto decl_args           = fn2_node->findFirstChildOfType<DeclArgumentList>();
  std::string mangled_name = named;
  if (dt_node) {
    auto return_type = dt_node->typedValueForKey<std::string>("data_type").value();
    mangled_name += "<" + return_type + ">";
    //printf("mangle function<%s> return type: %s \n", named.c_str(), return_type.c_str());
  }
  if (decl_args) {
    mangled_name += "(";
    AstNode::visitChildren(decl_args, [&](astnode_ptr_t node) {
      auto tid_node = std::dynamic_pointer_cast<TypedIdentifier>(node);
      if (tid_node != nullptr) {
        auto arg_dt = tid_node->typedValueForKey<std::string>("data_type").value();
        mangled_name += arg_dt + ",";
      } else {
        dumpAstNode(node);
        OrkAssert(false);
      }
      // printf( "mangle walkdown - argsnode : %s\n", node->_name.c_str() );
    });
    mangled_name += ")";
    //printf("mangled_name<%s>\n", mangled_name.c_str());
    //////////////////////////////////////////////////////////////////////
    fn2_node->setValueForKey<std::string>("function_name", named);
    fn2_node->setValueForKey<std::string>("unmangled_name", named);
    fn2_node->setValueForKey<std::string>("mangled_name", mangled_name);
    //////////////////////////////////////////////////////////////////////
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

  auto nodes = AstNode::collectNodesOfType<node_t>(top);
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
        n->template setValueForKey<std::string>("raw_name", the_name);
      } else if constexpr (std::is_same<node_t, FunctionDef1>::value) {
        OrkAssert(false);
      } else if constexpr (std::is_same<node_t, ImportDirective>::value) {
        n->template setValueForKey<std::string>("import_path", the_name);
        n->template setValueForKey<std::string>("raw_name", the_name);
        the_name = FormatString("ImportDirective<%s>", the_name.c_str());
      }
      else{
        n->template setValueForKey<std::string>("raw_name", the_name);
      }

      std::string mangled_name;

      if (n->hasKey("mangled_name")) {
        mangled_name = n->template typedValueForKey<std::string>("mangled_name").value();
        the_name = mangled_name;
      }

      ////////////////////////////////////////////////////////////

      n->template setValueForKey<std::string>("object_name", the_name);

      auto it = outmap.find(the_name);
      if (it != outmap.end()) {
        logerrchannel()->log("A: duplicate named object<%s> mangled_name<%s>", the_name.c_str(), mangled_name.c_str());
        continue; 
      }

      outmap[the_name] = n;

      printf( "cache: objname: %s\n", the_name.c_str() );

      auto it2 = slp->_slp_cache->_translatables.find(the_name);
      if (it != slp->_slp_cache->_translatables.end()) {
         logerrchannel()->log("B: duplicate named object<%s> mangled_name<%s>", the_name.c_str(), mangled_name.c_str());
         OrkAssert(false);
      }

      slp->_slp_cache->_translatables[the_name] = n;

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

  auto nodes = AstNode::collectNodesOfType<node_t>(top);
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
  auto top_tunit = std::dynamic_pointer_cast<TranslationUnit>(top);
  auto nodes     = AstNode::collectNodesOfType<ImportDirective>(top);

  printf( "ShadLangParser<%p:%s> ImportCount<%zu>\n", //
          (void*) slp, //
          slp->_name.c_str(), //
          nodes.size() );

  for (auto import_node : nodes) {
    //
    file::Path::NameType a, b;
    file::Path proc_import_path;
    //
    auto raw_import_path = import_node->template typedValueForKey<std::string>("import_path").value();
    //printf("Import RawPath<%s>\n", raw_import_path.c_str());

    ////////////////////////////////////////////////////
    // if string has enclosing quotes, remove them
    ////////////////////////////////////////////////////

    if (raw_import_path.front() == '"')
      raw_import_path.erase(0, 1);
    if (raw_import_path.back() == '"')
      raw_import_path.pop_back();

    import_node->setValueForKey<std::string>("raw_import_path", raw_import_path);
    ////////////////////////////////////////////////////

    auto rpath = file::Path(raw_import_path);
    rpath.split(a, b, ':');
    if (b.length() != 0) { // use from import
      proc_import_path = rpath;
      //printf("Import ProcPath1<%s>\n", proc_import_path.c_str());
    } else { // infer from container
      proc_import_path = slp->_shader_path;
      //printf("Import ProcPath2<%s>\n", proc_import_path.c_str());
      proc_import_path.split(a, b, ':');
      ork::FixedString<256> fxs;
      fxs.format("%s://%s", a.c_str(), rpath.c_str());
      proc_import_path = fxs.c_str();
      //printf("Import ProcPath3<%s>\n", proc_import_path.c_str());
      // OrkAssert(false);
    }
    import_node->setValueForKey<std::string>("proc_import_path", proc_import_path.c_str());

    ////////////////////////////////////////////////////////
    // fetch translation unit
    ////////////////////////////////////////////////////////

    auto cache = slp->_slp_cache;
    translationunit_ptr_t sub_tunit;
    auto it_imp = cache->_import_cache.find(proc_import_path.c_str());
    // CACHED ?
    if( it_imp != cache->_import_cache.end() ) {
      sub_tunit = it_imp->second;
      printf("Parser<%s> Importing<%s> already cached\n", slp->_name.c_str(), proc_import_path.c_str());
      import_node->setValueForKey<transunit_ptr_t>("transunit", sub_tunit);
    }
    // NOT CACHED..
    else{
      printf("Parser<%s> Importing<%s>\n", slp->_name.c_str(), proc_import_path.c_str());
      sub_tunit = shadlang::parseFromFile(slp->_slp_cache, proc_import_path);
      OrkAssert(sub_tunit);
      cache->_import_cache[proc_import_path.c_str()] = sub_tunit;
      import_node->setValueForKey<transunit_ptr_t>("transunit", sub_tunit);
    }

    ////////////////////////////////////////////////////////
    // hoist translatables from sub tunit into parent tunit
    ////////////////////////////////////////////////////////

    if (1) { // inline imported translatables ?
      for (auto item : sub_tunit->_translatables_by_name) {
        auto name         = item.first;
        auto translatable = item.second;
        ////////////////////////////////////////////////////////////////////////////////////////
        if (auto as_lib_block = std::dynamic_pointer_cast<LibraryBlock>(translatable)) {
          slp->importTranslatable<LibraryBlock>(name, as_lib_block, slp->_slp_cache->_library_blocks);
        } 
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_typ_block = std::dynamic_pointer_cast<TypeBlock>(translatable)) {
          slp->importTranslatable<TypeBlock>(name, as_typ_block, slp->_slp_cache->_type_blocks);
        } 
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_uniset = std::dynamic_pointer_cast<UniformSet>(translatable)) {
          slp->importTranslatable<UniformSet>(name, as_uniset, slp->_slp_cache->_uniform_sets);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_uniblk = std::dynamic_pointer_cast<UniformBlk>(translatable)) {
          slp->importTranslatable<UniformBlk>(name, as_uniblk, slp->_slp_cache->_uniform_blocks);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_vif = std::dynamic_pointer_cast<VertexInterface>(translatable)) {
          slp->importTranslatable<VertexInterface>(name, as_vif, slp->_slp_cache->_vertex_interfaces);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_gif = std::dynamic_pointer_cast<GeometryInterface>(translatable)) {
          slp->importTranslatable<GeometryInterface>(name, as_gif, slp->_slp_cache->_geometry_interfaces);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_fif = std::dynamic_pointer_cast<FragmentInterface>(translatable)) {
          slp->importTranslatable<FragmentInterface>(name, as_fif, slp->_slp_cache->_fragment_interfaces);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_cif = std::dynamic_pointer_cast<ComputeInterface>(translatable)) {
          slp->importTranslatable<ComputeInterface>(name, as_cif, slp->_slp_cache->_compute_interfaces);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_vsh = std::dynamic_pointer_cast<VertexShader>(translatable)) {
          slp->importTranslatable<VertexShader>(name, as_vsh, slp->_slp_cache->_vertex_shaders);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_gsh = std::dynamic_pointer_cast<GeometryShader>(translatable)) {
          slp->importTranslatable<GeometryShader>(name, as_gsh, slp->_slp_cache->_geometry_shaders);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_fsh = std::dynamic_pointer_cast<FragmentShader>(translatable)) {
          slp->importTranslatable<FragmentShader>(name, as_fsh, slp->_slp_cache->_fragment_shaders);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_csh = std::dynamic_pointer_cast<ComputeShader>(translatable)) {
          slp->importTranslatable<ComputeShader>(name, as_csh, slp->_slp_cache->_compute_shaders);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_sb = std::dynamic_pointer_cast<StateBlock>(translatable)) {
          slp->importTranslatable<StateBlock>(name, as_sb, slp->_stateblocks);
        }
        ////////////////////////////////////////////////////////////////////////////////////////
        else if (auto as_tek = std::dynamic_pointer_cast<Technique>(translatable)) {
          slp->importTranslatable<Technique>(name, as_tek, slp->_techniques);
        }
      }
    } else {
      // place under import node
      import_node->appendChild(sub_tunit);
    }
  }

  ///////////////////////////////////////////////////////////////
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNamePrimaryIdentifers(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<PrimaryIdentifier>(top);
  for (auto prim_node : nodes) {
    auto match       = slp->matchForAstNode(prim_node);
    auto seq         = match->asShared<Sequence>();
    auto cm          = seq->itemAsShared<ClassMatch>(0);
    auto name        = cm->_token->text;
    prim_node->_name = FormatString("PID: %s", name.c_str());
    prim_node->setValueForKey<std::string>("identifier_name", name);
    prim_node->_children.clear();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameMemberAccessOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<MemberAccessOperator>(top);
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
  auto nodes = AstNode::collectNodesOfType<AdditiveOperator>(top);
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
  auto nodes = AstNode::collectNodesOfType<MultiplicativeOperator>(top);
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
  auto nodes = AstNode::collectNodesOfType<RelationalOperator>(top);
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

void _semaNameEqualityOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<EqualityOperator>(top);
  for (auto ao_node : nodes) {
    auto match     = slp->matchForAstNode(ao_node);
    auto sel       = match->asShared<OneOf>()->_selected;
    auto cm        = sel->asShared<ClassMatch>();
    auto name      = cm->_token->text;
    ao_node->_name = FormatString("EqualityOperator: %s", name.c_str());
    ao_node->setValueForKey<std::string>("operator", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameShiftOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<ShiftOperator>(top);
  for (auto so_node : nodes) {
    auto match     = slp->matchForAstNode(so_node);
    auto sel       = match->asShared<OneOf>()->_selected;
    auto cm        = sel->asShared<ClassMatch>();
    auto name      = cm->_token->text;
    so_node->_name = FormatString("ShiftOperator: %s", name.c_str());
    so_node->setValueForKey<std::string>("operator", name);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaNameAssignmentOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<AssignmentOperator>(top);
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
  auto nodes = AstNode::collectNodesOfType<InheritListItem>(top);
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

bool _isBuiltInDataType(impl::ShadLangParser* slp, astnode_ptr_t astnode) {
  bool builtin                        = false;
  match_ptr_t id_match                = slp->matchForAstNode(astnode);
  scannerlightview_constptr_t id_view = id_match->_view;
  const Token* id_tok                 = id_view->token(0);
  uint64_t id_class                   = id_tok->_class;
  switch (id_class) {
    case "KW_FLOAT"_crcu:
    case "KW_INT"_crcu:
    case "KW_UINT"_crcu:
    case "KW_VEC2"_crcu:
    case "KW_VEC3"_crcu:
    case "KW_VEC4"_crcu:
    case "KW_IVEC2"_crcu:
    case "KW_IVEC3"_crcu:
    case "KW_IVEC4"_crcu:
    case "KW_UVEC2"_crcu:
    case "KW_UVEC3"_crcu:
    case "KW_UVEC4"_crcu:
    case "KW_SAMP1D"_crcu:
    case "KW_SAMP2D"_crcu:
    case "KW_SAMP3D"_crcu:
    case "KW_ISAMP1D"_crcu:
    case "KW_ISAMP2D"_crcu:
    case "KW_ISAMP3D"_crcu:
    case "KW_USAMP1D"_crcu:
    case "KW_USAMP2D"_crcu:
    case "KW_USAMP3D"_crcu:
      builtin = true;
      break;
    default:
      break;
  }
  // printf( "id<%s> builtin<%d> vst<%zu> ven<%zu>\n", id_tok->text.c_str(), int(builtin), id_view->_start, id_view->_end );
  return builtin;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

bool _isConstructableBuiltInDataType(impl::ShadLangParser* slp, astnode_ptr_t astnode) {
  bool builtin                        = false;
  match_ptr_t id_match                = slp->matchForAstNode(astnode);
  scannerlightview_constptr_t id_view = id_match->_view;
  const Token* id_tok                 = id_view->token(0);
  uint64_t id_class                   = id_tok->_class;
  switch (id_class) {
    case "KW_FLOAT"_crcu:
    case "KW_INT"_crcu:
    case "KW_UINT"_crcu:
    case "KW_VEC2"_crcu:
    case "KW_VEC3"_crcu:
    case "KW_VEC4"_crcu:
    case "KW_IVEC2"_crcu:
    case "KW_IVEC3"_crcu:
    case "KW_IVEC4"_crcu:
    case "KW_UVEC2"_crcu:
    case "KW_UVEC3"_crcu:
    case "KW_UVEC4"_crcu:
      builtin = true;
      break;
    default:
      break;
  }
  // printf( "id<%s> builtin<%d> class<%zx> vst<%zu> ven<%zu>\n", id_tok->text.c_str(), int(builtin), id_tok->_class,
  // id_view->_start, id_view->_end );
  return builtin;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaResolvePrimaryExpressions(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<PrimaryExpression>(top);
  for (auto pe_node : nodes) {
    ////////////////////////////////////
    size_t num_children = pe_node->_children.size();
    if (2 != num_children)
      continue;
    ////////////////////////////////////
    if (nullptr == pe_node)
      continue;
    auto dt_node = pe_node->childAs<DataType>(0);
    ////////////////////////////////////
    if (nullptr == dt_node)
      continue;
    auto type_name = dt_node->typedValueForKey<std::string>("data_type").value();
    ////////////////////////////////////
    auto parens_exp = pe_node->childAs<ParensExpression>(1);
    if (nullptr == parens_exp)
      continue;
    ////////////////////////////////////
    bool is_builtin = _isConstructableBuiltInDataType(slp, pe_node);
    if (not is_builtin)
      continue;
    ////////////////////////////////////
    // technically, since is_builtin is true
    //  this is a 'constructor call',
    //  not just a method call..
    ////////////////////////////////////
    auto dt_match  = slp->matchForAstNode(dt_node);
    auto sema_id   = slp->ast_create<SemaIdentifier>(dt_match);
    sema_id->_name = "SemaId: ";
    sema_id->_name += " " + type_name;
    sema_id->setValueForKey<std::string>("identifier_name", type_name);
    slp->replaceInParent(dt_node, sema_id);
    ////////////////////////////////////
    auto pe_match      = slp->matchForAstNode(pe_node);
    auto id_call       = slp->ast_create<IdentifierCall>(pe_match);
    id_call->_children = pe_node->_children;
    slp->replaceInParent(pe_node, id_call);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaResolveIdentifierCalls(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<IdentifierCall>(top);
  for (auto id_call : nodes) {
    auto sema_id = id_call->childAs<SemaIdentifier>(0);
    OrkAssert(sema_id);
    auto id_name    = sema_id->typedValueForKey<std::string>("identifier_name").value();
    bool is_builtin = _isConstructableBuiltInDataType(slp, sema_id);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaResolveSemaFunctionArguments(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<SemaFunctionArguments>(top);
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

void _semaFindInterfaceInputSemantics(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto inputs = AstNode::collectNodesOfType<InterfaceInput>(top);
   printf("  num_inputs<%zu>\n", inputs.size());
  for (auto input : inputs) {
    auto tid = input->childAs<TypedIdentifier>(0);
    if(tid){
      auto colon = input->childAs<COLON>(1);
      auto semantic = input->childAs<SemaIdentifier>(2);
      if( colon and semantic ){
        auto sema_id = semantic->typedValueForKey<std::string>("identifier_name").value();
        printf( "sema_id<%s>\n", sema_id.c_str() );
        input->setValueForKey<std::string>("semantic", sema_id);
      }
    }
    else { 
      // try layout(local_size_x = ?, local_size_y = ?, local_size_z = ?); ?
      auto layout = input->childAs<InterfaceLayout>(0);
      if(layout){
        //OrkAssert(false);
      }
      else{
        dumpAstNode(input);
        OrkAssert(false);
      }
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////

template <typename node_t> //
int _semaLinkToInheritances(
    impl::ShadLangParser* slp, //
    astnode_ptr_t top) {       //
  int count  = 0;
  auto nodes = AstNode::collectNodesOfType<node_t>(top);
  for (auto n : nodes) {
    astnode_ptr_t inh_item;
    auto objname = n->template typedValueForKey<std::string>("object_name").value();
    /////////////////////////////////
    auto check_inheritance = [](std::string inh_name, std::string set_name, SHAST::astnode_map_t& in_map) -> bool { //
      auto it    = in_map.find(inh_name);
      bool found = (it != in_map.end());
      if(not found){
        //printf( "check_inheritance<%s> in set<%s> not found\n", inh_name.c_str(), set_name.c_str() );
      }
      return found;
    };
    /////////////////////////////////
    AstNode::walkDownAST(n, [&](astnode_ptr_t node) -> bool {
      auto as_inh_item = std::dynamic_pointer_cast<InheritListItem>(node);
      if (as_inh_item) {
        inh_item      = as_inh_item;
        auto inh_name = inh_item->typedValueForKey<std::string>("inherited_object").value();
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
          semalib->setValueForKey<std::string>("extension_name", id_name);
          slp->replaceInParent(inh_item, semalib);
          count++;
          return false;
        }
        /////////////////////////////////
        bool check_lib_blocks  = false;
        bool check_typ_blocks  = false;
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
          check_typ_blocks = true;
          //check_uni_sets   = true;
          //check_uni_blks   = true;
        }
        else if constexpr (std::is_same<node_t, TypeBlock>::value) {
          check_typ_blocks = true;
          //check_uni_sets   = true;
          //check_uni_blks   = true;
        }
        /////////////////////////////////
        // VertexShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, VertexShader>::value) {
          check_lib_blocks = true;
          check_typ_blocks = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_vtx_iface  = true;
        }
        /////////////////////////////////
        // GeometryShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, GeometryShader>::value) {
          check_lib_blocks = true;
          check_typ_blocks = true;
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
          check_typ_blocks = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_vtx_iface  = true;
          check_geo_iface  = true;
          check_frg_iface  = true;
        }
        /////////////////////////////////
        // ComputeShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, ComputeShader>::value) {
          check_lib_blocks = true;
          check_typ_blocks = true;
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
        /////////////////////////////////
        /////////////////////////////////
        /////////////////////////////////
        if (check_typ_blocks and check_inheritance(inh_name, "typ", slp->_slp_cache->_type_blocks)) {
          auto typelib   = std::make_shared<SemaInheritTypeBlock>();
          typelib->_name = FormatString("SemaInheritTypeBlock: %s", inh_name.c_str());
          typelib->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, typelib);
          count++;
        }
        else if (check_lib_blocks and check_inheritance(inh_name, "lib", slp->_slp_cache->_library_blocks)) {
          auto semanode   = std::make_shared<SemaInheritLibrary>();
          semanode->_name = FormatString("SemaInheritLibrary: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_uni_sets and check_inheritance(inh_name, "uset", slp->_slp_cache->_uniform_sets)) {
          auto semanode   = std::make_shared<SemaInheritUniformSet>();
          semanode->_name = FormatString("SemaInheritUniformSet: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_uni_blks and check_inheritance(inh_name, "ublk", slp->_slp_cache->_uniform_blocks)) {
          auto semanode   = std::make_shared<SemaInheritUniformBlk>();
          semanode->_name = FormatString("SemaInheritUniformBlk: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_vtx_iface and check_inheritance(inh_name, "vif", slp->_slp_cache->_vertex_interfaces)) {
          auto semanode   = std::make_shared<SemaInheritVertexInterface>();
          semanode->_name = FormatString("SemaInheritVertexInterface: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_geo_iface and check_inheritance(inh_name, "gif", slp->_slp_cache->_geometry_interfaces)) {
          auto semanode   = std::make_shared<SemaInheritGeometryInterface>();
          semanode->_name = FormatString("SemaInheritGeometryInterface: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_frg_iface and check_inheritance(inh_name, "fif", slp->_slp_cache->_fragment_interfaces)) {
          auto semanode   = std::make_shared<SemaInheritFragmentInterface>();
          semanode->_name = FormatString("SemaInheritFragmentInterface: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_com_iface and check_inheritance(inh_name, "cif", slp->_slp_cache->_compute_interfaces)) {
          auto semanode   = std::make_shared<SemaInheritComputeInterface>();
          semanode->_name = FormatString("SemaInheritComputeInterface: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_stateblocks and check_inheritance(inh_name, "sblk", slp->_stateblocks)) {
          auto semanode   = std::make_shared<SemaInheritStateBlock>();
          semanode->_name = FormatString("SemaInheritStateBlock: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semanode);
          count++;
        }
        else if( inh_name!="default" ){
          printf( "check_inheritance<%s> not found\n", inh_name.c_str() );
          OrkAssert(false);
        }
      } // if (as_inh_item) {
      return true;
    });
  }
  return count;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

template <typename node_t> void _semaMoveNames(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<node_t>(top);
  for (auto tnode : nodes) {

    if constexpr (std::is_same<node_t, ImportDirective>::value) {
      // OrkAssert(false);
    }
    else
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
  auto nodes = AstNode::collectNodesOfType<IntegerLiteral>(top);
  for (auto node : nodes) {
    std::string out_str;
    SHAST::_dumpAstTreeVisitor(node, 0, out_str);
    // printf("AST: %s\n", out_str.c_str());
    auto match = slp->matchForAstNode(node);
    match      = match->asShared<OneOf>()->_selected;
    match->dump1(0);
    auto cm = match->asShared<ClassMatch>();
    // printf("cm<%p>\n", (void*)cm.get());
    auto literal_value = cm->_token->text;
    if (literal_value == "") {
      OrkAssert(false);
    }
    auto sema_node   = std::make_shared<SemaIntegerLiteral>();
    sema_node->_name = FormatString("SemaIntegerLiteral<%s>", literal_value.c_str());
    sema_node->setValueForKey<std::string>("literal_value", literal_value);
    slp->replaceInParent(node, sema_node);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaFloatLiterals(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<FloatLiteral>(top);
  for (auto node : nodes) {
    auto match = slp->matchForAstNode(node);
    match->dump1(0);
    auto seq           = match->asShared<Sequence>();
    auto cm            = seq->itemAsShared<ClassMatch>(0);
    auto literal_value = cm->_token->text;
    auto sema_node     = std::make_shared<SemaFloatLiteral>();
    sema_node->setValueForKey<std::string>("literal_value", literal_value);
    slp->replaceInParent(node, sema_node);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////
void _semaDecorateArrayDeclarations(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto arrays = AstNode::collectNodesOfType<ArrayDeclaration>(top);
  for (auto array : arrays) {

  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::semaAST(astnode_ptr_t top) {

  printf("ShadLangParser<%p:%s> semaAST CP-A\n", this, _name.c_str() );

  //////////////////////////////////
  // Pass 2 - Imports
  //////////////////////////////////

  _semaCollectNamedOfType<ImportDirective>(this, top, _import_directives);
  _semaPerformImports(this, top);

  printf("ShadLangParser<%p:%s> semaAST CP-B\n", this, _name.c_str() );

  //////////////////////////////////

  if (1) {
    _semaNameBuiltInDataTypes(this, top);
    _semaNormalizeDtUserTypes(this, top);

    _semaNameIdentifers(this, top);

    //_semaNameIdentiferCalls(this, top);
    _semaNameTypedIdentifers(this, top);
  }

  printf("ShadLangParser<%p:%s> semaAST CP-C\n", this, _name.c_str() );

  //////////////////////////////////

  if (1) {
    _semaIntegerLiterals(this, top);
    _semaFloatLiterals(this, top);
  }

  printf("ShadLangParser<%p:%s> semaAST CP-D\n", this, _name.c_str() );

  //////////////////////////////////
  // Pass 1 : Build Symbol Tables
  //////////////////////////////////


  if (1) {
    _semaCollectNamedOfType<VertexInterface>(this, top, _slp_cache->_vertex_interfaces);
    _semaCollectNamedOfType<GeometryInterface>(this, top, _slp_cache->_geometry_interfaces);
    _semaCollectNamedOfType<FragmentInterface>(this, top, _slp_cache->_fragment_interfaces);
    _semaCollectNamedOfType<ComputeInterface>(this, top, _slp_cache->_compute_interfaces);

    _semaCollectNamedOfType<VertexShader>(this, top, _slp_cache->_vertex_shaders);
    _semaCollectNamedOfType<FragmentShader>(this, top, _slp_cache->_fragment_shaders);
    _semaCollectNamedOfType<GeometryShader>(this, top, _slp_cache->_geometry_shaders);
    _semaCollectNamedOfType<ComputeShader>(this, top, _slp_cache->_compute_shaders);

    _semaCollectNamedOfType<UniformSet>(this, top, _slp_cache->_uniform_sets);
    _semaCollectNamedOfType<UniformBlk>(this, top, _slp_cache->_uniform_blocks);
    _semaCollectNamedOfType<LibraryBlock>(this, top, _slp_cache->_library_blocks);
    _semaCollectNamedOfType<TypeBlock>(this, top, _slp_cache->_type_blocks);

    _semaCollectNamedOfType<StructDecl>(this, top, _structs);
    _semaCollectNamedOfType<StateBlock>(this, top, _stateblocks);
    _semaCollectNamedOfType<Technique>(this, top, _techniques);

    _semaCollectNamedOfType<FunctionDef1>(this, top, _fndef1s);
    _semaCollectNamedOfType<FunctionDef2>(this, top, _fndef2s);

    _semaCollectNamedOfType<FxConfigDecl>(this, top, _fxconfig_decls);

    _semaProcNamedOfType<FxConfigRef>(this, top);
    _semaProcNamedOfType<Pass>(this, top);

    _semaMoveNames<Translatable>(this, top);
    _semaMoveNames<FxConfigRef>(this, top);
    _semaMoveNames<Pass>(this, top);
  }

  printf("ShadLangParser<%p:%s> semaAST CP-E\n", this, _name.c_str() );

  //////////////////////////////////
  // Pass 3
  //////////////////////////////////

  if (1) {
    _semaNamePrimaryIdentifers(this, top);
    _semaNameMemberAccessOperators(this, top);
    _semaNameAdditiveOperators(this, top);
    _semaNameMultiplicativeOperators(this, top);
    _semaNameRelationalOperators(this, top);
    _semaNameEqualityOperators(this, top);
    _semaNameShiftOperators(this, top);
    _semaNameAssignmentOperators(this, top);
    _semaNameInheritListItems(this, top);
    _semaResolvePrimaryExpressions(this, top);
    _semaResolveIdentifierCalls(this, top);
    _semaResolveSemaFunctionArguments(this, top);
    _semaDecorateArrayDeclarations(this, top);
    _semaFindInterfaceInputSemantics(this, top);
  }

  printf("ShadLangParser<%p:%s> semaAST CP-F\n", this, _name.c_str() );

  //////////////////////////////////
  // Pass 4..
  //////////////////////////////////

  bool keep_going = true;
  while (keep_going) {
    int count = 0;
    count += _semaLinkToInheritances<LibraryBlock>(this, top);
    count += _semaLinkToInheritances<TypeBlock>(this, top);

    count += _semaLinkToInheritances<VertexInterface>(this, top);
    count += _semaLinkToInheritances<GeometryInterface>(this, top);
    count += _semaLinkToInheritances<FragmentInterface>(this, top);
    count += _semaLinkToInheritances<ComputeInterface>(this, top);

    count += _semaLinkToInheritances<VertexShader>(this, top);
    count += _semaLinkToInheritances<FragmentShader>(this, top);
    count += _semaLinkToInheritances<GeometryShader>(this, top);
    count += _semaLinkToInheritances<ComputeShader>(this, top);

    count += _semaLinkToInheritances<StateBlock>(this, top);
    keep_going = (count > 0);
  }

  printf("ShadLangParser<%p:%s> semaAST CP-G\n", this, _name.c_str() );

  auto as_tu                    = std::dynamic_pointer_cast<TranslationUnit>(top);

  for( auto trans_item : _slp_cache->_translatables ){
    auto name = trans_item.first;
    auto trans = trans_item.second;

    auto it1 = as_tu->_translatables_by_name.find(name);
    if( it1 == as_tu->_translatables_by_name.end() ){
      as_tu->_translatables_by_name[name] = trans;
    }

    auto it2 = std::find(as_tu->_children.begin(), as_tu->_children.end(),trans);
    if(it2==as_tu->_children.end()){
      as_tu->appendChild(trans);
    }
  }

}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif
