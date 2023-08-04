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

std::string scanner_spec = R"xxx(

macro(M1)           <| "xyz" |>

MULTI_LINE_COMMENT  <| "\/\*([^*]|\*+[^/*])*\*+\/" |>
SINGLE_LINE_COMMENT <| "\/\/.*[\n\r]" |>
WHITESPACE          <| "\s+" |>
NEWLINE             <| "[\n\r]+" |>

STAREQ              <| "\*=" |>
PLUSEQ              <| "\+=" |>
MINUSEQ             <| "\-=" |>
SLASHEQ             <| "\/=" |>

PLUSPLUS            <| "\+\+" |>
MINUSMINUS          <| "\-\-" |>

IS_EQ_TO            <| "==" |>
IS_NEQ_TO           <| "!=" |>
IS_GEQ_TO           <| ">=" |>
IS_LEQ_TO           <| "<=" |>

L_SHIFT             <| "\<\<" |>
R_SHIFT             <| "\>\>" |>

LOGICAL_OR          <| "\|\|" |>
LOGICAL_AND         <| "&&" |>

BITWISE_OR          <| "\|" |>
BITWISE_AND         <| "&" |>

EQUALS              <| "=" |>
COMMA               <| "," |>
COLON               <| ":" |>
SEMICOLON           <| ";" |>
L_SQUARE            <| "\[" |>
R_SQUARE            <| "\]" |>
L_PAREN             <| "\(" |>
R_PAREN             <| "\)" |>
L_CURLY             <| "\{" |>
R_CURLY             <| "\}" |>
L_ANGLE             <| "\<" |>
R_ANGLE             <| "\>" |>
DOT                 <| "\." |>
STAR                <| "\*" |>
PLUS                <| "\+" |>
MINUS               <| "\-" |>
SLASH               <| "\/" |>
NOT                 <| "!" |>
TILDE               <| "~" |>
PERCENT             <| "%" |>
CARET               <| "\^" |>

INTEGER             <| "-?(\d+)" |>
FLOATING_POINT      <| "-?(\d*\.?)(\d+)([eE][-+]?\d+)?" |>

KW_FLOAT            <| "float" |>
KW_INT              <| "int" |>

KW_VEC2             <| "vec2" |>
KW_VEC3             <| "vec3" |>
KW_VEC4             <| "vec4" |>
KW_MAT2             <| "mat2" |>
KW_MAT3             <| "mat3" |>
KW_MAT4             <| "mat4" |>
KW_SAMP1D           <| "sampler1D" |>
KW_SAMP2D           <| "sampler2D" |>
KW_SAMP3D           <| "sampler3D" |>

KW_IVEC2            <| "ivec2" |>
KW_IVEC3            <| "ivec3" |>
KW_IVEC4            <| "ivec4" |>
KW_IMAT2            <| "imat2" |>
KW_IMAT3            <| "imat3" |>
KW_IMAT4            <| "imat4" |>
KW_ISAMP1D          <| "isampler1D" |>
KW_ISAMP2D          <| "isampler2D" |>
KW_ISAMP3D          <| "isampler3D" |>

KW_UVEC2            <| "uvec2" |>
KW_UVEC3            <| "uvec3" |>
KW_UVEC4            <| "uvec4" |>
KW_UMAT2            <| "umat2" |>
KW_UMAT3            <| "umat3" |>
KW_UMAT4            <| "umat4" |>
KW_USAMP1D          <| "usampler1D" |>
KW_USAMP2D          <| "usampler2D" |>
KW_USAMP3D          <| "usampler3D" |>

KW_VTXSHADER        <| "vertex_shader" |>
KW_FRGSHADER        <| "fragment_shader" |>
KW_COMSHADER        <| "compute_shader" |>

KW_UNISET           <| "uniform_set" |>
KW_UNIBLK           <| "uniform_block" |>
KW_VTXIFACE         <| "vertex_interface" |>
KW_FRGIFACE         <| "fragment_interface" |>
KW_INPUTS           <| "inputs" |>
KW_OUTPUTS          <| "outputs" |>

KW_STATEBLOCK       <| "state_block" |>
KW_PASS             <| "pass" |>
KW_TECHNIQUE        <| "technique" |>
KW_BLENDMODE        <| "BlendMode" |>
KW_CULLTEST         <| "CullTest" |>
KW_DEPTHTEST        <| "DepthTest" |>
KW_DEPTHMASK        <| "DepthMask" |>
KW_FXCONFIG         <| "fxconfig" |>
KW_LIBBLOCK         <| "libblock" |>

KW_FUNCTION         <| "function" |>
KW_LAYOUT           <| "layout" |>
KW_FOR              <| "for" |>
KW_IF               <| "if" |>
KW_ELSE             <| "else" |>
KW_RETURN           <| "return" |>
KW_WHILE            <| "while" |>

IDENTIFIER          <| "[a-zA-Z_][a-zA-Z0-9_]*" |>
QUOTED_STRING       <| "" |>

)xxx";

///////////////////////////////////////////////////////////////////////////////

std::string parser_spec = R"xxx(

datatype       <| sel{ KW_FLOAT   KW_INT 
                       KW_VEC2    KW_VEC3    KW_VEC4 
                       KW_MAT2    KW_MAT3    KW_MAT4 
                       KW_SAMP1D  KW_SAMP2D  KW_SAMP3D
                       KW_UVEC2   KW_UVEC3   KW_UVEC4 
                       KW_UMAT2   KW_UMAT3   KW_UMAT4 
                       KW_USAMP1D KW_USAMP2D KW_USAMP3D
                       KW_IVEC2   KW_IVEC3   KW_IVEC4 
                       KW_IMAT2   KW_IMAT3   KW_IMAT4 
                       KW_ISAMP1D KW_ISAMP2D KW_ISAMP3D
               }|>

typed_identifier <|[ datatype IDENTIFIER ]|>

constant       <| sel{INTEGER FLOATING_POINT} |>

member_ref     <| [ DOT IDENTIFIER ] |>
array_ref      <| [ L_SQUARE expression R_SQUARE ] |>

object_subref  <| sel{ member_ref array_ref } |>

inh_list_item  <| [ COLON IDENTIFIER opt{ [L_PAREN IDENTIFIER R_PAREN] } ] |>
inh_list       <| zom{ inh_list_item } |>

exec_arg         <| [ expression opt{COMMA} ] |>
exec_arglist     <| zom{ exec_arg } |>
decl_arglist     <| zom{ [ typed_identifier opt{COMMA} ] } |>

fn_invok <|[
    [ IDENTIFIER ] : "fni_name"
    L_PAREN
    L_PAREN argument_expression_list R_PAREN
    R_PAREN
]|>

rvalue_constructor <|[
    datatype
    L_PAREN
    argument_expression_list
    R_PAREN
]|>

///////////////////////////////////////////////////////////

wronly_operator <| EQUALS |>
rdwr_operator <| sel{ PLUSEQ MINUSEQ STAREQ SLASHEQ } |>

unary_operator <| sel{
  PLUS
  MINUS
  NOT
  TILDE
  STAR
  BITWISE_AND
}|>

///////////////////////////////////////////////////////////

expression <|[
    assignment_expression
    zom{[ COMMA assignment_expression ]}
]|>

assignment_expression <| sel{
    [ opt{datatype} unary_expression wronly_operator assignment_expression ]
    [ unary_expression rdwr_operator assignment_expression ]
    [ conditional_expression ]
}|>

unary_expression <| sel{
    [ postfix_expression ]
    [ unary_operator unary_expression ]
}|>

postfix_expression <|[
    primary_expression
    postfix_expression_tail
]|>

primary_expression <| sel{ 
  [ L_PAREN expression R_PAREN ]
  [ IDENTIFIER ] 
  [ constant ]
  [ rvalue_constructor ]
  // [ QUOTED_STRING ] // no strings in shadlang yet..
}|>

argument_expression_list <|[
    assignment_expression
    zom{ [ COMMA assignment_expression ] }
]|>

postfix_expression_tail <|[
    opt{[
        sel{
            [ L_SQUARE expression R_SQUARE postfix_expression_tail ]
            [ L_PAREN argument_expression_list R_PAREN postfix_expression_tail ]
            [ DOT IDENTIFIER postfix_expression_tail ]
            [ PLUSPLUS postfix_expression_tail ]
            [ MINUSMINUS postfix_expression_tail ]
        }
    ]}
]|>

conditional_expression <|[
    logical_or_expression
]|>

logical_or_expression <|[
    logical_and_expression
    zom{[ LOGICAL_OR logical_and_expression ]}
]|>

logical_and_expression <|[
    inclusive_or_expression
    zom{[ LOGICAL_AND inclusive_or_expression ]}
]|>

inclusive_or_expression <|[
    exclusive_or_expression
    zom{[ LOGICAL_OR exclusive_or_expression ]}
]|>

exclusive_or_expression <|[
    and_expression
    zom{[ CARET and_expression ]}
]|>

and_expression <|[
    equality_expression
    //and_expression_tail
    zom{[ BITWISE_AND equality_expression ]}
]|>

equality_expression <|[
    relational_expression
    equality_expression_tail
]|>

equality_expression_tail <|[
    opt{
        sel{
            [ IS_EQ_TO relational_expression equality_expression_tail ]
            [ IS_NEQ_TO relational_expression equality_expression_tail ]
        }
    }
]|>

relational_expression <|[
    shift_expression
    relational_expression_tail
]|>

relational_expression_tail <|[
    opt{
        sel{
            [ L_ANGLE shift_expression relational_expression_tail ]
            [ R_ANGLE shift_expression relational_expression_tail ]
            [ IS_LEQ_TO shift_expression relational_expression_tail ]
            [ IS_GEQ_TO shift_expression relational_expression_tail ]
        }
    }
]|>

shift_expression <|[
    additive_expression
    shift_expression_tail
]|>

shift_expression_tail <|[
    opt{
        sel{
            [ L_SHIFT additive_expression shift_expression_tail ]
            [ R_SHIFT additive_expression shift_expression_tail ]
        }
    }
]|>

additive_expression <|[
    multiplicative_expression
    additive_expression_tail
]|>

additive_expression_tail <|[
    opt{
        sel{
            [ PLUS multiplicative_expression additive_expression_tail ]
            [ MINUS multiplicative_expression additive_expression_tail ]
        }
    }
]|>

multiplicative_expression <|[
    unary_expression
    multiplicative_expression_tail
]|>

multiplicative_expression_tail <|[
    opt{
        sel{
            [ STAR unary_expression multiplicative_expression_tail ]
            [ SLASH unary_expression multiplicative_expression_tail ]
            [ PERCENT unary_expression multiplicative_expression_tail ]
        }
    }
]|>

///////////////////////////////////////////////////////////

if_statement <|[
    KW_IF
    L_PAREN
    expression
    R_PAREN
    statement
    opt{[
        KW_ELSE
        statement
    ]}
]|>

while_statement <|[
    KW_WHILE
    L_PAREN
    expression
    R_PAREN
    statement
]|>

for_statement <|[
    KW_FOR
    L_PAREN
    opt{expression_statement}
    opt{expression_statement}
    opt{expression}
    R_PAREN
    statement
]|>

return_statement <|[
    KW_RETURN
    opt{[
        expression
    ]}
    SEMICOLON
]|>

compound_statement <|[
    L_CURLY
    statement_list
    R_CURLY
]|>

expression_statement <| sel{ 
    [ expression SEMICOLON ]
    [ SEMICOLON ]
} |>

statement <| sel{ 
    [ expression_statement ]
    [ compound_statement ]
    [ if_statement]
    [ while_statement]
    [ for_statement ]
    [ return_statement ]
} |>

statement_list <| zom{ statement } |>

///////////////////////////////////////////////////////////

fn_def <|[
    KW_FUNCTION
    [ IDENTIFIER ] : "fn_name"
    L_PAREN
    decl_arglist : "fn_args"
    R_PAREN
    L_CURLY
    statement_list : "fn_statements"
    R_CURLY
]|>

fn2_def <|[
    datatype
    [ IDENTIFIER ] : "fn2_name"
    L_PAREN
    decl_arglist : "fn2_args"
    R_PAREN
    L_CURLY
    statement_list : "fn2_statements"
    R_CURLY
]|>

libblock <|[
    KW_LIBBLOCK
    [ IDENTIFIER ] : "lib_name"
    zom{inh_list_item} : "lib_dependencies"
    L_CURLY
    zom{fn2_def}
    R_CURLY
]|>

vtx_shader <|[
    KW_VTXSHADER
    [ IDENTIFIER ] : "vtx_name"
    zom{inh_list_item} : "vtx_dependencies"
    compound_statement : "vtx_statements"
]|>

frg_shader <|[
    KW_FRGSHADER
    [ IDENTIFIER ] : "frg_name"
    zom{ inh_list_item } : "frg_dependencies"
    compound_statement : "frg_statements"
]|>

com_shader <|[
    KW_COMSHADER
    [ IDENTIFIER ] : "com_name"
    zom{ inh_list_item } : "com_dependencies"
    compound_statement : "com_statements"
]|>

data_decl <| [typed_identifier SEMICOLON] |>

data_decls <| zom{ data_decl } |>

uniset <|[
  KW_UNISET
  [ IDENTIFIER ] : "uniset_name"
  L_CURLY
  data_decls : "uniset_decls"
  R_CURLY
]|>

uniblk <|[
  KW_UNIBLK
  [ IDENTIFIER ] : "uniblk_name"
  L_CURLY
  data_decls : "uniblk_decls"
  R_CURLY
]|>

iface_input <|[ 
  typed_identifier 
  opt{ [COLON IDENTIFIER] }
  SEMICOLON 
]|>

iface_inputs <|[
  KW_INPUTS
  L_CURLY
  zom{ iface_input } : "inputlist"
  R_CURLY
]|>

iface_layout  <|[
   KW_LAYOUT 
   L_PAREN 
   IDENTIFIER 
   EQUALS 
   INTEGER 
   R_PAREN
]|>

iface_outputs <|[
  KW_OUTPUTS
  L_CURLY
  zom{ 
      [ opt{iface_layout} data_decl ] : "output_decl"
  }
  R_CURLY
]|>

vtx_iface <|[
  KW_VTXIFACE
  [ IDENTIFIER ] : "vif_name"
  zom{ inh_list_item } : "vif_dependencies"
  L_CURLY
  iface_inputs
  iface_outputs
  R_CURLY
]|>

frg_iface <|[
  KW_FRGIFACE
  [ IDENTIFIER ] : "fif_name"
  zom{ inh_list_item } : "fif_dependencies"
  L_CURLY
  iface_inputs
  iface_outputs
  R_CURLY
]|>

sb_key <| sel{ KW_BLENDMODE KW_CULLTEST KW_DEPTHTEST KW_DEPTHMASK } |>

sb_item <|[
    [ sb_key EQUALS IDENTIFIER SEMICOLON ] : "stateblock_item"
]|>

stateblock <|[
  KW_STATEBLOCK
  [ IDENTIFIER ] : "sb_name"
  zom{ inh_list_item } : "sb_dependencies"
  L_CURLY
  zom{ sb_item }
  R_CURLY
]|>

pass_binding_key <| sel{  KW_VTXSHADER KW_FRGSHADER KW_STATEBLOCK } |>

pass_binding <|[
  pass_binding_key EQUALS IDENTIFIER SEMICOLON
]|>

pass <|[
  KW_PASS
  [ IDENTIFIER ] : "pass_name"
  L_CURLY
  zom{ 
      pass_binding : "pass_item"
  }
  R_CURLY
]|>


fxconfig_ref <|[
  KW_FXCONFIG
  EQUALS
  [ IDENTIFIER ] : "fxconfigref_name"
  SEMICOLON
]|>

fxconfig_decl <|[
  KW_FXCONFIG
  [ IDENTIFIER ] : "fxconfigdecl_name"
  L_CURLY
  zom{[
    [ IDENTIFIER ] : "fxconfigdecl_key"
    EQUALS
    [ QUOTED_STRING ] : "fxconfigdecl_val"
    SEMICOLON
  ]}
  R_CURLY
]|>

technique <|[
  KW_TECHNIQUE
  [ IDENTIFIER ] : "technique_name"
  L_CURLY
  opt{ fxconfig_ref }
  oom{ pass }
  R_CURLY
]|>

translatable <| 
  sel{ 
    fn_def 
    vtx_shader 
    frg_shader 
    com_shader 
    uniset 
    uniblk 
    vtx_iface 
    frg_iface 
    stateblock
    technique
    fxconfig_decl 
    libblock
  }
|>

translation_unit <|
  zom{ translatable } 
|>

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
    _name = "shadlang";
    //_DEBUG_MATCH       = true;
    //_DEBUG_INFO        = true;
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
    onPost("fif_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("lib_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("sb_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("pass_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("fxconfigdecl_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("fxconfigref_name", [=](match_ptr_t match) { objectNameAst(match); });
    onPost("technique_name", [=](match_ptr_t match) { objectNameAst(match); });
    // onPost("primary_var_ref", [=](match_ptr_t match) { objectNameAst(match); });
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
    // onPost("term", [=](match_ptr_t match) { auto term = ast_create<SHAST::Term>(match); });
    ///////////////////////////////////////////////////////////
    onPost("exec_arg", [=](match_ptr_t match) { auto fn_arg = ast_create<SHAST::FunctionInvokationArgument>(match); });
    ///////////////////////////////////////////////////////////
    //onPost("exec_arglist", [=](match_ptr_t match) { auto fn_args = ast_create<SHAST::FunctionInvokationArguments>(match); });
    ///////////////////////////////////////////////////////////
    onPost("fn_invok", [=](match_ptr_t match) { auto fn_invok = ast_create<SHAST::FunctionInvokation>(match); });
    ///////////////////////////////////////////////////////////
    onPost("primary_expression", [=](match_ptr_t match) {
      auto primary   = ast_create<SHAST::Primary>(match);
      primary->_name = "primary";
    });
    ///////////////////////////////////////////////////////////
    onPost("multiplicative_expression", [=](match_ptr_t match) {
      auto product      = ast_create<SHAST::Product>(match);
      product->_name    = "product";
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
    onPost("additive_expression", [=](match_ptr_t match) {
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
    if (0) {
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
    }
    ///////////////////////////////////////////////////////////
    if (0)
      onPost("lvalue", [=](match_ptr_t match) { //
        auto ast_lvalue         = ast_create<SHAST::LValue>(match);
        auto seq                = match->asShared<Sequence>();
        auto identifier         = seq->_items[0]->followImplAsShared<ClassMatch>();
        ast_lvalue->_identifier = identifier->_token->text;
      });
    ///////////////////////////////////////////////////////////
    if (0)
      onPost("astatement_varref", [=](match_ptr_t match) { //
        auto ast_varref = ast_create<SHAST::AssignmentStatementVarRef>(match);
        auto seq        = match->asShared<Sequence>();
        auto var_item   = seq->_items[0];
        auto var_seq    = var_item->followImplAsShared<Sequence>();

        // ast_varref->_identifier = var->_token->text;
      });
    ///////////////////////////////////////////////////////////
    if(0)onPost("assignment_statement", [=](match_ptr_t match) { //
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
    onPost("rvalue_constructor", [=](match_ptr_t match) {
      auto ast_rvc       = ast_create<SHAST::RValueConstructor>(match);
      auto seq           = match->asShared<Sequence>();
      auto ast_dt        = ast_get<SHAST::DataType>(seq->_items[0]);
      ast_rvc->_datatype = ast_dt->_datatype;
      ast_dt->_showDOT   = false;
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
    onPost("libblock", [=](match_ptr_t match) {
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
      tid->_showDOT          = false;
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
      auto ast_output_decl         = ast_create<SHAST::InterfaceOutput>(match);
      auto seq                     = match->asShared<Sequence>();
      auto ddecl_layout            = seq->_items[0]->followImplAsShared<Optional>();
      auto ddecl_seq               = seq->_items[1]->followImplAsShared<Sequence>();
      auto tid                     = ast_get<SHAST::TypedIdentifier>(ddecl_seq->_items[0]->asShared<Proxy>()->_selected);
      ast_output_decl->_identifier = tid->_identifier;
      ast_output_decl->_datatype   = tid->_datatype;
    });
    ///////////////////////////////////////////////////////////
    onPost("iface_outputs", [=](match_ptr_t match) {
      auto ast_iface_outputs   = ast_create<SHAST::InterfaceOutputs>(match);
      auto seq                 = match->asShared<Sequence>();
      auto fn_name             = seq->_items[1]->followImplAsShared<ClassMatch>();
      ast_iface_outputs->_name = fn_name->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("vtx_iface", [=](match_ptr_t match) {
      auto ast_vtx_iface   = ast_create<SHAST::VertexInterface>(match);
      auto seq             = match->asShared<Sequence>();
      auto objname         = ast_get<SHAST::ObjectName>(seq->_items[1]);
      ast_vtx_iface->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("frg_iface", [=](match_ptr_t match) {
      auto frg_iface   = ast_create<SHAST::FragmentInterface>(match);
      auto seq         = match->asShared<Sequence>();
      auto objname     = ast_get<SHAST::ObjectName>(seq->_items[1]);
      frg_iface->_name = objname->_name;
    });
    ///////////////////////////////////////////////////////////
    onPost("stateblock_item", [=](match_ptr_t match) {
      auto ast_sb_item    = ast_create<SHAST::StateBlockItem>(match);
      auto seq            = match->asShared<Sequence>();
      auto key_item       = seq->_items[0]->followImplAsShared<OneOf>()->_selected;
      ast_sb_item->_key   = key_item->followImplAsShared<ClassMatch>()->_token->text;
      ast_sb_item->_value = seq->_items[2]->followImplAsShared<ClassMatch>()->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("stateblock", [=](match_ptr_t match) {
      auto ast_sb   = ast_create<SHAST::StateBlock>(match);
      auto seq      = match->asShared<Sequence>();
      auto name_seq = seq->_items[1]->asShared<Sequence>();
      auto sb_name  = name_seq->_items[0]->followImplAsShared<ClassMatch>();
      ast_sb->_name = sb_name->_token->text;
    });
    ///////////////////////////////////////////////////////////
    onPost("inh_list_item", [=](match_ptr_t match) {
      auto seq     = match->asShared<Sequence>();
      auto fn_name = seq->_items[1]->followImplAsShared<ClassMatch>();
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
      }
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
          auto it = translation_unit->_translatables_by_name.find(translatable->_name);
          OrkAssert(it == translation_unit->_translatables_by_name.end());
          translation_unit->_translatables_by_name[translatable->_name] = translatable;
          translation_unit->_translatable_by_order.push_back(translatable);
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