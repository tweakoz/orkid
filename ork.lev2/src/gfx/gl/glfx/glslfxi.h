////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <unordered_map>
#include <deque>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/util/scanner.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
class ContextGL;
}

namespace ork::lev2::glslfx {

using Scanner        = ork::Scanner;
using ScannerView    = ork::ScannerView;
using ScanViewFilter = ork::ScanViewFilter;

struct RootContainer;
struct Pass;
struct Technique;
struct UniformBlockBinding;

using rootcontainer_ptr_t = std::shared_ptr<RootContainer>;

///////////////////////////////////////////////////////////////////////////////

struct Config {
  std::string mName;
};

///////////////////////////////////////////////////////////////////////////////

constexpr const char* block_regex = "(fxconfig|uniform_set|uniform_block|"
                                    "libblock|state_block|"
                                    "vertex_interface|"
                                    "vertex_shader|"
                                    "tessctrl_interface|tesseval_interface|"
                                    "tessctrl_shader|tesseval_shader|"
                                    "geometry_interface|fragment_interface|"
                                    "geometry_shader|fragment_shader|"
                                    "compute_shader|compute_interface|"
                                    "nvtask_shader|nvmesh_shader|"
                                    "nvtask_interface|nvmesh_interface|"
                                    "technique|pass)";

///////////////////////////////////////////////////////////////////////////////

struct Uniform {
  std::string _name;
  std::string _typeName;
  GLenum _type;
  std::string _semantic;
  int _arraySize;
  int _state = -1;

  Uniform(const std::string& nam, const std::string& sem = "")
      : _name(nam)
      , _type(GL_ZERO)
      , _semantic(sem)
      , _arraySize(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct UniformInstance {
  Uniform* mpUniform = nullptr;
  std::vector<GLint> _locations;
  bool _is_array = false;
  svar16_t mPrivData;
};

///////////////////////////////////////////////////////////////////////////////

struct Attribute {
  std::string mName;
  std::string mSemantic;
  GLenum meType;
  GLint mLocation;
  int mArraySize;
  std::string mTypeName;
  std::string mDirection;
  std::string mLayout;
  std::string mInlineStruct;
  std::string mComment;
  bool _typeIsInlineStruct = false;
  bool _isInteger = false;
  std::vector<std::string> _inlineStructToks;
  std::set<std::string> _typequalifier;
  Attribute(const std::string& nam, const std::string& sem = "")
      : mName(nam)
      , mSemantic(sem)
      , meType(GL_ZERO)
      , mLocation(-1)
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
  GLuint _glbufid             = 0;
  size_t _length              = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockBinding {

  struct Item {
    GLuint _actuniindex = 0;
    GLint _blockindex   = 0;
    GLint _offset       = 0;
    GLint _type         = 0;
    GLint _size         = 0;
    GLint _arraystride  = 0;
    GLint _matrixstride = 0;
  };

  Pass* _pass          = nullptr; // program to which this binding is bound
  UniformBlock* _block = nullptr;
  GLuint _blockIndex   = 0xffffffff;
  GLuint _bindingPoint          = 0;
  std::string _name;
  GLint _blockSize = 0;
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
  GLenum mInterfaceType;
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

struct Shader {

  Shader(const std::string& nam, GLenum etyp)
      : mName(nam)
      , mShaderType(etyp) {
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
  GLuint mShaderObjectId           = 0;
  GLenum mShaderType;
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
      : Shader(nam, GL_VERTEX_SHADER) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderFrg : Shader {
  ShaderFrg(const std::string& nam = "")
      : Shader(nam, GL_FRAGMENT_SHADER) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderGeo : Shader {
  ShaderGeo(const std::string& nam = "")
      : Shader(nam, GL_GEOMETRY_SHADER) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderTsC : Shader {
  ShaderTsC(const std::string& nam = "")
      : Shader(nam, GL_TESS_CONTROL_SHADER) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderTsE : Shader {
  ShaderTsE(const std::string& nam = "")
      : Shader(nam, GL_TESS_EVALUATION_SHADER) {
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
      : Shader(nam, GL_MESH_SHADER_NV) {
  }
};
struct ShaderNvTask : Shader {
  ShaderNvTask(const std::string& nam = "")
      : Shader(nam, GL_TASK_SHADER_NV) {
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
  GLuint _glbufid               = 0;
  size_t _length                = 0;
};
struct StorageBlockMapping {

  uint8_t* _mapaddr            = nullptr;
  ShaderStorageBuffer* _buffer = nullptr;
};
typedef std::unordered_map<uint32_t, ShaderStorageBuffer*> ssbo_map_t;
struct PipelineCompute {

  ComputeShader* _computeShader = nullptr;
  GLuint _programObjectId       = 0;
  ssbo_map_t _ssboBindingMap;
};
struct ComputeShader : Shader {
  ComputeShader(const std::string& nam = "")
      : Shader(nam, GL_COMPUTE_SHADER) {
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
  GLuint _programObjectId = 0;
  uni_map_t _uniformInstances;
  attr_map_t _vtxAttributesBySemantic;
  ubb_map_t _uboBindingMap;
  Attribute* _vtxAttributeById[kmaxattrID];
  Technique* _technique = nullptr;
  std::unordered_map<int,int> _samplerBindingMap;

  int assignSampler(int loc);

  Pass(const std::string& name)
      : _name(name)
      , _stateBlock(nullptr)
      , _programObjectId(0) {
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
  bool _dump_and_exit = false;

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
  UniformSet* uniformset(const std::string& name) const;

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

///////////////////////////////////////////////////////////////////////////////

class Interface final : public FxInterface {
public:
  void _doBeginFrame() final;

  int BeginBlock(fxtechnique_constptr_t tek, const RenderContextInstData& data) final;
  bool BindPass(int ipass) final;
  void EndPass() final;
  void EndBlock() final;
  void CommitParams(void) final;
  void reset() final;

  const FxShaderTechnique* technique(FxShader* hfx, const std::string& name) final;
  const FxShaderParam* parameter(FxShader* hfx, const std::string& name) final;
  const FxShaderParamBlock* parameterBlock(FxShader* hfx, const std::string& name) final;
#if defined(ENABLE_COMPUTE_SHADERS)
  const FxComputeShader* computeShader(FxShader* hfx, const std::string& name) final;
#endif
#if defined(ENABLE_SHADER_STORAGE)
  const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) final;
#endif

  void BindParamBool(const FxShaderParam* hpar, const bool bval) final;
  void BindParamInt(const FxShaderParam* hpar, const int ival) final;
  void BindParamVect2(const FxShaderParam* hpar, const fvec2& Vec) final;
  void BindParamVect3(const FxShaderParam* hpar, const fvec3& Vec) final;
  void BindParamVect4(const FxShaderParam* hpar, const fvec4& Vec) final;
  void BindParamVect2Array(const FxShaderParam* hpar, const fvec2* Vec, const int icount) final;
  void BindParamVect3Array(const FxShaderParam* hpar, const fvec3* Vec, const int icount) final;
  void BindParamVect4Array(const FxShaderParam* hpar, const fvec4* Vec, const int icount) final;
  void BindParamFloatArray(const FxShaderParam* hpar, const float* pfA, const int icnt) final;
  void BindParamFloat(const FxShaderParam* hpar, float fA) final;
  void BindParamMatrix(const FxShaderParam* hpar, const fmtx4& Mat) final;
  void BindParamMatrix(const FxShaderParam* hpar, const fmtx3& Mat) final;
  void BindParamMatrixArray(const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) final;
  void BindParamU32(const FxShaderParam* hpar, uint32_t uval) final;
  void BindParamCTex(const FxShaderParam* hpar, const Texture* pTex) final;
  void BindParamU64(const FxShaderParam* hpar, uint64_t uval) final;
  void bindParamTextureList(const FxShaderParam* hpar, texture_rawlist_t rawlist) final;

  bool LoadFxShader(const AssetPath& pth, FxShader* ptex) final;
  FxShader* shaderFromShaderText(const std::string& name, const std::string& shadertext) final;

  Interface(ContextGL& glctx);

  void BindContainerToAbstract(rootcontainer_ptr_t pcont, FxShader* fxh);

  rootcontainer_ptr_t GetActiveEffect() const {
    return _active_effect;
  }

  bool compileAndLink(rootcontainer_ptr_t container);
  bool compilePipelineVTG(rootcontainer_ptr_t container);
  bool compilePipelineNVTM(rootcontainer_ptr_t container);

  // ubo
  FxShaderParamBuffer* createParamBuffer(size_t length) final;
  parambuffermappingptr_t mapParamBuffer(FxShaderParamBuffer* b, size_t base, size_t length) final;
  void unmapParamBuffer(FxShaderParamBufferMapping* mapping) final;
  void bindParamBlockBuffer(const FxShaderParamBlock* block, FxShaderParamBuffer* buffer) final;

private:
  typedef std::function<void(int iloc, GLenum checktype)> stdparambinder_t;

  void _stdbindparam(const FxShaderParam* hpar, const stdparambinder_t& binder);

  rootcontainer_ptr_t _active_effect;
  const Pass* mLastPass;
  FxShaderTechnique* mhCurrentTek;

  ContextGL& mTarget;
};

#if defined(ENABLE_COMPUTE_SHADERS)

struct ComputeInterface : public lev2::ComputeInterface {

  ComputeInterface(ContextGL& glctx);
  ContextGL& _targetGL;
  Interface* _fxi                          = nullptr;
  PipelineCompute* _currentComputePipeline = nullptr;

  void dispatchCompute(const FxComputeShader* shader, uint32_t numgroups_x, uint32_t numgroups_y, uint32_t numgroups_z) final;

  void dispatchComputeIndirect(const FxComputeShader* shader, int32_t* indirect) final;

  FxShaderStorageBuffer* createStorageBuffer(size_t length) final;
  storagebuffermappingptr_t mapStorageBuffer(FxShaderStorageBuffer* b, size_t base = 0, size_t length = 0) final;
  void unmapStorageBuffer(FxShaderStorageBufferMapping* mapping) final;
  void bindStorageBuffer(const FxComputeShader* shader, uint32_t binding_index, FxShaderStorageBuffer* buffer) final;
  void bindImage(const FxComputeShader* shader, uint32_t binding_index, Texture* tex, ImageBindAccess access) final;

  PipelineCompute* createComputePipe(ComputeShader* csh);
  void bindComputeShader(ComputeShader* csh);
};
#endif

rootcontainer_ptr_t LoadFxFromFile(const AssetPath& pth);

} // namespace ork::lev2::glslfx

///////////////////////////////////////////////////////////////////////////////
