#pragma once

#include "glslfxi_scanner.h"

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct ContainerNode;

struct AstNode {
  AstNode(ContainerNode* cnode=nullptr) : _container(cnode) {}
  ContainerNode* _container;
};

///////////////////////////////////////////////////////////////////////////////

struct ConstantNumericNode : public AstNode {

};


///////////////////////////////////////////////////////////////////////////////

struct ExpressionNode : public AstNode {

};

///////////////////////////////////////////////////////////////////////////////

struct AssignmentNode : public AstNode {

};

///////////////////////////////////////////////////////////////////////////////

struct VariableDefinitionNode : public AstNode {
  AssignmentNode* _assigment = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct ForLoopNode : public AstNode {

};

///////////////////////////////////////////////////////////////////////////////

struct StatementNode : public AstNode {

};

///////////////////////////////////////////////////////////////////////////////

struct LibraryFunctionNode  : public AstNode {
  std::vector<StatementNode*> _statements;

};

///////////////////////////////////////////////////////////////////////////////

struct LibraryStructMemberNode  : public AstNode {
  std::string _typename;
  std::string _identifier;
};

///////////////////////////////////////////////////////////////////////////////

struct ConfigNode  : public AstNode {
  ConfigNode(ContainerNode* cnode) : AstNode(cnode) {}

  std::string _name;
  void parse(ScannerView view);
  Config* generate() const;
};

///////////////////////////////////////////////////////////////////////////////

struct PassNode  : public AstNode {
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

struct TechniqueNode  : public AstNode {
  std::vector<PassNode*> _passNodes;
};

///////////////////////////////////////////////////////////////////////////////

struct DecoBlockNode  : public AstNode {

  DecoBlockNode(ContainerNode* cnode) : AstNode(cnode) {}

  void parse(const ScannerView& view);

  std::string _name;
  std::string _blocktype;

  std::vector<std::string> _deconames;
  std::set<std::string> _decodupecheck;
};

///////////////////////////////////////////////////////////////////////////////

struct InterfaceLayoutNode : public AstNode {
  InterfaceLayoutNode(ContainerNode* cnode) : AstNode(cnode) {}
  int parse(const ScannerView& view);
  std::vector<const Token*> _tokens;
};

struct InterfaceInlineStructNode : public AstNode {
  InterfaceInlineStructNode(ContainerNode* cnode) : AstNode(cnode) {}
  std::vector<const Token*> _tokens;
};

struct InterfaceInputNode : public AstNode {
  InterfaceInputNode(ContainerNode* cnode) : AstNode(cnode) {}
  std::string _name;
  std::string _typeName;
  std::string _semantic;
  int _arraySize = 0;
};

struct InterfaceOutputNode : public AstNode {
  InterfaceOutputNode(ContainerNode* cnode) : AstNode(cnode) {}
  std::string _name;
  std::string _typeName;
  InterfaceInlineStructNode* _inlineStruct = nullptr;
  std::string _semantic;
  std::set<std::string> _output_decorators;
  int _arraySize = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct InterfaceNode  : public DecoBlockNode {
  InterfaceNode(ContainerNode* cnode) : DecoBlockNode(cnode) {}

  void parse(const ScannerView& view);
  void parseInputs(const ScannerView& view);
  void parseOutputs(const ScannerView& view);


  StreamInterface* _generate(Container*, GLenum type);
  std::vector<InterfaceLayoutNode*> _inputlayouts;
  std::vector<InterfaceLayoutNode*> _outputlayouts;
  std::vector<InterfaceInputNode*> _inputs;
  std::vector<InterfaceOutputNode*> _outputs;
  std::set<std::string> _inputdupecheck;
  std::set<std::string> _outputdupecheck;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderNode : public DecoBlockNode {


};

///////////////////////////////////////////////////////////////////////////////

struct VertexInterfaceNode  : public InterfaceNode {
  VertexInterfaceNode(ContainerNode* cnode) : InterfaceNode(cnode) {}
  StreamInterface* generate(Container*);
};
struct FragmentInterfaceNode  : public InterfaceNode {
  FragmentInterfaceNode(ContainerNode* cnode) : InterfaceNode(cnode) {}
  StreamInterface* generate(Container*);
};

struct VertexShaderNode : public ShaderNode {};
struct FragmentShaderNode : public ShaderNode {};
struct TessCtrlInterfaceNode  : public InterfaceNode {};
struct TessCtrlShaderNode : public ShaderNode {};
struct TessEvalInterfaceNode  : public InterfaceNode {};
struct TessEvalShaderNode : public ShaderNode {};
struct GeometryInterfaceNode  : public InterfaceNode {};
struct GeometryShaderNode : public ShaderNode {};

#if defined ENABLE_NVMESH_SHADERS
struct NvTaskInterfaceNode : public InterfaceNode {};
struct NvMeshInterfaceNode : public InterfaceNode {};
struct NvTaskShaderNode : public ShaderNode {};
struct NvMeshShaderNode : public ShaderNode {};
#endif

///////////////////////////////////////////////////////////////////////////////

struct UniformDeclNode : public AstNode {
  std::string _typeName;
  std::string _name;
  int _arraySize = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderDataNode : public DecoBlockNode {
  ShaderDataNode(ContainerNode* cnode) : DecoBlockNode(cnode) {}
  void parse(ScannerView view);
  std::vector<UniformDeclNode*> _uniformdecls;
  std::set<std::string> _dupenamecheck;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformSetNode  : public ShaderDataNode {
  UniformSetNode(ContainerNode* cnode) : ShaderDataNode(cnode) {}
  UniformSet* generate(Container* outcon) const;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockNode  : public ShaderDataNode {
  UniformBlockNode(ContainerNode* cnode) : ShaderDataNode(cnode) {}
  UniformBlock* generate(Container* outcon) const;
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryStructNode  : public AstNode {
  std::vector<LibraryStructMemberNode*> _memberNodes;
};

///////////////////////////////////////////////////////////////////////////////

struct LibraryBlockNode  : public DecoBlockNode {
  std::map<std::string,LibraryFunctionNode*> _functionNodes;
  std::map<std::string,LibraryStructNode*> _structNodes;
};

///////////////////////////////////////////////////////////////////////////////

struct ContainerNode : public AstNode {

  ContainerNode(const AssetPath &pth, const Scanner &s);
  bool IsTokenOneOfTheBlockTypes(const Token &tok);

  void parse();

  bool validateTypeName(const std::string typeName) const;
  bool validateMemberName(const std::string typeName) const;
  bool isOutputDecorator(const std::string typeName) const;

  Container* createContainer() const;
  int itokidx = 0;

  void addBlockNode(DecoBlockNode*node);

  const Scanner &_scanner;
  const AssetPath _path;
  ConfigNode* _configNode = nullptr;
  std::map<std::string,DecoBlockNode*> _blockNodes;
  std::map<std::string,TechniqueNode*> _techniqueNodes;

  std::set<std::string> _validTypeNames;
  std::set<std::string> _validOutputDecorators;
};

/////////////////////////////////////////////////////////////////////////////////////////////////

typedef std::shared_ptr<AstNode> astnode_t;

struct GlSlFxParser {

  GlSlFxParser(const AssetPath &pth, const Scanner &s);
  UniformBlock *parseUniformBlock();
  UniformSet *parseUniformSet();
  StreamInterface *ParseFxInterface(GLenum iftype);
  StateBlock *ParseFxStateBlock();
  LibBlock *ParseLibraryBlock();
  int ParseFxShaderCommon(Shader *pshader);
  ShaderVtx *ParseFxVertexShader();
  ShaderTsC *ParseFxTessCtrlShader();
  ShaderTsE *ParseFxTessEvalShader();
  ShaderGeo *ParseFxGeometryShader();
  ShaderFrg *ParseFxFragmentShader();
  #if defined(ENABLE_NVMESH_SHADERS)
  ShaderNvTask *ParseFxNvTaskShader();
  ShaderNvMesh *ParseFxNvMeshShader();
  #endif
  Technique *ParseFxTechnique();
  int ParseFxPass(int istart, Technique *ptek);
  void DumpAllTokens();

  const AssetPath mPath;
  const Scanner& scanner;

  int itokidx = 0;
  Container *mpContainer = nullptr;
  ContainerNode* _rootNode = nullptr;

  static const std::map<std::string, int> gattrsorter;

};
/////////////////////////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2::glslfx {
/////////////////////////////////////////////////////////////////////////////////////////////////
