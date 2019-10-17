#pragma once

#include <ork/util/scanner.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct ContainerNode;
struct GlSlFxParser;

///////////////////////////////////////////////////////////////////////////////

namespace shaderbuilder {
struct Node {

};
struct Section : public Node {

  std::vector<Node*> _children;
};
struct Root {
  
  std::map<int,Section> _sections;
  
};
struct BackEnd {
    BackEnd(const ContainerNode* cnode, Container* c);
    bool generate();
    Container* _container = nullptr;
    const ContainerNode* _cnode = nullptr;
    Root _root;
};
} // namespace shaderbuilder

///////////////////////////////////////////////////////////////////////////////

struct AstNode {
  AstNode(ContainerNode* cnode = nullptr)
      : _container(cnode) {}
  virtual ~AstNode() {}
  ContainerNode* _container;
};

///////////////////////////////////////////////////////////////////////////////

struct ConstantNumericNode : public AstNode {};

///////////////////////////////////////////////////////////////////////////////

struct ExpressionNode : public AstNode {};

///////////////////////////////////////////////////////////////////////////////

struct AssignmentNode : public AstNode {};

///////////////////////////////////////////////////////////////////////////////

struct VariableDefinitionNode : public AstNode {
  AssignmentNode* _assigment = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct ForLoopNode : public AstNode {};

///////////////////////////////////////////////////////////////////////////////

struct StatementNode : public AstNode {};

///////////////////////////////////////////////////////////////////////////////

struct LibraryFunctionNode : public AstNode {
  std::vector<StatementNode*> _statements;
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryStructMemberNode : public AstNode {
  std::string _typename;
  std::string _identifier;
};

///////////////////////////////////////////////////////////////////////////////

struct NamedBlockNode : public AstNode {

  NamedBlockNode(ContainerNode* cnode)
      : AstNode(cnode) {}

  void parse(const ScannerView& view);

  std::string _name;
  std::string _blocktype;

  std::vector<const Token*> _decorators;
  std::set<std::string> _decodupecheck;
};

///////////////////////////////////////////////////////////////////////////////

struct ConfigNode : public NamedBlockNode {
  ConfigNode(ContainerNode* cnode)
      : NamedBlockNode(cnode) {}

  std::string _name;
  std::vector<GlSlFxParser*> _imports;

  void parse(const ScannerView& view);
  void generate(Container *c) const;
};

///////////////////////////////////////////////////////////////////////////////

struct DecoBlockNode : public NamedBlockNode {

  DecoBlockNode(ContainerNode* cnode)
      : NamedBlockNode(cnode) {}

  void parse(const ScannerView& view);

    std::vector<std::string> _requiredExtensions;
};


///////////////////////////////////////////////////////////////////////////////

struct PassNode : public NamedBlockNode {
  PassNode(ContainerNode* cnode)
      : NamedBlockNode(cnode) {}

  Pass* generate(shaderbuilder::BackEnd& backend) const;

  int parse(const ScannerView& view, int startok );
  
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
      : DecoBlockNode(cnode) {}
  void parse(const ScannerView& view);
  Technique* generate(shaderbuilder::BackEnd& backend) const;
  std::string _fxconfig;
  std::unordered_map<std::string, PassNode*> _passNodes;
};

///////////////////////////////////////////////////////////////////////////////

struct InterfaceLayoutNode : public AstNode {
  InterfaceLayoutNode(ContainerNode* cnode)
      : AstNode(cnode) {}
  int parse(const ScannerView& view);
  std::vector<const Token*> _tokens;
  std::string _direction;
  bool _standaloneLayout = false;
};

struct InterfaceInlineStructNode : public AstNode {
  InterfaceInlineStructNode(ContainerNode* cnode)
      : AstNode(cnode) {}
  std::vector<const Token*> _tokens;
};

struct InterfaceIoNode : public AstNode {
  InterfaceIoNode(ContainerNode* cnode)
      : AstNode(cnode) {}
      
  std::string _name;
  std::string _typeName;
  InterfaceInlineStructNode* _inlineStruct = nullptr;
  std::string _semantic;
  InterfaceLayoutNode* _layout = nullptr;
  std::set<std::string> _decorators;
  int _arraySize = 0;
  
};

///////////////////////////////////////////////////////////////////////////////

struct InterfaceNode : public DecoBlockNode {
  InterfaceNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {
    _inputs._direction = "in";
    _outputs._direction = "out";
    _storage._direction = "storage";
  }

      
  typedef std::vector<InterfaceIoNode*> ionodevect_t;
  
  struct IoContainer {
    ionodevect_t _nodes;
    std::set<std::string> _dupecheck;
    InterfaceLayoutNode* _pendinglayout = nullptr;
    std::vector<InterfaceLayoutNode*> _layouts;
    std::string _direction;
  };
      
  
  void parse(const ScannerView& view);
  void parseIos(const ScannerView& view,IoContainer& ioc);

  StreamInterface* _generate(shaderbuilder::BackEnd& backend, GLenum type);
  std::vector<InterfaceLayoutNode*> _interfacelayouts;
  IoContainer _inputs;
  IoContainer _outputs;
  IoContainer _storage;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderLine {
  int _indent = 0;
  std::vector<const Token*> _tokens;
};
typedef std::vector<ShaderLine*> linevect_t;
struct ShaderBody {
  linevect_t _lines;
  void parse(const ScannerView& view);
};

struct ShaderNode : public DecoBlockNode {
  explicit ShaderNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {}
  void parse(const ScannerView& view);
  void _generateCommon(Shader* subsh);
  ShaderBody _body;
};

///////////////////////////////////////////////////////////////////////////////

struct StateBlockNode : public DecoBlockNode {
  explicit StateBlockNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {}
  void parse(const ScannerView& view);
  StateBlock* generate(shaderbuilder::BackEnd& backend) const;
  std::string _culltest;
  std::string _depthmask;
  std::string _depthtest;
  std::string _blendmode;
};

struct VertexShaderNode : public ShaderNode {
  explicit VertexShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {}
  ShaderVtx* generate(shaderbuilder::BackEnd& backend);
};

struct FragmentShaderNode : public ShaderNode {
  explicit FragmentShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {}
  ShaderFrg* generate(shaderbuilder::BackEnd& backend);
};
struct TessCtrlShaderNode : public ShaderNode {
  explicit TessCtrlShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {}
  ShaderTsC* generate(shaderbuilder::BackEnd& backend);
};
struct TessEvalShaderNode : public ShaderNode {
  explicit TessEvalShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {}
  ShaderTsE* generate(shaderbuilder::BackEnd& backend);
};
struct GeometryShaderNode : public ShaderNode {
  explicit GeometryShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {}
  ShaderGeo* generate(shaderbuilder::BackEnd& backend);
};

///////////////////////////////////////////////////////////////////////////////

struct VertexInterfaceNode : public InterfaceNode {
  explicit VertexInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode) {}
  StreamInterface* generate(shaderbuilder::BackEnd& backend);
};
struct FragmentInterfaceNode : public InterfaceNode {
  explicit FragmentInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode) {}
  StreamInterface* generate(shaderbuilder::BackEnd& backend);
};
struct TessCtrlInterfaceNode : public InterfaceNode {
  explicit TessCtrlInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode) {}
  StreamInterface* generate(shaderbuilder::BackEnd& backend);
};
struct TessEvalInterfaceNode : public InterfaceNode {
  explicit TessEvalInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode) {}
  StreamInterface* generate(shaderbuilder::BackEnd& backend);
};
struct GeometryInterfaceNode : public InterfaceNode {
  explicit GeometryInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode) {}
  StreamInterface* generate(shaderbuilder::BackEnd& backend);
};

#if defined ENABLE_COMPUTE_SHADERS
struct ComputeShaderNode : public ShaderNode {
  explicit ComputeShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {}
  ComputeShader* generate(shaderbuilder::BackEnd& backend);
};
struct ComputeInterfaceNode : public InterfaceNode {
  explicit ComputeInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode) {}
  StreamInterface* generate(shaderbuilder::BackEnd& backend);
};
#endif

#if defined ENABLE_NVMESH_SHADERS
struct NvTaskInterfaceNode : public InterfaceNode {
  explicit NvTaskInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode) {}
  StreamInterface* generate(shaderbuilder::BackEnd& backend);
};
struct NvMeshInterfaceNode : public InterfaceNode {
  explicit NvMeshInterfaceNode(ContainerNode* cnode)
      : InterfaceNode(cnode) {}
  StreamInterface* generate(shaderbuilder::BackEnd& backend);
};
struct NvTaskShaderNode : public ShaderNode {
  explicit NvTaskShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {}
  ShaderNvTask* generate(shaderbuilder::BackEnd& backend);
};
struct NvMeshShaderNode : public ShaderNode {
  explicit NvMeshShaderNode(ContainerNode* cnode)
      : ShaderNode(cnode) {}
  ShaderNvMesh* generate(shaderbuilder::BackEnd& backend);
};
#endif

///////////////////////////////////////////////////////////////////////////////

struct UniformDeclNode : public AstNode {
  std::string _typeName;
  std::string _name;
  int _arraySize = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderDataNode : public DecoBlockNode {
  ShaderDataNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {}
  void parse(const ScannerView& view);
  std::vector<UniformDeclNode*> _uniformdecls;
  std::set<std::string> _dupenamecheck;
  virtual void generate(shaderbuilder::BackEnd& backend) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformSetNode : public ShaderDataNode {
  UniformSetNode(ContainerNode* cnode)
      : ShaderDataNode(cnode) {}
  void generate(shaderbuilder::BackEnd& backend) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockNode : public ShaderDataNode {
  UniformBlockNode(ContainerNode* cnode)
      : ShaderDataNode(cnode) {}
  void generate(shaderbuilder::BackEnd& backend) const final;
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryStructNode : public AstNode {
  std::vector<LibraryStructMemberNode*> _memberNodes;
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryBlockNode : public DecoBlockNode {
  explicit LibraryBlockNode(ContainerNode* cnode)
      : DecoBlockNode(cnode) {}
  // std::unordered_map<std::string, LibraryFunctionNode*> _functionNodes;
  // std::unordered_map<std::string, LibraryStructNode*> _structNodes;
  void parse(const ScannerView& view);
  void generate(shaderbuilder::BackEnd& backend) const;
  ShaderBody _body;
};

///////////////////////////////////////////////////////////////////////////////

struct ContainerNode : public AstNode {

  ContainerNode(const AssetPath& pth, const Scanner& s);
  bool IsTokenOneOfTheBlockTypes(const Token& tok);

  void parse();

  bool validateTypeName(const std::string typeName) const;
  bool validateMemberName(const std::string typeName) const;
  bool isIoAttrDecorator(const std::string typeName) const;

  Container* createContainer() const;
  
  template <typename T> void generateBlocks(shaderbuilder::BackEnd& backend) const;
  template <typename T> void forEachBlockOfType(std::function<void(T*)> operation) const;
  void generate(shaderbuilder::BackEnd& backend) const;
  
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
};

/////////////////////////////////////////////////////////////////////////////////////////////////


template <typename T> void ContainerNode::forEachBlockOfType(std::function<void(T*)> operation) const {
   for( auto blocknode : _orderedBlockNodes )
     if( auto as_t = dynamic_cast<T*>(blocknode) )
         operation(as_t);
}

template <typename T> void ContainerNode::generateBlocks(shaderbuilder::BackEnd& backend) const {
   forEachBlockOfType<T>([&](T*as_t){as_t->generate(backend);});
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
