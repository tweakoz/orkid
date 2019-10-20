#pragma once

#include <ork/util/scanner.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct ContainerNode;
struct GlSlFxParser;
struct Pass;
struct UniformSetNode;
struct UniformBlockNode;
struct LibraryBlockNode;
struct InterfaceNode;

///////////////////////////////////////////////////////////////////////////////

namespace shaderbuilder {
struct Node {};
struct Section : public Node {

  std::vector<Node*> _children;
};
struct Root {

  std::map<int, Section> _sections;
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
  BackEnd(const ContainerNode* cnode, Container* c);
  bool generate();

  Container* _container       = nullptr;
  const ContainerNode* _cnode = nullptr;
  Root _root;
  BackendCodeGen _codegen;
  std::map<std::string, svar32_t> _statemap;
};
} // namespace shaderbuilder

///////////////////////////////////////////////////////////////////////////////

struct FnParseContext {
  FnParseContext(ContainerNode* c, const ScannerView& v)
      : _container(c)
      , _view(v) {
  }
  FnParseContext(const FnParseContext&oth)
    : _container(oth._container)
    , _view(oth._view)
    , _startIndex(oth._startIndex){

  }
  FnParseContext operator = (const FnParseContext&oth){
    FnParseContext rval(oth._container,oth._view);
    rval._startIndex = oth._startIndex;
    return rval;
  }
  FnParseContext advance (size_t count) const {
    FnParseContext rval(*this);
    rval._startIndex = count;
    return rval;
  }

  std::string tokenValue(size_t offset) const;

  ContainerNode* _container = nullptr;
  size_t _startIndex = 0;
  const ScannerView& _view;
};

struct FnMatchResultsBas {

  FnMatchResultsBas(FnParseContext ctx)
      : _ctx(ctx) {
  }
  operator bool() const {
    return _matched;
  }



  size_t _start = -1;
  size_t _count   = -1;
  size_t end() const { return _start+_count; }
  bool _matched = false;
  FnParseContext _ctx;
};

template <typename T> struct FnMatchResults : public FnMatchResultsBas {

  FnMatchResults(FnParseContext ctx)
      : FnMatchResultsBas(ctx) {
  }
  FnMatchResults(const FnMatchResultsBas& oth)
      : FnMatchResultsBas(oth) {
  }

  struct ParseResult {
    size_t _numtokens = 0;
    T* _node          = nullptr;
  };

  ParseResult parse() {
    return T::parse(*this);
  }

  FnMatchResults operator+ (const FnMatchResults& rhs) const {
    FnMatchResults rval = *this;
    assert(_matched and rhs._matched);
    rval._count += rhs._count;
    return rval;
  }

  FnParseContext consume() const {
    assert(_matched);
    FnParseContext rval = _ctx;
    rval._startIndex += _count;
    return rval;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct AstNode {
  AstNode(ContainerNode* cnode = nullptr)
      : _container(cnode) {
  }
  virtual ~AstNode() {
  }
  virtual void generate(shaderbuilder::BackEnd& backend) const {
  }
  virtual void pregen(shaderbuilder::BackEnd& backend) {
  }
  ContainerNode* _container;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderBodyElement : public AstNode {
  ShaderBodyElement(ContainerNode* cnode)
      : AstNode(cnode) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderEmittable : public AstNode {
  ShaderEmittable(ContainerNode* cnode)
      : AstNode(cnode) {
  }
  virtual void emit(shaderbuilder::BackEnd& backend) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

#define DECLARE_STD_FNS(xxx)\
typedef FnMatchResults<xxx> match_t;\
typedef match_t::ParseResult parsed_t;\
static match_t match(FnParseContext ctx);\
static parsed_t parse(const match_t& match);\

#define DECLARE_STD_EMITTABLE_FNS(xxx)\
DECLARE_STD_FNS(xxx)\
void emit(shaderbuilder::BackEnd& backend) const final;\

#define DECLARE_STD_EMITTABLE(xxx)\
struct xxx : public ShaderEmittable {\
xxx(ContainerNode* cnode)\
    : ShaderEmittable(cnode) {\
}\
DECLARE_STD_EMITTABLE_FNS(xxx)\
};

///////////////////////////////////////////////////////////////////////////////

DECLARE_STD_EMITTABLE(OpenBracket);
DECLARE_STD_EMITTABLE(CloseBracket);

///////////////////////////////////////////////////////////////////////////////

struct FnElement : public ShaderEmittable {
  FnElement(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct VariableDeclaration : public FnElement {
  VariableDeclaration(ContainerNode* cnode)
      : FnElement(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(VariableDeclaration);
};

///////////////////////////////////////////////////////////////////////////////

struct DeclarationList : public ShaderEmittable {
  DeclarationList(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(DeclarationList);
  std::vector<VariableDeclaration*> _children;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct Expression : public ShaderEmittable {
  Expression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(Expression);
  std::vector<ShaderBodyElement*> _children;
};

struct AssignmentExpression : public ShaderEmittable {
  AssignmentExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(AssignmentExpression);
};

struct ConditionalExpression : public ShaderEmittable {
  ConditionalExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_FNS(ConditionalExpression);
};

struct TernaryExpression : public ConditionalExpression {
  TernaryExpression(ContainerNode* cnode)
      : ConditionalExpression(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(TernaryExpression);
};

struct LogicalOrExpression : public ShaderEmittable {
  LogicalOrExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(LogicalOrExpression);
};

struct LogicalAndExpression : public ShaderEmittable {
  LogicalAndExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(LogicalAndExpression);
};

struct InclusiveOrExpression : public ShaderEmittable {
  InclusiveOrExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(InclusiveOrExpression);
};

struct ExclusiveOrExpression : public ShaderEmittable {
  ExclusiveOrExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(ExclusiveOrExpression);
};

struct AndExpression : public ShaderEmittable {
  AndExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(AndExpression);
};

struct EqualityExpression : public ShaderEmittable {
  EqualityExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(EqualityExpression);
};

struct RelationalExpression : public ShaderEmittable {
  RelationalExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(RelationalExpression);
};

struct ShiftExpression : public ShaderEmittable {
  ShiftExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(ShiftExpression);
};

struct AdditiveExpression : public ShaderEmittable {
  AdditiveExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(AdditiveExpression);
};

struct UnaryExpression : public ShaderEmittable {
  UnaryExpression(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(UnaryExpression);
};

struct AssignmentOperator : public ShaderEmittable {
  AssignmentOperator(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(AssignmentOperator);
};

///////////////////////////////////////////////////////////////////////////////
struct Statement : public ShaderEmittable {
  Statement(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(Statement);
};

///////////////////////////////////////////////////////////////////////////////

struct ExpressionStatement : public ShaderEmittable {
  ExpressionStatement(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(ExpressionStatement);
};

///////////////////////////////////////////////////////////////////////////////

struct StatementList : public ShaderEmittable {
  StatementList(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(StatementList);
  std::vector<Statement*> _children;
};

///////////////////////////////////////////////////////////////////////////////

struct CompoundStatement : public FnElement {
  CompoundStatement(ContainerNode* cnode)
      : FnElement(cnode) {
  }
  DECLARE_STD_EMITTABLE_FNS(CompoundStatement);
};

///////////////////////////////////////////////////////////////////////////////

struct IterationStatement : public ShaderEmittable {
  IterationStatement(ContainerNode* cnode)
      : ShaderEmittable(cnode) {
  }
  DECLARE_STD_FNS(IterationStatement);
};

///////////////////////////////////////////////////////////////////////////////
/*
struct ReturnStatement : public StatementNode {
  ReturnStatement(ContainerNode* cnode)
      : StatementNode(cnode) {}

  typedef FnMatchResults<ReturnStatement> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

  Expression* _returnValue = nullptr;
};
*/

///////////////////////////////////////////////////////////////////////////////

struct ForLoopStatement : public IterationStatement {
  ForLoopStatement(ContainerNode* cnode)
      : IterationStatement(cnode) {}

  DECLARE_STD_EMITTABLE_FNS(ForLoopStatement);

  const Token* _variable = nullptr;
  Expression* _condition = nullptr;
  //AssignmentNode* _advance = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
/*
struct WhileLoopStatement : public IterationStatement {
  WhileLoopStatement(ContainerNode* cnode)
      : IterationStatement(cnode) {}

  typedef FnMatchResults<WhileLoopStatement> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

  Expression* _condition = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct ElseNode : public ShaderEmittable {
  ElseNode(ContainerNode* cnode)
      : ShaderEmittable(cnode) {}

  typedef FnMatchResults<ElseNode> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

};

struct ElseIfNode : public ShaderEmittable {
  ElseIfNode(ContainerNode* cnode)
      : ShaderEmittable(cnode) {}

  typedef FnMatchResults<ElseIfNode> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

  Expression* _condition = nullptr;
};

struct IfStatement : public StatementNode {
  IfStatement(ContainerNode* cnode)
      : StatementNode(cnode) {}
  typedef FnMatchResults<IfStatement> match_t;
  typedef match_t::ParseResult parsed_t;
  static match_t match(const FnParseContext& ctx);
  static parsed_t parse(const match_t& match);
  void emit(shaderbuilder::BackEnd& backend) const final;

  Expression* _condition = nullptr;
  std::vector<ElseIfNode*> _elseifs;
  ElseNode* _elseNode = nullptr;
};
*/
///////////////////////////////////////////////////////////////////////////////

struct ParsedFunctionNode : public AstNode {
  ParsedFunctionNode(ContainerNode* cnode)
      : AstNode(cnode) {
  }

  int parse(const ScannerView& view);
  void emit(shaderbuilder::BackEnd& backend) const;

  std::vector<FnElement*> _elements;
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryStructMemberNode : public AstNode {
  std::string _typename;
  std::string _identifier;
};

///////////////////////////////////////////////////////////////////////////////

struct NamedBlockNode : public AstNode {

  NamedBlockNode(ContainerNode* cnode)
      : AstNode(cnode) {
  }

  void parse(const ScannerView& view);

  std::string _name;
  std::string _blocktype;

  std::vector<const Token*> _decorators;
  std::set<std::string> _decodupecheck;
};

///////////////////////////////////////////////////////////////////////////////

struct ConfigNode : public NamedBlockNode {
  ConfigNode(ContainerNode* cnode)
      : NamedBlockNode(cnode) {
  }

  std::string _name;
  std::vector<GlSlFxParser*> _imports;

  void parse(const ScannerView& view);
  void generate(Container* c) const;
};

///////////////////////////////////////////////////////////////////////////////

struct RequiredExtensionNode : public AstNode {
  RequiredExtensionNode(ContainerNode* c)
      : AstNode(c) {
  }
  void emit(shaderbuilder::BackEnd& backend);
  std::string _extension;
};

///////////////////////////////////////////////////////////////////////////////

struct DecoBlockNode : public NamedBlockNode {

  DecoBlockNode(ContainerNode* cnode)
      : NamedBlockNode(cnode) {
  }

  void parse(const ScannerView& view);
  void _pregen(shaderbuilder::BackEnd& backend);
  void _emit(shaderbuilder::BackEnd& backend) const;
  virtual void emit(shaderbuilder::BackEnd& backend) const {
    _emit(backend);
  }

  std::vector<RequiredExtensionNode*> _requiredExtensions;

  // children nodes
  std::vector<InterfaceNode*> _interfaceNodes;
  std::vector<LibraryBlockNode*> _libraryBlocks;
  std::vector<UniformSetNode*> _uniformSets;
  std::vector<UniformBlockNode*> _uniformBlocks;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderLine {
  int _indent = 0;
  std::vector<const Token*> _tokens;
};
typedef std::vector<ShaderLine*> linevect_t;

///////////////////////////////////////////////////////////////////////////////

struct ShaderBody {
  linevect_t _lines;
  int parse(const ScannerView& view);
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
  StructNode(ContainerNode* cnode)
      : AstNode(cnode) {
  }
  void pregen(shaderbuilder::BackEnd& backend) final;
  int parse(const ScannerView& view);
  void emit(shaderbuilder::BackEnd& backend) const;
  std::vector<StructMemberNode*> _members;
  const Token* _name      = nullptr;
  bool _emitstructandname = true;
};

///////////////////////////////////////////////////////////////////////////////

struct FunctionArgumentNode : public AstNode {
  FunctionArgumentNode(ContainerNode* cnode)
      : AstNode(cnode) {
  }
  const Token* _type      = nullptr;
  const Token* _name      = nullptr;
  const Token* _direction = nullptr;
  int _arraySize          = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct FunctionNode : public AstNode {
  FunctionNode(ContainerNode* cnode)
      : AstNode(cnode) {
  }
  void pregen(shaderbuilder::BackEnd& backend) final;
  int parse(const ScannerView& view);
  void emit(shaderbuilder::BackEnd& backend) const;
  const Token* _name = nullptr;
  std::vector<FunctionArgumentNode*> _arguments;
  const Token* _returnType = nullptr;
  ShaderBody _body;

  ParsedFunctionNode* _parsedfnnode = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct PassNode : public NamedBlockNode {
  PassNode(ContainerNode* cnode)
      : NamedBlockNode(cnode) {
  }

  void generate(shaderbuilder::BackEnd& backend) const final;

  int parse(const ScannerView& view, int start);

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
  TechniqueNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {
  }
  void parse(const ScannerView& view);
  void generate(shaderbuilder::BackEnd& backend) const final;
  std::string _fxconfig;
  std::unordered_map<std::string, PassNode*> _passNodes;
};

///////////////////////////////////////////////////////////////////////////////

struct InterfaceLayoutNode : public AstNode {
  InterfaceLayoutNode(ContainerNode* cnode)
      : AstNode(cnode) {
  }
  int parse(const ScannerView& view);
  void pregen(shaderbuilder::BackEnd& backend) final;
  void emit(shaderbuilder::BackEnd& backend);
  std::vector<const Token*> _tokens;
  std::string _direction;
  bool _standaloneLayout = false;
};

struct InterfaceIoNode : public AstNode {
  InterfaceIoNode(ContainerNode* cnode)
      : AstNode(cnode) {
  }

  void pregen(shaderbuilder::BackEnd& backend) final;

  std::string _name;
  std::string _typeName;
  StructNode* _inlineStruct = nullptr;
  std::string _semantic;
  InterfaceLayoutNode* _layout = nullptr;
  std::set<std::string> _decorators;
  int _arraySize = 0;
};

///////////////////////////////////////////////////////////////////////////////

typedef std::vector<InterfaceIoNode*> ionodevect_t;

struct IoContainerNode : public AstNode {

  IoContainerNode(ContainerNode* c)
      : AstNode(c) {
  }

  void pregen(shaderbuilder::BackEnd& backend) final;
  void emit(shaderbuilder::BackEnd& backend) const;
  ionodevect_t _nodes;
  std::set<std::string> _dupecheck;
  InterfaceLayoutNode* _pendinglayout = nullptr;
  std::vector<InterfaceLayoutNode*> _layouts;
  std::string _direction;
};

///////////////////////////////////////////////////////////////////////////////

struct InterfaceNode : public DecoBlockNode {
  InterfaceNode(ContainerNode* cnode, GLenum type)
      : DecoBlockNode(cnode)
      , _gltype(type) {
    _inputs              = new IoContainerNode(cnode);
    _outputs             = new IoContainerNode(cnode);
    _storage             = new IoContainerNode(cnode);
    _inputs->_direction  = "in";
    _outputs->_direction = "out";
    _storage->_direction = "storage";
  }

  void parse(const ScannerView& view);
  void parseIos(const ScannerView& view, IoContainerNode* ioc);

  void pregen(shaderbuilder::BackEnd& backend) final;
  void emit(shaderbuilder::BackEnd& backend) const override;
  void _generate(shaderbuilder::BackEnd& backend) const;

  std::vector<InterfaceLayoutNode*> _interfacelayouts;
  IoContainerNode* _inputs  = nullptr;
  IoContainerNode* _outputs = nullptr;
  IoContainerNode* _storage = nullptr;
  GLenum _gltype            = GL_NONE;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderNode : public DecoBlockNode {
  explicit ShaderNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {
  }
  void pregen(shaderbuilder::BackEnd& backend) final;
  void parse(const ScannerView& view);
  void _generateCommon(shaderbuilder::BackEnd& backend) const;
  ShaderBody _body;
};

///////////////////////////////////////////////////////////////////////////////

struct StateBlockNode : public DecoBlockNode {
  explicit StateBlockNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {
  }
  void parse(const ScannerView& view);
  void generate(shaderbuilder::BackEnd& backend) const final;
  std::string _culltest;
  std::string _depthmask;
  std::string _depthtest;
  std::string _blendmode;
};

struct VertexShaderNode : public ShaderNode {
  explicit VertexShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};

struct FragmentShaderNode : public ShaderNode {
  explicit FragmentShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct TessCtrlShaderNode : public ShaderNode {
  explicit TessCtrlShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct TessEvalShaderNode : public ShaderNode {
  explicit TessEvalShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct GeometryShaderNode : public ShaderNode {
  explicit GeometryShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct VertexInterfaceNode : public InterfaceNode {
  explicit VertexInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode, GL_VERTEX_SHADER) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct FragmentInterfaceNode : public InterfaceNode {
  explicit FragmentInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode, GL_FRAGMENT_SHADER) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct TessCtrlInterfaceNode : public InterfaceNode {
  explicit TessCtrlInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode, GL_TESS_CONTROL_SHADER) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct TessEvalInterfaceNode : public InterfaceNode {
  explicit TessEvalInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode, GL_TESS_EVALUATION_SHADER) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct GeometryInterfaceNode : public InterfaceNode {
  explicit GeometryInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode, GL_GEOMETRY_SHADER) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};

#if defined ENABLE_COMPUTE_SHADERS
struct ComputeShaderNode : public ShaderNode {
  explicit ComputeShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct ComputeInterfaceNode : public InterfaceNode {
  explicit ComputeInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode, GL_COMPUTE_SHADER) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
#endif

#if defined ENABLE_NVMESH_SHADERS
struct NvTaskInterfaceNode : public InterfaceNode {
  explicit NvTaskInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode, GL_TASK_SHADER_NV) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct NvMeshInterfaceNode : public InterfaceNode {
  explicit NvMeshInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode, GL_MESH_SHADER_NV) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct NvTaskShaderNode : public ShaderNode {
  explicit NvTaskShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
struct NvMeshShaderNode : public ShaderNode {
  explicit NvMeshShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
};
#endif

///////////////////////////////////////////////////////////////////////////////

struct UniformDeclNode : public AstNode {
  std::string _typeName;
  std::string _name;
  int _arraySize               = 0;
  InterfaceLayoutNode* _layout = nullptr;
  void emit(shaderbuilder::BackEnd& backend, bool emit_unitxt) const;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderDataNode : public DecoBlockNode {
  ShaderDataNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {
  }
  void parse(const ScannerView& view);
  std::vector<UniformDeclNode*> _uniformdecls;
  std::set<std::string> _dupenamecheck;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformSetNode : public ShaderDataNode {
  UniformSetNode(ContainerNode* cnode)
      : ShaderDataNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
  void emit(shaderbuilder::BackEnd& backend) const;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockNode : public ShaderDataNode {
  UniformBlockNode(ContainerNode* cnode)
      : ShaderDataNode(cnode) {
  }
  void generate(shaderbuilder::BackEnd& backend) const final;
  void emit(shaderbuilder::BackEnd& backend) const;
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryBlockNode : public DecoBlockNode {
  explicit LibraryBlockNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {
  }
  void parse(const ScannerView& view);
  void pregen(shaderbuilder::BackEnd& backend) final;
  void generate(shaderbuilder::BackEnd& backend) const;
  void emit(shaderbuilder::BackEnd& backend) const;
  // ShaderBody _body;

  std::vector<svarp_t> _children;
};

///////////////////////////////////////////////////////////////////////////////

struct ContainerNode : public AstNode {

  ContainerNode(const AssetPath& pth, const Scanner& s);
  bool IsTokenOneOfTheBlockTypes(const Token& tok);

  void parse();

  bool validateTypeName(const std::string typeName) const;
  bool validateIdentifierName(const std::string typeName) const;
  bool isIoAttrDecorator(const std::string typeName) const;

  typedef std::vector<AstNode*> nodevect_t;

  template <typename T> void generateBlocks(shaderbuilder::BackEnd& backend) const;
  template <typename T> void forEachBlockOfType(std::function<void(T*)> operation) const;
  template <typename T> void collectNodesOfType(nodevect_t& outnvect) const;
  void generate(shaderbuilder::BackEnd& backend) const final;

  void addStructType(StructNode* snode);
  nodevect_t collectAllNodes() const;

  int itokidx = 0;

  void addBlockNode(DecoBlockNode* node);

  const Scanner& _scanner;
  const AssetPath _path;
  ConfigNode* _configNode = nullptr;

  std::vector<DecoBlockNode*> _orderedBlockNodes;

  std::unordered_map<std::string, DecoBlockNode*> _blockNodes;
  std::unordered_map<std::string, TechniqueNode*> _techniqueNodes;

  std::set<std::string> _validTypeNames;
  std::set<std::string> _validOutputDecorators;
  std::map<std::string, StructNode*> _structTypes;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

template <typename T> void ContainerNode::forEachBlockOfType(std::function<void(T*)> operation) const {
  for (auto blocknode : _orderedBlockNodes)
    if (auto as_t = dynamic_cast<T*>(blocknode))
      operation(as_t);
}
template <typename T> void ContainerNode::collectNodesOfType(nodevect_t& outnvect) const {
  for (auto blocknode : _orderedBlockNodes)
    if (auto as_t = dynamic_cast<T*>(blocknode))
      outnvect.push_back(as_t);
}

template <typename T> void ContainerNode::generateBlocks(shaderbuilder::BackEnd& backend) const {
  forEachBlockOfType<T>([&](T* as_t) { as_t->generate(backend); });
}

/////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<AstNode> astnode_t;

struct GlSlFxParser {

  GlSlFxParser(const std::string& pth, const Scanner& s);

  void DumpAllTokens();

  const std::string mPath;
  const Scanner& scanner;

  int itokidx              = 0;
  Container* mpContainer   = nullptr;
  ContainerNode* _rootNode = nullptr;
};
/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::glslfx
/////////////////////////////////////////////////////////////////////////////////////////////////
