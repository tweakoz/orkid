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
#include <boost/filesystem.hpp>

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

std::string _smp_extract_type(smp_ptr_t smp_node, match_ptr_t dt_match) {
  auto seq     = dt_match->asShared<Sequence>();
  auto dt_name = seq->itemAsShared<OneOf>(0)->_selected;
  auto dt_cm   = dt_name->asShared<ClassMatch>();

  auto type_name = dt_cm->_token->text;
  return type_name;
}
void _semaNameSamplerTypes(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<SamplerType>(top);
  for (auto id_node : nodes) {
    // dumpAstNode(id_node);
    auto match        = slp->matchForAstNode(id_node);
    auto sampler_type = _smp_extract_type(id_node, match);
    id_node->setValueForKey<std::string>("sampler_type", sampler_type);
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

    int seq_index = 0;
    
    ////////////////////
    // Check if first item is optional (interpolation qualifier)
    ////////////////////
    
    auto first_item = seq->_items[0];
    
    if (auto opt_qual = first_item->tryAsShared<Optional>()) {
      // First item IS an Optional, so the grammar includes the optional qualifier
      // Whether it's actually present or not, the structure is:
      // [0] = Optional, [1] = OneOf (type), [2] = ClassMatch (identifier)
      
      if (opt_qual.value()->_subitem) {
        // The optional qualifier is present
        auto qual_oneof = opt_qual.value()->_subitem->asShared<OneOf>();
        auto qual_cm = qual_oneof->_selected->asShared<ClassMatch>();
        auto qual_text = qual_cm->_token->text;
        tid_node->setValueForKey<std::string>("interpolation_qualifier", qual_text);
      }
      seq_index = 1; // Type is at index 1 when Optional is present
    } else {
      // Old-style TypedIdentifier without optional qualifier
      // [0] = OneOf (type), [1] = ClassMatch (identifier)
      seq_index = 0; // Type is at index 0
    }

    ////////////////////
    // Next item is type
    ////////////////////

    auto type_item = seq->itemAsShared<OneOf>(seq_index);
    auto sel = type_item->_selected;
    std::string type_name;
    if (auto as_cm = sel->tryAsShared<ClassMatch>()) {
      type_name = as_cm.value()->_token->text;
    } else { // its a DataTypeNode or SamplerType
      auto seq  = sel->asShared<Sequence>();
      auto sel0 = seq->tryItemAsShared<OneOf>(0);
      if (sel0) { // SamplerType - has OneOf at index 0
        auto cm   = sel0.value()->_selected->asShared<ClassMatch>();
        type_name = cm->_token->text;
      } else {
        // DataType - has OneOf at index 2 (after two Optionals)
        auto sel2 = seq->tryItemAsShared<OneOf>(2);
        if (sel2) {
          auto cm   = sel2.value()->_selected->asShared<ClassMatch>();
          type_name = cm->_token->text;
        } else {
          OrkAssert(false);
        }
      }
    }

    // tid_node->_name += FormatString("type: %s\n", type_name.c_str());
    tid_node->setValueForKey<std::string>("data_type", type_name);
    seq_index++;

    ////////////////////
    // item 2 (identifier) 
    ////////////////////

    auto cm1     = seq->itemAsShared<ClassMatch>(seq_index);
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
    // printf("mangle function<%s> return type: %s \n", named.c_str(), return_type.c_str());
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
    // printf("mangled_name<%s>\n", mangled_name.c_str());
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
      } else if constexpr (std::is_same<node_t, Technique>::value) {
        if (0)
          printf("_name<%s> Technique<%s>\n", slp->_name.c_str(), the_name.c_str());
        n->template setValueForKey<std::string>("raw_name", the_name);
      } else {
        n->template setValueForKey<std::string>("raw_name", the_name);
      }

      std::string mangled_name;

      if (n->hasKey("mangled_name")) {
        mangled_name = n->template typedValueForKey<std::string>("mangled_name").value();
        the_name     = mangled_name;
      }

      ////////////////////////////////////////////////////////////

      n->template setValueForKey<std::string>("object_name", the_name);

      auto it = outmap.find(the_name);
      if (it != outmap.end()) {
        logerrchannel()->log("A: duplicate named object<%s> mangled_name<%s>", the_name.c_str(), mangled_name.c_str());
        continue;
      }

      outmap[the_name] = n;

      // printf( "cache: objname: %s\n", the_name.c_str() );

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

  if (0) {
    printf(
        "ShadLangParser<%p:%s> ImportCount<%zu>\n", //
        (void*)slp,                                 //
        slp->_name.c_str(),                         //
        nodes.size());
  }

  for (auto import_node : nodes) {
    //
    auto raw_import_path = import_node->template typedValueForKey<std::string>("import_path").value();
    // printf("Import RawPath<%s>\n", raw_import_path.c_str());

    ////////////////////////////////////////////////////
    // if string has enclosing quotes, remove them
    ////////////////////////////////////////////////////

    if (raw_import_path.front() == '"')
      raw_import_path.erase(0, 1);
    if (raw_import_path.back() == '"')
      raw_import_path.pop_back();

    import_node->setValueForKey<std::string>("raw_import_path", raw_import_path);
    ////////////////////////////////////////////////////

    // Use Path::resolveRelativeTo for proper resolution
    file::Path import_file_path(raw_import_path);

    // Use the toplevel path from cache if shader path is empty
    file::Path container_path = slp->_shader_path;
    if (!container_path.isAbsolute() && slp->_slp_cache) {
      container_path = slp->_slp_cache->_toplevel_path;
      if (0)
        printf("shadlang using toplevel_path from cache: '%s'\n", container_path.c_str());
    }

    // This will handle both absolute paths (with schemes) and relative paths correctly
    auto proc_import_path = import_file_path.resolveRelativeTo(container_path);
    if (0)
      printf(
          "shadlang import resolved: container='%s' import='%s' -> resolved='%s'\n",
          container_path.c_str(),
          raw_import_path.c_str(),
          proc_import_path.c_str());

    import_node->setValueForKey<std::string>("proc_import_path", proc_import_path.c_str());

    ////////////////////////////////////////////////////////
    // fetch translation unit
    ////////////////////////////////////////////////////////

    auto cache                      = slp->_slp_cache;
    translationunit_ptr_t sub_tunit = shadlang::parseFromFile(slp->_slp_cache, proc_import_path);
    import_node->setValueForKey<transunit_ptr_t>("transunit", sub_tunit);

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
        else if (auto as_smpset = std::dynamic_pointer_cast<SamplerSet>(translatable)) {
          slp->importTranslatable<SamplerSet>(name, as_smpset, slp->_slp_cache->_sampler_sets);
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
        else if (auto as_sif = std::dynamic_pointer_cast<StorageInterface>(translatable)) {
          slp->importTranslatable<StorageInterface>(name, as_sif, slp->_slp_cache->_storage_interfaces);
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

void _semaExtractDescriptorSetIds(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<DescriptorSetId>(top);
  for (auto did_node : nodes) {
    auto intnode     = AstNode::collectNodesOfType<SemaIntegerLiteral>(did_node)[0];
    auto literal_val = intnode->typedValueForKey<std::string>("literal_value").value();
    did_node->setValueForKey<int>("descriptor_set_id", atoi(literal_val.c_str()));
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

void _semaNameUnaryOperators(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto nodes = AstNode::collectNodesOfType<UnaryOperator>(top);
  for (auto uo_node : nodes) {
    auto match     = slp->matchForAstNode(uo_node);
    auto sel       = match->asShared<OneOf>()->_selected;
    auto cm        = sel->asShared<ClassMatch>();
    auto name      = cm->_token->text;
    uo_node->_name = FormatString("UnaryOperator: %s", name.c_str());
    uo_node->setValueForKey<std::string>("operator", name);
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
    case "KW_SAMP1DARRAY"_crcu:
    case "KW_SAMP2DARRAY"_crcu:
    case "KW_SAMP3DARRAY"_crcu:
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
  // printf("  num_inputs<%zu>\n", inputs.size());
  for (auto input : inputs) {
    auto tid = input->childAs<TypedIdentifier>(0);
    if (tid) {
      auto colon    = input->childAs<COLON>(1);
      auto semantic = input->childAs<SemaIdentifier>(2);
      if (colon and semantic) {
        auto sema_id = semantic->typedValueForKey<std::string>("identifier_name").value();
        // printf( "sema_id<%s>\n", sema_id.c_str() );
        input->setValueForKey<std::string>("semantic", sema_id);
      }
    } else {
      // try layout(local_size_x = ?, local_size_y = ?, local_size_z = ?); ?
      auto layout = input->childAs<InterfaceLayout>(0);
      if (layout) {
        // OrkAssert(false);
      } else {
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
      if (not found) {
        // printf( "check_inheritance<%s> in set<%s> not found\n", inh_name.c_str(), set_name.c_str() );
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
        bool check_smp_sets    = false;
        bool check_uni_sets    = false;
        bool check_uni_blks    = false;
        bool check_vtx_iface   = false;
        bool check_geo_iface   = false;
        bool check_frg_iface   = false;
        bool check_com_iface   = false;
        bool check_sto_iface   = false;
        bool check_stateblocks = false;
        /////////////////////////////////
        // LibraryBlocks
        /////////////////////////////////
        if constexpr (std::is_same<node_t, LibraryBlock>::value) {
          check_lib_blocks = true;
          check_typ_blocks = true;
          check_smp_sets   = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
        } else if constexpr (std::is_same<node_t, TypeBlock>::value) {
          check_typ_blocks = true;
          // check_uni_sets   = true;
          // check_uni_blks   = true;
        }
        /////////////////////////////////
        // VertexShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, VertexShader>::value) {
          check_lib_blocks = true;
          check_typ_blocks = true;
          check_smp_sets   = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_vtx_iface  = true;
          check_sto_iface  = true;
        }
        /////////////////////////////////
        // GeometryShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, GeometryShader>::value) {
          check_lib_blocks = true;
          check_typ_blocks = true;
          check_smp_sets   = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_sto_iface  = true;
          // check_vtx_iface  = true;
          check_geo_iface = true;
        }
        /////////////////////////////////
        // FragmentShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, FragmentShader>::value) {
          check_lib_blocks = true;
          check_typ_blocks = true;
          check_smp_sets   = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_vtx_iface  = true;
          check_geo_iface  = true;
          check_frg_iface  = true;
          check_sto_iface  = true;
        }
        /////////////////////////////////
        // ComputeShaders
        /////////////////////////////////
        else if constexpr (std::is_same<node_t, ComputeShader>::value) {
          check_lib_blocks = true;
          check_typ_blocks = true;
          check_smp_sets   = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          check_com_iface  = true;
          check_sto_iface  = true;
        }
        /////////////////////////////////
        // PipelineInterfaces
        /////////////////////////////////
        else if constexpr (std::is_base_of<PipelineInterface, node_t>::value) {
          check_smp_sets  = true;
          check_uni_sets  = true;
          check_uni_blks  = true;
          check_vtx_iface = true;
          check_geo_iface = true;
          check_frg_iface = true;
          check_com_iface = true;
          check_sto_iface  = true;
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
        } else if (check_lib_blocks and check_inheritance(inh_name, "lib", slp->_slp_cache->_library_blocks)) {
          auto semanode   = std::make_shared<SemaInheritLibrary>();
          semanode->_name = FormatString("SemaInheritLibrary: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_smp_sets and check_inheritance(inh_name, "sset", slp->_slp_cache->_sampler_sets)) {
          auto semanode   = std::make_shared<SemaInheritSamplerSet>();
          semanode->_name = FormatString("SemaInheritSamplerSet: %s", inh_name.c_str());
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
        } else if (check_sto_iface and check_inheritance(inh_name, "sif", slp->_slp_cache->_storage_interfaces)) {
          auto semanode   = std::make_shared<SemaInheritStorageInterface>();
          semanode->_name = FormatString("SemaInheritStorageInterface: %s", inh_name.c_str());
          semanode->setValueForKey<std::string>("inherit_id", inh_name);
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (check_stateblocks and check_inheritance(inh_name, "sblk", slp->_stateblocks)) {
          auto semanode   = std::make_shared<SemaInheritStateBlock>();
          semanode->_name = FormatString("SemaInheritStateBlock: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semanode);
          count++;
        } else if (inh_name != "default") {
          // Instead of asserting, just continue and let the inheritance be unresolved
          // OrkAssert(false);
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
    } else if (auto as_objname = tnode->template typedValueForKey<std::string>("object_name")) {
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

void _semaAttachMergedResourceNodesToPasses(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto passes = AstNode::collectNodesOfType<Pass>(top);

  if (0) {
    printf("=== MERGED RESOURCE ATTACHMENT ===\n");
    printf("  Passes found: %zu\n", passes.size());
    printf("  Vertex interfaces: %zu\n", slp->_slp_cache->_vertex_interfaces.size());
    printf("  Fragment interfaces: %zu\n", slp->_slp_cache->_fragment_interfaces.size());
    printf("  Vertex shaders: %zu\n", slp->_slp_cache->_vertex_shaders.size());
    printf("  Fragment shaders: %zu\n", slp->_slp_cache->_fragment_shaders.size());
    printf("  Sampler sets: %zu\n", slp->_slp_cache->_sampler_sets.size());
    printf("  Uniform blocks: %zu\n", slp->_slp_cache->_uniform_blocks.size());
  }

  //////////////////////////////////////////////////////////////////////////////
  // GLOBAL BINDING ID ASSIGNMENT
  // Shaders are compiled ONCE but may be reused across multiple techniques with
  // different resource layouts. To ensure consistent binding IDs, we:
  // 1. PHASE 1: Collect ALL unique resources from ALL passes
  // 2. PHASE 2: Assign global binding IDs (deterministic ordering)
  // 3. PHASE 3: Create per-pass merged resources using global IDs
  //////////////////////////////////////////////////////////////////////////////

  // Structure to hold global resource info
  struct GlobalResourceInfo {
    int descriptor_set_id;
    std::string resource_key;
    std::string name;
    std::string datatype;
    std::string original_source;
    MergedShaderResources::ResourceBinding::Type type;
    int binding_id = -1; // Will be assigned in Phase 2
  };

  // Global map: [descriptor_set_id][resource_key] -> GlobalResourceInfo
  std::map<int, std::map<std::string, GlobalResourceInfo>> global_resources;

  // Recursive lambda to collect inherited resources from interfaces and library blocks
  std::function<void(impl::ShadLangParser*, astnode_ptr_t, std::vector<astnode_ptr_t>&, std::vector<astnode_ptr_t>&, std::vector<astnode_ptr_t>&)> collectInheritedResources =
      [&](impl::ShadLangParser* slp_inner, astnode_ptr_t node, std::vector<astnode_ptr_t>& sampler_sets, std::vector<astnode_ptr_t>& uniform_blocks, std::vector<astnode_ptr_t>& storage_interfaces) {
        // Check for direct sampler sets, uniform blocks, and storage interfaces in this node
        auto node_sampler_sets   = AstNode::collectNodesOfType<SemaInheritSamplerSet>(node);
        auto node_uniform_blocks = AstNode::collectNodesOfType<SemaInheritUniformBlk>(node);
        auto node_storage_interfaces = AstNode::collectNodesOfType<SemaInheritStorageInterface>(node);
        sampler_sets.insert(sampler_sets.end(), node_sampler_sets.begin(), node_sampler_sets.end());
        uniform_blocks.insert(uniform_blocks.end(), node_uniform_blocks.begin(), node_uniform_blocks.end());
        storage_interfaces.insert(storage_interfaces.end(), node_storage_interfaces.begin(), node_storage_interfaces.end());

        // Check for inherited interfaces in this node
        auto node_vertex_interfaces   = AstNode::collectNodesOfType<SemaInheritVertexInterface>(node);
        auto node_fragment_interfaces = AstNode::collectNodesOfType<SemaInheritFragmentInterface>(node);
        auto node_geometry_interfaces = AstNode::collectNodesOfType<SemaInheritGeometryInterface>(node);
        auto node_compute_interfaces  = AstNode::collectNodesOfType<SemaInheritComputeInterface>(node);

        // Recursively process inherited interfaces
        for (auto iface_inherit : node_vertex_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj  = slp_inner->_slp_cache->_vertex_interfaces.find(iface_name);
          if (iface_obj != slp_inner->_slp_cache->_vertex_interfaces.end()) {
            collectInheritedResources(slp_inner, iface_obj->second, sampler_sets, uniform_blocks, storage_interfaces);
          }
        }
        for (auto iface_inherit : node_fragment_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj  = slp_inner->_slp_cache->_fragment_interfaces.find(iface_name);
          if (iface_obj != slp_inner->_slp_cache->_fragment_interfaces.end()) {
            collectInheritedResources(slp_inner, iface_obj->second, sampler_sets, uniform_blocks, storage_interfaces);
          }
        }
        for (auto iface_inherit : node_geometry_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj  = slp_inner->_slp_cache->_geometry_interfaces.find(iface_name);
          if (iface_obj != slp_inner->_slp_cache->_geometry_interfaces.end()) {
            collectInheritedResources(slp_inner, iface_obj->second, sampler_sets, uniform_blocks, storage_interfaces);
          }
        }
        for (auto iface_inherit : node_compute_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj  = slp_inner->_slp_cache->_compute_interfaces.find(iface_name);
          if (iface_obj != slp_inner->_slp_cache->_compute_interfaces.end()) {
            collectInheritedResources(slp_inner, iface_obj->second, sampler_sets, uniform_blocks, storage_interfaces);
          }
        }
        for (auto iface_inherit : node_storage_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj  = slp_inner->_slp_cache->_storage_interfaces.find(iface_name);
          if (iface_obj != slp_inner->_slp_cache->_storage_interfaces.end()) {
            collectInheritedResources(slp_inner, iface_obj->second, sampler_sets, uniform_blocks, storage_interfaces);
          }
        }

        // Check for inherited library blocks in this node
        auto node_library_blocks = AstNode::collectNodesOfType<SemaInheritLibrary>(node);
        for (auto lib_inherit : node_library_blocks) {
          auto lib_name = lib_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto lib_obj  = slp_inner->_slp_cache->_library_blocks.find(lib_name);
          if (lib_obj != slp_inner->_slp_cache->_library_blocks.end()) {
            collectInheritedResources(slp_inner, lib_obj->second, sampler_sets, uniform_blocks, storage_interfaces);
          }
        }
      };

  // Helper lambda to collect all shaders for a pass
  auto collectShadersForPass = [&](astnode_ptr_t pass) -> std::vector<astnode_ptr_t> {
    std::vector<astnode_ptr_t> pass_shaders;

    auto vtx_refs = AstNode::collectNodesOfType<VertexShaderRef>(pass);
    auto frg_refs = AstNode::collectNodesOfType<FragmentShaderRef>(pass);
    auto geo_refs = AstNode::collectNodesOfType<GeometryShaderRef>(pass);
    auto com_refs = AstNode::collectNodesOfType<ComputeShaderRef>(pass);

    for (auto vtx_ref : vtx_refs) {
      auto shader_name = vtx_ref->typedValueForKey<std::string>("ref_id").value();
      auto shader = slp->_slp_cache->_vertex_shaders.find(shader_name);
      if (shader != slp->_slp_cache->_vertex_shaders.end()) {
        pass_shaders.push_back(shader->second);
      }
    }
    for (auto frg_ref : frg_refs) {
      auto shader_name = frg_ref->typedValueForKey<std::string>("ref_id").value();
      auto shader = slp->_slp_cache->_fragment_shaders.find(shader_name);
      if (shader != slp->_slp_cache->_fragment_shaders.end()) {
        pass_shaders.push_back(shader->second);
      }
    }
    for (auto geo_ref : geo_refs) {
      auto shader_name = geo_ref->typedValueForKey<std::string>("ref_id").value();
      auto shader      = slp->_slp_cache->_geometry_shaders.find(shader_name);
      if (shader != slp->_slp_cache->_geometry_shaders.end()) {
        pass_shaders.push_back(shader->second);
      }
    }
    for (auto com_ref : com_refs) {
      auto shader_name = com_ref->typedValueForKey<std::string>("ref_id").value();
      auto shader      = slp->_slp_cache->_compute_shaders.find(shader_name);
      if (shader != slp->_slp_cache->_compute_shaders.end()) {
        pass_shaders.push_back(shader->second);
      }
    }

    return pass_shaders;
  };

  // Helper lambda to collect all inherited interfaces for a shader
  auto collectAllInheritedInterfaces = [&](astnode_ptr_t shader) -> std::vector<astnode_ptr_t> {
    std::vector<astnode_ptr_t> all_inherited_interfaces;

    auto inherited_interfaces          = AstNode::collectNodesOfType<SemaInheritVertexInterface>(shader);
    auto inherited_fragment_interfaces = AstNode::collectNodesOfType<SemaInheritFragmentInterface>(shader);
    auto inherited_geometry_interfaces = AstNode::collectNodesOfType<SemaInheritGeometryInterface>(shader);
    auto inherited_compute_interfaces  = AstNode::collectNodesOfType<SemaInheritComputeInterface>(shader);
    auto inherited_storage_interfaces  = AstNode::collectNodesOfType<SemaInheritStorageInterface>(shader);

    for (auto iface : inherited_interfaces) {
      auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
      auto iface_obj  = slp->_slp_cache->_vertex_interfaces.find(iface_name);
      if (iface_obj != slp->_slp_cache->_vertex_interfaces.end()) {
        all_inherited_interfaces.push_back(iface_obj->second);
      }
    }
    for (auto iface : inherited_fragment_interfaces) {
      auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
      auto iface_obj  = slp->_slp_cache->_fragment_interfaces.find(iface_name);
      if (iface_obj != slp->_slp_cache->_fragment_interfaces.end()) {
        all_inherited_interfaces.push_back(iface_obj->second);
      }
    }
    for (auto iface : inherited_geometry_interfaces) {
      auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
      auto iface_obj  = slp->_slp_cache->_geometry_interfaces.find(iface_name);
      if (iface_obj != slp->_slp_cache->_geometry_interfaces.end()) {
        all_inherited_interfaces.push_back(iface_obj->second);
      }
    }
    for (auto iface : inherited_compute_interfaces) {
      auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
      auto iface_obj  = slp->_slp_cache->_compute_interfaces.find(iface_name);
      if (iface_obj != slp->_slp_cache->_compute_interfaces.end()) {
        all_inherited_interfaces.push_back(iface_obj->second);
      }
    }
    for (auto iface : inherited_storage_interfaces) {
      auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
      auto iface_obj  = slp->_slp_cache->_storage_interfaces.find(iface_name);
      if (iface_obj != slp->_slp_cache->_storage_interfaces.end()) {
        all_inherited_interfaces.push_back(iface_obj->second);
      }
    }

    return all_inherited_interfaces;
  };

  //////////////////////////////////////////////////////////////////////////////
  // PHASE 1: Collect ALL unique resources from ALL passes into global_resources
  //////////////////////////////////////////////////////////////////////////////
  printf("=== PHASE 1: Collecting global resources from ALL passes ===\n");
  for (auto pass : passes) {
    auto pass_name = pass->typedValueForKey<std::string>("object_name").value();
    auto technique = pass->findAncestorOfType<Technique>();
    OrkAssert(technique);
    auto tech_name = technique->typedValueForKey<std::string>("object_name").value();

    auto pass_shaders = collectShadersForPass(pass);

    for (auto shader : pass_shaders) {
      // Collect direct inherited resources from the shader
      std::vector<astnode_ptr_t> inherited_sampler_sets;
      std::vector<astnode_ptr_t> inherited_uniform_blocks;
      std::vector<astnode_ptr_t> inherited_storage_interfaces;

      auto direct_sampler_sets   = AstNode::collectNodesOfType<SemaInheritSamplerSet>(shader);
      auto direct_uniform_blocks = AstNode::collectNodesOfType<SemaInheritUniformBlk>(shader);
      auto direct_storage_interfaces = AstNode::collectNodesOfType<SemaInheritStorageInterface>(shader);
      inherited_sampler_sets.insert(inherited_sampler_sets.end(), direct_sampler_sets.begin(), direct_sampler_sets.end());
      inherited_uniform_blocks.insert(inherited_uniform_blocks.end(), direct_uniform_blocks.begin(), direct_uniform_blocks.end());
      inherited_storage_interfaces.insert(inherited_storage_interfaces.end(), direct_storage_interfaces.begin(), direct_storage_interfaces.end());

      // Collect from inherited interfaces
      auto all_inherited_interfaces = collectAllInheritedInterfaces(shader);
      for (auto iface : all_inherited_interfaces) {
        collectInheritedResources(slp, iface, inherited_sampler_sets, inherited_uniform_blocks, inherited_storage_interfaces);
      }
      // Also collect resources directly from the shader itself recursively
      collectInheritedResources(slp, shader, inherited_sampler_sets, inherited_uniform_blocks, inherited_storage_interfaces);

      // Process sampler sets -> add to global_resources
      for (auto inherit_node : inherited_sampler_sets) {
        auto sset_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        auto sset_it = slp->_slp_cache->_sampler_sets.find(sset_name);
        if (sset_it != slp->_slp_cache->_sampler_sets.end()) {
          auto sampler_set = sset_it->second;
          int descriptor_set_id = 0;
          auto dset_ids = AstNode::collectNodesOfType<DescriptorSetId>(sampler_set);
          if (dset_ids.size() > 0) {
            descriptor_set_id = dset_ids[0]->typedValueForKey<int>("descriptor_set_id").value();
          }
          auto sampler_decls = AstNode::collectNodesOfType<SamplerDeclaration>(sampler_set);
          for (auto sampler_decl : sampler_decls) {
            auto sampler_type = sampler_decl->childAs<SamplerType>(0);
            auto sampler_name = sampler_decl->childAs<SemaIdentifier>(1);
            if (sampler_type && sampler_name) {
              auto type_name = sampler_type->typedValueForKey<std::string>("sampler_type").value();
              auto name      = sampler_name->typedValueForKey<std::string>("identifier_name").value();
              std::string resource_key = sset_name + "::" + name;
              // Only add if not already in global map
              if (global_resources[descriptor_set_id].find(resource_key) == global_resources[descriptor_set_id].end()) {
                GlobalResourceInfo info;
                info.descriptor_set_id = descriptor_set_id;
                info.resource_key = resource_key;
                info.name = name;
                info.datatype = type_name;
                info.original_source = sset_name;
                info.type = MergedShaderResources::ResourceBinding::Type::Sampler;
                global_resources[descriptor_set_id][resource_key] = info;
              }
            }
          }
        }
      }

      // Process uniform blocks -> add to global_resources
      for (auto inherit_node : inherited_uniform_blocks) {
        auto ublk_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        auto ublk_it = slp->_slp_cache->_uniform_blocks.find(ublk_name);
        if (ublk_it != slp->_slp_cache->_uniform_blocks.end()) {
          auto uniform_block = ublk_it->second;
          int descriptor_set_id = 0;
          auto dset_ids = AstNode::collectNodesOfType<DescriptorSetId>(uniform_block);
          if (dset_ids.size() > 0) {
            descriptor_set_id = dset_ids[0]->typedValueForKey<int>("descriptor_set_id").value();
          }
          std::string resource_key = ublk_name;
          if (global_resources[descriptor_set_id].find(resource_key) == global_resources[descriptor_set_id].end()) {
            GlobalResourceInfo info;
            info.descriptor_set_id = descriptor_set_id;
            info.resource_key = resource_key;
            info.name = ublk_name;
            info.datatype = "uniform_block";
            info.original_source = ublk_name;
            info.type = MergedShaderResources::ResourceBinding::Type::UniformBlock;
            global_resources[descriptor_set_id][resource_key] = info;
          }
        }
      }

      // Process storage interfaces -> add to global_resources
      for (auto inherit_node : inherited_storage_interfaces) {
        auto storage_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        auto storage_it = slp->_slp_cache->_storage_interfaces.find(storage_name);
        if (storage_it != slp->_slp_cache->_storage_interfaces.end()) {
          auto storage_interface = storage_it->second;
          int descriptor_set_id = 0;
          auto dset_ids = AstNode::collectNodesOfType<DescriptorSetId>(storage_interface);
          if (dset_ids.size() > 0) {
            descriptor_set_id = dset_ids[0]->typedValueForKey<int>("descriptor_set_id").value();
          }
          std::string resource_key = storage_name;
          if (global_resources[descriptor_set_id].find(resource_key) == global_resources[descriptor_set_id].end()) {
            GlobalResourceInfo info;
            info.descriptor_set_id = descriptor_set_id;
            info.resource_key = resource_key;
            info.name = storage_name;
            info.datatype = "storage_buffer";
            info.original_source = storage_name;
            info.type = MergedShaderResources::ResourceBinding::Type::SSBO;
            global_resources[descriptor_set_id][resource_key] = info;
          }
        }
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // PHASE 2: Assign global binding IDs (sorted by key for determinism)
  //////////////////////////////////////////////////////////////////////////////
  printf("=== PHASE 2: Assigning global binding IDs ===\n");
  for (auto& [dset_id, resources] : global_resources) {
    int binding_id = 0;
    // std::map is already sorted by key, so iteration is deterministic
    for (auto& [key, info] : resources) {
      info.binding_id = binding_id++;
      printf("  GLOBAL: dset<%d> key<%s> name<%s> type<%s> -> binding<%d>\n",
             dset_id, key.c_str(), info.name.c_str(), info.datatype.c_str(), info.binding_id);
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // PHASE 3: Create per-pass merged resources using global IDs
  //////////////////////////////////////////////////////////////////////////////
  printf("=== PHASE 3: Creating per-pass merged resources with global IDs ===\n");
  for (auto pass : passes) {
    auto pass_name = pass->typedValueForKey<std::string>("object_name").value();

    auto technique = pass->findAncestorOfType<Technique>();
    OrkAssert(technique);
    auto tech_name = technique->typedValueForKey<std::string>("object_name").value();
    printf("  Processing merged resources for technique<%s> pass: %s\n", tech_name.c_str(), pass_name.c_str());

    auto pass_shaders = collectShadersForPass(pass);

    // Collect resources for THIS pass (but use global binding IDs)
    std::map<int, std::map<std::string, MergedShaderResources::ResourceBinding>> merged_descriptor_sets;
    std::set<std::string> processed_sampler_resources;
    std::set<std::string> processed_uniform_block_resources;
    std::set<std::string> processed_storage_block_resources;

    for (auto shader : pass_shaders) {
      std::vector<astnode_ptr_t> inherited_sampler_sets;
      std::vector<astnode_ptr_t> inherited_uniform_blocks;
      std::vector<astnode_ptr_t> inherited_storage_interfaces;

      auto direct_sampler_sets   = AstNode::collectNodesOfType<SemaInheritSamplerSet>(shader);
      auto direct_uniform_blocks = AstNode::collectNodesOfType<SemaInheritUniformBlk>(shader);
      auto direct_storage_interfaces = AstNode::collectNodesOfType<SemaInheritStorageInterface>(shader);
      inherited_sampler_sets.insert(inherited_sampler_sets.end(), direct_sampler_sets.begin(), direct_sampler_sets.end());
      inherited_uniform_blocks.insert(inherited_uniform_blocks.end(), direct_uniform_blocks.begin(), direct_uniform_blocks.end());
      inherited_storage_interfaces.insert(inherited_storage_interfaces.end(), direct_storage_interfaces.begin(), direct_storage_interfaces.end());

      auto all_inherited_interfaces = collectAllInheritedInterfaces(shader);
      for (auto iface : all_inherited_interfaces) {
        collectInheritedResources(slp, iface, inherited_sampler_sets, inherited_uniform_blocks, inherited_storage_interfaces);
      }
      collectInheritedResources(slp, shader, inherited_sampler_sets, inherited_uniform_blocks, inherited_storage_interfaces);

      // Process sampler sets - use GLOBAL binding IDs
      for (auto inherit_node : inherited_sampler_sets) {
        auto sset_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        auto sset_it = slp->_slp_cache->_sampler_sets.find(sset_name);
        if (sset_it != slp->_slp_cache->_sampler_sets.end()) {
          auto sampler_set = sset_it->second;
          int descriptor_set_id = 0;
          auto dset_ids = AstNode::collectNodesOfType<DescriptorSetId>(sampler_set);
          if (dset_ids.size() > 0) {
            descriptor_set_id = dset_ids[0]->typedValueForKey<int>("descriptor_set_id").value();
          }
          auto sampler_decls = AstNode::collectNodesOfType<SamplerDeclaration>(sampler_set);
          for (auto sampler_decl : sampler_decls) {
            auto sampler_type = sampler_decl->childAs<SamplerType>(0);
            auto sampler_name = sampler_decl->childAs<SemaIdentifier>(1);
            if (sampler_type && sampler_name) {
              auto type_name = sampler_type->typedValueForKey<std::string>("sampler_type").value();
              auto name      = sampler_name->typedValueForKey<std::string>("identifier_name").value();
              std::string resource_key = sset_name + "::" + name;

              if (processed_sampler_resources.find(resource_key) != processed_sampler_resources.end()) {
                continue;
              }

              // LOOK UP GLOBAL BINDING ID instead of incrementing local counter
              auto& global_info = global_resources[descriptor_set_id][resource_key];
              MergedShaderResources::ResourceBinding binding;
              binding.type            = MergedShaderResources::ResourceBinding::Type::Sampler;
              binding.name            = name;
              binding.datatype        = type_name;
              binding.binding_id      = global_info.binding_id; // USE GLOBAL ID!
              binding.original_source = sset_name;

              merged_descriptor_sets[descriptor_set_id][resource_key] = binding;
              processed_sampler_resources.insert(resource_key);
            }
          }
        } else {
          printf("      WARNING: Sampler set not found in symbol table: %s\n", sset_name.c_str());
        }
      }

      // Process uniform blocks - use GLOBAL binding IDs
      for (auto inherit_node : inherited_uniform_blocks) {
        auto ublk_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        auto ublk_it = slp->_slp_cache->_uniform_blocks.find(ublk_name);
        if (ublk_it != slp->_slp_cache->_uniform_blocks.end()) {
          auto uniform_block = ublk_it->second;
          int descriptor_set_id = 0;
          auto dset_ids = AstNode::collectNodesOfType<DescriptorSetId>(uniform_block);
          if (dset_ids.size() > 0) {
            descriptor_set_id = dset_ids[0]->typedValueForKey<int>("descriptor_set_id").value();
          }
          std::string resource_key = ublk_name;

          if (processed_uniform_block_resources.find(resource_key) != processed_uniform_block_resources.end()) {
            continue;
          }

          // LOOK UP GLOBAL BINDING ID
          auto& global_info = global_resources[descriptor_set_id][resource_key];
          MergedShaderResources::ResourceBinding binding;
          binding.type            = MergedShaderResources::ResourceBinding::Type::UniformBlock;
          binding.name            = ublk_name;
          binding.datatype        = "uniform_block";
          binding.binding_id      = global_info.binding_id; // USE GLOBAL ID!
          binding.original_source = ublk_name;

          merged_descriptor_sets[descriptor_set_id][resource_key] = binding;
          processed_uniform_block_resources.insert(resource_key);
          printf("        Added uniform block: %s binding %d (dset %d)\n", ublk_name.c_str(), binding.binding_id, descriptor_set_id);
        } else {
          printf("      WARNING: Uniform block not found in symbol table: %s\n", ublk_name.c_str());
        }
      }

      // Process storage interfaces - use GLOBAL binding IDs
      for (auto inherit_node : inherited_storage_interfaces) {
        auto storage_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        auto storage_it = slp->_slp_cache->_storage_interfaces.find(storage_name);
        if (storage_it != slp->_slp_cache->_storage_interfaces.end()) {
          auto storage_interface = storage_it->second;
          int descriptor_set_id = 0;
          auto dset_ids = AstNode::collectNodesOfType<DescriptorSetId>(storage_interface);
          if (dset_ids.size() > 0) {
            descriptor_set_id = dset_ids[0]->typedValueForKey<int>("descriptor_set_id").value();
          }
          std::string resource_key = storage_name;

          if (processed_storage_block_resources.find(resource_key) != processed_storage_block_resources.end()) {
            continue;
          }

          // LOOK UP GLOBAL BINDING ID
          auto& global_info = global_resources[descriptor_set_id][resource_key];
          MergedShaderResources::ResourceBinding binding;
          binding.type            = MergedShaderResources::ResourceBinding::Type::SSBO;
          binding.name            = storage_name;
          binding.datatype        = "storage_buffer";
          binding.binding_id      = global_info.binding_id; // USE GLOBAL ID!
          binding.original_source = storage_name;

          merged_descriptor_sets[descriptor_set_id][resource_key] = binding;
          processed_storage_block_resources.insert(resource_key);
          printf("        Added storage block: %s binding %d (dset %d)\n", storage_name.c_str(), binding.binding_id, descriptor_set_id);
        } else {
          printf("      WARNING: Storage interface not found in symbol table: %s\n", storage_name.c_str());
        }
      }
    }

    // Create AST nodes for the merged resources
    size_t num_descriptor_sets = merged_descriptor_sets.size();
    if (num_descriptor_sets == 0) {
      merged_descriptor_sets[0] = {};
      num_descriptor_sets       = 1;
    }
    auto merged_node   = std::make_shared<MergedShaderResourcesNode>();
    merged_node->_name = FormatString("MergedResources: %s", pass_name.c_str());

    for (auto& [descriptor_set_id, bindings] : merged_descriptor_sets) {
      auto set_node                = std::make_shared<DescriptorSetNode>();
      set_node->_name              = FormatString("DescriptorSet: %d", descriptor_set_id);
      set_node->_descriptor_set_id = descriptor_set_id;

      std::map<std::string, std::vector<std::pair<std::string, MergedShaderResources::ResourceBinding>>> sources_by_type;
      for (auto& [key, binding] : bindings) {
        sources_by_type[binding.original_source].push_back({key, binding});
      }

      for (auto& [source_name, resource_pairs] : sources_by_type) {
        auto source_node = std::make_shared<DescriptorSetSourceNode>();
        std::string source_type = "unknown";
        if (!resource_pairs.empty()) {
          auto& first_binding = resource_pairs[0].second;
          if (first_binding.type == MergedShaderResources::ResourceBinding::Type::Sampler) {
            source_type = "sampler_set";
          } else if (first_binding.type == MergedShaderResources::ResourceBinding::Type::UniformBlock) {
            source_type = "uniform_block";
          } else if (first_binding.type == MergedShaderResources::ResourceBinding::Type::SSBO) {
            source_type = "storage_interface";
          }
        }
        source_node->_name        = FormatString("From: %s (%s)", source_name.c_str(), source_type.c_str());
        source_node->_source_name = source_name;
        source_node->_source_type = source_type;

        int binding_counter = 0;
        for (auto& [key, binding] : resource_pairs) {
          auto binding_node   = std::make_shared<ResourceBindingNode>();
          binding_node->_name = FormatString("b%d : %s\n%s", binding_counter++, binding.datatype.c_str(), binding.name.c_str());

          printf("  MERGED: tech_name<%s> binding_name<%s>, type<%s> id<%d>\n",
                  tech_name.c_str(),
                  binding.name.c_str(),
                  binding.datatype.c_str(),
                  binding.binding_id);
          binding_node->_binding_id      = binding.binding_id;
          binding_node->_binding_name    = binding.name;
          binding_node->_datatype        = binding.datatype;
          binding_node->_original_source = binding.original_source;
          binding_node->_resource_type   = binding.type;
          source_node->appendChild(binding_node);
        }
        set_node->appendChild(source_node);
      }
      merged_node->appendChild(set_node);
    }
    pass->appendChild(merged_node);
  }
  if (0)
    printf("=== END MERGED RESOURCE ATTACHMENT ===\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaTransformVfPassToExplicitPass(impl::ShadLangParser* slp, astnode_ptr_t top) {
  // printf("=== VF_PASS TRANSFORMATION ===\n");

  // Collect all VtxFrgPass nodes and their parent techniques
  std::vector<std::pair<std::shared_ptr<Technique>, std::shared_ptr<VtxFrgPass>>> vfpass_nodes;

  AstNode::walkDownAST(top, [&](astnode_ptr_t node) -> bool {
    if (auto technique = std::dynamic_pointer_cast<Technique>(node)) {
      // Look for VtxFrgPass children in this technique
      for (auto child : technique->_children) {
        if (auto vfpass = std::dynamic_pointer_cast<VtxFrgPass>(child)) {
          vfpass_nodes.push_back({technique, vfpass});
        }
      }
    }
    return true;
  });

  int pass_counter = 0;
  for (auto& [technique, vfpass] : vfpass_nodes) {
    auto tech_name = technique->typedValueForKey<std::string>("object_name").value();
    // printf("  Processing VtxFrgPass in technique: %s\n", tech_name.c_str());

    // Extract shader names from SemaId children
    std::string vtx_name, frg_name, sb_name;

    for (auto child : vfpass->_children) {
      if (auto sema_id = std::dynamic_pointer_cast<SemaIdentifier>(child)) {
        auto id_name = sema_id->typedValueForKey<std::string>("identifier_name").value();

        if (vtx_name.empty()) {
          vtx_name = id_name;
        } else if (frg_name.empty()) {
          frg_name = id_name;
        } else if (sb_name.empty()) {
          sb_name = id_name;
        }
      }
    }

    if (0)
      printf("    Found vf_pass: vs=%s, ps=%s, sb=%s\n", vtx_name.c_str(), frg_name.c_str(), sb_name.c_str());

    // Create a new Pass node
    auto pass_node        = std::make_shared<Pass>();
    std::string pass_name = FormatString("p%d", pass_counter++);
    pass_node->_name      = FormatString("Pass %s", pass_name.c_str());

    // Store the pass name in the values map
    pass_node->setValueForKey<std::string>("object_name", pass_name);
    pass_node->setValueForKey<std::string>("pass_name", pass_name);

    // Create VertexShaderRef node
    if (!vtx_name.empty()) {
      auto vs_ref   = std::make_shared<VertexShaderRef>();
      vs_ref->_name = FormatString("VertexShaderRef: %s", vtx_name.c_str());
      vs_ref->setValueForKey<std::string>("ref_id", vtx_name);

      // Create SemaIdentifier child
      auto vs_sema_id   = std::make_shared<SemaIdentifier>();
      vs_sema_id->_name = FormatString("SemaIdentifier: %s", vtx_name.c_str());
      vs_sema_id->setValueForKey<std::string>("identifier_name", vtx_name);
      vs_ref->appendChild(vs_sema_id);

      pass_node->appendChild(vs_ref);
    }

    // Create FragmentShaderRef node
    if (!frg_name.empty()) {
      auto ps_ref   = std::make_shared<FragmentShaderRef>();
      ps_ref->_name = FormatString("FragmentShaderRef: %s", frg_name.c_str());
      ps_ref->setValueForKey<std::string>("ref_id", frg_name);

      // Create SemaIdentifier child
      auto ps_sema_id   = std::make_shared<SemaIdentifier>();
      ps_sema_id->_name = FormatString("SemaIdentifier: %s", frg_name.c_str());
      ps_sema_id->setValueForKey<std::string>("identifier_name", frg_name);
      ps_ref->appendChild(ps_sema_id);

      pass_node->appendChild(ps_ref);
    }

    // Create StateBlockRef node if specified
    if (!sb_name.empty()) {
      auto sb_ref   = std::make_shared<StateBlockRef>();
      sb_ref->_name = FormatString("StateBlockRef: %s", sb_name.c_str());
      sb_ref->setValueForKey<std::string>("ref_id", sb_name);

      // Create SemaIdentifier child
      auto sb_sema_id   = std::make_shared<SemaIdentifier>();
      sb_sema_id->_name = FormatString("SemaIdentifier: %s", sb_name.c_str());
      sb_sema_id->setValueForKey<std::string>("identifier_name", sb_name);
      sb_ref->appendChild(sb_sema_id);

      pass_node->appendChild(sb_ref);
    }

    // Replace the VtxFrgPass node with the new Pass node
    slp->replaceInParent(vfpass, pass_node);

    if (0)
      printf("    Replaced VtxFrgPass with Pass %s (%zu children)\n", pass_name.c_str(), pass_node->_children.size());
  }

  // printf("=== END VF_PASS TRANSFORMATION (processed %d vf_pass nodes) ===\n", (int)vfpass_nodes.size());
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::semaAST(astnode_ptr_t top) {

  // printf("ShadLangParser<%p:%s> semaAST CP-A\n", this, _name.c_str() );

  //////////////////////////////////
  // Pass 2 - Imports
  //////////////////////////////////

  _semaCollectNamedOfType<ImportDirective>(this, top, _import_directives);
  _semaPerformImports(this, top);

  // printf("ShadLangParser<%p:%s> semaAST CP-B\n", this, _name.c_str() );

  //////////////////////////////////

  if (1) {
    _semaNameBuiltInDataTypes(this, top);
    _semaNameSamplerTypes(this, top);
    _semaNormalizeDtUserTypes(this, top);

    _semaNameIdentifers(this, top);

    //_semaNameIdentiferCalls(this, top);
    _semaNameTypedIdentifers(this, top);
  }

  // printf("ShadLangParser<%p:%s> semaAST CP-C\n", this, _name.c_str() );

  //////////////////////////////////

  if (1) {
    _semaIntegerLiterals(this, top);
    _semaFloatLiterals(this, top);
    _semaExtractDescriptorSetIds(this, top);
  }

  // printf("ShadLangParser<%p:%s> semaAST CP-D\n", this, _name.c_str() );

  //////////////////////////////////
  // Pass 1 : Build Symbol Tables
  //////////////////////////////////

  if (1) {
    _semaCollectNamedOfType<VertexInterface>(this, top, _slp_cache->_vertex_interfaces);
    _semaCollectNamedOfType<GeometryInterface>(this, top, _slp_cache->_geometry_interfaces);
    _semaCollectNamedOfType<FragmentInterface>(this, top, _slp_cache->_fragment_interfaces);
    _semaCollectNamedOfType<ComputeInterface>(this, top, _slp_cache->_compute_interfaces);
    _semaCollectNamedOfType<StorageInterface>(this, top, _slp_cache->_storage_interfaces);

    _semaCollectNamedOfType<VertexShader>(this, top, _slp_cache->_vertex_shaders);
    _semaCollectNamedOfType<FragmentShader>(this, top, _slp_cache->_fragment_shaders);
    _semaCollectNamedOfType<GeometryShader>(this, top, _slp_cache->_geometry_shaders);
    _semaCollectNamedOfType<ComputeShader>(this, top, _slp_cache->_compute_shaders);

    _semaCollectNamedOfType<SamplerSet>(this, top, _slp_cache->_sampler_sets);
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

  // printf("ShadLangParser<%p:%s> semaAST CP-E\n", this, _name.c_str() );

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
    _semaNameUnaryOperators(this, top);
    _semaNameInheritListItems(this, top);
    _semaResolvePrimaryExpressions(this, top);
    _semaResolveIdentifierCalls(this, top);
    _semaResolveSemaFunctionArguments(this, top);
    _semaDecorateArrayDeclarations(this, top);
    _semaFindInterfaceInputSemantics(this, top);
  }

  // printf("ShadLangParser<%p:%s> semaAST CP-F\n", this, _name.c_str() );

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
    //count += _semaLinkToInheritances<StorageInterface>(this, top);

    count += _semaLinkToInheritances<VertexShader>(this, top);
    count += _semaLinkToInheritances<FragmentShader>(this, top);
    count += _semaLinkToInheritances<GeometryShader>(this, top);
    count += _semaLinkToInheritances<ComputeShader>(this, top);

    count += _semaLinkToInheritances<StateBlock>(this, top);
    keep_going = (count > 0);
  }

  //////////////////////////////////
  // Pass 5.
  //////////////////////////////////

  _semaTransformVfPassToExplicitPass(this, top);
  _semaAttachMergedResourceNodesToPasses(this, top);

  //////////////////////////////////
  // finalize
  //////////////////////////////////
  // printf("ShadLangParser<%p:%s> semaAST CP-G\n", this, _name.c_str() );

  auto as_tu = std::dynamic_pointer_cast<TranslationUnit>(top);

  // printf("=== TRANSLATABLES BEFORE FINALIZATION ===\n");
  for (auto trans_item : _slp_cache->_translatables) {
    auto name = trans_item.first;
    // printf("  translatable: %s\n", name.c_str());
  }
  // printf("=== END TRANSLATABLES ===\n");

  for (auto trans_item : _slp_cache->_translatables) {
    auto name  = trans_item.first;
    auto trans = trans_item.second;

    auto it1 = as_tu->_translatables_by_name.find(name);
    if (it1 == as_tu->_translatables_by_name.end()) {
      as_tu->_translatables_by_name[name] = trans;
    }

    auto it2 = std::find(as_tu->_children.begin(), as_tu->_children.end(), trans);
    if (it2 == as_tu->_children.end()) {
      as_tu->appendChild(trans);
    }
  }

  //////////////////////////////////
  // Generate pass reports for debugging
  //////////////////////////////////
  if(0){
    collectPassReportData(top);
    writePassReports();
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::collectPassReportData(astnode_ptr_t top) {
  auto techniques = AstNode::collectNodesOfType<Technique>(top);

  for (auto tech_node : techniques) {
    std::string raw_name = tech_node->_name;

    // The technique name seems to have "Technique\n{actual_name}" format
    // Extract the actual name after the newline
    std::string technique_name;
    size_t newline_pos = raw_name.find('\n');
    if (newline_pos != std::string::npos && newline_pos + 1 < raw_name.length()) {
      technique_name = raw_name.substr(newline_pos + 1);
    } else {
      technique_name = raw_name; // fallback if no newline found
    }

    if (0)
      printf("DEBUG: Processing technique: %s (from raw: %zu chars)\n", technique_name.c_str(), raw_name.length());

    // Find all passes in this technique
    auto passes  = AstNode::collectNodesOfType<Pass>(tech_node);
    int pass_num = 0;

    for (auto pass_node : passes) {
      std::string key        = technique_name + "." + std::to_string(pass_num);
      PassReportData& report = _pass_reports[key];

      report.shader_name    = _name;
      report.technique_name = technique_name;
      report.pass_num       = pass_num;

      // Check for shader stage references in the pass
      auto vertex_shader_refs   = AstNode::collectNodesOfType<VertexShaderRef>(pass_node);
      auto fragment_shader_refs = AstNode::collectNodesOfType<FragmentShaderRef>(pass_node);

      if (0)
        printf(
            "DEBUG: Pass %d has %zu vertex refs, %zu fragment refs\n",
            pass_num,
            vertex_shader_refs.size(),
            fragment_shader_refs.size());

      report.has_vertex_shader   = !vertex_shader_refs.empty();
      report.has_fragment_shader = !fragment_shader_refs.empty();
      report.has_geometry_shader = false;

      // Now find the actual shaders from the refs
      std::vector<astnode_ptr_t> vertex_shaders;
      std::vector<astnode_ptr_t> fragment_shaders;

      // Look up vertex shaders by name from the refs (using ref_id like in _semaAttachMergedResourceNodesToPasses)
      // Use the already collected shaders from _slp_cache
      for (auto vref : vertex_shader_refs) {
        auto ref_name = vref->typedValueForKey<std::string>("ref_id").value();
        if (0)
          printf("DEBUG: Looking for vertex shader: %s\n", ref_name.c_str());

        // Look in the already collected vertex shaders
        auto it = _slp_cache->_vertex_shaders.find(ref_name);
        if (it != _slp_cache->_vertex_shaders.end()) {
          if (0)
            printf("DEBUG: Found vertex shader: %s\n", ref_name.c_str());
          vertex_shaders.push_back(it->second);
        }
      }

      // Look up fragment shaders by name from the refs (using ref_id)
      // Use the already collected shaders from _slp_cache
      for (auto fref : fragment_shader_refs) {
        auto ref_name = fref->typedValueForKey<std::string>("ref_id").value();
        if (0)
          printf("DEBUG: Looking for fragment shader: %s\n", ref_name.c_str());

        // Look in the already collected fragment shaders
        auto it = _slp_cache->_fragment_shaders.find(ref_name);
        if (it != _slp_cache->_fragment_shaders.end()) {
          if (0)
            printf("DEBUG: Found fragment shader: %s\n", ref_name.c_str());
          fragment_shaders.push_back(it->second);
        }
      }

      if (0)
        printf("DEBUG: Collected %zu vertex shaders, %zu fragment shaders\n", vertex_shaders.size(), fragment_shaders.size());

      // Get the MergedShaderResourcesNode from the pass - it contains all descriptor sets and bindings
      auto merged_resource_nodes = AstNode::collectNodesOfType<MergedShaderResourcesNode>(pass_node);
      if (0)
        printf("DEBUG: Pass has %zu merged resource nodes\n", merged_resource_nodes.size());

      if (!merged_resource_nodes.empty()) {
        auto merged_node = merged_resource_nodes[0];

        // Get descriptor set nodes
        auto descriptor_set_nodes = AstNode::collectNodesOfType<DescriptorSetNode>(merged_node);
        if (0)
          printf("DEBUG: Found %zu descriptor sets\n", descriptor_set_nodes.size());

        for (auto dset_node : descriptor_set_nodes) {
          size_t dset_id = dset_node->_descriptor_set_id;
          if (0)
            printf("DEBUG: Processing descriptor set %zu\n", dset_id);

          // Get resource bindings from this descriptor set
          auto resource_binding_nodes = AstNode::collectNodesOfType<ResourceBindingNode>(dset_node);
          if (0)
            printf("DEBUG: Descriptor set %zu has %zu resource bindings\n", dset_id, resource_binding_nodes.size());

          for (auto binding_node : resource_binding_nodes) {
            std::string resource_name = binding_node->_binding_name;
            std::string datatype      = binding_node->_datatype;
            size_t binding_id         = binding_node->_binding_id;
            auto resource_type        = binding_node->_resource_type;

            if (0)
              printf("DEBUG: Resource: %s (datatype: %s, binding: %zu)\n", resource_name.c_str(), datatype.c_str(), binding_id);

            // Check if it's a uniform block or sampler
            if (resource_type == MergedShaderResources::ResourceBinding::Type::UniformBlock) {
              report.uniform_block_names.push_back(resource_name);

              // Look up the actual uniform block to get member details
              auto ub_it = _slp_cache->_uniform_blocks.find(resource_name);
              if (ub_it != _slp_cache->_uniform_blocks.end()) {
                auto uniform_block = ub_it->second;

                UniformBlockInfo ub_info;
                ub_info.name              = resource_name;
                ub_info.descriptor_set_id = dset_id;
                ub_info.binding_id        = binding_id;

                // Get uniform block members (both arrays and non-arrays)
                auto data_decls = AstNode::collectNodesOfType<DataDeclarationBase>(uniform_block);
                size_t current_offset = 0;
                
                for (auto decl : data_decls) {
                  UniformBlockMember ub_member;
                  
                  // Check if this is an array declaration
                  bool is_array = false;
                  size_t array_length = 0;
                  astnode_ptr_t tid_node = nullptr;
                  
                  if (auto as_array = std::dynamic_pointer_cast<ArrayDeclaration>(decl)) {
                    is_array = true;
                    tid_node = as_array->childAs<TypedIdentifier>(0);
                    auto len_node = as_array->childAs<SemaIntegerLiteral>(1);
                    if (len_node) {
                      auto ary_len_str = len_node->typedValueForKey<std::string>("literal_value").value();
                      array_length = atoi(ary_len_str.c_str());
                    }
                  } else {
                    // Regular DataDeclaration
                    tid_node = decl->childAs<TypedIdentifier>(0);
                  }
                  
                  if (!tid_node) continue;
                  
                  // Get the actual identifier name (not the node type name)
                  auto id_name = tid_node->typedValueForKey<std::string>("identifier_name");
                  if (id_name) {
                    ub_member.name = id_name.value();
                  } else {
                    ub_member.name = tid_node->_name; // fallback
                  }
                  
                  // Get the type
                  std::string base_type = "vec4"; // default
                  auto datatypes = AstNode::collectNodesOfType<DataType>(tid_node);
                  if (!datatypes.empty()) {
                    auto type_val = datatypes[0]->typedValueForKey<std::string>("base_type");
                    if (type_val) {
                      base_type = type_val.value();
                      ub_member.type = base_type;
                      if (is_array) {
                        ub_member.type += "[" + std::to_string(array_length) + "]";
                      }
                    } else {
                      ub_member.type = base_type;
                    }
                  } else {
                    ub_member.type = base_type;
                  }

                  ub_member.offset = current_offset;
                  
                  // Calculate size based on whether it's an array
                  if (is_array && array_length > 0) {
                    size_t element_size = getStd140Size(base_type);
                    // In std140, array elements are aligned to vec4 boundaries
                    size_t aligned_element_size = ((element_size + 15) / 16) * 16;
                    ub_member.size = aligned_element_size * array_length;
                  } else {
                    ub_member.size = getStd140Size(ub_member.type);
                  }
                  
                  current_offset = calculateStd140Offset(ub_member.type, current_offset + ub_member.size);
                  ub_info.members.push_back(ub_member);
                }

                ub_info.total_size = current_offset;

                // Determine which stages use this uniform block
                if (report.has_vertex_shader) {
                  ub_info.stages.insert("vertex");
                }
                if (report.has_fragment_shader) {
                  ub_info.stages.insert("fragment");
                }

                // Add to descriptor set info
                auto& ds_info  = report.descriptor_sets[dset_id];
                ds_info.set_id = dset_id;
                ds_info.uniform_blocks.push_back(ub_info);
                ds_info.total_buffer_size += ub_info.total_size;
              }
            } else if (resource_type == MergedShaderResources::ResourceBinding::Type::Sampler) {
              report.sampler_names.push_back(resource_name);

              SamplerInfo sampler_info;
              sampler_info.name              = resource_name;
              sampler_info.type              = datatype;
              sampler_info.descriptor_set_id = dset_id;
              sampler_info.binding_id        = binding_id;

              // Determine which stages use this sampler
              if (report.has_vertex_shader) {
                sampler_info.stages.insert("vertex");
              }
              if (report.has_fragment_shader) {
                sampler_info.stages.insert("fragment");
              }

              auto& ds_info  = report.descriptor_sets[dset_id];
              ds_info.set_id = dset_id;
              ds_info.samplers.push_back(sampler_info);
            }
          }
        }
      }

      // Calculate total bindings per descriptor set
      for (auto& [dset_id, ds_info] : report.descriptor_sets) {
        ds_info.total_bindings = ds_info.uniform_blocks.size() + ds_info.samplers.size();
      }

      pass_num++;
    }
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::writePassReports() {
  for (const auto& [key, report] : _pass_reports) {
    writePassReport(key, report);
  }
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::writePassReport(const std::string& key, const PassReportData& report) {
  // Get STAGE environment variable
  const char* stage_env = std::getenv("OBT_STAGE");
  if (!stage_env) {
    // Skip report generation if STAGE not set
    return;
  }

  std::string stage_dir = std::string(stage_env);
  std::string tempdir   = stage_dir + "/shader_reports";

  // Create tempdir if it doesn't exist
  auto tempdir_path = file::Path(tempdir);
  tempdir_path.ensureDirectoryExists();

  file::Path P = report.shader_name;
  auto as_bfs  = P.toBFS().leaf();
  // Generate filename
  std::string filename = FormatString("%s.%s.%d.md", as_bfs.string().c_str(), report.technique_name.c_str(), report.pass_num);

  std::string filepath = tempdir + "/" + filename;

  // Open file for writing
  FILE* fp = fopen(filepath.c_str(), "w");
  if (!fp) {
    printf("Warning: Could not create pass report file: %s\n", filepath.c_str());
    return;
  }

  // Get current time
  time_t now      = time(0);
  char* timestamp = ctime(&now);

  // Write header
  fprintf(
      fp, "# Shader Pass Report: %s - %s - Pass %d\n", report.shader_name.c_str(), report.technique_name.c_str(), report.pass_num);
  fprintf(fp, "Generated: %s\n", timestamp);

  // Write statistics
  fprintf(fp, "## Statistics\n");
  fprintf(fp, "- Vertex Shader: %s (%d lines)\n", report.has_vertex_shader ? "present" : "absent", report.vertex_shader_lines);
  fprintf(
      fp, "- Fragment Shader: %s (%d lines)\n", report.has_fragment_shader ? "present" : "absent", report.fragment_shader_lines);
  fprintf(
      fp, "- Geometry Shader: %s (%d lines)\n", report.has_geometry_shader ? "present" : "absent", report.geometry_shader_lines);

  // Uniform blocks list
  fprintf(fp, "- Uniform Blocks: %zu", report.uniform_block_names.size());
  if (!report.uniform_block_names.empty()) {
    fprintf(fp, " [");
    for (size_t i = 0; i < report.uniform_block_names.size(); ++i) {
      if (i > 0)
        fprintf(fp, ", ");
      fprintf(fp, "%s", report.uniform_block_names[i].c_str());
    }
    fprintf(fp, "]");
  }
  fprintf(fp, "\n");

  // Samplers list
  fprintf(fp, "- Samplers: %zu", report.sampler_names.size());
  if (!report.sampler_names.empty()) {
    fprintf(fp, " [");
    for (size_t i = 0; i < report.sampler_names.size(); ++i) {
      if (i > 0)
        fprintf(fp, ", ");
      fprintf(fp, "%s", report.sampler_names[i].c_str());
    }
    fprintf(fp, "]");
  }
  fprintf(fp, "\n");

  // Descriptor sets used
  fprintf(fp, "- Descriptor Sets Used: %zu", report.descriptor_sets.size());
  if (!report.descriptor_sets.empty()) {
    fprintf(fp, " [");
    bool first = true;
    for (const auto& [dset_id, _] : report.descriptor_sets) {
      if (!first)
        fprintf(fp, ", ");
      fprintf(fp, "%zu", dset_id);
      first = false;
    }
    fprintf(fp, "]");
  }
  fprintf(fp, "\n");

  // Push constants
  fprintf(fp, "- Push Constants: %zu bytes", report.push_constant_size);
  if (!report.push_constant_names.empty()) {
    fprintf(fp, " [");
    for (size_t i = 0; i < report.push_constant_names.size(); ++i) {
      if (i > 0)
        fprintf(fp, ", ");
      fprintf(fp, "%s", report.push_constant_names[i].c_str());
    }
    fprintf(fp, "]");
  }
  fprintf(fp, "\n\n");

  // Write descriptor set layouts
  fprintf(fp, "## Descriptor Set Layouts\n\n");

  for (const auto& [dset_id, ds_info] : report.descriptor_sets) {
    fprintf(fp, "### Descriptor Set %zu - %s.%d\n\n", dset_id, report.technique_name.c_str(), report.pass_num);

    fprintf(
        fp, "**Total Bindings:** %zu | **Total Buffer Size:** %zu bytes\n\n", ds_info.total_bindings, ds_info.total_buffer_size);

    // Create sorted list of all resources
    struct ResourceEntry {
      int binding_id;
      std::string type;
      std::string name;
      std::string stages;
      size_t size;
      bool is_sampler;
    };
    std::vector<ResourceEntry> resources;

    // Add uniform blocks
    for (const auto& ub : ds_info.uniform_blocks) {
      ResourceEntry entry;
      entry.binding_id = ub.binding_id;
      entry.type       = "UBO";
      entry.name       = ub.name;
      entry.size       = ub.total_size;
      entry.is_sampler = false;

      // Abbreviate stages
      std::string stages_str;
      if (ub.stages.count("vertex"))
        stages_str += "V";
      if (ub.stages.count("fragment"))
        stages_str += "F";
      if (ub.stages.count("geometry"))
        stages_str += "G";
      if (ub.stages.count("compute"))
        stages_str += "C";
      entry.stages = stages_str.empty() ? "VF" : stages_str; // default to VF if empty

      resources.push_back(entry);
    }

    // Add samplers with abbreviated types
    for (const auto& sampler : ds_info.samplers) {
      ResourceEntry entry;
      entry.binding_id = sampler.binding_id;

      // Abbreviate sampler types
      if (sampler.type == "sampler2D")
        entry.type = "Tex2D";
      else if (sampler.type == "sampler2DArray")
        entry.type = "Tex2DA";
      else if (sampler.type == "samplerCube")
        entry.type = "TexCube";
      else if (sampler.type == "sampler3D")
        entry.type = "Tex3D";
      else if (sampler.type == "usampler2D")
        entry.type = "uTex2D";
      else
        entry.type = sampler.type;

      entry.name       = sampler.name;
      entry.size       = 0;
      entry.is_sampler = true;

      // Abbreviate stages
      std::string stages_str;
      if (sampler.stages.count("vertex"))
        stages_str += "V";
      if (sampler.stages.count("fragment"))
        stages_str += "F";
      if (sampler.stages.count("geometry"))
        stages_str += "G";
      if (sampler.stages.count("compute"))
        stages_str += "C";
      entry.stages = stages_str.empty() ? "VF" : stages_str; // default to VF if empty

      resources.push_back(entry);
    }

    // Sort by binding ID
    std::sort(resources.begin(), resources.end(), [](const ResourceEntry& a, const ResourceEntry& b) {
      return a.binding_id < b.binding_id;
    });

    // Fixed-width table
    fprintf(fp, "```\n");
    fprintf(fp, "Bind | Size   | Type    | Stages | Name\n");
    fprintf(fp, "-----|--------|---------|--------|---------------------------------\n");

    for (const auto& res : resources) {
      if (res.is_sampler) {
        fprintf(fp, "%4d | %6s | %-7s | %-6s | %s\n", res.binding_id, "-", res.type.c_str(), res.stages.c_str(), res.name.c_str());
      } else {
        fprintf(
            fp,
            "%4d | %6zu | %-7s | %-6s | %s\n",
            res.binding_id,
            res.size,
            res.type.c_str(),
            res.stages.c_str(),
            res.name.c_str());
      }
    }
    fprintf(fp, "```\n");

    fprintf(fp, "\n#### Aggregate Memory Layout\n");
    fprintf(fp, "Total Buffer Size: %zu bytes\n", ds_info.total_buffer_size);
    fprintf(fp, "Binding Strategy: Separate buffers per uniform block\n\n");

    fprintf(fp, "```\n");

    // Detailed memory layout for each uniform block
    for (const auto& ub : ds_info.uniform_blocks) {
      fprintf(fp, "Binding %zu: %s (%zu bytes)\n", ub.binding_id, ub.name.c_str(), ub.total_size);
      fprintf(fp, "+----------+---------------------------+----------+------------------+\n");
      fprintf(fp, "| Offset   | Member                    | Size     | Type             |\n");
      fprintf(fp, "+----------+---------------------------+----------+------------------+\n");

      for (const auto& member : ub.members) {
        fprintf(fp, "| 0x%06zX | %-25s | %-8zu | %-16s |\n", member.offset, member.name.c_str(), member.size, member.type.c_str());
      }
      fprintf(fp, "+----------+---------------------------+----------+------------------+\n\n");
    }

    // List samplers
    for (const auto& sampler : ds_info.samplers) {
      fprintf(fp, "Binding %zu: %s (%s)\n", sampler.binding_id, sampler.name.c_str(), sampler.type.c_str());
    }

    fprintf(fp, "```\n\n");

    // Stage access patterns
    fprintf(fp, "#### Stage Access Patterns\n");

    if (report.has_vertex_shader) {
      fprintf(fp, "- **Vertex Stage**: ");
      bool first = true;
      for (const auto& ub : ds_info.uniform_blocks) {
        if (ub.stages.count("vertex")) {
          if (!first)
            fprintf(fp, ", ");
          fprintf(fp, "%s", ub.name.c_str());
          first = false;
        }
      }
      for (const auto& sampler : ds_info.samplers) {
        if (sampler.stages.count("vertex")) {
          if (!first)
            fprintf(fp, ", ");
          fprintf(fp, "%s", sampler.name.c_str());
          first = false;
        }
      }
      fprintf(fp, "\n");
    }

    if (report.has_fragment_shader) {
      fprintf(fp, "- **Fragment Stage**: ");
      bool first = true;
      for (const auto& ub : ds_info.uniform_blocks) {
        if (ub.stages.count("fragment")) {
          if (!first)
            fprintf(fp, ", ");
          fprintf(fp, "%s", ub.name.c_str());
          first = false;
        }
      }
      for (const auto& sampler : ds_info.samplers) {
        if (sampler.stages.count("fragment")) {
          if (!first)
            fprintf(fp, ", ");
          fprintf(fp, "%s", sampler.name.c_str());
          first = false;
        }
      }
      fprintf(fp, "\n");
    }

    fprintf(fp, "\n");
  }

  // Write warnings if any
  if (!report.warnings.empty()) {
    fprintf(fp, "## Warnings\n");
    for (const auto& warning : report.warnings) {
      fprintf(fp, "- ⚠️ %s\n", warning.c_str());
    }
    fprintf(fp, "\n");
  }

  fclose(fp);

  printf("Pass report written to: %s\n", filepath.c_str());
}

/////////////////////////////////////////////////////////////////////////////////////////////////

size_t impl::ShadLangParser::calculateStd140Offset(const std::string& type, size_t current_offset) {
  // std140 layout rules:
  // - vec4: 16-byte aligned
  // - vec3: 16-byte aligned (treated as vec4)
  // - vec2: 8-byte aligned
  // - float: 4-byte aligned
  // - mat4: 16-byte aligned (4 vec4s)
  // - mat3: 16-byte aligned (3 vec4s)
  // - mat2: 16-byte aligned (2 vec4s)

  size_t alignment = 4; // default to float alignment

  if (type == "vec4" || type == "vec3" || type == "mat4" || type == "mat3" || type == "mat2") {
    alignment = 16;
  } else if (type == "vec2") {
    alignment = 8;
  } else if (type == "int" || type == "uint" || type == "bool" || type == "float") {
    alignment = 4;
  }

  // Round up to alignment
  size_t aligned_offset = ((current_offset + alignment - 1) / alignment) * alignment;
  return aligned_offset;
}

/////////////////////////////////////////////////////////////////////////////////////////////////

size_t impl::ShadLangParser::getStd140Size(const std::string& type) {
  // std140 sizes:
  if (type == "float" || type == "int" || type == "uint" || type == "bool") {
    return 4;
  } else if (type == "vec2") {
    return 8;
  } else if (type == "vec3") {
    return 16; // vec3 is padded to vec4 in std140
  } else if (type == "vec4") {
    return 16;
  } else if (type == "mat2") {
    return 32; // 2 * vec4
  } else if (type == "mat3") {
    return 48; // 3 * vec4
  } else if (type == "mat4") {
    return 64; // 4 * vec4
  }

  // Default/unknown type
  return 4;
}

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::shadlang
/////////////////////////////////////////////////////////////////////////////////////////////////

#endif
