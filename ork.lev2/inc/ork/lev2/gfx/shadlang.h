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
struct Shader;
struct VertexShader;
struct FragmentShader;
struct ComputeShader;
struct Translatable;
struct TranslationUnit;

using astnode_ptr_t         = std::shared_ptr<AstNode>;
using expression_ptr_t      = std::shared_ptr<Expression>;
using varref_ptr_t          = std::shared_ptr<VariableReference>;
using primary_ptr_t         = std::shared_ptr<Primary>;
using product_ptr_t         = std::shared_ptr<Product>;
using sum_ptr_t             = std::shared_ptr<Sum>;
using datatype_ptr_t        = std::shared_ptr<DataType>;
using argument_decl_ptr_t   = std::shared_ptr<ArgumentDeclaration>;
using assignment_stmt_ptr_t = std::shared_ptr<AssignmentStatement>;
using fndef_ptr_t           = std::shared_ptr<FunctionDef>;
using shader_ptr_t          = std::shared_ptr<Shader>;
using vtxshader_ptr_t       = std::shared_ptr<VertexShader>;
using frgshader_ptr_t       = std::shared_ptr<FragmentShader>;
using comshader_ptr_t       = std::shared_ptr<ComputeShader>;
using translatable_ptr_t    = std::shared_ptr<Translatable>;
using translationunit_ptr_t = std::shared_ptr<TranslationUnit>;
/////////////////////

struct AstNode {
  AstNode() {
    static int gid = 0;
    _nodeID = gid++;
  }
  virtual ~AstNode() {
  }
  virtual std::string desc() const {
    return _name;
  }
  template <typename T> std::shared_ptr<T> childAs(size_t index) {
    OrkAssert(index < _children.size());
    auto ch  = _children[index];
    auto ret = std::dynamic_pointer_cast<T>(ch);
    OrkAssert(ret);
    return ret;
  }
  ///////////////////////////
  std::string _name;
  bool _descend = true;
  bool _showDOT = true;
  int _nodeID = -1;
  astnode_ptr_t _parent;
  std::vector<astnode_ptr_t> _children;

};

struct LanguageElement : public AstNode {
  inline LanguageElement() {
    _name = "LanguageElement";
  }
};

struct MemberRef : public LanguageElement {
  virtual std::string desc() const {
    return FormatString("MemberRef(%s)", _member.c_str());
  }
  std::string _member;
};
struct ArrayRef : public LanguageElement {
  virtual std::string desc() const {
    return FormatString("ArrayRef");
  }
};

struct Statement : public LanguageElement {
  inline Statement() {
    _name = "Statement";
  }
};
struct LValue : public LanguageElement {
  virtual std::string desc() const {
    return FormatString("LValue(%s)", _identifier.c_str());
  }
  std::string _identifier;
};
//
struct DataType : public LanguageElement {
  inline DataType() {
    _name = "DataType";
  }
  std::string _datatype;
  virtual std::string desc() const {
    return FormatString("DataType(%s)", _datatype.c_str());
  }
};
//
struct AssignmentStatementVarRef : public LanguageElement {
  virtual std::string desc() const {
    return FormatString("AssignmentStatementVarRef(%s)", _identifier.c_str() );
  }
  std::string _identifier;
};
struct AssignmentStatementVarDecl : public LanguageElement {
  std::string desc() const final {
    return FormatString("AssignmentStatementVarDecl dt<%s> id<%s>", _datatype.c_str(), _identifier.c_str());
  }
  std::string _datatype;
  std::string _identifier;
};
//
struct AssignmentStatement : public Statement {
  datatype_ptr_t _datatype;
  expression_ptr_t _expression;
  virtual std::string desc() const {
    return FormatString("AssignmentStatement");
  }
};
struct EmptyStatement : public Statement {};
//
struct Expression : public LanguageElement { //
  inline Expression() {
    _name = "Expression";
  }
  sum_ptr_t _sum;
};
//
struct Sum : public LanguageElement {
  inline Sum() {
    _name = "Sum";
  }
  product_ptr_t _left;
  product_ptr_t _right;
  char _op = 0;
  virtual std::string desc() const {
    return FormatString("Sum(%c)", _op);
  }
};
//
struct Product : public LanguageElement {
  inline Product() {
    _name = "Product";
  }
  std::vector<primary_ptr_t> _primaries;
};
//
struct Primary : public LanguageElement { //
  inline Primary() {
    _name = "Primary";
  }
  svar64_t _impl;
};
//
struct Literal : public LanguageElement {};
//
struct NumericLiteral : public Literal {
  inline NumericLiteral() {
    _name = "NumericLiteral";
  }
};
//
struct FloatLiteral : public NumericLiteral { //
  inline FloatLiteral() {
    _name = "FloatLiteral";
  }
  float _value = 0.0f;
  std::string _strval;
  virtual std::string desc() const {
    return FormatString("FloatLiteral(%s)", _strval.c_str());
  }
};
//
struct IntegerLiteral : public NumericLiteral { //
  inline IntegerLiteral() {
    _name = "IntegerLiteral";
  }
  int _value = 0;
  virtual std::string desc() const {
    return FormatString("IntegerLiteral(%d)", _value);
  }
};
//
struct Term : public LanguageElement { //
  inline Term() {
    _name = "Term";
  }
  expression_ptr_t _subexpression;
};
//
struct ArgumentDeclaration : public LanguageElement {
  std::string _variable_name;
  datatype_ptr_t _datatype;
  std::string desc() const final {
    return FormatString("ArgumentDeclaration(%s)", _variable_name.c_str());
  }
};

struct FunctionInvokationArgument : public LanguageElement { //
  std::string desc() const final {
    return FormatString("FunctionInvokationArgument");
  }
};
struct FunctionInvokationArguments : public LanguageElement { //
  std::string desc() const final {
    return FormatString("FunctionInvokationArguments[]");
  }
};

struct FunctionInvokation : public LanguageElement { //
  std::string desc() const final {
    return FormatString("FunctionInvokation ()");
  }
};

struct DataDeclarations : public AstNode { //
  std::string desc() const final {
    return FormatString("DataDeclarations[]");
  }
};

struct DataDeclaration : public LanguageElement { //
  std::string desc() const final {
    return FormatString("DataDeclaration dt<%s> id<%s>", _datatype.c_str(), _identifier.c_str());
  }
  std::string _datatype;
  std::string _identifier;
};
struct TypedIdentifier : public LanguageElement { //
  TypedIdentifier() {
    _descend = false;
  }
  std::string desc() const final {
    return FormatString("TypedIdentifier dt<%s> id<%s>", _datatype.c_str(), _identifier.c_str());
  }
  std::string _datatype;
  std::string _identifier;
};

struct ObjectName : public LanguageElement { //
  std::string desc() const final {
    return FormatString("ObjectName(%s)", _name.c_str());
  }
};
struct RValueConstructor : public LanguageElement { //
  std::string desc() const final {
    return FormatString("RValueConstructor datatype<%s>", _datatype.c_str() );
  }
  std::string _datatype;
};

struct Translatable : public AstNode {};

struct UniformSet : public AstNode { //
  std::string desc() const final {
    return FormatString("UniformSet(%s) {}", _name.c_str());
  }
};
struct UniformBlk : public AstNode { //
  std::string desc() const final {
    return FormatString("UniformBlk(%s) {}", _name.c_str());
  }
};
struct DeclArgumentList : public AstNode { //
  std::string desc() const final {
    return FormatString("DeclArgumentList(%s) []", _name.c_str());
  }
};
struct StatementList : public AstNode { //
  std::string desc() const final {
    return FormatString("StatementList[]");
  }
};
struct InterfaceInput : public AstNode { //
  InterfaceInput() {
    _descend = false;
  }
  std::string desc() const final {
    return FormatString("InterfaceInput id<%s> sem<%s> dt<%s>", _identifier.c_str(), _semantic.c_str(), _datatype.c_str());
  }
  std::string _semantic;
  std::string _identifier;
  std::string _datatype;
};
struct InterfaceOutput : public AstNode { //
  InterfaceOutput() {
    _descend = false;
  }
  std::string desc() const final {
    return FormatString("InterfaceOutput id<%s> dt<%s>", _identifier.c_str(), _datatype.c_str());
  }
  std::string _identifier;
  std::string _datatype;
};
struct InterfaceInputs : public AstNode { //
  std::string desc() const final {
    return FormatString("InterfaceInputs[]");
  }
};
struct InterfaceOutputs : public AstNode { //
  std::string desc() const final {
    return FormatString("InterfaceOutputs[]");
  }
};
struct Dependency : public AstNode { //
  std::string desc() const override {
    return FormatString("Dependency(%s)", _name.c_str());
  }
};
struct Extension : public Dependency { //
  std::string desc() const final {
    return FormatString("Extension(%s)", _extension_name.c_str());
  }
  std::string _extension_name;
};
struct LibraryBlock : public Translatable { //
  std::string desc() const final {
    return FormatString("LibraryBlock(%s)", _name.c_str());
  }
};
struct Shader : public Translatable { //
};
struct VertexShader : public Shader { //
  std::string desc() const final {
    return FormatString("VertexShader(%s) {}", _name.c_str());
  }
};
struct FragmentShader : public Shader { //
  std::string desc() const final {
    return FormatString("FragmentShader(%s) {}", _name.c_str());
  }
};
struct ComputeShader : public Shader { //
  std::string desc() const final {
    return FormatString("ComputeShader(%s) {}", _name.c_str());
  }
};
struct StateBlockItem : public LanguageElement { //
  std::string desc() const final {
    return FormatString("StateBlockItem key<%s> value<%s>", _key.c_str(), _value.c_str() );
  }
  std::string _key;
  std::string _value;
};
struct StateBlock : public Translatable { //
  std::string desc() const final {
    return FormatString("StateBlock(%s) {}", _name.c_str());
  }
};
struct Technique : public Translatable { //
  std::string desc() const final {
    return FormatString("Technique(%s) {}", _name.c_str());
  }
};
//
struct FunctionDef : public Translatable { //
  datatype_ptr_t _returntype;
  std::vector<argument_decl_ptr_t> _arguments;
  std::vector<assignment_stmt_ptr_t> _statements;
  std::string desc() const final {
    return FormatString("FunctionDef(%s) {}", _name.c_str());
  }

  // void createDotFile(file::Path outpath) const;
};
struct PipelineInterface : public Translatable { //

};
struct VertexInterface : public PipelineInterface { //
  std::string desc() const final {
    return FormatString("VertexInterface(%s) {}", _name.c_str());
  }
};
struct FragmentInterface : public PipelineInterface { //
  std::string desc() const final {
    return FormatString("FragmentInterface(%s) {}", _name.c_str());
  }
};
struct TranslationUnit : public AstNode { //
  std::string desc() const final {
    return FormatString("TranslationUnit[]");
  }
  std::map<std::string, translatable_ptr_t> _translatables_by_name;
  std::vector<translatable_ptr_t> _translatable_by_order;
};

void _dumpAstTreeVisitor(astnode_ptr_t node, int indent, std::string& out_str);

using visitor_fn_t = std::function<void(astnode_ptr_t)>;

struct Visitor{
  visitor_fn_t _on_pre;
  visitor_fn_t _on_post;
  std::stack<astnode_ptr_t> _nodestack;
  svar64_t _userdata;
};

using visitor_ptr_t = std::shared_ptr<Visitor>;

///////////////////////////////////////////////////////////////////////////////
} // namespace SHAST


SHAST::translationunit_ptr_t parse(const std::string& shader_text);
std::string toASTstring(SHAST::astnode_ptr_t);
std::string toGL4SL(SHAST::translationunit_ptr_t top);
std::string toDotFile(SHAST::translationunit_ptr_t top);
void visitAST(SHAST::astnode_ptr_t top, SHAST::visitor_ptr_t visitor);

} // namespace ork::lev2::shadlang
///////////////////////////////////////////////////////////////////////////////
#endif