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
static logchannel_ptr_t logchan         = logger()->createChannel("ORKSLIMPL", fvec3(1, 1, .9), true);
static logchannel_ptr_t logchan_grammar = logger()->createChannel("ORKSLGRAM", fvec3(1, 1, .8), true);
static logchannel_ptr_t logchan_lexer   = logger()->createChannel("ORKSLLEXR", fvec3(1, 1, .7), true);

///////////////////////////////////////////////////////////////////////////////

std::string scanner_spec = R"xxx(
    macro(M1)           -< "xyz" >-
    MULTI_LINE_COMMENT  -< "\/\*([^*]|\*+[^/*])*\*+\/" >-
    SINGLE_LINE_COMMENT -< "\/\/.*[\n\r]" >-
    WHITESPACE          -< "\s+" >-
    NEWLINE             -< "[\n\r]+" >-
    EQUALS              -< "=" >-
    COMMA               -< "," >-
    COLON               -< ":" >-
    SEMICOLON           -< ";" >-
    L_PAREN             -< "\(" >-
    R_PAREN             -< "\)" >-
    L_CURLY             -< "\{" >-
    R_CURLY             -< "\}" >-
    DOT                 -< "\." >-
    STAR                -< "\*" >-
    PLUS                -< "\+" >-
    MINUS               -< "\-" >-
    FLOATING_POINT      -< "-?(\d*\.?)(\d+)([eE][-+]?\d+)?" >-
    INTEGER             -< "-?(\d+)" >-
    FUNCTION            -< "function" >-
    KW_FLOAT            -< "float" >-
    KW_INT              -< "int" >-
    KW_VEC2             -< "vec2" >-
    KW_VEC3             -< "vec3" >-
    KW_VEC4             -< "vec4" >-
    KW_MAT2             -< "mat2" >-
    KW_MAT3             -< "mat3" >-
    KW_MAT4             -< "mat4" >-
    KW_VTXSHADER        -< "vertex_shader" >-
    KW_FRGSHADER        -< "fragment_shader" >-
    KW_COMSHADER        -< "compute_shader" >-
    KW_UNISET           -< "uniform_set" >-
    KW_UNIBLK           -< "uniform_block" >-
    KW_VTXIFACE         -< "vertex_interface" >-
    KW_FRGIFACE         -< "fragment_interface" >-
    KW_INPUTS           -< "inputs" >-
    KW_OUTPUTS          -< "outputs" >-
    KW_SAMP1D           -< "sampler1D" >-
    KW_SAMP2D           -< "sampler2D" >-
    KW_SAMP3D           -< "sampler3D" >-
    KW_OR_ID            -< "[a-zA-Z_][a-zA-Z0-9_]*" >-
)xxx";

///////////////////////////////////////////////////////////////////////////////

std::string parser_spec = R"xxx(
    datatype       -< sel{ KW_FLOAT KW_INT 
                           KW_VEC2 KW_VEC3 KW_VEC4 
                           KW_MAT2 KW_MAT3 KW_MAT4 
                           KW_SAMP1D KW_SAMP2D KW_SAMP3D } >-
    number         -< sel{FLOATING_POINT INTEGER} >-
    kw_or_id       -< KW_OR_ID >-
    dot            -< DOT >-
    l_paren        -< L_PAREN >-
    r_paren        -< R_PAREN >-
    plus           -< PLUS >-
    minus          -< MINUS >-
    star           -< STAR >-
    l_curly        -< L_CURLY >-
    r_curly        -< R_CURLY >-
    semicolon      -< SEMICOLON >-
    colon          -< COLON >-
    equals         -< EQUALS >-
    kw_function    -< FUNCTION >-
    kw_vtxshader   -< KW_VTXSHADER >-
    kw_frgshader   -< KW_FRGSHADER >-
    kw_comshader   -< KW_COMSHADER >-
    kw_uniset      -< KW_UNISET >-
    kw_uniblk      -< KW_UNIBLK >-
    kw_vtxiface    -< KW_VTXIFACE >-
    kw_frgiface    -< KW_FRGIFACE >-
    kw_inputs      -< KW_INPUTS >-
    kw_outputs     -< KW_OUTPUTS >-

    member_ref     -< [ dot kw_or_id ] >-

    inh_list_item  -< [ colon kw_or_id ] >-
    inh_list       -< zom{ inh_list_item } >-

    fn_arg         -< [ expression opt{COMMA} ] >-
    fn_args        -< zom{ fn_arg } >-

    fn_invok -< [
        [ kw_or_id ] : "fni_name"
        l_paren
        fn_args
        r_paren
    ] >-

    product -< [ primary opt{ [star primary] } ] >-

    sum -< sel{
        [ product plus product ] : "add"
        [ product minus product ] : "sub"
        product : "pro"
    } >-

    expression -< [ sum ] >-

    term -< [ l_paren expression r_paren ] >-

    typed_identifier -< [datatype kw_or_id] >-

    primary -< sel{ fn_invok
                    number
                    term
                    [ kw_or_id zom{member_ref} ] : "primary_var_ref"
                  } >-

    assignment_statement -< [
        sel { 
          [ typed_identifier ] : "astatement_vardecl"
          [ kw_or_id ] : "astatement_varref"
        }
        equals
        expression
    ] >-

    statement -< sel{ 
        [ assignment_statement semicolon ]
        [ fn_invok semicolon ]
        semicolon
    } >-

    arg_list -< zom{ [ typed_identifier opt{COMMA} ] } >-
    statement_list -< zom{ statement } >-

    fn_def -< [
        kw_function
        [ kw_or_id ] : "fn_name"
        l_paren
        arg_list : "args"
        r_paren
        l_curly
        statement_list : "fn_statements"
        r_curly
    ] >-
    
    vtx_shader -< [
        kw_vtxshader
        [ kw_or_id ] : "vtx_name"
        zom{inh_list_item} : "vtx_dependencies"
        l_curly
        statement_list : "vtx_statements"
        r_curly
    ] >-

    frg_shader -< [
        kw_frgshader
        [ kw_or_id ] : "frg_name"
        zom{ inh_list_item } : "frg_dependencies"
        l_curly
        statement_list : "frg_statements"
        r_curly
    ] >-

    com_shader -< [
        kw_comshader
        [ kw_or_id ] : "com_name"
        zom{ inh_list_item } : "com_dependencies"
        l_curly
        statement_list : "com_statements"
        r_curly
    ] >-

    data_decl -< [typed_identifier semicolon] >-

    data_decls -< zom{ data_decl } >-

    uniset -< [
      kw_uniset
      [ kw_or_id ] : "uniset_name"
      l_curly
      data_decls : "uniset_decls"
      r_curly
    ] >-

    uniblk -< [
      kw_uniblk
      [ kw_or_id ] : "uniblk_name"
      l_curly
      data_decls : "uniblk_decls"
      r_curly
    ] >-

    iface_input -< [ typed_identifier opt{ [colon kw_or_id] } semicolon ] >-

    iface_inputs -< [
      kw_inputs
      l_curly
      zom{ iface_input } : "inputlist"
      r_curly
    ] >-

    iface_outputs -< [
      kw_outputs
      l_curly
      zom{ 
        [ data_decl ] : "output_decl"
      }
      r_curly
    ] >-

    vtx_iface -< [
      kw_vtxiface
      [ kw_or_id ] : "vif_name"
      zom{ inh_list_item } : "vif_dependencies"
      l_curly
      iface_inputs
      iface_outputs
      r_curly
    ] >-

    frg_iface -< [
      kw_frgiface
      [ kw_or_id ] : "fif_name"
      zom{ inh_list_item } : "fif_dependencies"
      l_curly
      iface_inputs
      iface_outputs
      r_curly
    ] >-

    translatable -< sel{ fn_def vtx_shader frg_shader com_shader uniset uniblk vtx_iface frg_iface } >-

    translation_unit -< zom{ translatable } >-

)xxx";

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
    _name              = "shadlang";
    _DEBUG_MATCH       = true;
    _DEBUG_INFO        = true;
    auto scanner_match = this->loadPEGScannerSpec(scanner_spec);
    OrkAssert(scanner_match);

    auto parser_match = this->loadPEGParserSpec(parser_spec);
    OrkAssert(parser_match);
    OrkAssert(_DEBUG_MATCH);
    ///////////////////////////////////////////////////////////
    // parser should be compiled and linked at this point
    ///////////////////////////////////////////////////////////
    auto expression           = rule("expression");
    auto product              = rule("product");
    auto term                 = rule("term");
    auto primary              = rule("primary");
    auto semicolon            = rule("SEMICOLON");
    auto assignment_statement = rule("assignment_statement");

    ///////////////////////////////////////////////////////////

    auto objectNameAst = [&](match_ptr_t match) {
      auto objname     = ast_create<SHAST::ObjectName>(match);
      auto fn_name_seq = match->asShared<Sequence>();
      auto fn_name     = fn_name_seq->_items[0]->followImplAsShared<ClassMatch>();
      objname->_name   = fn_name->_token->text;
    };
    ///////////////////////////////////////////////////////////
    onPost("fn_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("fni_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("vtx_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("frg_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("com_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("uniset_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("uniblk_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("vif_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("fif_name", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("primary_var_ref", [=](match_ptr_t match) { objectNameAst(match); });
    ///////////////////////////////////////////////////////////
    onPost("member_ref", [=](match_ptr_t match) { //
      auto member_ref = ast_create<SHAST::MemberRef>(match); 
      auto seq        = match->asShared<Sequence>();
      member_ref->_member = seq->_items[1]->followImplAsShared<ClassMatch>()->_token->text;
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
      auto selected       = match->asShared<OneOf>()->_selected;
      auto datatype       = ast_create<SHAST::DataType>(match);
      datatype->_datatype = selected->asShared<ClassMatch>()->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("term", [=](match_ptr_t match) { auto term = ast_create<SHAST::Term>(match); });
    ///////////////////////////////////////////////////////////
    onPost("fn_arg", [=](match_ptr_t match) { auto fn_arg = ast_create<SHAST::FunctionInvokationArgument>(match); });
    ///////////////////////////////////////////////////////////
    onPost("fn_args", [=](match_ptr_t match) { auto fn_args = ast_create<SHAST::FunctionInvokationArguments>(match); });
    ///////////////////////////////////////////////////////////
    onPost("fn_invok", [=](match_ptr_t match) { auto fn_invok = ast_create<SHAST::FunctionInvokation>(match); });
    ///////////////////////////////////////////////////////////
    onPost("primary", [=](match_ptr_t match) {
      auto primary   = ast_create<SHAST::Primary>(match);
      primary->_name = "primary";
    });
    ///////////////////////////////////////////////////////////
    onPost("product", [=](match_ptr_t match) {
      auto product      = ast_create<SHAST::Product>(match);
      product->_name    = "product";
      auto seq          = match->asShared<Sequence>();
      auto primary1     = seq->_items[0];
      auto primary1_ast = ast_get<SHAST::Primary>(primary1);
      auto opt          = seq->itemAsShared<Optional>(1);
      auto primary2     = opt->_subitem;

      product->_primaries.push_back(primary1_ast);
      if (primary2) {
        auto seq          = primary2->asShared<Sequence>();
        primary2          = seq->_items[1];
        auto primary2_ast = ast_get<SHAST::Primary>(primary2);
        product->_primaries.push_back(primary2_ast);

      } else { // "prI1"
      }
    });
    ///////////////////////////////////////////////////////////
    onPost("sum", [=](match_ptr_t match) {
      auto sum      = ast_create<SHAST::Sum>(match);
      auto selected = match->asShared<OneOf>()->_selected;
      match->asShared<OneOf>()->dump("sum");
      if (selected->_matcher == product) {
        sum->_left = ast_get<SHAST::Product>(selected);
        sum->_op   = '_';
      } else if (selected->_matcher->_name == "add") {
        auto seq    = selected->asShared<Sequence>();
        sum->_left  = ast_get<SHAST::Product>(seq->_items[0]);
        sum->_right = ast_get<SHAST::Product>(seq->_items[2]);
        sum->_op    = '+';
      } else if (selected->_matcher->_name == "sub") {
        auto seq    = selected->asShared<Sequence>();
        sum->_left  = ast_get<SHAST::Product>(seq->_items[0]);
        sum->_right = ast_get<SHAST::Product>(seq->_items[2]);
        sum->_op    = '-';
      } else {
        OrkAssert(false);
      }
    });
    ///////////////////////////////////////////////////////////
    onPost("expression", [=](match_ptr_t match) { //
      auto expression = ast_create<SHAST::Expression>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("astatement_vardecl", [=](match_ptr_t match) { //
      auto var_decl = ast_create<SHAST::AssignmentStatementVarDecl>(match);
    });
    onLink("astatement_vardecl", [=](match_ptr_t match) { //
      auto var_decl         = ast_get<SHAST::AssignmentStatementVarDecl>(match);
      auto tid              = var_decl->childAs<SHAST::TypedIdentifier>(0);
      var_decl->_datatype   = tid->_datatype;
      var_decl->_identifier = tid->_identifier;
      var_decl->_descend    = false;
    });
    ///////////////////////////////////////////////////////////
    onPost("astatement_varref", [=](match_ptr_t match) { //
      auto varref = ast_create<SHAST::AssignmentStatementVarRef>(match);
    });
    ///////////////////////////////////////////////////////////
    onPost("assignment_statement", [=](match_ptr_t match) { //
      auto statement = ast_create<SHAST::AssignmentStatement>(match);
      /*auto ass1of   = match->asShared<Sequence>()->itemAsShared<OneOf>(0);
      if (ass1of->_selected->_matcher == variableDeclaration) {
        auto seq      = ass1of->_selected->asShared<Sequence>();
        auto datatype = ast_get<SHAST::Product>(seq->_items[0]);
        //auto expr     = match->asShared<Sequence>()->itemAsShared<SHAST::Expression>(2);
      } else if (ass1of->_selected->_matcher == variableReference) {
        auto expr             = match->asShared<Sequence>()->itemAsShared<SHAST::Expression>(2);
        ast_node->_datatype   = nullptr;
        ast_node->_expression = expr;
      } else {
        // OrkAssert(false);
      }*/
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
    onPost("arg_list", [=](match_ptr_t match) {
      auto arg_list = ast_create<SHAST::ArgumentList>(match);
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
        auto seq = item->asShared<Sequence>();
        // auto statement = ast_get<SHAST::Stat>(seq->_items[0]);
        // arg_list->_arguments.push_back(tid);
      }
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
      auto seq       = match->asShared<Sequence>();
      seq->dump("POST:data_decl");
      auto dtsel       = seq->_items[0]->asShared<Proxy>()->_selected;
      auto dt          = ast_get<SHAST::DataType>(dtsel);
      auto kwid        = seq->_items[1]->followImplAsShared<ClassMatch>();
      data_decl->_name = kwid->_token->text;
    });
    onLink("data_decl", [=](match_ptr_t match) {
      auto data_decl         = ast_get<SHAST::DataDeclaration>(match);
      auto tid               = data_decl->childAs<SHAST::TypedIdentifier>(0);
      data_decl->_datatype   = tid->_datatype;
      data_decl->_identifier = tid->_identifier;
      data_decl->_descend    = false;
    });
    ///////////////////////////////////////////////////////////
    onPost("data_decls", [=](match_ptr_t match) { auto data_decls = ast_create<SHAST::DataDeclarations>(match); });
    ///////////////////////////////////////////////////////////
    onPost("uniset", [=](match_ptr_t match) {
      auto uniset   = ast_create<SHAST::UniformSet>(match);
      auto seq      = match->asShared<Sequence>();
      auto objname  = ast_get<SHAST::ObjectName>(seq->_items[1]);
      uniset->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("uniblk", [=](match_ptr_t match) {
      auto uniblk   = ast_create<SHAST::UniformBlk>(match);
      auto seq      = match->asShared<Sequence>();
      auto objname  = ast_get<SHAST::ObjectName>(seq->_items[1]);
      uniblk->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("iface_input", [=](match_ptr_t match) {
      auto iface_input = ast_create<SHAST::InterfaceInput>(match);
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
    });
    ///////////////////////////////////////////////////////////
    onPost("iface_inputs", [=](match_ptr_t match) {
      auto iface_inputs   = ast_create<SHAST::InterfaceInputs>(match);
      auto seq            = match->asShared<Sequence>();
      auto fn_name        = seq->_items[1]->followImplAsShared<ClassMatch>();
      iface_inputs->_name = fn_name->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("output_decl", [=](match_ptr_t match) {
      auto output_decl         = ast_create<SHAST::InterfaceOutput>(match);
      auto seq                 = match->asShared<Sequence>();
      auto ddecl_seq           = seq->_items[0]->followImplAsShared<Sequence>();
      auto tid                 = ast_get<SHAST::TypedIdentifier>(ddecl_seq->_items[0]->asShared<Proxy>()->_selected);
      output_decl->_identifier = tid->_identifier;
      output_decl->_datatype   = tid->_datatype;
    });
    ///////////////////////////////////////////////////////////
    onPost("iface_outputs", [=](match_ptr_t match) {
      auto iface_outputs   = ast_create<SHAST::InterfaceOutputs>(match);
      auto seq             = match->asShared<Sequence>();
      auto fn_name         = seq->_items[1]->followImplAsShared<ClassMatch>();
      iface_outputs->_name = fn_name->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("vtx_iface", [=](match_ptr_t match) {
      auto vtx_iface   = ast_create<SHAST::VertexInterface>(match);
      auto seq         = match->asShared<Sequence>();
      auto objname     = ast_get<SHAST::ObjectName>(seq->_items[1]);
      vtx_iface->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("frg_iface", [=](match_ptr_t match) {
      auto frg_iface   = ast_create<SHAST::FragmentInterface>(match);
      auto seq         = match->asShared<Sequence>();
      auto objname     = ast_get<SHAST::ObjectName>(seq->_items[1]);
      frg_iface->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("inh_list_item", [=](match_ptr_t match) {
      auto inh_list_item   = ast_create<SHAST::Dependency>(match);
      auto seq             = match->asShared<Sequence>();
      auto fn_name         = seq->_items[1]->followImplAsShared<ClassMatch>();
      inh_list_item->_name = fn_name->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("translation_unit", [=](match_ptr_t match) {
      auto translation_unit = ast_create<SHAST::TranslationUnit>(match);
      auto the_nom          = match->asShared<NOrMore>();
      for (auto item : the_nom->_items) {

        SHAST::translatable_ptr_t translatable;

        std::string ast_name;

        if (auto as_fndef = try_ast_get<SHAST::Translatable>(item)) {
          translatable = as_fndef;
        }

        if (translatable) {
          auto it = translation_unit->_translatables.find(translatable->_name);
          OrkAssert(it == translation_unit->_translatables.end());
          translation_unit->_translatables[translatable->_name] = translatable;
        }
      }
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

  void _dumpAstTreeVisitor(SHAST::astnode_ptr_t node, int indent) {
    auto indentstr = std::string(indent * 2, ' ');
    printf("%s%s\n", indentstr.c_str(), node->desc().c_str());
    if (node->_descend) {
      for (auto c : node->_children) {
        _dumpAstTreeVisitor(c, indent + 1);
      }
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
    top_view.dump("top_view");
    auto slv    = std::make_shared<ScannerLightView>(top_view);
    _tu_matcher = findMatcherByName("translation_unit");
    OrkAssert(_tu_matcher);
    auto match = this->match(_tu_matcher, slv, [this](match_ptr_t m) { _buildAstTreeVisitor(m); });
    OrkAssert(match);

    auto ast_top = match->_uservars.typedValueForKey<SHAST::astnode_ptr_t>("astnode").value();
    printf("///////////////////////////////\n");
    printf("// AST TREE\n");
    printf("///////////////////////////////\n");
    _dumpAstTreeVisitor(ast_top, 0);
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