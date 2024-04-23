#pragma once

#include <ork/util/scanner.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

namespace parser{
struct Program;
struct AstNode;
struct NamedBlockNode;
struct TopNode;
struct GlSlFxParser;
struct LibraryBlockNode;
struct TechniqueNode;
struct FnMatchResultsBas;
struct FnMatchResultsWrap;
struct ShaderBodyElement;
struct VariableDeclaration;
struct Statement;
struct Expression;
struct InterfaceNode;
struct InterfaceIoNode;
struct IoContainerNode;
struct UniformSetNode;
struct UniformBlockNode;
struct RequiredExtensionNode;
struct InterfaceLayoutNode;
struct StructMemberNode;
struct FunctionArgumentNode;
struct OrkSlFunctionNode;
struct FnElement;
struct PassNode;
struct DecoBlockNode;
struct ConfigNode;
struct StructNode;
struct FunctionNode;
struct UniformDeclNode;
struct ImportNode;

using parser_ptr_t = std::shared_ptr<GlSlFxParser>;
using parser_rawptr_t = GlSlFxParser*;

using program_ptr_t = std::shared_ptr<Program>;
using topnode_ptr_t = std::shared_ptr<TopNode>;
using astnode_ptr_t = std::shared_ptr<AstNode>;
using namednode_ptr_t = std::shared_ptr<NamedBlockNode>;
using confignode_ptr_t = std::shared_ptr<ConfigNode>;
using libblock_ptr_t = std::shared_ptr<LibraryBlockNode>;
using bodyelem_ptr_t = std::shared_ptr<ShaderBodyElement>;
using vardecl_ptr_t = std::shared_ptr<VariableDeclaration>;
using statement_ptr_t = std::shared_ptr<Statement>;
using expression_ptr_t = std::shared_ptr<Expression>;
using interfacenode_ptr_t = std::shared_ptr<InterfaceNode>;
using interfaceionode_ptr_t = std::shared_ptr<InterfaceIoNode>;
using interfacelayoutnode_ptr_t = std::shared_ptr<InterfaceLayoutNode>;
using iocontainernode_ptr_t = std::shared_ptr<IoContainerNode>;
using uniformsetnode_ptr_t = std::shared_ptr<UniformSetNode>;
using uniformblocknode_ptr_t = std::shared_ptr<UniformBlockNode>;
using requiredextensionnode_ptr_t = std::shared_ptr<RequiredExtensionNode>;
using structmembernode_ptr_t = std::shared_ptr<StructMemberNode>;
using functionargumentnode_ptr_t = std::shared_ptr<FunctionArgumentNode>;
using orkslfunctionnode_ptr_t = std::shared_ptr<OrkSlFunctionNode>;
using fnelementnode_ptr_t = std::shared_ptr<FnElement>;
using techniquenode_ptr_t = std::shared_ptr<TechniqueNode>;
using passnode_ptr_t = std::shared_ptr<PassNode>;
using decoblocknode_ptr_t = std::shared_ptr<DecoBlockNode>;
using decoblocknode_rawptr_t = DecoBlockNode*;
using structnode_ptr_t = std::shared_ptr<StructNode>;
using functionnode_ptr_t = std::shared_ptr<FunctionNode>;
using uniformdeclnode_ptr_t = std::shared_ptr<UniformDeclNode>;
using importnode_ptr_t = std::shared_ptr<ImportNode>;

using orkslmatch_ptr_t = std::shared_ptr<FnMatchResultsBas>;

using libblock_constptr_t = std::shared_ptr<const LibraryBlockNode>;
using decoblocknode_rawconstptr_t = const DecoBlockNode*;
using decoblocknode_constptr_t = std::shared_ptr<const DecoBlockNode>;
using interfacenode_constptr_t = std::shared_ptr<const InterfaceNode>;
using uniformsetnode_constptr_t = std::shared_ptr<const UniformSetNode>;
using uniformblocknode_constptr_t = std::shared_ptr<const UniformBlockNode>;
using parser_constptr_t = std::shared_ptr<const GlSlFxParser>;



using match_results_t = FnMatchResultsWrap;
//using match_fn_t = std::function<orkslmatch_ptr_t(FnParseContext)>;
};

///////////////////////////////////////////////////////////////////////////////

namespace shaderbuilder{
  struct BackEnd;
  struct Node;
  struct Section;
  using node_ptr_t = std::shared_ptr<Node>;
  using section_ptr_t = std::shared_ptr<Section>;
};

///////////////////////////////////////////////////////////////////////////////

namespace parser{

struct NodeSelection {
  bool _lib_nodes = false;
  bool _stateblock_nodes = false;
  bool _interface_nodes = false;
  bool _shader_nodes = false;
  bool _shaderdata_nodes = false;
  bool _technique_nodes = false;
};

struct NodeCollection{
  using nodevect_t = std::vector<namednode_ptr_t>;
  using nodeset_t = std::set<std::string>;
  nodevect_t _nodevect;
  nodeset_t _uniqueset;
};

struct Program {

  Program(std::string name);

  NodeCollection collectNodes( const NodeSelection& selection );
  void addBlockNode(decoblocknode_ptr_t node);
  void generate_all(shaderbuilder::BackEnd& backend);
  template <typename T> void generateBlocks(shaderbuilder::BackEnd& backend);
  template <typename T> void forEachBlockOfType(std::function<void(std::shared_ptr<T>)> operation);
  template <typename T> void collectNodesOfType(NodeCollection& outnvect);

  std::unordered_map<std::string, decoblocknode_ptr_t> _blockNodes;
  std::vector<decoblocknode_ptr_t> _orderedBlockNodes;
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

void performScan(scanner_ptr_t scanner);

enum class TokenClass : uint64_t {
  CrcEnum(SINGLE_LINE_COMMENT),  
  CrcEnum(MULTI_LINE_COMMENT),
  CrcEnum(WHITESPACE),
  CrcEnum(NEWLINE),
  CrcEnum(UNSIGNED_DECIMAL_INTEGER),
  CrcEnum(HEX_INTEGER),
  CrcEnum(MISC_INTEGER),
  CrcEnum(FLOATING_POINT),
  CrcEnum(STRING),
  CrcEnum(KW_OR_ID),
  CrcEnum(L_CURLY),
  CrcEnum(R_CURLY),
  CrcEnum(L_SQUARE),
  CrcEnum(R_SQUARE),
  CrcEnum(L_PAREN),
  CrcEnum(R_PAREN),
  CrcEnum(COLON),
  CrcEnum(SEMICOLON),
  CrcEnum(L_SHIFT),
  CrcEnum(R_SHIFT),
  CrcEnum(LESS_THAN),
  CrcEnum(GREATER_THAN),
  CrcEnum(LESS_THAN_EQ),
  CrcEnum(GREATER_THAN_EQ),
  CrcEnum(EQUAL_TO),
  CrcEnum(NOT_EQUAL_TO),
  CrcEnum(PLUS_EQ),
  CrcEnum(MINUS_EQ),
  CrcEnum(TIMES_EQ),
  CrcEnum(DIVIDE_EQ),
  CrcEnum(OR_EQ),
  CrcEnum(AND_EQ),
  CrcEnum(LOGICAL_OR),
  CrcEnum(LOGICAL_AND),
  CrcEnum(INCREMENT),
  CrcEnum(DECREMENT),

  CrcEnum(EQUALS),
  CrcEnum(COMMA),
  CrcEnum(DOT),
  CrcEnum(QUESTION_MARK),
  CrcEnum(STAR),
  CrcEnum(SLASH),
  CrcEnum(PERCENT),
  CrcEnum(EXCLAMATION),
  CrcEnum(AMPERSAND),
  CrcEnum(CARET),
  CrcEnum(PIPE),
  CrcEnum(PLUS),
  CrcEnum(MINUS)
};

///////////////////////////////////////////////////////////////////////////////

template <typename rule_receiver_t> //
inline void loadScannerRules(rule_receiver_t& r){ //
  r.addRule("\\/\\*([^*]|\\*+[^/*])*\\*+\\/", uint64_t(TokenClass::MULTI_LINE_COMMENT));
  r.addRule("\\/\\/.*[\\n\\r]", uint64_t(TokenClass::SINGLE_LINE_COMMENT));
  r.addRule("\\s+", uint64_t(TokenClass::WHITESPACE));
  r.addRule("[\\n\\r]+", uint64_t(TokenClass::NEWLINE));
  /////////
  r.addRule("[0-9]+u", uint64_t(TokenClass::UNSIGNED_DECIMAL_INTEGER));
  r.addRule("0x[0-9a-fA-F]+u?", uint64_t(TokenClass::HEX_INTEGER));
  r.addRule("-?(\\d+)", uint64_t(TokenClass::MISC_INTEGER));
  r.addRule("-?(\\d*\\.?)(\\d+)([eE][-+]?\\d+)?", uint64_t(TokenClass::FLOATING_POINT));
  /////////
  r.addRule("[\"].*[\"]", uint64_t(TokenClass::STRING));
  /////////
  r.addRule("[a-zA-Z_]+[a-zA-Z0-9_]+", uint64_t(TokenClass::KW_OR_ID));
  /////////
  r.addRule("\\{", uint64_t(TokenClass::L_CURLY));
  r.addRule("\\}", uint64_t(TokenClass::R_CURLY));
  r.addRule("\\(", uint64_t(TokenClass::L_PAREN));
  r.addRule("\\)", uint64_t(TokenClass::R_PAREN));
  r.addRule("\\[", uint64_t(TokenClass::L_SQUARE));
  r.addRule("\\]", uint64_t(TokenClass::R_SQUARE));
  /////////
  r.addRule("\\?", uint64_t(TokenClass::QUESTION_MARK));
  r.addRule(":", uint64_t(TokenClass::COLON));
  r.addRule(";", uint64_t(TokenClass::SEMICOLON));
  r.addRule("<", uint64_t(TokenClass::LESS_THAN));
  r.addRule(">", uint64_t(TokenClass::GREATER_THAN));
  r.addRule("&", uint64_t(TokenClass::AMPERSAND));
  r.addRule("\\|", uint64_t(TokenClass::PIPE));
  r.addRule("\\*", uint64_t(TokenClass::STAR));
  r.addRule("\\/", uint64_t(TokenClass::SLASH));
  r.addRule("%", uint64_t(TokenClass::PERCENT));
  r.addRule("!", uint64_t(TokenClass::EXCLAMATION));
  r.addRule("\\+", uint64_t(TokenClass::PLUS));
  r.addRule("\\-", uint64_t(TokenClass::MINUS));
  r.addRule("=", uint64_t(TokenClass::EQUALS));
  r.addRule(",", uint64_t(TokenClass::COMMA));
  r.addRule(".", uint64_t(TokenClass::DOT));
  r.addRule("\\^", uint64_t(TokenClass::CARET));
  /////////
  r.addRule("<<", uint64_t(TokenClass::L_SHIFT));
  r.addRule(">>", uint64_t(TokenClass::R_SHIFT));
  r.addRule("<=", uint64_t(TokenClass::LESS_THAN_EQ));
  r.addRule(">=", uint64_t(TokenClass::GREATER_THAN_EQ));
  r.addRule("==", uint64_t(TokenClass::EQUAL_TO));
  r.addRule("!=", uint64_t(TokenClass::NOT_EQUAL_TO));
  r.addRule("\\+=", uint64_t(TokenClass::PLUS_EQ));
  r.addRule("\\-=", uint64_t(TokenClass::MINUS_EQ));
  r.addRule("\\*=", uint64_t(TokenClass::TIMES_EQ));
  r.addRule("\\/=", uint64_t(TokenClass::DIVIDE_EQ));
  r.addRule("\\|=", uint64_t(TokenClass::OR_EQ));
  r.addRule("&=", uint64_t(TokenClass::AND_EQ));
  /////////
  r.addRule("\\+\\+", uint64_t(TokenClass::INCREMENT));
  r.addRule("--", uint64_t(TokenClass::DECREMENT));
  /////////
  r.addRule("\\|\\|", uint64_t(TokenClass::LOGICAL_OR));
  r.addRule("&&", uint64_t(TokenClass::LOGICAL_AND));
  
};

///////////////////////////////////////////////////////////////////////////////

struct AstNode {
  AstNode(parser_rawptr_t parser=nullptr)
    : _parser(parser) {
  }
  virtual ~AstNode() {
  }
  virtual void _generate1(shaderbuilder::BackEnd& backend) const {
  }
  virtual void _generate2(shaderbuilder::BackEnd& backend) const {
  }
  virtual void pregen(shaderbuilder::BackEnd& backend) const {
  }
  parser_rawptr_t _parser;
};

///////////////////////////////////////////////////////////////////////////////

#if defined(USE_ORKSL_LANG)

struct FnParseContext {
  FnParseContext(GlSlFxParser* parser, const ScannerView* v);
  FnParseContext(const FnParseContext& oth);
  FnParseContext& operator=(const FnParseContext& oth);
  FnParseContext advance(size_t count) const;
  std::string tokenValue(size_t offset) const;
  void dump(const std::string dumpid) const;

  GlSlFxParser* _parser;
  size_t _startIndex        = 0;
  const ScannerView* _view;
};

///////////////////////////////////////////////////////////////////////////////

struct ParseResult {
  size_t _numtokens = 0;
  astnode_ptr_t _node    = nullptr;
};
typedef ParseResult parsed_t;

struct FnMatchResultsBas {

  FnMatchResultsBas(FnParseContext ctx)
      : _ctx(ctx) {
  }
  virtual ~FnMatchResultsBas() {
  }
  operator bool() const {
    return _matched;
  }
  virtual orkslmatch_ptr_t merge(orkslmatch_ptr_t rhs) const = 0;

  virtual ParseResult parse() = 0;

  FnParseContext consume() const {
    assert(_matched);
    FnParseContext rval = _ctx;
    rval._startIndex += _count;
    return rval;
  }

  size_t _start = 0;
  size_t _count = 0;
  size_t end() const {
    return _start + _count-1;
  }
  bool _matched = false;
  FnParseContext _ctx;
  std::vector<orkslmatch_ptr_t> _subMatches;
};

///////////////////////////////////////////////////////////////////////////////

struct FnMatchResultsSt : public FnMatchResultsBas {
    orkslmatch_ptr_t merge(orkslmatch_ptr_t rhs) const final { return nullptr; }
    ParseResult parse() final { return ParseResult(); }
};

///////////////////////////////////////////////////////////////////////////////

struct FnMatchResultsWrap {
  FnMatchResultsWrap(orkslmatch_ptr_t p = nullptr)
      : _results(p) {
  }
  void dump(std::string dumpid) const {
    if(this->operator bool()){
      size_t st = _results->_start;
      size_t en = _results->end();
      std::string sttok = _results->_ctx._view->token(st)->text;
      std::string entok = _results->_ctx._view->token(en)->text;
      printf( "matchres<%s> matched st<%zd:%s> en<%zd:%s>\n", dumpid.c_str(), st,sttok.c_str(), en, entok.c_str() );
    }
    else{
      printf( "matchres<%s> no-match\n", dumpid.c_str() );
    }
  }
  template <typename T> inline FnMatchResultsWrap& operator=(std::shared_ptr<T> p) {
    _results = std::dynamic_pointer_cast<FnMatchResultsBas>(p);
    return *this;
  }
  template <typename T> inline void make(FnParseContext ctx) {
    _results = std::dynamic_pointer_cast<FnMatchResultsBas>(std::make_shared<T>(ctx));
  }
  inline operator bool() const {
    if (not _results)
      return false;
    return _results->operator bool();
  }
  inline FnMatchResultsWrap operator+(FnMatchResultsWrap b) const {
    return b._results ? _results->merge(b._results) : _results;
  }
  inline FnMatchResultsBas* operator->() {
    return _results.get();
  }
  orkslmatch_ptr_t _results;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct FnMatchResults : public FnMatchResultsBas {

  FnMatchResults(FnParseContext ctx)
      : FnMatchResultsBas(ctx) {
  }
  FnMatchResults(const FnMatchResultsBas& oth)
      : FnMatchResultsBas(oth) {
  }

  ParseResult parse() final {
    return ParseResult(); // T::parse(*this);
  }

  orkslmatch_ptr_t merge(orkslmatch_ptr_t rhs) const final {
    orkslmatch_ptr_t rval = std::make_shared<FnMatchResults>(*this);
    if (false == _matched)
      rval->_start = rhs->_start;
    rval->_count += rhs->_count;
    rval->_matched |= rhs->_matched;
    return rval;
  }
};

#endif

///////////////////////////////////////////////////////////////////////////////

struct ShaderBodyElement : public AstNode {
  ShaderBodyElement()
      : AstNode() {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderEmittable : public AstNode {
  ShaderEmittable()
      : AstNode() {
  }
  virtual void emit(shaderbuilder::BackEnd& backend) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

#if defined(USE_ORKSL_LANG)

#define DECLARE_STD_FNS(xxx)                                                                                                       \
  typedef FnMatchResults<xxx> match_t;                                                                                             \
  static match_results_t match(FnParseContext ctx);
// static parsed_t parse(const match_t& match);

#define DECLARE_STD_EMITTABLE_FNS(xxx)                                                                                             \
  DECLARE_STD_FNS(xxx)                                                                                                             \
  void emit(shaderbuilder::BackEnd& backend) const final;

#define DECLARE_STD_EMITTABLE(xxx)                                                                                                 \
  struct xxx : public ShaderEmittable {                                                                                            \
    xxx()                                                                                                      \
        : ShaderEmittable() {                                                                                                 \
    }                                                                                                                              \
    DECLARE_STD_EMITTABLE_FNS(xxx)                                                                                                 \
  };

#define DECLARE_STD_ABSTRACT_EMITTABLE(xxx)                                                                                        \
  struct xxx : public ShaderEmittable {                                                                                            \
    xxx()                                                                                                      \
        : ShaderEmittable() {                                                                                                 \
    }                                                                                                                              \
    DECLARE_STD_FNS(xxx)                                                                                                           \
  };

#define DECLARE_RECURSIVE_FNS(xxx)                                                                                                       \
  typedef FnMatchResults<xxx> match_t;                                                                                             \
  static match_results_t match(FnParseContext ctx, int level=0);

#define DECLARE_RECURSIVE_EMITTABLE_FNS(xxx)                                                                                             \
  DECLARE_RECURSIVE_FNS(xxx)                                                                                                             \
  void emit(shaderbuilder::BackEnd& backend) const final;

#define DECLARE_RECURSIVE_EMITTABLE(xxx)                                                                                                 \
  struct xxx : public ShaderEmittable {                                                                                            \
    xxx()                                                                                                      \
        : ShaderEmittable() {                                                                                                 \
    }                                                                                                                              \
    DECLARE_RECURSIVE_EMITTABLE_FNS(xxx)                                                                                                 \
  };

#endif 

///////////////////////////////////////////////////////////////////////////////
// good

DECLARE_STD_EMITTABLE(PrimaryExpression);
DECLARE_RECURSIVE_EMITTABLE(PostFixExpression);

///////////////////////////////////////////////////////////////////////////////
// elemental types
///////////////////////////////////////////////////////////////////////////////

#if defined(USE_ORKSL_LANG)

DECLARE_STD_EMITTABLE(Constant);
DECLARE_STD_EMITTABLE(StringLiteral);
DECLARE_STD_EMITTABLE(TypeName);
DECLARE_STD_EMITTABLE(Identifier);
DECLARE_STD_EMITTABLE(IdentifierPath);
DECLARE_STD_EMITTABLE(Reference);
DECLARE_STD_EMITTABLE(Keyword);

DECLARE_STD_EMITTABLE(OpenCurly);
DECLARE_STD_EMITTABLE(CloseCurly);
DECLARE_STD_EMITTABLE(OpenSquare);
DECLARE_STD_EMITTABLE(CloseSquare);
DECLARE_STD_EMITTABLE(OpenParen);
DECLARE_STD_EMITTABLE(CloseParen);

DECLARE_STD_EMITTABLE(SizeofOp);
DECLARE_STD_EMITTABLE(MathOp);

DECLARE_STD_EMITTABLE(SemicolonOp);
DECLARE_STD_EMITTABLE(CommaOp);
DECLARE_STD_EMITTABLE(DotOp);
DECLARE_STD_EMITTABLE(NotOp);
DECLARE_STD_EMITTABLE(BitNotOp);

DECLARE_STD_EMITTABLE(IncOp);
DECLARE_STD_EMITTABLE(DecOp);
DECLARE_STD_EMITTABLE(OrOp);
DECLARE_STD_EMITTABLE(OrOrOp);
DECLARE_STD_EMITTABLE(AndOp);
DECLARE_STD_EMITTABLE(AndAndOp);
DECLARE_STD_EMITTABLE(XorOp);
DECLARE_STD_EMITTABLE(EqOp);
DECLARE_STD_EMITTABLE(NeqOp);
DECLARE_STD_EMITTABLE(LtOp);
DECLARE_STD_EMITTABLE(LtEqOp);
DECLARE_STD_EMITTABLE(GtOp);
DECLARE_STD_EMITTABLE(GtEqOp);
DECLARE_STD_EMITTABLE(LeftOp);
DECLARE_STD_EMITTABLE(RightOp);
DECLARE_STD_EMITTABLE(AddOp);
DECLARE_STD_EMITTABLE(SubOp);
DECLARE_STD_EMITTABLE(MulOp);
DECLARE_STD_EMITTABLE(DivOp);
DECLARE_STD_EMITTABLE(ModOp);

///////////////////////////////////////////////////////////////////////////////
// ???

DECLARE_STD_EMITTABLE(InitialAssignmentOperator);
DECLARE_STD_EMITTABLE(MutatingAssignmentOperator);
///////////////////////////////////////////////////////////////////////////////
DECLARE_STD_EMITTABLE(ArgumentExpressionList);

DECLARE_STD_EMITTABLE(CastExpression);
DECLARE_STD_EMITTABLE(ExpressionNode);
DECLARE_STD_EMITTABLE(MultiplicativeExpression);
DECLARE_STD_EMITTABLE(LogicalOrExpression);
DECLARE_STD_EMITTABLE(LogicalAndExpression);
DECLARE_STD_EMITTABLE(ExclusiveOrExpression);
DECLARE_STD_EMITTABLE(InclusiveOrExpression);
DECLARE_STD_EMITTABLE(AndExpression);
DECLARE_STD_EMITTABLE(EqualityExpression);
DECLARE_STD_EMITTABLE(RelationalExpression);
DECLARE_STD_EMITTABLE(ShiftExpression);
DECLARE_STD_EMITTABLE(AdditiveExpression);
DECLARE_STD_EMITTABLE(UnaryExpression);

DECLARE_STD_EMITTABLE(Statement);
DECLARE_STD_EMITTABLE(ExpressionStatement);
DECLARE_STD_EMITTABLE(InstantiationStatement);
DECLARE_STD_EMITTABLE(ReturnStatement);
DECLARE_STD_EMITTABLE(AssignmentStatement);

DECLARE_STD_ABSTRACT_EMITTABLE(ConditionalExpression);
DECLARE_STD_ABSTRACT_EMITTABLE(IterationStatement);

///////////////////////////////////////////////////////////////////////////////

struct FnElement : public ShaderEmittable {
  FnElement()
      : ShaderEmittable() {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct VariableDeclaration : public FnElement {
  VariableDeclaration()
      : FnElement() {
  }
  DECLARE_STD_EMITTABLE_FNS(VariableDeclaration);
};

///////////////////////////////////////////////////////////////////////////////

struct DeclarationList : public ShaderEmittable {
  DeclarationList()
      : ShaderEmittable() {
  }
  DECLARE_STD_EMITTABLE_FNS(DeclarationList);
  std::vector<vardecl_ptr_t> _children;
};


///////////////////////////////////////////////////////////////////////////////

struct Expression : public ShaderEmittable {
  Expression()
      : ShaderEmittable() {
  }
  DECLARE_STD_EMITTABLE_FNS(Expression);
  std::vector<bodyelem_ptr_t> _children;
};

struct TernaryExpression : public ConditionalExpression {
  TernaryExpression()
      : ConditionalExpression() {
  }
  DECLARE_STD_EMITTABLE_FNS(TernaryExpression);
};

///////////////////////////////////////////////////////////////////////////////

struct StatementList : public ShaderEmittable {
  StatementList()
      : ShaderEmittable() {
  }
  DECLARE_STD_EMITTABLE_FNS(StatementList);
  std::vector<statement_ptr_t> _children;
};

///////////////////////////////////////////////////////////////////////////////

struct CompoundStatement : public FnElement {
  CompoundStatement()
      : FnElement() {
  }
  DECLARE_STD_EMITTABLE_FNS(CompoundStatement);
};

///////////////////////////////////////////////////////////////////////////////

/*struct ReturnStatement : public StatementNode {
  ReturnStatement(TopNode* cnode)
      : StatementNode() {}

  typedef FnMatchResults<ReturnStatement> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

  expression_ptr_t _returnValue = nullptr;
};*/


///////////////////////////////////////////////////////////////////////////////

struct ForLoopStatement : public IterationStatement {
  ForLoopStatement()
      : IterationStatement() {
  }

  DECLARE_STD_EMITTABLE_FNS(ForLoopStatement);

  const Token* _variable = nullptr;
  expression_ptr_t _condition = nullptr;
  // AssignmentNode* _advance = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
/*
struct WhileLoopStatement : public IterationStatement {
  WhileLoopStatement()
      : IterationStatement() {}

  typedef FnMatchResults<WhileLoopStatement> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

  expression_ptr_t _condition = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
struct ElseNode : public ShaderEmittable {
  ElseNode(TopNode* cnode)
      : ShaderEmittable() {}

  typedef FnMatchResults<ElseNode> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

};

struct ElseIfNode : public ShaderEmittable {
  ElseIfNode(TopNode* cnode)
      : ShaderEmittable() {}

  typedef FnMatchResults<ElseIfNode> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

  expression_ptr_t _condition = nullptr;
};


struct IfStatement : public StatementNode {
  IfStatement(TopNode* cnode)
      : StatementNode() {}
  typedef FnMatchResults<IfStatement> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

  expression_ptr_t _condition = nullptr;
  std::vector<ElseIfNode*> _elseifs;
  ElseNode* _elseNode = nullptr;
};*/

#endif 

///////////////////////////////////////////////////////////////////////////////

struct OrkSlFunctionNode : public AstNode {

  OrkSlFunctionNode(parser_rawptr_t parser);
  int parse(const ScannerView& view);
  void emit(shaderbuilder::BackEnd& backend) const;

  static svar16_t _getimpl(OrkSlFunctionNode* node);
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryStructMemberNode : public AstNode {
  std::string _typename;
  std::string _identifier;
};

///////////////////////////////////////////////////////////////////////////////

struct NamedBlockNode : public AstNode {

  NamedBlockNode()
      : AstNode() {
  }

  void parse(GlSlFxParser* parser, const ScannerView& view);

  std::string _name;
  std::string _blocktype;

  std::vector<const Token*> _decorators;
  std::set<std::string> _decodupecheck;
};

///////////////////////////////////////////////////////////////////////////////

struct ConfigNode : public NamedBlockNode {
  ConfigNode()
      : NamedBlockNode() {
  }

  std::string _name;

  void parse(GlSlFxParser* parser, const ScannerView& view);
  void generate(rootcontainer_ptr_t c) const;
};

///////////////////////////////////////////////////////////////////////////////

struct RequiredExtensionNode : public AstNode {
  RequiredExtensionNode()
      : AstNode() {
  }
  void emit(shaderbuilder::BackEnd& backend);
  std::string _extension;
};

///////////////////////////////////////////////////////////////////////////////

struct DecoBlockNode : public NamedBlockNode {

  DecoBlockNode()
      : NamedBlockNode() {
  }

  void parse(GlSlFxParser* parser, const ScannerView& view);
  void _pregen(shaderbuilder::BackEnd& backend) const;
  //void emitChildren(shaderbuilder::BackEnd& backend) const;

  std::vector<requiredextensionnode_ptr_t> _requiredExtensions;

};

struct DecoChildren{
  
  const DecoBlockNode* _parent = nullptr;

  std::vector<interfacenode_ptr_t> _interfaceNodes;
  std::vector<libblock_ptr_t> _libraryBlocks;
  std::vector<uniformsetnode_ptr_t> _uniformSets;
  std::vector<uniformblocknode_ptr_t> _uniformBlocks;
};

using decochildren_ptr_t = std::shared_ptr<DecoChildren>;

///////////////////////////////////////////////////////////////////////////////

struct ShaderLine {
  int _indent = 0;
  std::vector<const Token*> _tokens;
};
typedef std::vector<ShaderLine*> linevect_t;

///////////////////////////////////////////////////////////////////////////////

struct ShaderBody {
  linevect_t _lines;
  int parse(GlSlFxParser* parser, const ScannerView& view);
  void emit(shaderbuilder::BackEnd& backend) const;
};

///////////////////////////////////////////////////////////////////////////////

struct StructMemberNode : public AstNode {
  const Token* _type = nullptr;
  const Token* _name = nullptr;
  int _arraySize     = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct StructNode : public AstNode {
  StructNode()
      : AstNode() {
  }
  void pregen(shaderbuilder::BackEnd& backend) const final;
  int parse(GlSlFxParser* parser, const ScannerView& view);
  void emit(shaderbuilder::BackEnd& backend) const;
  std::vector<structmembernode_ptr_t> _members;
  const Token* _name      = nullptr;
  bool _emitstructandname = true;
};

///////////////////////////////////////////////////////////////////////////////

struct FunctionArgumentNode : public AstNode {
  FunctionArgumentNode()
      : AstNode() {
  }
  const Token* _type      = nullptr;
  const Token* _name      = nullptr;
  const Token* _direction = nullptr;
  int _arraySize          = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct FunctionNode : public AstNode {
  FunctionNode()
      : AstNode() {
  }
  void pregen(shaderbuilder::BackEnd& backend) const final;
  int parse(GlSlFxParser* parser, const ScannerView& view);
  void emit(shaderbuilder::BackEnd& backend) const;
  const Token* _name = nullptr;
  std::vector<functionargumentnode_ptr_t> _arguments;
  const Token* _returnType = nullptr;
  ShaderBody _body;

  //orkslfunctionnode_ptr_t _parsedfnnode = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct PassNode : public NamedBlockNode {
  PassNode(TechniqueNode* tek)
      : NamedBlockNode()
      , _techniqueNode(tek) {
  }

  void _generate2(shaderbuilder::BackEnd& backend) const final;

  int parse(GlSlFxParser* parser, const ScannerView& view, int start);

  techniquenode_ptr_t _techniqueNode;
  std::string _vertexshader;
  std::string _fragmentshader;
  std::string _tessctrlshader;
  std::string _tessevalshader;
  std::string _geometryshader;
  std::string _nvtaskshader;
  std::string _nvmeshshader;
  std::string _stateblock;
};

///////////////////////////////////////////////////////////////////////////////

struct TechniqueNode : public DecoBlockNode {
  TechniqueNode()
      : DecoBlockNode() {
  }
  void parse(GlSlFxParser* parser, const ScannerView& view);
  void _generate2(shaderbuilder::BackEnd& backend) const final;
  std::string _fxconfig;
  std::unordered_map<std::string, PassNode*> _passNodes;
};

///////////////////////////////////////////////////////////////////////////////

struct InterfaceLayoutNode : public AstNode {
  InterfaceLayoutNode()
      : AstNode() {
  }
  int parse(GlSlFxParser* parser, const ScannerView& view);
  void pregen(shaderbuilder::BackEnd& backend) const final;
  void emit(shaderbuilder::BackEnd& backend);
  std::vector<const Token*> _tokens;
  std::string _direction;
  bool _standaloneLayout = false;
};

struct InterfaceIoNode : public AstNode {
  InterfaceIoNode()
      : AstNode() {
  }

  void pregen(shaderbuilder::BackEnd& backend) const final;

  std::string _name;
  std::string _typeName;
  structnode_ptr_t _inlineStruct = nullptr;
  std::string _semantic;
  interfacelayoutnode_ptr_t _layout = nullptr;
  std::set<std::string> _qualifiers;
  int _arraySize = 0;
  bool _isArray = false;
  bool _isSizedArray = false;
};

///////////////////////////////////////////////////////////////////////////////

typedef std::vector<interfaceionode_ptr_t> ionodevect_t;

struct IoContainerNode : public AstNode {

  IoContainerNode()
      : AstNode() {
  }

  void pregen(shaderbuilder::BackEnd& backend) const final;
  void emit(shaderbuilder::BackEnd& backend) const;
  ionodevect_t _nodes;
  std::set<std::string> _dupecheck;
  interfacelayoutnode_ptr_t _pendinglayout = nullptr;
  std::vector<interfacelayoutnode_ptr_t> _layouts;
  std::string _direction;
};

///////////////////////////////////////////////////////////////////////////////

struct InterfaceNode : public DecoBlockNode {
  InterfaceNode(GLenum type)
      : DecoBlockNode()
      , _gltype(type) {
    _inputs              = std::make_shared<IoContainerNode>();
    _outputs             = std::make_shared<IoContainerNode>();
    _storage             = std::make_shared<IoContainerNode>();
    _sif                 = new StreamInterface;

    _inputs->_direction  = "in";
    _outputs->_direction = "out";
    _storage->_direction = "storage";
  }

  void parse(GlSlFxParser* parser, const ScannerView& view);
  void parseIos(GlSlFxParser* parser, const ScannerView& view, iocontainernode_ptr_t ioc);

  void pregen(shaderbuilder::BackEnd& backend) const final;
  void _generate2(shaderbuilder::BackEnd& backend) const final;
  void emitInterface(shaderbuilder::BackEnd& backend) const;

  std::vector<interfacelayoutnode_ptr_t> _interfacelayouts;
  iocontainernode_ptr_t _inputs  = nullptr;
  iocontainernode_ptr_t _outputs = nullptr;
  iocontainernode_ptr_t _storage = nullptr;
  GLenum _gltype            = GL_NONE;
  StreamInterface* _sif = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderNode : public DecoBlockNode {
  explicit ShaderNode()
      : DecoBlockNode() {
  }
  void pregen(shaderbuilder::BackEnd& backend) const final;
  void parse(GlSlFxParser* parser, const ScannerView& view);
  void _generate2Common(shaderbuilder::BackEnd& backend) const;
  void emitShaderDeps(shaderbuilder::BackEnd& backend) const;
  ShaderBody _body;
};

///////////////////////////////////////////////////////////////////////////////

struct StateBlockNode : public DecoBlockNode {
  explicit StateBlockNode()
      : DecoBlockNode() {
  }
  void parse(GlSlFxParser* parser, const ScannerView& view);
  void _generate2(shaderbuilder::BackEnd& backend) const final;
  std::string _culltest;
  std::string _depthmask;
  std::string _depthtest;
  std::string _blendmode;
};

struct VertexShaderNode : public ShaderNode {
  explicit VertexShaderNode()
      : ShaderNode() {
  }
  void _generate2(shaderbuilder::BackEnd& backend) const final;
};

struct FragmentShaderNode : public ShaderNode {
  explicit FragmentShaderNode()
      : ShaderNode() {
  }
  void _generate2(shaderbuilder::BackEnd& backend) const final;
};
struct TessCtrlShaderNode : public ShaderNode {
  explicit TessCtrlShaderNode()
      : ShaderNode() {
  }
  void _generate2(shaderbuilder::BackEnd& backend) const final;
};
struct TessEvalShaderNode : public ShaderNode {
  explicit TessEvalShaderNode()
      : ShaderNode() {
  }
  void _generate2(shaderbuilder::BackEnd& backend) const final;
};
struct GeometryShaderNode : public ShaderNode {
  explicit GeometryShaderNode()
      : ShaderNode() {
  }
  void _generate2(shaderbuilder::BackEnd& backend) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct VertexInterfaceNode : public InterfaceNode {
  explicit VertexInterfaceNode()
      : InterfaceNode(GL_VERTEX_SHADER) {
  }
  void _generate1(shaderbuilder::BackEnd& backend) const final;
};
struct FragmentInterfaceNode : public InterfaceNode {
  explicit FragmentInterfaceNode()
      : InterfaceNode(GL_FRAGMENT_SHADER) {
  }
  void _generate1(shaderbuilder::BackEnd& backend) const final;
};
struct TessCtrlInterfaceNode : public InterfaceNode {
  explicit TessCtrlInterfaceNode()
      : InterfaceNode(GL_TESS_CONTROL_SHADER) {
  }
  void _generate1(shaderbuilder::BackEnd& backend) const final;
};
struct TessEvalInterfaceNode : public InterfaceNode {
  explicit TessEvalInterfaceNode()
      : InterfaceNode(GL_TESS_EVALUATION_SHADER) {
  }
  void _generate1(shaderbuilder::BackEnd& backend) const final;
};
struct GeometryInterfaceNode : public InterfaceNode {
  explicit GeometryInterfaceNode()
      : InterfaceNode(GL_GEOMETRY_SHADER) {
  }
  void _generate1(shaderbuilder::BackEnd& backend) const final;
};

struct ComputeShaderNode : public ShaderNode {
  explicit ComputeShaderNode();
  void _generate2(shaderbuilder::BackEnd& backend) const final;
};
struct ComputeInterfaceNode : public InterfaceNode {
  explicit ComputeInterfaceNode();
  void _generate1(shaderbuilder::BackEnd& backend) const final;
};

struct NvTaskInterfaceNode : public InterfaceNode {
  explicit NvTaskInterfaceNode();
  void _generate1(shaderbuilder::BackEnd& backend) const final;
};
struct NvMeshInterfaceNode : public InterfaceNode {
  explicit NvMeshInterfaceNode();
  void _generate1(shaderbuilder::BackEnd& backend) const final;
};
struct NvTaskShaderNode : public ShaderNode {
  explicit NvTaskShaderNode();
  void _generate2(shaderbuilder::BackEnd& backend) const final;
};
struct NvMeshShaderNode : public ShaderNode {
  explicit NvMeshShaderNode();
  void _generate2(shaderbuilder::BackEnd& backend) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformDeclNode : public AstNode {
  std::string _typeName;
  std::string _name;
  int _arraySize               = 0;
  interfacelayoutnode_ptr_t _layout = nullptr;
  void emit(shaderbuilder::BackEnd& backend, bool emit_unitxt) const;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderDataNode : public DecoBlockNode {
  ShaderDataNode()
      : DecoBlockNode() {
  }
  void parse(GlSlFxParser* parser, const ScannerView& view);
  std::vector<uniformdeclnode_ptr_t> _uniformdecls;
  std::set<std::string> _dupenamecheck;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformSetNode : public ShaderDataNode {
  UniformSetNode()
      : ShaderDataNode() {
  }
  void _generate2(shaderbuilder::BackEnd& backend) const final;
  void emit(shaderbuilder::BackEnd& backend) const;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockNode : public ShaderDataNode {
  UniformBlockNode()
      : ShaderDataNode() {
  }
  void _generate2(shaderbuilder::BackEnd& backend) const final;
  void emit(shaderbuilder::BackEnd& backend) const;
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryBlockNode : public DecoBlockNode {
  explicit LibraryBlockNode()
      : DecoBlockNode() {
  }
  void parse(GlSlFxParser* parser, const ScannerView& view);
  void pregen(shaderbuilder::BackEnd& backend) const final;
  void generate(shaderbuilder::BackEnd& backend) const;
  void emitLibrary(shaderbuilder::BackEnd& backend) const;
  // ShaderBody _body;

  std::vector<svar32_t> _children;
  bool _is_typelib = false;
};

///////////////////////////////////////////////////////////////////////////////
struct TopNodeStuff{

};
struct TopNode : public AstNode {

  TopNode(GlSlFxParser* parser);
  bool IsTokenOneOfTheBlockTypes(const Token& tok);

  void parse();
  void pregen(shaderbuilder::BackEnd& backend) const final;
  void _enumerateValidationData(shaderbuilder::BackEnd& backend) const;

  bool isTypeName(const std::string typeName) const;
  bool validateKeyword(const std::string typeName) const;
  void validateTypeName(const std::string typeName) const;
  bool validateIdentifierName(const std::string typeName) const;
  bool isIoAttrDecorator(const std::string typeName) const;

  void addStructType(structnode_ptr_t snode);

  int itokidx = 0;

  GlSlFxParser* _parser = nullptr;
  scanner_constptr_t _scanner;
  confignode_ptr_t _configNode = nullptr;

  std::set<std::string> _validTypeNames;
  std::set<std::string> _keywords;
  std::set<std::string> _validOutputDecorators;
  std::map<std::string, structnode_ptr_t> _structTypes;
  std::map<std::string, std::string> _stddefines;

  std::vector<importnode_ptr_t> _imports;


};

///////////////////////////////////////////////////////////////////////////////

struct ImportNode : public AstNode {
  ImportNode(std::string name, TopNode* parent){
    _name = name;
    _parent_topnode = parent;
  }
  void load();
  void pregen(shaderbuilder::BackEnd& backend) const;

  std::string _name;
  TopNode* _parent_topnode = nullptr;
  parser_ptr_t _parser;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

struct GlSlFxParser {


  GlSlFxParser(std::string name,
               program_ptr_t progam,
               scanner_constptr_t s);

  void DumpAllTokens();

  std::string _name;
  int itokidx = 0;

  scanner_constptr_t _scanner;
  topnode_ptr_t _topNode;
  program_ptr_t _program;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T> void Program::forEachBlockOfType(std::function<void(std::shared_ptr<T>)> operation) {
  for (auto blocknode : _orderedBlockNodes)
    if (auto as_t = std::dynamic_pointer_cast<T>(blocknode))
      operation(as_t);
}
template <typename T> void Program::collectNodesOfType(NodeCollection& nodes) {
  
    const std::string type_name = demangled_typename<T>();

    /*printf("Program<%p:%s> collectingnodes of type<%s>\n", 
           this, 
           _name.c_str(), 
           type_name.c_str() );*/
      
  for (auto blocknode : _orderedBlockNodes){
    if (auto as_typed = std::dynamic_pointer_cast<T>(blocknode)){
      
      namednode_ptr_t namednode = std::dynamic_pointer_cast<NamedBlockNode>(blocknode);

      size_t index = nodes._nodevect.size();
      /*printf("Program<%p:%s> collectingnode<%zu:%p:%s>\n", 
             this, 
             _name.c_str(), 
             index, 
             namednode.get(), 
             namednode->_name.c_str());*/

      auto it = nodes._uniqueset.find(namednode->_name);
      OrkAssert(it==nodes._uniqueset.end());
      nodes._uniqueset.insert(namednode->_name);
      nodes._nodevect.push_back(namednode);
    }
  }
}

template <typename T> void Program::generateBlocks(shaderbuilder::BackEnd& backend) {
  forEachBlockOfType<T>([&](std::shared_ptr<T> as_t) { as_t->generate(backend); });
}

} // parser 

///////////////////////////////////////////////////////////////////////////////

namespace shaderbuilder {
struct Node {};

struct Section : public Node {
  std::vector<node_ptr_t> _children;
};
struct Root {

  std::map<int, section_ptr_t> _sections;
};
struct BackendCodeGen {
  void beginLine();
  void endLine();
  void incIndent();
  void decIndent();
  void format(const char* fmt, ...);
  void output(std::string str);
  void formatLine(const char* fmt, ...);
  std::string flush();
  std::vector<std::string> _lines;
  std::string _curline;
  int _indentLevel = 0;
};
struct BackEnd {
  BackEnd(parser::parser_constptr_t parser, rootcontainer_ptr_t c);
  bool generate();
  void validateTypeName(const std::string typeName) const;
  bool isTypeName(const std::string typeName) const;

  rootcontainer_ptr_t _container;
  parser::parser_constptr_t _parser = nullptr;
  Root _root;
  BackendCodeGen _codegen;
  std::map<std::string, svar32_t> _statemap;

  std::map<std::string, parser::libblock_constptr_t> _libblocks;
  std::map<const parser::DecoBlockNode*,parser::decochildren_ptr_t> _decochildrenmap;

  std::set<std::string> _validTypeNames;
  std::map<std::string, parser::structnode_ptr_t> _structTypes;

};
} // namespace shaderbuilder

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
