////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>

#if ! defined(__APPLE__)
#define ENABLE_NVMESH_SHADERS
#endif

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {
class GfxTargetGL;
}

namespace ork::lev2::glslfx {

struct Container;
struct Scanner;
struct ScannerView;
struct ScanViewFilter;
struct Pass;
struct UniformBlockBinding;

///////////////////////////////////////////////////////////////////////////////

struct Config {
  std::string mName;
};

///////////////////////////////////////////////////////////////////////////////

constexpr const char* token_regex =
  "(fxconfig|uniform_set|uniform_block|"
  "libblock|state_block|"
  "vertex_interface|"
  "vertex_shader|"
  "tessctrl_interface|tesseval_interface|"
  "tessctrl_shader|tesseval_shader|"
  "geometry_interface|fragment_interface|"
  "geometry_shader|fragment_shader|"
  #if defined(ENABLE_NVMESH_SHADERS)
  "nvtask_shader|nvmesh_shader|"
  "nvtask_interface|nvmesh_interface|"
  #endif
  "technique|pass)";
///////////////////////////////////////////////////////////////////////////////

struct Uniform {
  std::string mName;
  std::string mTypeName;
  GLenum meType;
  std::string mSemantic;
  int mArraySize;

  Uniform(const std::string& nam, const std::string& sem = "")
      : mName(nam)
      , mSemantic(sem)
      , meType(GL_ZERO)
      , mArraySize(0) {}
};

///////////////////////////////////////////////////////////////////////////////

struct UniformInstance {
  GLint mLocation;
  Uniform* mpUniform;
  int mSubItemIndex;
  svar16_t mPrivData;

  UniformInstance()
      : mLocation(-1)
      , mpUniform(nullptr)
      , mSubItemIndex(0) {}
};

///////////////////////////////////////////////////////////////////////////////

struct Attribute {
  std::string mName;
  std::string mTypeName;
  std::string mDirection;
  GLenum meType;
  GLint mLocation;
  std::string mSemantic;
  std::string mComment;
  int mArraySize;

  Attribute(const std::string& nam, const std::string& sem = "")
      : mName(nam)
      , mSemantic(sem)
      , meType(GL_ZERO)
      , mLocation(-1)
      , mArraySize(0) {}
};

///////////////////////////////////////////////////////////////////////////////

struct UniformSet {

  typedef std::map<std::string, Uniform*> uniform_map_t;

  UniformSet() {}

  std::string _name;
  uniform_map_t _uniforms;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlock {
  typedef std::map<std::string, Uniform*> uniform_map_t;

  UniformBlock() {}

  std::string _name;
  uniform_map_t _uniforms;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockItem {
  UniformBlockBinding* _binding = nullptr;
  size_t _offset = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockBinding {
  Pass* _pass = nullptr; // program to which this binding is bound
  GLuint _blockIndex = 0xffffffff;
  std::string _name;
  UniformBlockItem findUniform(std::string named) const;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockMapping {
   UniformBlockBinding* _binding = nullptr;
   uint8_t* _mapaddr = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

struct UniformBlockLayout {

  template <typename T> UniformBlockItem alloc(){
    size_t index = _counter;
     _counter += sizeof(T);
    return UniformBlockItem{index};
 }

  size_t _counter = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct StreamInterface {
  StreamInterface();

  typedef std::map<std::string, Attribute*> AttrMap;
  typedef std::vector<std::string> preamble_t;

  std::string mName;
  AttrMap mAttributes;
  GLenum mInterfaceType;
  preamble_t mPreamble;
  int mGsPrimSize;
  std::set<UniformBlock*> _uniformBlocks;
  std::set<UniformSet*> _uniformSets;

  void Inherit(const StreamInterface& par);
};

///////////////////////////////////////////////////////////////////////////////

typedef std::function<void(GfxTarget*)> state_applicator_t;

///////////////////////////////////////////////////////////////////////////////

struct StateBlock {
  std::string mName;
  // SRasterState	mState;
  std::vector<state_applicator_t> mApplicators;

  void addStateFn(const state_applicator_t& f) { mApplicators.push_back(f); }
};

///////////////////////////////////////////////////////////////////////////////

struct Shader {
  std::string mName;
  std::string mShaderText;
  StreamInterface* mpInterface;
  Container* mpContainer;
  GLuint mShaderObjectId;
  GLenum mShaderType;
  bool mbCompiled;
  bool mbError;
  std::set<std::string> _requiredExtensions;

  Shader(const std::string& nam, GLenum etyp)
      : mName(nam)
      , mShaderObjectId(0)
      , mShaderType(etyp)
      , mbCompiled(false)
      , mbError(false)
      , mpInterface(nullptr)
      , mpContainer(nullptr) {}

  bool Compile();
  bool IsCompiled() const;
  void requireExtension(std::string ext) { _requiredExtensions.insert(ext); }
};

///////////////////////////////////////////////////////////////////////////////
// VTG prim pipeline
///////////////////////////////////////////////////////////////////////////////

struct ShaderVtx : Shader {
  ShaderVtx(const std::string& nam = "")
      : Shader(nam, GL_VERTEX_SHADER) {}
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderFrg : Shader {
  ShaderFrg(const std::string& nam = "")
      : Shader(nam, GL_FRAGMENT_SHADER) {}
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderGeo : Shader {
  ShaderGeo(const std::string& nam = "")
      : Shader(nam, GL_GEOMETRY_SHADER) {}
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderTsC : Shader {
  ShaderTsC(const std::string& nam = "")
      : Shader(nam, GL_TESS_CONTROL_SHADER) {}
};

///////////////////////////////////////////////////////////////////////////////

struct ShaderTsE : Shader {
  ShaderTsE(const std::string& nam = "")
      : Shader(nam, GL_TESS_EVALUATION_SHADER) {}
};

///////////////////////////////////////////////////////////////////////////////

struct PrimPipelineVTG {
  ShaderVtx* _vertexShader = nullptr;
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
      : Shader(nam, GL_MESH_SHADER_NV) {}
};
struct ShaderNvTask : Shader {
  ShaderNvTask(const std::string& nam = "")
      : Shader(nam, GL_TASK_SHADER_NV) {}
};
struct PrimPipelineNVMT {
  ShaderNvTask* _nvTaskShader = nullptr;
  ShaderNvMesh* _nvMeshShader = nullptr;
};
#endif

///////////////////////////////////////////////////////////////////////////////

struct LibBlock {
  LibBlock(const Scanner& s);

  std::string mName;
  ScanViewFilter* mFilter;
  ScannerView* mView;
};

///////////////////////////////////////////////////////////////////////////////

struct Pass {
  typedef std::map<std::string, UniformInstance*> uni_map_t;
  typedef std::map<std::string, Attribute*> attr_map_t;

  static const int kmaxattrID = 16;
  svar64_t _primpipe;
  std::string _name;
  StateBlock* _stateBlock;
  GLuint _programObjectId;
  uni_map_t _uniformInstances;
  attr_map_t _vtxAttributesBySemantic;
  Attribute* _vtxAttributeById[kmaxattrID];
  int _samplerCount;

  Pass(const std::string& name)
      : _name(name)
      , _programObjectId(0)
      , _stateBlock(nullptr)
      , _samplerCount(0) {
    for (int i = 0; i < kmaxattrID; i++)
      _vtxAttributeById[i] = nullptr;
  }
  bool hasUniformInstance(UniformInstance* puni) const;
  const UniformInstance* uniformInstance(Uniform* puni) const;

  UniformBlockBinding findUniformBlock(std::string blockname);

};

///////////////////////////////////////////////////////////////////////////////

struct Technique {
  orkvector<Pass*> mPasses;
  std::string mName;

  Technique(const std::string& nam) { mName = nam; }

  void addPass(Pass* pps) { mPasses.push_back(pps); }
};

///////////////////////////////////////////////////////////////////////////////

struct Container {

  Container(const std::string& nam);

  // bool Load( const AssetPath& filename, FxShader*pfxshader );
  void Destroy(void);
  bool IsValid(void);

  void addConfig(Config* pcfg);
  void addUniformSet(UniformSet* pif);
  void addUniformBlock(UniformBlock* pif);

  Uniform* MergeUniform(const std::string& name);
  void addStateBlock(StateBlock* pSB);
  void addTechnique(Technique* ptek);

  void addLibBlock(LibBlock* plb);

  StateBlock* GetStateBlock(const std::string& name) const;
  Uniform* GetUniform(const std::string& name) const;
  UniformBlock* uniformBlock(const std::string& name) const;
  UniformSet* uniformSet(const std::string& name) const;

  std::string mEffectName;
  const Technique* mActiveTechnique;
  std::map<std::string, Config*> mConfigs;
  std::map<std::string, UniformSet*> _uniformSets;
  std::map<std::string, UniformBlock*> _uniformBlocks;
  //
  std::map<std::string, StateBlock*> _stateBlocks;
  std::map<std::string, Uniform*> _uniforms;
  std::map<std::string, Technique*> _techniqueMap;
  std::map<std::string, LibBlock*> _libBlocks;
  const Pass* mActivePass;
  int mActiveNumPasses;
  const FxShader* mFxShader;
  bool mShaderCompileFailed;

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

  std::map<std::string, ShaderVtx*> _vertexShaders;
  std::map<std::string, ShaderTsC*> _tessCtrlShaders;
  std::map<std::string, ShaderTsE*> _tessEvalShaders;
  std::map<std::string, ShaderGeo*> _geometryShaders;
  std::map<std::string, ShaderFrg*> _fragmentShaders;
  std::map<std::string, StreamInterface*> _vertexInterfaces;
  std::map<std::string, StreamInterface*> _tessCtrlInterfaces;
  std::map<std::string, StreamInterface*> _tessEvalInterfaces;
  std::map<std::string, StreamInterface*> _geometryInterfaces;
  std::map<std::string, StreamInterface*> _fragmentInterfaces;

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

  std::map<std::string, ShaderNvTask*> _nvTaskShaders;
  std::map<std::string, ShaderNvMesh*> _nvMeshShaders;
  std::map<std::string, StreamInterface*> _nvTaskInterfaces;
  std::map<std::string, StreamInterface*> _nvMeshInterfaces;

#endif

///////////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

class Interface : public FxInterface {
public:
  virtual void DoBeginFrame();

  int BeginBlock(FxShader* hfx, const RenderContextInstData& data) final;
  bool BindPass(FxShader* hfx, int ipass) final;
  bool BindTechnique(FxShader* hfx, const FxShaderTechnique* htek) final;
  void EndPass(FxShader* hfx) final;
  void EndBlock(FxShader* hfx) final;
  void CommitParams(void) final;

  const FxShaderTechnique* technique(FxShader* hfx, const std::string& name) final;
  const FxShaderParam* parameter(FxShader* hfx, const std::string& name) final;
  const FxShaderParamBlock* parameterBlock(FxShader* hfx, const std::string& name) final;
  #if defined(ENABLE_SHADER_STORAGE)
  const FxShaderStorageBlock* storageBlock(FxShader* hfx, const std::string& name) final;
  #endif

  void BindParamBool(FxShader* hfx, const FxShaderParam* hpar, const bool bval) final;
  void BindParamInt(FxShader* hfx, const FxShaderParam* hpar, const int ival) final;
  void BindParamVect2(FxShader* hfx, const FxShaderParam* hpar, const fvec2& Vec) final;
  void BindParamVect3(FxShader* hfx, const FxShaderParam* hpar, const fvec3& Vec) final;
  void BindParamVect4(FxShader* hfx, const FxShaderParam* hpar, const fvec4& Vec) final;
  void BindParamVect4Array(FxShader* hfx, const FxShaderParam* hpar, const fvec4* Vec, const int icount) final;
  void BindParamFloatArray(FxShader* hfx, const FxShaderParam* hpar, const float* pfA, const int icnt) final;
  void BindParamFloat(FxShader* hfx, const FxShaderParam* hpar, float fA) final;
  void BindParamFloat2(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB) final;
  void BindParamFloat3(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC) final;
  void BindParamFloat4(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD) final;
  void BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx4& Mat) final;
  void BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx3& Mat) final;
  void BindParamMatrixArray(FxShader* hfx, const FxShaderParam* hpar, const fmtx4* MatArray, int iCount) final;
  void BindParamU32(FxShader* hfx, const FxShaderParam* hpar, U32 uval) final;
  void BindParamCTex(FxShader* hfx, const FxShaderParam* hpar, const Texture* pTex) final;
  bool LoadFxShader(const AssetPath& pth, FxShader* ptex) final;

  Interface(GfxTargetGL& glctx);

  void BindContainerToAbstract(Container* pcont, FxShader* fxh);

  Container* GetActiveEffect() const { return mpActiveEffect; }

  bool compileAndLink(Container* container);

protected:
  Container* mpActiveEffect;
  const Pass* mLastPass;
  FxShaderTechnique* mhCurrentTek;

  GfxTargetGL& mTarget;
};

Container* LoadFxFromFile(const AssetPath& pth);

} // namespace ork::lev2::glslfx

///////////////////////////////////////////////////////////////////////////////
