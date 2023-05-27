////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork { namespace lev2 {

///////////////////////////////////////////////////////////////////////////////

class FxParamRec {
public:
  enum EBindingScope {
    ESCOPE_CONSTANT = 0,
    ESCOPE_PERFRAME,
    ESCOPE_PERMATERIALINST,
    ESCOPE_PEROBJECT,
  };

  FxParamRec();

  std::string _name;
  std::string mParameterSemantic;
  EPropType meParameterType;

  fxparam_constptr_t mParameterHandle;
  EBindingScope meBindingScope;
  U32 mTargetHash;
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderPass {
  std::string mPassName;
  void* mInternalHandle;
  bool mbRestorePass;

  RenderQueueSortingData mRenderQueueSortingData;

  FxShaderPass(void* ih = 0);
  void* GetPlatformHandle(void) const {
    return mInternalHandle;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderTechnique {

  std::string mTechniqueName;
  const void* mInternalHandle;
  orkvector<FxShaderPass*> mPasses;
  bool mbValidated;
  fxshader_ptr_t _shader = nullptr;

  FxShaderTechnique(void* ih = 0);

  const void* GetPlatformHandle(void) const {
    return mInternalHandle;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderParamInBlockInfo {
  FxShaderParamBlock* _parent = nullptr;
};

struct FxShaderParam {
  std::string _name;
  std::string mParameterSemantic;
  std::string mParameterType;
  EPropType meParamType;
  void* mInternalHandle;
  bool mBindable;
  FxShaderParamInBlockInfo* _blockinfo = nullptr;
  FxShaderParam* mChildParam;

  orklut<std::string, std::string> _annotations;
  FxShaderParam(void* ih = 0);
  void* GetPlatformHandle(void) const {
    return mInternalHandle;
  }
};

struct FxShaderParamBlock {
  std::string _name;
  FxShaderParam* param(const std::string& name) const;
  std::map<std::string, FxShaderParam*> _subparams;
  svarp_t _impl;
  FxInterface* _fxi = nullptr;
};
struct FxShaderParamBuffer {
  size_t _length = 0;
  svarp_t _impl;
};
struct FxShaderParamBufferMapping {
  FxShaderParamBufferMapping();
  ~FxShaderParamBufferMapping();
  void unmap();
  FxShaderParamBuffer* _buffer = nullptr;
  FxInterface* _fxi            = nullptr;
  size_t _offset               = 0;
  size_t _cursor               = 0;
  size_t _length               = 0;
  svarp_t _impl;
  ///////////////////////////////////////////////////
  template <typename T> T& ref(size_t offset) {
    size_t end = offset + sizeof(T);
    OrkAssert(end <= _length);
    auto tstar = (T*)(((char*)_mappedaddr) + offset);
    return *tstar;
  }
  ///////////////////////////////////////////////////
  template <typename T, typename ... A> T& make(A&&... args) {
    size_t end = _cursor + sizeof(T);
    OrkAssert(end <= _length);
    auto tstar = (T*)(((char*)_mappedaddr) + _cursor);
    new (tstar) T(std::forward<A>(args)...);
    _cursor += sizeof(T);
    return *tstar;
  }
  ///////////////////////////////////////////////////
  void seek(size_t offset) {
    _cursor = offset;
  }
  ///////////////////////////////////////////////////
  void* _mappedaddr = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_SHADER_STORAGE)

struct FxShaderStorageBlock {
  std::string _name;
  svarp_t _impl;
};
struct FxShaderStorageBuffer {
  size_t _length = 0;
  svarp_t _impl;
};
struct FxShaderStorageBufferMapping {
  FxShaderStorageBufferMapping();
  ~FxShaderStorageBufferMapping();
  void unmap();
  FxShaderStorageBuffer* _buffer = nullptr;
  ComputeInterface* _ci          = nullptr;
  size_t _offset                 = 0;
  size_t _cursor                 = 0;
  size_t _length                 = 0;
  svarp_t _impl;

  template <typename T> T& ref(size_t offset) {
    size_t end = offset + sizeof(T);
    assert(end <= _length);
    auto tstar = (T*)(((char*)_mappedaddr) + offset);
    return *tstar;
  }

  ///////////////////////////////////////////////////
  template <typename T, typename ... A> T& make(A&&... args) {
    size_t end = _cursor + sizeof(T);
    OrkAssert(end <= _length);
    auto tstar = (T*)(((char*)_mappedaddr) + _cursor);
    new (tstar) T(std::forward<A>(args)...);
    _cursor += sizeof(T);
    return *tstar;
  }
  ///////////////////////////////////////////////////
  void seek(size_t offset) {
    _cursor = offset;
  }

  void* _mappedaddr = nullptr;
};

#endif

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_COMPUTE_SHADERS)
struct FxComputeShader {
  svar64_t _impl;
  std::string _name;
};
#endif

///////////////////////////////////////////////////////////////////////////////

struct FxShader {

  using parambynamemap_t      = std::map<std::string, fxparam_constptr_t>;
  using paramblockbynamemap_t = std::map<std::string, const FxShaderParamBlock*>;
  using techniquebynamemap_t  = std::map<std::string, fxtechnique_constptr_t>;
  using computebynamemap_t    = std::map<std::string, const FxComputeShader*>;

  void OnReset();

  static void SetLoaderTarget(Context* targ);

  FxShader();

  static void RegisterLoaders(const file::Path& base, const std::string& ext);

  static const char* assetTypeNameStatic(void) {
    return "fxshader";
  }

  void addTechnique(fxtechnique_constptr_t tek);
  void addParameter(fxparam_constptr_t param);
  void addParameterBlock(const FxShaderParamBlock* block);
  void addComputeShader(const FxComputeShader* csh);

  const techniquebynamemap_t& techniques(void) const {
    return _techniques;
  }
  const parambynamemap_t& namedParams(void) const {
    return _parameterByName;
  }
  const paramblockbynamemap_t& namedParamBlocks(void) const {
    return _parameterBlockByName;
  }
  const computebynamemap_t& namedComputeShaders(void) const {
    return _computeShaderByName;
  }

  FxShaderParam* FindParamByName(const std::string& named);
  FxShaderParamBlock* FindParamBlockByName(const std::string& named);
  FxShaderTechnique* FindTechniqueByName(const std::string& named);

#if defined(ENABLE_COMPUTE_SHADERS)
  FxComputeShader* findComputeShader(const std::string& named);
#endif

  void SetAllowCompileFailure(bool bv) {
    mAllowCompileFailure = bv;
  }
  bool GetAllowCompileFailure() const {
    return mAllowCompileFailure;
  }
  void SetFailedCompile(bool bv) {
    mFailedCompile = bv;
  }
  bool GetFailedCompile() const {
    return mFailedCompile;
  }

  void SetName(const char*);
  const char* GetName();

  ////////////////////////////////////////////////////
  // SSBO support
  ////////////////////////////////////////////////////

#if defined(ENABLE_SHADER_STORAGE)
  typedef orkmap<std::string, const FxShaderStorageBlock*> storageblockbynamemap_t;
  storageblockbynamemap_t _storageBlockByName;
  const storageblockbynamemap_t& namedStorageBlocks(void) const {
    return _storageBlockByName;
  }
  void addStorageBlock(const FxShaderStorageBlock* block);
  FxShaderStorageBlock* storageBlockByName(const std::string& named);
#endif

  ////////////////////////////////////////////////////

  svar16_t _internalHandle;
  techniquebynamemap_t _techniques;
  parambynamemap_t _parameterByName;
  paramblockbynamemap_t _parameterBlockByName;
  computebynamemap_t _computeShaderByName;
  ork::varmap::VarMap _varmap;
  
  bool mAllowCompileFailure = false;
  bool mFailedCompile       = false;
  std::string mName;
};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
