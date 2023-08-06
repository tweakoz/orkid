#pragma once

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <ork/util/crc.h>
#include <ork/kernel/varmap.inl>
#include <ork/lev2/config.h>
#include <ork/file/file.h>

#if defined(USE_ORKSL_LANG)
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::shadlang {

enum class TokenClass : uint64_t {
  CrcEnum(SINGLE_LINE_COMMENT),
  CrcEnum(MULTI_LINE_COMMENT),
  CrcEnum(WHITESPACE),
  CrcEnum(NEWLINE),
  CrcEnum(SEMICOLON),
  CrcEnum(L_PAREN),
  CrcEnum(R_PAREN),
  CrcEnum(L_CURLY),
  CrcEnum(R_CURLY),
  CrcEnum(EQUALS),
  CrcEnum(STAR),
  CrcEnum(PLUS),
  CrcEnum(MINUS),
  CrcEnum(COMMA),
  CrcEnum(FLOATING_POINT),
  CrcEnum(INTEGER),
  CrcEnum(KW_OR_ID)
};

namespace SHAST {
///////////////////////////////////////////////////////////////////////////////

struct AstNode;
struct Expression;
struct PrimaryExpression;
struct DataType;
struct ArgumentDeclaration;
struct AssignmentStatement;
struct FunctionDef;
struct Translatable;
struct TranslationUnit;

using astnode_ptr_t           = std::shared_ptr<AstNode>;
using expression_ptr_t        = std::shared_ptr<Expression>;
using primaryexpression_ptr_t = std::shared_ptr<PrimaryExpression>;
using datatype_ptr_t          = std::shared_ptr<DataType>;
using argument_decl_ptr_t     = std::shared_ptr<ArgumentDeclaration>;
using assignment_stmt_ptr_t   = std::shared_ptr<AssignmentStatement>;
using fndef_ptr_t             = std::shared_ptr<FunctionDef>;
using translatable_ptr_t      = std::shared_ptr<Translatable>;
using translationunit_ptr_t   = std::shared_ptr<TranslationUnit>;

using import_map_t = std::map<std::string, translationunit_ptr_t>;

/////////////////////


void _dumpAstTreeVisitor(astnode_ptr_t node, int indent, std::string& out_str);

using visitor_fn_t = std::function<void(astnode_ptr_t)>;
using walk_visitor_fn_t = std::function<bool(astnode_ptr_t)>;

struct Visitor{
  visitor_fn_t _on_pre;
  visitor_fn_t _on_post;
  std::stack<astnode_ptr_t> _nodestack;
  svar64_t _userdata;
};

using visitor_ptr_t = std::shared_ptr<Visitor>;
using astnode_map_t = std::unordered_map<std::string, astnode_ptr_t>;

///////////////////////////////////////////////////////////////////////////////
} // namespace SHAST


SHAST::translationunit_ptr_t parseFromString(const std::string& shader_text);
SHAST::translationunit_ptr_t parseFromFile(file::Path shader_path);
std::string toASTstring(SHAST::astnode_ptr_t);
std::string toGLFX1(SHAST::translationunit_ptr_t top);
std::string toDotFile(SHAST::translationunit_ptr_t top);

} // namespace ork::lev2::shadlang
///////////////////////////////////////////////////////////////////////////////
#include "shadlang_nodes.h"
#endif