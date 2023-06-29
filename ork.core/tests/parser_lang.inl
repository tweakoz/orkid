#pragma once

#include <ork/util/parser.h>
#include <utpp/UnitTest++.h>
#include <ork/util/crc.h>
#include <string.h>
#include <math.h>

using namespace ork;

///////////////////////////////////////////////////////////////////////////////

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

///////////////////////////////////////////////////////////////////////////////
// AST
///////////////////////////////////////////////////////////////////////////////

namespace AST {

struct VariableReference;
struct Expression;
struct Primary;
struct Product;
struct Sum;
struct DataType;

using expression_ptr_t = std::shared_ptr<Expression>;
using varref_ptr_t = std::shared_ptr<VariableReference>;
using primary_ptr_t = std::shared_ptr<Primary>;
using product_ptr_t = std::shared_ptr<Product>;
using sum_ptr_t = std::shared_ptr<Sum>;
using datatype_ptr_t = std::shared_ptr<DataType>;

///////////////////// 

struct AstNode {
  virtual ~AstNode() {
  }
};

struct Statement : public AstNode {};
//
struct DataType : public AstNode {
    std::string _name;
};
//
struct VariableReference : public AstNode { //
  std::string _name;
};
//
struct AssignmentStatement : public Statement {
    std::string _name;
    datatype_ptr_t _datatype;
    expression_ptr_t _expression;
};
//
struct Expression : public AstNode { //
    sum_ptr_t _sum;
};
//
struct Sum : public AstNode {
    product_ptr_t _left;
    product_ptr_t _right;
    char _op = 0;
};
//
struct Product : public AstNode {
    std::vector<primary_ptr_t> _primaries;
};
//
struct Primary : public AstNode { //
    svar64_t _impl;
};
//
struct Literal : public AstNode {};
//
struct NumericLiteral : public AstNode {};
//
struct FloatLiteral : public NumericLiteral { //
  float _value = 0.0f;
};
//
struct IntegerLiteral : public NumericLiteral { //
  int _value = 0;
};
//
struct Term : public AstNode { //
    expression_ptr_t _subexpression;
};

} // namespace AST