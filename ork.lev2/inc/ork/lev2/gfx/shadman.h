////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/gfxenv.h>

namespace ork {
namespace lev2 {

struct FxShaderParam;
struct FxShaderParamBlock;
struct FxShaderParamBlockMapping;
typedef std::shared_ptr<FxShaderParamBufferMapping> parambuffermappingptr_t;

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

  const FxShaderParam *mParameterHandle;
  EBindingScope meBindingScope;
  U32 mTargetHash;
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderPass {
  std::string mPassName;
  void *mInternalHandle;
  bool mbRestorePass;

  RenderQueueSortingData mRenderQueueSortingData;

  FxShaderPass(void *ih = 0);
  void *GetPlatformHandle(void) const { return mInternalHandle; }
};

///////////////////////////////////////////////////////////////////////////////

struct FxShaderTechnique {
  std::string mTechniqueName;
  const void *mInternalHandle;
  orkvector<FxShaderPass *> mPasses;
  bool mbValidated;

  FxShaderTechnique(void *ih = 0);

  const void *GetPlatformHandle(void) const { return mInternalHandle; }
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
  void *mInternalHandle;
  bool mBindable;
  FxShaderParamInBlockInfo* _blockinfo = nullptr;
  FxShaderParam *mChildParam;

  orklut<std::string, std::string> mAnnotations;
  FxShaderParam(void *ih = 0);
  void *GetPlatformHandle(void) const { return mInternalHandle; }
};

struct FxShaderParamBlock {
  std::string _name;
  FxShaderParam *param(const std::string &name) const;
  std::map<std::string,FxShaderParam*> _subparams;
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
  FxInterface* _fxi = nullptr;
  size_t _offset = 0;
  size_t _length = 0;
  svarp_t _impl;

  template <typename T> T& ref(size_t offset) {
    size_t end = offset + sizeof(T);
    assert(end<=_length);
    auto tstar = (T*) (((char*)_mappedaddr)+offset);
    return *tstar;
  }


  void* _mappedaddr = nullptr;


};

///////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_SHADER_STORAGE)

struct FxShaderStorageBlock;
struct FxShaderStorageBlockMapping;

struct FxShaderStorageBlock {
  std::string _name;
  //FxShaderParam *param(const std::string &name) const;
  FxShaderStorageBlockMapping *map() const;
};
struct FxShaderStorageBlockMapping {
  ~FxShaderStorageBlockMapping();
  FxShaderStorageBlock *_block = nullptr;
  void unmap();
};

#endif

///////////////////////////////////////////////////////////////////////////////

struct FxShader {
  void *mInternalHandle;

  typedef orkmap<std::string, const FxShaderParam *> parambynamemap_t;
  typedef orkmap<std::string, const FxShaderParamBlock *> paramblockbynamemap_t;
  typedef orkmap<std::string, const FxShaderTechnique *> techniquebynamemap_t;

  techniquebynamemap_t _techniques;
  parambynamemap_t _parameterByName;
  paramblockbynamemap_t _parameterBlockByName;

  bool mAllowCompileFailure;
  bool mFailedCompile;
  std::string mName;

  void OnReset();

  static void SetLoaderTarget(GfxTarget *targ);

  FxShader();

  static void RegisterLoaders(const file::Path::NameType &base,
                              const file::Path::NameType &ext);

  void SetInternalHandle(void *ph) { mInternalHandle = ph; }
  void *GetInternalHandle(void) { return mInternalHandle; }

  static const char *GetAssetTypeNameStatic(void) { return "fxshader"; }

  void addTechnique(const FxShaderTechnique *tek);
  void addParameter(const FxShaderParam *param);
  void addParameterBlock(const FxShaderParamBlock *block);

  const techniquebynamemap_t &techniques(void) const { return _techniques; }
  const parambynamemap_t &namedParams(void) const { return _parameterByName; }
  const paramblockbynamemap_t &namedParamBlocks(void) const {
    return _parameterBlockByName;
  }

  // const orkmap<std::string,const FxShaderParam*>& 	GetParametersBySemantic(
  // void ) const { return mParameterBySemantic; }

  FxShaderParam *FindParamByName(const std::string &named);
  FxShaderParamBlock *FindParamBlockByName(const std::string &named);
  FxShaderTechnique *FindTechniqueByName(const std::string &named);

  void SetAllowCompileFailure(bool bv) { mAllowCompileFailure = bv; }
  bool GetAllowCompileFailure() const { return mAllowCompileFailure; }
  void SetFailedCompile(bool bv) { mFailedCompile = bv; }
  bool GetFailedCompile() const { return mFailedCompile; }

  void SetName(const char *);
  const char *GetName();

  ////////////////////////////////////////////////////
  // SSBO support
  ////////////////////////////////////////////////////

  #if defined(ENABLE_SHADER_STORAGE)
  typedef orkmap<std::string, const FxShaderStorageBlock *> storageblockbynamemap_t;
  storageblockbynamemap_t _storageBlockByName;
  const storageblockbynamemap_t &namedStorageBlocks(void) const {
    return _storageBlockByName;
  }
  void addStorageBlock(const FxShaderStorageBlock *block);
  FxShaderStorageBlock *storageBlockByName(const std::string &named);
  #endif

  ////////////////////////////////////////////////////

};

///////////////////////////////////////////////////////////////////////////////

} // namespace lev2
} // namespace ork
