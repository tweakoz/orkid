#pragma once

#include <ork/util/scanner.h>
#include <parsertl/generator.hpp>
#include <parsertl/match.hpp>
#include <unordered_map>
#include <ork/kernel/svariant.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/config.h>

/////////////////////////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::orksl {
/////////////////////////////////////////////////////////////////////////////////////////////////

struct RootContainer;
struct Pass;
struct Technique;
struct UniformBlockBinding;
using rootcontainer_ptr_t = std::shared_ptr<RootContainer>;

using Scanner        = ork::Scanner;
using ScannerView    = ork::ScannerView;
using ScanViewFilter = ork::ScanViewFilter;

struct Config {
  std::string mName;
};

enum class EStreamInterfaceType{
  Vertex = 0,
  Fragment,
  TessCtrl,
  TessEval,
  Geometry,

#if defined ENABLE_COMPUTE_SHADERS
  Compute,
#endif

#if defined ENABLE_NVMESH_SHADERS
  NvTask,
  NvMesh,
#endif
  None
};

///////////////////////////////////////////////////////////////////////////////

enum class EUniformType {
  Float,
  Vec2f,
  Vec3f,
  Vec4f,
  Mat33f,
  Mat44f,

  Int,
  Vec2i,
  Vec3i,
  Vec4i,

  UnsignedInt,
  Vec2u,
  Vec3u,
  Vec4u,

  Sampler1D,
  Sampler2D,
  Sampler3D,
  SamplerCube,
  SamplerShadow1D,
  SamplerShadow2D,

  Sampler1Du,
  Sampler2Du,
  Sampler3Du,
  SamplerCubeu,

  None
};
enum class EAttributeType {
  Float,
  Vec2f,
  Vec3f,
  Vec4f,
  Vec2i,
  Vec3i,
  Vec4i,
  Vec2u,
  Vec3u,
  Vec4u,
  None,
};

///////////////////////////////////////////////////////////////////////////////

struct Uniform {
  std::string _name;
  std::string _typeName;
  EUniformType _type;
  svar16_t _apitype;
  std::string _semantic;
  int _arraySize;

  Uniform(const std::string& nam, const std::string& sem = "")
      : _name(nam)
      , _semantic(sem)
      , _type(EUniformType::None)
      , _arraySize(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct UniformInstance {
  int _location;
  Uniform* mpUniform;
  int mSubItemIndex;
  svar16_t mPrivData;

  UniformInstance()
      : _location(-1)
      , mpUniform(nullptr)
      , mSubItemIndex(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Attribute {
  std::string mName;
  std::string mTypeName;
  std::string mDirection;
  std::string mLayout;
  std::string mInlineStruct;
  EAttributeType _type;
  svar16_t _apitype;
  int _location;
  std::string mSemantic;
  std::string mComment;
  int mArraySize;
  bool _typeIsInlineStruct = false;
  std::vector<std::string> _inlineStructToks;
  std::set<std::string> _typequalifier;
  Attribute(const std::string& nam, const std::string& sem = "")
      : mName(nam)
      , mSemantic(sem)
      , _type(EAttributeType::None)
      , _location(-1)
      , mArraySize(0) {
  }
};

typedef std::unordered_map<std::string, Uniform*> uniform_map_t;

///////////////////////////////////////////////////////////////////////////////

struct UniformSet {

  UniformSet() {
  }

  std::string _name;
  uniform_map_t _uniforms;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlock {

  UniformBlock() {
  }

  std::string _name;
  std::vector<Uniform*> _subuniforms;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockItem {
  UniformBlockBinding* _binding = nullptr;
  size_t _offset                = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBuffer {
  FxShaderParamBuffer* _fxspb = nullptr;
  int _bufid             = 0;
  size_t _length              = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockBinding {

  struct Item {
    uint32_t _actuniindex = 0;
    int32_t _blockindex   = 0;
    int32_t _offset       = 0;
    int32_t _type         = 0;
    int32_t _size         = 0;
    int32_t _arraystride  = 0;
    int32_t _matrixstride = 0;
  };

  Pass* _pass          = nullptr; // program to which this binding is bound
  UniformBlock* _block = nullptr;
  uint32_t _blockIndex   = 0xffffffff;
  std::string _name;
  int _blockSize = 0;
  std::vector<Item> _ubbitems;
  UniformBlockItem findUniform(std::string named) const;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockMapping {
  UniformBlockBinding* _binding = nullptr;
  uint8_t* _mapaddr             = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockLayout {

  template <typename T> UniformBlockItem alloc() {
    size_t index = _counter;
    _counter += sizeof(T);
    return UniformBlockItem{nullptr, index};
  }

  size_t _counter = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct StreamInterface {
  StreamInterface();

  typedef std::unordered_map<std::string, Attribute*> attrmap_t;
  typedef std::vector<std::string> preamble_t;

  std::string mName;
  attrmap_t _inputAttributes;
  attrmap_t _outputAttributes;
  EStreamInterfaceType _interfaceType;
  // preamble_t mPreamble;
  int _gspriminpsize = 0;
  int _gsprimoutsize = 0;
  std::vector<UniformBlock*> _uniformBlocks;
  std::vector<UniformSet*> _uniformSets;

  void Inherit(const StreamInterface& par);
};

///////////////////////////////////////////////////////////////////////////////

typedef std::function<void(Context*)> state_applicator_t;

///////////////////////////////////////////////////////////////////////////////

struct StateBlock {
  std::string mName;
  std::vector<state_applicator_t> mApplicators;

  void addStateFn(const state_applicator_t& f) {
    mApplicators.push_back(f);
  }
};

///////////////////////////////////////////////////////////////////////////////

enum class EShaderType {
  Vertex,
  Fragment,
  TessCtrl,
  TessEval,
  Geometry,

#if defined(ENABLE_COMPUTE_SHADERS)
  Compute,
#endif 

#if defined(ENABLE_NVMESH_SHADERS)
  NvTask,
  NvMesh,
#endif 
  None
};

struct Shader {

  Shader(const std::string& nam, EShaderType etyp)
      : mName(nam)
      , _shadertype(etyp) {
  }

  bool Compile();
  bool IsCompiled() const;
  void addUniformSet(UniformSet*);
  void addUniformBlock(UniformBlock*);
  void setInputInterface(StreamInterface* iface);

  void dumpFinalText() const;

  std::string mName;
  std::string mShaderText;
  std::string _finalText;
  StreamInterface* _inputInterface = nullptr;
  rootcontainer_ptr_t _rootcontainer;
  uint32_t mShaderObjectId           = 0;
  EShaderType _shadertype;
  bool mbCompiled = false;
  bool mbError    = false;
  std::vector<UniformBlock*> _uniblocks;
  std::vector<UniformSet*> _unisets;
};

///////////////////////////////////////////////////////////////////////////////
// VTG prim pipeline
///////////////////////////////////////////////////////////////////////////////

struct ShaderVtx : Shader {
  ShaderVtx(const std::string& nam = "")
      : Shader(nam, EShaderType::Vertex) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderFrg : Shader {
  ShaderFrg(const std::string& nam = "")
      : Shader(nam, EShaderType::Fragment) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderGeo : Shader {
  ShaderGeo(const std::string& nam = "")
      : Shader(nam, EShaderType::Geometry) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderTsC : Shader {
  ShaderTsC(const std::string& nam = "")
      : Shader(nam, EShaderType::TessCtrl) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderTsE : Shader {
  ShaderTsE(const std::string& nam = "")
      : Shader(nam, EShaderType::TessEval) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct PrimPipelineVTG {
  ShaderVtx* _vertexShader   = nullptr;
  ShaderTsC* _tessCtrlShader = nullptr;
  ShaderTsE* _tessEvalShader = nullptr;
  ShaderGeo* _geometryShader = nullptr;
  ShaderFrg* _fragmentShader = nullptr;
};

///////////////////////////////////////////////////////////////////////////////
// NvMeshShader prim pipeline
///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_NVMESH_SHADERS)
struct ShaderNvMesh : Shader {
  ShaderNvMesh(const std::string& nam = "")
      : Shader(nam, EShaderType::NvMesh) {
  }
};
struct ShaderNvTask : Shader {
  ShaderNvTask(const std::string& nam = "")
      : Shader(nam, EShaderType::NvTask) {
  }
};
struct PrimPipelineNVTM {
  ShaderNvTask* _nvTaskShader = nullptr;
  ShaderNvMesh* _nvMeshShader = nullptr;
  ShaderFrg* _fragmentShader  = nullptr;
};
#endif

///////////////////////////////////////////////////////////////////////////////

typedef std::unordered_map<std::string, UniformInstance*> uni_map_t;
typedef std::unordered_map<UniformBlock*, UniformBlockBinding*> ubb_map_t;
typedef std::unordered_map<std::string, Attribute*> attr_map_t;

///////////////////////////////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
struct ComputeShader;
struct ShaderStorageBuffer {
  FxShaderStorageBuffer* _fxssb = nullptr;
  uint32_t _bufid               = 0;
  size_t _length                = 0;
};
struct StorageBlockMapping {

  uint8_t* _mapaddr            = nullptr;
  ShaderStorageBuffer* _buffer = nullptr;
};
typedef std::unordered_map<uint32_t, ShaderStorageBuffer*> ssbo_map_t;
struct PipelineCompute {

  ComputeShader* _computeShader = nullptr;
  uint32_t _programObjectId       = 0;
  ssbo_map_t _ssboBindingMap;
};
struct ComputeShader : Shader {
  ComputeShader(const std::string& nam = "")
      : Shader(nam, EShaderType::Compute) {
  }
  PipelineCompute* _computePipe = nullptr;
};
#endif
///////////////////////////////////////////////////////////////////////////////

struct Pass {

  static const int kmaxattrID = 16;
  svar64_t _primpipe;
  std::string _name;
  StateBlock* _stateBlock = nullptr;
  uint32_t _programObjectId = 0;
  uni_map_t _uniformInstances;
  attr_map_t _vtxAttributesBySemantic;
  ubb_map_t _uboBindingMap;
  Attribute* _vtxAttributeById[kmaxattrID];
  int _samplerCount     = 0;
  Technique* _technique = nullptr;

  Pass(const std::string& name)
      : _name(name)
      , _programObjectId(0)
      , _stateBlock(nullptr)
      , _samplerCount(0) {
    for (int i = 0; i < kmaxattrID; i++)
      _vtxAttributeById[i] = nullptr;
  }

  void postProc(rootcontainer_ptr_t c);

  bool hasUniformInstance(UniformInstance* puni) const;
  const UniformInstance* uniformInstance(Uniform* puni) const;

  UniformBlockBinding* uniformBlockBinding(UniformBlock* block);

  void bindUniformBlockBuffer(UniformBlock* block, UniformBuffer* buffer);

  std::vector<UniformBuffer*> _ubobindings;
};

///////////////////////////////////////////////////////////////////////////////

struct Technique {
  orkvector<Pass*> mPasses;
  std::string _name;

  Technique(const std::string& nam) {
    _name = nam;
  }

  void addPass(Pass* pps) {
    pps->_technique = this;
    mPasses.push_back(pps);
  }
};

///////////////////////////////////////////////////////////////////////////////

struct RootContainer {

  RootContainer(const std::string& nam);

  void Destroy(void);
  bool IsValid(void);

  void addConfig(Config* pcfg);
  void addUniformSet(UniformSet* pif);
  void addUniformBlock(UniformBlock* pif);

  Uniform* MergeUniform(const std::string& name);
  void addStateBlock(StateBlock* pSB);
  void addTechnique(Technique* ptek);

  StateBlock* GetStateBlock(const std::string& name) const;
  Uniform* GetUniform(const std::string& name) const;
  UniformBlock* uniformBlock(const std::string& name) const;
  UniformSet* uniformSet(const std::string& name) const;

  std::string mEffectName;
  const Technique* mActiveTechnique;
  std::unordered_map<std::string, Config*> mConfigs;
  std::unordered_map<std::string, UniformSet*> _uniformSets;
  std::unordered_map<std::string, UniformBlock*> _uniformBlocks;
  //
  std::unordered_map<std::string, StateBlock*> _stateBlocks;
  std::unordered_map<std::string, Uniform*> _uniforms;
  std::unordered_map<std::string, Technique*> _techniqueMap;

  Pass* _activePass;
  int mActiveNumPasses;
  const FxShader* mFxShader;
  bool mShaderCompileFailed;

  uniform_map_t flatUniMap() const;

  ///////////////////////////////////////////////////////
  // vtg //
  ///////////////////////////////////////////////////////

  void addVertexInterface(StreamInterface* sif);
  void addTessCtrlInterface(StreamInterface* sif);
  void addTessEvalInterface(StreamInterface* sif);
  void addGeometryInterface(StreamInterface* sif);
  void addFragmentInterface(StreamInterface* sif);
  void addVertexShader(ShaderVtx* shader);
  void addTessCtrlShader(ShaderTsC* shader);
  void addTessEvalShader(ShaderTsE* shader);
  void addGeometryShader(ShaderGeo* shader);
  void addFragmentShader(ShaderFrg* shader);
  ShaderVtx* vertexShader(const std::string& name) const;
  ShaderTsC* tessCtrlShader(const std::string& name) const;
  ShaderTsE* tessEvalShader(const std::string& name) const;
  ShaderGeo* geometryShader(const std::string& name) const;
  ShaderFrg* fragmentShader(const std::string& name) const;
  StreamInterface* vertexInterface(const std::string& name) const;
  StreamInterface* tessCtrlInterface(const std::string& name) const;
  StreamInterface* tessEvalInterface(const std::string& name) const;
  StreamInterface* geometryInterface(const std::string& name) const;
  StreamInterface* fragmentInterface(const std::string& name) const;

  std::unordered_map<std::string, ShaderVtx*> _vertexShaders;
  std::unordered_map<std::string, ShaderTsC*> _tessCtrlShaders;
  std::unordered_map<std::string, ShaderTsE*> _tessEvalShaders;
  std::unordered_map<std::string, ShaderGeo*> _geometryShaders;
  std::unordered_map<std::string, ShaderFrg*> _fragmentShaders;
  std::unordered_map<std::string, StreamInterface*> _vertexInterfaces;
  std::unordered_map<std::string, StreamInterface*> _tessCtrlInterfaces;
  std::unordered_map<std::string, StreamInterface*> _tessEvalInterfaces;
  std::unordered_map<std::string, StreamInterface*> _geometryInterfaces;
  std::unordered_map<std::string, StreamInterface*> _fragmentInterfaces;

  ///////////////////////////////////////////////////////
  // nvtask/nvmesh //
  ///////////////////////////////////////////////////////
#if defined(ENABLE_NVMESH_SHADERS)
  void addNvTaskInterface(StreamInterface* sif);
  void addNvMeshInterface(StreamInterface* sif);
  void addNvTaskShader(ShaderNvTask* shader);
  void addNvMeshShader(ShaderNvMesh* shader);
  StreamInterface* nvTaskInterface(const std::string& name) const;
  StreamInterface* nvMeshInterface(const std::string& name) const;
  ShaderNvTask* nvTaskShader(const std::string& name) const;
  ShaderNvMesh* nvMeshShader(const std::string& name) const;

  std::unordered_map<std::string, ShaderNvTask*> _nvTaskShaders;
  std::unordered_map<std::string, ShaderNvMesh*> _nvMeshShaders;
  std::unordered_map<std::string, StreamInterface*> _nvTaskInterfaces;
  std::unordered_map<std::string, StreamInterface*> _nvMeshInterfaces;
#endif

///////////////////////////////////////////////////////
#if defined(ENABLE_SHADER_STORAGE)
#endif
///////////////////////////////////////////////////////
#if defined(ENABLE_COMPUTE_SHADERS)
  std::unordered_map<std::string, StreamInterface*> _computeInterfaces;
  std::unordered_map<std::string, ComputeShader*> _computeShaders;
  ComputeShader* computeShader(const std::string& name) const;
  StreamInterface* computeInterface(const std::string& name) const;
  void addComputeInterface(StreamInterface* sif);
  void addComputeShader(ComputeShader* pif);
#endif
  ///////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::orksl {
