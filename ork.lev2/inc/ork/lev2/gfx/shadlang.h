#pragma once 

#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <ork/util/crc.h>
#include <ork/kernel/varmap.inl>
#include <ork/lev2/config.h>

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
struct VariableReference;
struct Expression;
struct Primary;
struct Product;
struct Sum;
struct DataType;
struct ArgumentDeclaration;
struct AssignmentStatement;
struct FunctionDef;
struct FunctionDefs;

using astnode_ptr_t = std::shared_ptr<AstNode>;
using expression_ptr_t = std::shared_ptr<Expression>;
using varref_ptr_t = std::shared_ptr<VariableReference>;
using primary_ptr_t = std::shared_ptr<Primary>;
using product_ptr_t = std::shared_ptr<Product>;
using sum_ptr_t = std::shared_ptr<Sum>;
using datatype_ptr_t = std::shared_ptr<DataType>;
using argument_decl_ptr_t = std::shared_ptr<ArgumentDeclaration>;
using assignment_stmt_ptr_t = std::shared_ptr<AssignmentStatement>;
using fndef_ptr_t = std::shared_ptr<FunctionDef>;
using fndefs_ptr_t = std::shared_ptr<FunctionDefs>;
///////////////////// 

struct AstNode {
  std::string _name;
  virtual ~AstNode() {
  }
  std::vector<astnode_ptr_t> _children;
  astnode_ptr_t _parent;
};

struct Statement : public AstNode {
  inline Statement() { _name = "Statement"; }
};
//
struct DataType : public AstNode {
  inline DataType() { _name = "DataType"; }
};
//
struct VariableReference : public AstNode { //
  inline VariableReference() { _name = "VariableReference"; }
};
//
struct AssignmentStatement : public Statement {
  inline AssignmentStatement() { _name = "AssignmentStatement"; }
    datatype_ptr_t _datatype;
    expression_ptr_t _expression;
};
struct EmptyStatement : public Statement{
};
//
struct Expression : public AstNode { //
  inline Expression() { _name = "Expression"; }
    sum_ptr_t _sum;
};
//
struct Sum : public AstNode {
  inline Sum() { _name = "Sum"; }
    product_ptr_t _left;
    product_ptr_t _right;
    char _op = 0;
};
//
struct Product : public AstNode {
  inline Product() { _name = "Product"; }
    std::vector<primary_ptr_t> _primaries;
};
//
struct Primary : public AstNode { //
  inline Primary() { _name = "Primary"; }
    svar64_t _impl;
};
//
struct Literal : public AstNode {};
//
struct NumericLiteral : public AstNode {
  inline NumericLiteral() { _name = "NumericLiteral"; }
};
//
struct FloatLiteral : public NumericLiteral { //
  inline FloatLiteral() { _name = "FloatLiteral"; }
  float _value = 0.0f;
};
//
struct IntegerLiteral : public NumericLiteral { //
  inline IntegerLiteral() { _name = "IntegerLiteral"; }
  int _value = 0;
};
//
struct Term : public AstNode { //
  inline Term() { _name = "Term"; }
    expression_ptr_t _subexpression;
};
//
struct ArgumentDeclaration : public AstNode{
  inline ArgumentDeclaration() { _name = "ArgumentDeclaration"; }
    std::string _variable_name;
    datatype_ptr_t _datatype;
};
//
struct FunctionDef : public AstNode { //
  inline FunctionDef() { _name = "FunctionDef"; }
  datatype_ptr_t _returntype;
  std::vector<argument_decl_ptr_t> _arguments;
  std::vector<assignment_stmt_ptr_t> _statements;

  //void createDotFile(file::Path outpath) const;
};
struct FunctionDefs : public AstNode { //
  inline FunctionDefs() { _name = "FunctionDefs"; }
  std::map<std::string,fndef_ptr_t> _fndefs;
};


///////////////////////////////////////////////////////////////////////////////
} // namespace SHAST {

SHAST::fndefs_ptr_t parse_fndefs( const std::string& shader_text );

} // namespace ork::lev2::shadlang {
///////////////////////////////////////////////////////////////////////////////
#endif