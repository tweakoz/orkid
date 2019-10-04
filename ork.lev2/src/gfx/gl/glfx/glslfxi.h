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

struct Config {
  std::string mName;
};
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

struct UniformSet {

  typedef std::map<std::string, Uniform*> uniform_map_t;

  UniformSet() {}

  std::string _name;
  uniform_map_t _uniforms;
};

struct UniformBlock {
  typedef std::map<std::string, Uniform*> uniform_map_t;

  UniformBlock() {}

  std::string _name;
  uniform_map_t _uniforms;
};
struct UniformBlockItem {
  UniformBlockBinding* _binding = nullptr;
  size_t _offset = 0;
};
struct UniformBlockBinding {
  Pass* _pass = nullptr; // program to which this binding is bound
  GLuint _blockIndex = 0xffffffff;
  std::string _name;
  UniformBlockItem findUniform(std::string named) const;
};
struct UniformBlockMapping {
   UniformBlockBinding* _binding = nullptr;
   uint8_t* _mapaddr = nullptr;
};
struct UniformBlockLayout {

  template <typename T> UniformBlockItem alloc(){
    size_t index = _counter;
     _counter += sizeof(T);
    return UniformBlockItem{index};
 }

  size_t _counter = 0;
};

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

typedef std::function<void(GfxTarget*)> state_applicator_t;

struct StateBlock {
  std::string mName;
  // SRasterState	mState;
  std::vector<state_applicator_t> mApplicators;

  void AddStateFn(const state_applicator_t& f) { mApplicators.push_back(f); }
};

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
struct ShaderVtx : Shader {
  ShaderVtx(const std::string& nam = "")
      : Shader(nam, GL_VERTEX_SHADER) {}
};
struct ShaderFrg : Shader {
  ShaderFrg(const std::string& nam = "")
      : Shader(nam, GL_FRAGMENT_SHADER) {}
};
struct ShaderGeo : Shader {
  ShaderGeo(const std::string& nam = "")
      : Shader(nam, GL_GEOMETRY_SHADER) {}
};
struct ShaderTsC : Shader {
  ShaderTsC(const std::string& nam = "")
      : Shader(nam, GL_TESS_CONTROL_SHADER) {}
};
struct ShaderTsE : Shader {
  ShaderTsE(const std::string& nam = "")
      : Shader(nam, GL_TESS_EVALUATION_SHADER) {}
};

struct LibBlock {
  LibBlock(const Scanner& s);

  std::string mName;
  ScanViewFilter* mFilter;
  ScannerView* mView;
};

struct Pass {
  typedef std::map<std::string, UniformInstance*> uni_map_t;
  typedef std::map<std::string, Attribute*> attr_map_t;

  static const int kmaxattrID = 16;
  std::string mName;
  ShaderVtx* mVertexProgram;
  ShaderTsC* mTessCtrlProgram;
  ShaderTsE* mTessEvalProgram;
  ShaderGeo* mGeometryProgram;
  ShaderFrg* mFragmentProgram;
  StateBlock* mStateBlock;
  GLuint mProgramObjectId;
  uni_map_t mUniformInstances;
  attr_map_t mVtxAttributesBySemantic;
  Attribute* mVtxAttributeById[kmaxattrID];
  int mSamplerCount;

  Pass(const std::string& name)
      : mName(name)
      , mVertexProgram(nullptr)
      , mTessCtrlProgram(nullptr)
      , mTessEvalProgram(nullptr)
      , mGeometryProgram(nullptr)
      , mFragmentProgram(nullptr)
      , mProgramObjectId(0)
      , mStateBlock(nullptr)
      , mSamplerCount(0) {
    for (int i = 0; i < kmaxattrID; i++)
      mVtxAttributeById[i] = nullptr;
  }
  bool HasUniformInstance(UniformInstance* puni) const;
  const UniformInstance* GetUniformInstance(Uniform* puni) const;

  UniformBlockBinding findUniformBlock(std::string blockname);

};

struct Technique {
  orkvector<Pass*> mPasses;
  std::string mName;

  Technique(const std::string& nam) { mName = nam; }

  void AddPass(Pass* pps) { mPasses.push_back(pps); }
};

struct Container {
  std::string mEffectName;
  const Technique* mActiveTechnique;
  std::map<std::string, Config*> mConfigs;
  std::map<std::string, UniformSet*> _uniformSets;
  std::map<std::string, UniformBlock*> _uniformBlocks;
  std::map<std::string, StreamInterface*> mVertexInterfaces;
  std::map<std::string, StreamInterface*> mTessCtrlInterfaces;
  std::map<std::string, StreamInterface*> mTessEvalInterfaces;
  std::map<std::string, StreamInterface*> mGeometryInterfaces;
  std::map<std::string, StreamInterface*> mFragmentInterfaces;
  std::map<std::string, StateBlock*> mStateBlocks;
  std::map<std::string, Uniform*> mUniforms;
  std::map<std::string, ShaderVtx*> mVertexPrograms;
  std::map<std::string, ShaderTsC*> mTessCtrlPrograms;
  std::map<std::string, ShaderTsE*> mTessEvalPrograms;
  std::map<std::string, ShaderGeo*> mGeometryPrograms;
  std::map<std::string, ShaderFrg*> mFragmentPrograms;
  std::map<std::string, Technique*> mTechniqueMap;
  std::map<std::string, LibBlock*> mLibBlocks;
  const Pass* mActivePass;
  int mActiveNumPasses;
  const FxShader* mFxShader;
  bool mShaderCompileFailed;

  // bool Load( const AssetPath& filename, FxShader*pfxshader );
  void Destroy(void);
  bool IsValid(void);

  void AddConfig(Config* pcfg);
  void addUniformSet(UniformSet* pif);
  void addUniformBlock(UniformBlock* pif);
  void AddVertexInterface(StreamInterface* pif);
  void AddTessCtrlInterface(StreamInterface* pif);
  void AddTessEvalInterface(StreamInterface* pif);
  void AddGeometryInterface(StreamInterface* pif);
  void AddFragmentInterface(StreamInterface* pif);
  Uniform* MergeUniform(const std::string& name);
  void AddStateBlock(StateBlock* pSB);
  void AddTechnique(Technique* ptek);
  void AddVertexProgram(ShaderVtx* psha);
  void AddTessCtrlProgram(ShaderTsC* psha);
  void AddTessEvalProgram(ShaderTsE* psha);
  void AddGeometryProgram(ShaderGeo* psha);
  void AddFragmentProgram(ShaderFrg* psha);
  void AddLibBlock(LibBlock* plb);

  StateBlock* GetStateBlock(const std::string& name) const;
  Uniform* GetUniform(const std::string& name) const;
  ShaderVtx* GetVertexProgram(const std::string& name) const;
  ShaderTsC* GetTessCtrlProgram(const std::string& name) const;
  ShaderTsE* GetTessEvalProgram(const std::string& name) const;
  ShaderGeo* GetGeometryProgram(const std::string& name) const;
  ShaderFrg* GetFragmentProgram(const std::string& name) const;
  UniformBlock* uniformBlock(const std::string& name) const;
  UniformSet* uniformSet(const std::string& name) const;
  StreamInterface* GetVertexInterface(const std::string& name) const;
  StreamInterface* GetTessCtrlInterface(const std::string& name) const;
  StreamInterface* GetTessEvalInterface(const std::string& name) const;
  StreamInterface* GetGeometryInterface(const std::string& name) const;
  StreamInterface* GetFragmentInterface(const std::string& name) const;

  Container(const std::string& nam);
};


class Interface : public FxInterface {
public:
  virtual void DoBeginFrame();

  virtual int BeginBlock(FxShader* hfx, const RenderContextInstData& data);
  virtual bool BindPass(FxShader* hfx, int ipass);
  virtual bool BindTechnique(FxShader* hfx, const FxShaderTechnique* htek);
  virtual void EndPass(FxShader* hfx);
  virtual void EndBlock(FxShader* hfx);
  virtual void CommitParams(void);

  virtual const FxShaderTechnique* GetTechnique(FxShader* hfx, const std::string& name);
  virtual const FxShaderParam* GetParameterH(FxShader* hfx, const std::string& name);

  virtual void BindParamBool(FxShader* hfx, const FxShaderParam* hpar, const bool bval);
  virtual void BindParamInt(FxShader* hfx, const FxShaderParam* hpar, const int ival);
  virtual void BindParamVect2(FxShader* hfx, const FxShaderParam* hpar, const fvec2& Vec);
  virtual void BindParamVect3(FxShader* hfx, const FxShaderParam* hpar, const fvec3& Vec);
  virtual void BindParamVect4(FxShader* hfx, const FxShaderParam* hpar, const fvec4& Vec);
  virtual void BindParamVect4Array(FxShader* hfx, const FxShaderParam* hpar, const fvec4* Vec, const int icount);
  virtual void BindParamFloatArray(FxShader* hfx, const FxShaderParam* hpar, const float* pfA, const int icnt);
  virtual void BindParamFloat(FxShader* hfx, const FxShaderParam* hpar, float fA);
  virtual void BindParamFloat2(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB);
  virtual void BindParamFloat3(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC);
  virtual void BindParamFloat4(FxShader* hfx, const FxShaderParam* hpar, float fA, float fB, float fC, float fD);
  virtual void BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx4& Mat);
  virtual void BindParamMatrix(FxShader* hfx, const FxShaderParam* hpar, const fmtx3& Mat);
  virtual void BindParamMatrixArray(FxShader* hfx, const FxShaderParam* hpar, const fmtx4* MatArray, int iCount);
  virtual void BindParamU32(FxShader* hfx, const FxShaderParam* hpar, U32 uval);
  virtual void BindParamCTex(FxShader* hfx, const FxShaderParam* hpar, const Texture* pTex);
  virtual bool LoadFxShader(const AssetPath& pth, FxShader* ptex);

  Interface(GfxTargetGL& glctx);

  void BindContainerToAbstract(Container* pcont, FxShader* fxh);

  Container* GetActiveEffect() const { return mpActiveEffect; }

protected:
  // static CGcontext					mCgContext;
  Container* mpActiveEffect;
  const Pass* mLastPass;
  FxShaderTechnique* mhCurrentTek;

  GfxTargetGL& mTarget;
};

Container* LoadFxFromFile(const AssetPath& pth);

} // namespace ork::lev2::glslfx

///////////////////////////////////////////////////////////////////////////////
