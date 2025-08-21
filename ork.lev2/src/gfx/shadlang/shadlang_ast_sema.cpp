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
    //dumpAstNode(id_node);
    auto match     = slp->matchForAstNode(id_node);
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

    ////////////////////
    // item 0 (type)
    ////////////////////

    auto sel = seq->itemAsShared<OneOf>(0)->_selected;
    std::string type_name;
    if (auto as_cm = sel->tryAsShared<ClassMatch>()) {
      type_name = as_cm.value()->_token->text;
    } else { // its a DataTypeNode
      auto seq  = sel->asShared<Sequence>();
      auto sel0 = seq->tryItemAsShared<OneOf>(0);
      auto sel2 = seq->tryItemAsShared<OneOf>(2);
      if(sel0){ // SamplerType ?
        auto cm   = sel0.value()->_selected->asShared<ClassMatch>();
        type_name = cm->_token->text;
      }
      else if(sel2){ // DataType ?
        auto cm   = sel2.value()->_selected->asShared<ClassMatch>();
        type_name = cm->_token->text;
      }
      else{
        OrkAssert(false);
      }
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
      } else if constexpr (std::is_same<node_t, Technique>::value) {
        printf("_name<%s> Technique<%s>\n", slp->_name.c_str(), the_name.c_str());
        n->template setValueForKey<std::string>("raw_name", the_name);
      } else{
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

      //printf( "cache: objname: %s\n", the_name.c_str() );

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

  if(0){
    printf( "ShadLangParser<%p:%s> ImportCount<%zu>\n", //
          (void*) slp, //
          slp->_name.c_str(), //
          nodes.size() );
  }

  for (auto import_node : nodes) {
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

    // Use Path::resolveRelativeTo for proper resolution
    file::Path import_file_path(raw_import_path);
    
    // Use the toplevel path from cache if shader path is empty
    file::Path container_path = slp->_shader_path;
    if (!container_path.isAbsolute() && slp->_slp_cache) {
      container_path = slp->_slp_cache->_toplevel_path;
      printf("shadlang using toplevel_path from cache: '%s'\n", container_path.c_str());
    }
    
    // This will handle both absolute paths (with schemes) and relative paths correctly
    auto proc_import_path = import_file_path.resolveRelativeTo(container_path);
    printf("shadlang import resolved: container='%s' import='%s' -> resolved='%s'\n", 
           container_path.c_str(), raw_import_path.c_str(), proc_import_path.c_str());
    
    import_node->setValueForKey<std::string>("proc_import_path", proc_import_path.c_str());

    ////////////////////////////////////////////////////////
    // fetch translation unit
    ////////////////////////////////////////////////////////

    auto cache = slp->_slp_cache;
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
    auto intnode = AstNode::collectNodesOfType<SemaIntegerLiteral>(did_node)[0];
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
   //printf("  num_inputs<%zu>\n", inputs.size());
  for (auto input : inputs) {
    auto tid = input->childAs<TypedIdentifier>(0);
    if(tid){
      auto colon = input->childAs<COLON>(1);
      auto semantic = input->childAs<SemaIdentifier>(2);
      if( colon and semantic ){
        auto sema_id = semantic->typedValueForKey<std::string>("identifier_name").value();
        //printf( "sema_id<%s>\n", sema_id.c_str() );
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
        bool check_smp_sets    = false;
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
          check_smp_sets   = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
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
          check_smp_sets   = true;
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
          check_smp_sets   = true;
          check_uni_sets   = true;
          check_uni_blks   = true;
          //check_vtx_iface  = true;
          check_geo_iface  = true;
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
        } else if (check_stateblocks and check_inheritance(inh_name, "sblk", slp->_stateblocks)) {
          auto semanode   = std::make_shared<SemaInheritStateBlock>();
          semanode->_name = FormatString("SemaInheritStateBlock: %s", inh_name.c_str());
          slp->replaceInParent(inh_item, semanode);
          count++;
        }
        else if( inh_name!="default" ){
          printf( "check_inheritance<%s> not found\n", inh_name.c_str() );
          printf( "  Available library blocks:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_library_blocks ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available type blocks:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_type_blocks ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available sampler sets:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_sampler_sets ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available uniform sets:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_uniform_sets ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available uniform blocks:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_uniform_blocks ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available vertex interfaces:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_vertex_interfaces ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available fragment interfaces:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_fragment_interfaces ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available geometry interfaces:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_geometry_interfaces ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available compute interfaces:\n" );
          for( auto& [name, obj] : slp->_slp_cache->_compute_interfaces ){
            printf( "    %s\n", name.c_str() );
          }
          printf( "  Available state blocks:\n" );
          for( auto& [name, obj] : slp->_stateblocks ){
            printf( "    %s\n", name.c_str() );
          }
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

void _semaAttachMergedResourceNodesToPasses(impl::ShadLangParser* slp, astnode_ptr_t top) {
  auto passes = AstNode::collectNodesOfType<Pass>(top);
  
  printf("=== MERGED RESOURCE ATTACHMENT ===\n");
  printf("  Passes found: %zu\n", passes.size());
  printf("  Vertex interfaces: %zu\n", slp->_slp_cache->_vertex_interfaces.size());
  printf("  Fragment interfaces: %zu\n", slp->_slp_cache->_fragment_interfaces.size());
  printf("  Vertex shaders: %zu\n", slp->_slp_cache->_vertex_shaders.size());
  printf("  Fragment shaders: %zu\n", slp->_slp_cache->_fragment_shaders.size());
  printf("  Sampler sets: %zu\n", slp->_slp_cache->_sampler_sets.size());
  printf("  Uniform blocks: %zu\n", slp->_slp_cache->_uniform_blocks.size());
  
  for (auto pass : passes) {
    auto pass_name = pass->typedValueForKey<std::string>("object_name").value();
    printf("  Processing pass: %s\n", pass_name.c_str());
    
    // Step 1: Collect all shaders referenced by this pass
    std::vector<astnode_ptr_t> pass_shaders;
    
    // Find shader references in the pass
    auto vtx_refs = AstNode::collectNodesOfType<VertexShaderRef>(pass);
    auto frg_refs = AstNode::collectNodesOfType<FragmentShaderRef>(pass);
    auto geo_refs = AstNode::collectNodesOfType<GeometryShaderRef>(pass);
    auto com_refs = AstNode::collectNodesOfType<ComputeShaderRef>(pass);
    
    printf("    Vertex refs: %zu, Fragment refs: %zu\n", vtx_refs.size(), frg_refs.size());
    
    // Resolve actual shader objects using symbol tables
    for (auto vtx_ref : vtx_refs) {
      auto shader_name = vtx_ref->typedValueForKey<std::string>("ref_id").value();
      printf("    Looking for vertex shader: %s\n", shader_name.c_str());
      auto shader = slp->_slp_cache->_vertex_shaders.find(shader_name);
      if (shader != slp->_slp_cache->_vertex_shaders.end()) {
        printf("    Found vertex shader: %s\n", shader_name.c_str());
        pass_shaders.push_back(shader->second);
      } else {
        printf("    WARNING: Vertex shader not found: %s\n", shader_name.c_str());
      }
    }
    
    for (auto frg_ref : frg_refs) {
      auto shader_name = frg_ref->typedValueForKey<std::string>("ref_id").value();
      printf("    Looking for fragment shader: %s\n", shader_name.c_str());
      auto shader = slp->_slp_cache->_fragment_shaders.find(shader_name);
      if (shader != slp->_slp_cache->_fragment_shaders.end()) {
        printf("    Found fragment shader: %s\n", shader_name.c_str());
        pass_shaders.push_back(shader->second);
      } else {
        printf("    WARNING: Fragment shader not found: %s\n", shader_name.c_str());
      }
    }
    
    for (auto geo_ref : geo_refs) {
      auto shader_name = geo_ref->typedValueForKey<std::string>("ref_id").value();
      auto shader = slp->_slp_cache->_geometry_shaders.find(shader_name);
      if (shader != slp->_slp_cache->_geometry_shaders.end()) {
        pass_shaders.push_back(shader->second);
      }
    }
    
    for (auto com_ref : com_refs) {
      auto shader_name = com_ref->typedValueForKey<std::string>("ref_id").value();
      auto shader = slp->_slp_cache->_compute_shaders.find(shader_name);
      if (shader != slp->_slp_cache->_compute_shaders.end()) {
        pass_shaders.push_back(shader->second);
      }
    }
    
    printf("    Total shaders for pass: %zu\n", pass_shaders.size());
    
    // Step 2: Collect all resources from all shaders using symbol tables
    std::map<int, std::map<std::string, MergedShaderResources::ResourceBinding>> merged_descriptor_sets;
    std::set<std::string> processed_sampler_resources; // Track samplers to prevent duplicates
    std::set<std::string> processed_uniform_block_resources; // Track uniform blocks to prevent duplicates
    
    // ADD: Binding counter per descriptor set
    std::map<int, int> next_binding_id_per_descriptor_set;
    
    for (size_t shader_index = 0; shader_index < pass_shaders.size(); shader_index++) {
      auto shader = pass_shaders[shader_index];
      printf("    Processing shader[%zu]: %s\n", shader_index, shader->typedValueForKey<std::string>("object_name").value().c_str());
      printf("    Shader pointer: %p\n", (void*)shader.get());
      
      // Determine shader type for debugging
      std::string shader_type = "unknown";
      if (std::dynamic_pointer_cast<VertexShader>(shader)) {
        shader_type = "vertex";
      } else if (std::dynamic_pointer_cast<FragmentShader>(shader)) {
        shader_type = "fragment";
      } else if (std::dynamic_pointer_cast<GeometryShader>(shader)) {
        shader_type = "geometry";
      } else if (std::dynamic_pointer_cast<ComputeShader>(shader)) {
        shader_type = "compute";
      }
      printf("      Shader type: %s\n", shader_type.c_str());
      
      // Instead of using InheritanceTracker, directly collect inherited resources from the shader's AST
      // Look for SemaInheritSamplerSet and SemaInheritUniformBlk nodes in the shader
      std::vector<astnode_ptr_t> inherited_sampler_sets;
      std::vector<astnode_ptr_t> inherited_uniform_blocks;
      
      // Collect direct inherited resources from the shader
      auto direct_sampler_sets = AstNode::collectNodesOfType<SemaInheritSamplerSet>(shader);
      auto direct_uniform_blocks = AstNode::collectNodesOfType<SemaInheritUniformBlk>(shader);
      printf("      Direct SemaInheritSamplerSet nodes: %zu\n", direct_sampler_sets.size());
      printf("      Direct SemaInheritUniformBlk nodes: %zu\n", direct_uniform_blocks.size());
      inherited_sampler_sets.insert(inherited_sampler_sets.end(), direct_sampler_sets.begin(), direct_sampler_sets.end());
      inherited_uniform_blocks.insert(inherited_uniform_blocks.end(), direct_uniform_blocks.begin(), direct_uniform_blocks.end());
      
      // Also check interfaces that this shader inherits from
      auto inherited_interfaces = AstNode::collectNodesOfType<SemaInheritVertexInterface>(shader);
      auto inherited_fragment_interfaces = AstNode::collectNodesOfType<SemaInheritFragmentInterface>(shader);
      auto inherited_geometry_interfaces = AstNode::collectNodesOfType<SemaInheritGeometryInterface>(shader);
      auto inherited_compute_interfaces = AstNode::collectNodesOfType<SemaInheritComputeInterface>(shader);
      
      printf("      Inherited vertex interfaces: %zu\n", inherited_interfaces.size());
      printf("      Inherited fragment interfaces: %zu\n", inherited_fragment_interfaces.size());
      printf("      Inherited geometry interfaces: %zu\n", inherited_geometry_interfaces.size());
      printf("      Inherited compute interfaces: %zu\n", inherited_compute_interfaces.size());
      
      // Collect all interfaces this shader inherits from
      std::vector<astnode_ptr_t> all_inherited_interfaces;
      for (auto iface : inherited_interfaces) {
        auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
        auto iface_obj = slp->_slp_cache->_vertex_interfaces.find(iface_name);
        if (iface_obj != slp->_slp_cache->_vertex_interfaces.end()) {
          all_inherited_interfaces.push_back(iface_obj->second);
        }
      }
      for (auto iface : inherited_fragment_interfaces) {
        auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
        auto iface_obj = slp->_slp_cache->_fragment_interfaces.find(iface_name);
        if (iface_obj != slp->_slp_cache->_fragment_interfaces.end()) {
          all_inherited_interfaces.push_back(iface_obj->second);
        }
      }
      for (auto iface : inherited_geometry_interfaces) {
        auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
        auto iface_obj = slp->_slp_cache->_geometry_interfaces.find(iface_name);
        if (iface_obj != slp->_slp_cache->_geometry_interfaces.end()) {
          all_inherited_interfaces.push_back(iface_obj->second);
        }
      }
      for (auto iface : inherited_compute_interfaces) {
        auto iface_name = iface->typedValueForKey<std::string>("inherit_id").value();
        auto iface_obj = slp->_slp_cache->_compute_interfaces.find(iface_name);
        if (iface_obj != slp->_slp_cache->_compute_interfaces.end()) {
          all_inherited_interfaces.push_back(iface_obj->second);
        }
      }
      
      printf("      Total inherited interfaces: %zu\n", all_inherited_interfaces.size());
      
      // Recursively collect all inherited resources from interfaces and library blocks
      std::function<void(astnode_ptr_t, std::vector<astnode_ptr_t>&, std::vector<astnode_ptr_t>&)> 
      collectInheritedResources = [&](astnode_ptr_t node, 
                                     std::vector<astnode_ptr_t>& sampler_sets, 
                                     std::vector<astnode_ptr_t>& uniform_blocks) {
        // Check for direct sampler sets and uniform blocks in this node
        auto node_sampler_sets = AstNode::collectNodesOfType<SemaInheritSamplerSet>(node);
        auto node_uniform_blocks = AstNode::collectNodesOfType<SemaInheritUniformBlk>(node);
        sampler_sets.insert(sampler_sets.end(), node_sampler_sets.begin(), node_sampler_sets.end());
        uniform_blocks.insert(uniform_blocks.end(), node_uniform_blocks.begin(), node_uniform_blocks.end());
        
        // Check for inherited interfaces in this node
        auto node_vertex_interfaces = AstNode::collectNodesOfType<SemaInheritVertexInterface>(node);
        auto node_fragment_interfaces = AstNode::collectNodesOfType<SemaInheritFragmentInterface>(node);
        auto node_geometry_interfaces = AstNode::collectNodesOfType<SemaInheritGeometryInterface>(node);
        auto node_compute_interfaces = AstNode::collectNodesOfType<SemaInheritComputeInterface>(node);
        
        // Recursively process inherited interfaces
        for (auto iface_inherit : node_vertex_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj = slp->_slp_cache->_vertex_interfaces.find(iface_name);
          if (iface_obj != slp->_slp_cache->_vertex_interfaces.end()) {
            collectInheritedResources(iface_obj->second, sampler_sets, uniform_blocks);
          }
        }
        for (auto iface_inherit : node_fragment_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj = slp->_slp_cache->_fragment_interfaces.find(iface_name);
          if (iface_obj != slp->_slp_cache->_fragment_interfaces.end()) {
            collectInheritedResources(iface_obj->second, sampler_sets, uniform_blocks);
          }
        }
        for (auto iface_inherit : node_geometry_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj = slp->_slp_cache->_geometry_interfaces.find(iface_name);
          if (iface_obj != slp->_slp_cache->_geometry_interfaces.end()) {
            collectInheritedResources(iface_obj->second, sampler_sets, uniform_blocks);
          }
        }
        for (auto iface_inherit : node_compute_interfaces) {
          auto iface_name = iface_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto iface_obj = slp->_slp_cache->_compute_interfaces.find(iface_name);
          if (iface_obj != slp->_slp_cache->_compute_interfaces.end()) {
            collectInheritedResources(iface_obj->second, sampler_sets, uniform_blocks);
          }
        }
        
        // Check for inherited library blocks in this node
        auto node_library_blocks = AstNode::collectNodesOfType<SemaInheritLibrary>(node);
        for (auto lib_inherit : node_library_blocks) {
          auto lib_name = lib_inherit->typedValueForKey<std::string>("inherit_id").value();
          auto lib_obj = slp->_slp_cache->_library_blocks.find(lib_name);
          if (lib_obj != slp->_slp_cache->_library_blocks.end()) {
            collectInheritedResources(lib_obj->second, sampler_sets, uniform_blocks);
          }
        }
      };
      
      // Collect all inherited resources recursively from all interfaces
      for (auto iface : all_inherited_interfaces) {
        collectInheritedResources(iface, inherited_sampler_sets, inherited_uniform_blocks);
      }
      
      // ALSO: Collect resources directly from the shader itself recursively
      collectInheritedResources(shader, inherited_sampler_sets, inherited_uniform_blocks);
      
      printf("      Inherited sampler sets: %zu\n", inherited_sampler_sets.size());
      printf("      Inherited uniform blocks: %zu\n", inherited_uniform_blocks.size());
      
      // Debug: Print what resources were found
      for (auto inherit_node : inherited_sampler_sets) {
        auto sset_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        printf("        Found inherited sampler set: %s\n", sset_name.c_str());
      }
      for (auto inherit_node : inherited_uniform_blocks) {
        auto ublk_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        printf("        Found inherited uniform block: %s\n", ublk_name.c_str());
      }
      
      // Process inherited sampler sets
      for (auto inherit_node : inherited_sampler_sets) {
        auto sset_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        printf("      Processing inherited sampler set: %s\n", sset_name.c_str());
        
        // Find the actual sampler set in the symbol table
        auto sset_it = slp->_slp_cache->_sampler_sets.find(sset_name);
        if (sset_it != slp->_slp_cache->_sampler_sets.end()) {
          auto sampler_set = sset_it->second;
          
          // Get descriptor set ID
          int descriptor_set_id = 0; // Default
          auto dset_ids = AstNode::collectNodesOfType<DescriptorSetId>(sampler_set);
          if (dset_ids.size() > 0) {
            descriptor_set_id = dset_ids[0]->typedValueForKey<int>("descriptor_set_id").value();
          }
          
          // Process samplers in this set
          auto sampler_decls = AstNode::collectNodesOfType<SamplerDeclaration>(sampler_set);
          for (size_t binding_id = 0; binding_id < sampler_decls.size(); binding_id++) {
            auto sampler_decl = sampler_decls[binding_id];
            auto sampler_type = sampler_decl->childAs<SamplerType>(0);
            auto sampler_name = sampler_decl->childAs<SemaIdentifier>(1);
            
            if (sampler_type && sampler_name) {
              auto type_name = sampler_type->typedValueForKey<std::string>("sampler_type").value();
              auto name = sampler_name->typedValueForKey<std::string>("identifier_name").value();
              
              // Create unique key for this sampler resource
              std::string resource_key = sset_name + "::" + name;
              
              // Check for duplicates - if already processed, skip
              if (processed_sampler_resources.find(resource_key) != processed_sampler_resources.end()) {
                continue; // Skip duplicate
              }
              
              // Use counter to assign unique binding number
              int binding_id = next_binding_id_per_descriptor_set[descriptor_set_id]++;
              MergedShaderResources::ResourceBinding binding;
              binding.type = MergedShaderResources::ResourceBinding::Type::Sampler;
              binding.name = name;
              binding.datatype = type_name;
              binding.binding_id = binding_id;
              binding.original_source = sset_name;
              
              merged_descriptor_sets[descriptor_set_id][resource_key] = binding;
              processed_sampler_resources.insert(resource_key);
              printf("        Added sampler: %s (%s) binding %d\n", name.c_str(), type_name.c_str(), binding_id);
            }
          }
        } else {
          printf("      WARNING: Sampler set not found in symbol table: %s\n", sset_name.c_str());
        }
      }
      
      // Process inherited uniform blocks
      for (auto inherit_node : inherited_uniform_blocks) {
        auto ublk_name = inherit_node->typedValueForKey<std::string>("inherit_id").value();
        printf("      Processing inherited uniform block: %s\n", ublk_name.c_str());
        
        // Find the actual uniform block in the symbol table
        auto ublk_it = slp->_slp_cache->_uniform_blocks.find(ublk_name);
        if (ublk_it != slp->_slp_cache->_uniform_blocks.end()) {
          auto uniform_block = ublk_it->second;
          
          // Get descriptor set ID
          int descriptor_set_id = 0; // Default
          auto dset_ids = AstNode::collectNodesOfType<DescriptorSetId>(uniform_block);
          if (dset_ids.size() > 0) {
            descriptor_set_id = dset_ids[0]->typedValueForKey<int>("descriptor_set_id").value();
          }
          
          // Create unique key for this uniform block resource
          std::string resource_key = ublk_name;
          
          // Check for duplicates - if already processed, skip
          if (processed_uniform_block_resources.find(resource_key) != processed_uniform_block_resources.end()) {
            continue; // Skip duplicate
          }
          
          // Use counter to assign unique binding number
          int binding_id = next_binding_id_per_descriptor_set[descriptor_set_id]++;
          MergedShaderResources::ResourceBinding binding;
          binding.type = MergedShaderResources::ResourceBinding::Type::UniformBlock;
          binding.name = ublk_name;
          binding.datatype = "uniform_block";
          binding.binding_id = binding_id;
          binding.original_source = ublk_name;
          
          merged_descriptor_sets[descriptor_set_id][resource_key] = binding;
          processed_uniform_block_resources.insert(resource_key);
          printf("        Added uniform block: %s binding %d\n", ublk_name.c_str(), binding_id);
        } else {
          printf("      WARNING: Uniform block not found in symbol table: %s\n", ublk_name.c_str());
        }
      }
    }
    
    // Step 3: Create AST nodes for the merged resources
    // Always create a merged resource node, even if empty
    if (true) {
      size_t num_descriptor_sets = merged_descriptor_sets.size();
      if (num_descriptor_sets == 0) {
        // Create a single empty descriptor set 0 if none exist
        merged_descriptor_sets[0] = {};
        num_descriptor_sets = 1;
        printf("    Creating EMPTY merged resource node with 1 descriptor set\n");
      } else {
        printf("    Creating merged resource node with %zu descriptor sets\n", num_descriptor_sets);
      }
      auto merged_node = std::make_shared<MergedShaderResourcesNode>();
      merged_node->_name = FormatString("MergedResources: %s", pass_name.c_str());
      // Create descriptor set nodes
      for (auto& [descriptor_set_id, bindings] : merged_descriptor_sets) {
        auto set_node = std::make_shared<DescriptorSetNode>();
        set_node->_name = FormatString("DescriptorSet: %d", descriptor_set_id);
        set_node->_descriptor_set_id = descriptor_set_id;
        // Group resources by source
        std::map<std::string, std::vector<std::pair<std::string, MergedShaderResources::ResourceBinding>>> sources_by_type;
        for (auto& [key, binding] : bindings) {
          sources_by_type[binding.original_source].push_back({key, binding});
        }
        // Create source nodes with their resource bindings as children
        for (auto& [source_name, resource_pairs] : sources_by_type) {
          auto source_node = std::make_shared<DescriptorSetSourceNode>();
          // Determine source type from the first resource binding
          std::string source_type = "unknown";
          if (!resource_pairs.empty()) {
            auto& first_binding = resource_pairs[0].second;
            if (first_binding.type == MergedShaderResources::ResourceBinding::Type::Sampler) {
              source_type = "sampler_set";
            } else if (first_binding.type == MergedShaderResources::ResourceBinding::Type::UniformBlock) {
              source_type = "uniform_block";
            }
          }
          source_node->_name = FormatString("From: %s (%s)", source_name.c_str(), source_type.c_str());
          source_node->_source_name = source_name;
          source_node->_source_type = source_type;
          // Add resource bindings as children of this source node
          int binding_counter = 0;
          for (auto& [key, binding] : resource_pairs) {
            auto binding_node = std::make_shared<ResourceBindingNode>();
            binding_node->_name = FormatString("b%d : %s\n%s", 
                                             binding_counter++,
                                             binding.datatype.c_str(),
                                             binding.name.c_str());
            binding_node->_binding_id = binding.binding_id;
            binding_node->_binding_name = binding.name;
            binding_node->_datatype = binding.datatype;
            binding_node->_original_source = binding.original_source;
            binding_node->_resource_type = binding.type;
            source_node->appendChild(binding_node);
          }
          set_node->appendChild(source_node);
        }
        merged_node->appendChild(set_node);
      }
      pass->appendChild(merged_node);
    }
  }
  printf("=== END MERGED RESOURCE ATTACHMENT ===\n");
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void _semaTransformVfPassToExplicitPass(impl::ShadLangParser* slp, astnode_ptr_t top) {
  printf("=== VF_PASS TRANSFORMATION ===\n");
  
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
    printf("  Processing VtxFrgPass in technique: %s\n", tech_name.c_str());
    
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
    
    printf("    Found vf_pass: vs=%s, ps=%s, sb=%s\n", 
           vtx_name.c_str(), frg_name.c_str(), sb_name.c_str());
    
    // Create a new Pass node
    auto pass_node = std::make_shared<Pass>();
    std::string pass_name = FormatString("p%d", pass_counter++);
    pass_node->_name = FormatString("Pass %s", pass_name.c_str());
    
    // Store the pass name in the values map
    pass_node->setValueForKey<std::string>("object_name", pass_name);
    pass_node->setValueForKey<std::string>("pass_name", pass_name);
    
    // Create VertexShaderRef node
    if (!vtx_name.empty()) {
      auto vs_ref = std::make_shared<VertexShaderRef>();
      vs_ref->_name = FormatString("VertexShaderRef: %s", vtx_name.c_str());
      vs_ref->setValueForKey<std::string>("ref_id", vtx_name);
      
      // Create SemaIdentifier child
      auto vs_sema_id = std::make_shared<SemaIdentifier>();
      vs_sema_id->_name = FormatString("SemaIdentifier: %s", vtx_name.c_str());
      vs_sema_id->setValueForKey<std::string>("identifier_name", vtx_name);
      vs_ref->appendChild(vs_sema_id);
      
      pass_node->appendChild(vs_ref);
    }
    
    // Create FragmentShaderRef node
    if (!frg_name.empty()) {
      auto ps_ref = std::make_shared<FragmentShaderRef>();
      ps_ref->_name = FormatString("FragmentShaderRef: %s", frg_name.c_str());
      ps_ref->setValueForKey<std::string>("ref_id", frg_name);
      
      // Create SemaIdentifier child
      auto ps_sema_id = std::make_shared<SemaIdentifier>();
      ps_sema_id->_name = FormatString("SemaIdentifier: %s", frg_name.c_str());
      ps_sema_id->setValueForKey<std::string>("identifier_name", frg_name);
      ps_ref->appendChild(ps_sema_id);
      
      pass_node->appendChild(ps_ref);
    }
    
    // Create StateBlockRef node if specified
    if (!sb_name.empty()) {
      auto sb_ref = std::make_shared<StateBlockRef>();
      sb_ref->_name = FormatString("StateBlockRef: %s", sb_name.c_str());
      sb_ref->setValueForKey<std::string>("ref_id", sb_name);
      
      // Create SemaIdentifier child
      auto sb_sema_id = std::make_shared<SemaIdentifier>();
      sb_sema_id->_name = FormatString("SemaIdentifier: %s", sb_name.c_str());
      sb_sema_id->setValueForKey<std::string>("identifier_name", sb_name);
      sb_ref->appendChild(sb_sema_id);
      
      pass_node->appendChild(sb_ref);
    }
    
    // Replace the VtxFrgPass node with the new Pass node
    slp->replaceInParent(vfpass, pass_node);
    
    printf("    Replaced VtxFrgPass with Pass %s (%zu children)\n", 
           pass_name.c_str(), pass_node->_children.size());
  }
  
  printf("=== END VF_PASS TRANSFORMATION (processed %d vf_pass nodes) ===\n", (int)vfpass_nodes.size());
}

/////////////////////////////////////////////////////////////////////////////////////////////////

void impl::ShadLangParser::semaAST(astnode_ptr_t top) {

  //printf("ShadLangParser<%p:%s> semaAST CP-A\n", this, _name.c_str() );

  //////////////////////////////////
  // Pass 2 - Imports
  //////////////////////////////////

  _semaCollectNamedOfType<ImportDirective>(this, top, _import_directives);
  _semaPerformImports(this, top);

  //printf("ShadLangParser<%p:%s> semaAST CP-B\n", this, _name.c_str() );

  //////////////////////////////////

  if (1) {
    _semaNameBuiltInDataTypes(this, top);
    _semaNameSamplerTypes(this, top);
    _semaNormalizeDtUserTypes(this, top);

    _semaNameIdentifers(this, top);

    //_semaNameIdentiferCalls(this, top);
    _semaNameTypedIdentifers(this, top);
  }

  //printf("ShadLangParser<%p:%s> semaAST CP-C\n", this, _name.c_str() );

  //////////////////////////////////

  if (1) {
    _semaIntegerLiterals(this, top);
    _semaFloatLiterals(this, top);
    _semaExtractDescriptorSetIds(this, top);
  }

  //printf("ShadLangParser<%p:%s> semaAST CP-D\n", this, _name.c_str() );

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

  //printf("ShadLangParser<%p:%s> semaAST CP-E\n", this, _name.c_str() );

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

  //printf("ShadLangParser<%p:%s> semaAST CP-F\n", this, _name.c_str() );

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

  //////////////////////////////////
  // Pass 5. 
  //////////////////////////////////

  _semaTransformVfPassToExplicitPass(this, top);
  _semaAttachMergedResourceNodesToPasses(this,top);

  //////////////////////////////////
  // finalize
  //////////////////////////////////
  //printf("ShadLangParser<%p:%s> semaAST CP-G\n", this, _name.c_str() );

  auto as_tu                    = std::dynamic_pointer_cast<TranslationUnit>(top);

  printf("=== TRANSLATABLES BEFORE FINALIZATION ===\n");
  for( auto trans_item : _slp_cache->_translatables ){
    auto name = trans_item.first;
    printf("  translatable: %s\n", name.c_str());
  }
  printf("=== END TRANSLATABLES ===\n");

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
