////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <unordered_map>
#include <ork/lev2/gfx/gfxenv_enum.h> // For ETextureDest
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxrasterstate.h>
#include <ork/kernel/varmap.inl>

namespace ork::lev2 {

struct FxStateInstance;
using fxinstance_ptr_t      = std::shared_ptr<FxStateInstance>;
using fxinstance_constptr_t = std::shared_ptr<const FxStateInstance>;

///////////////////////////////////////////////////////////////////////////////

/*enum FxStateBasePermutation { MONO = 0, //
                              STEREO, // 
                              PICK, // 
                              COUNT };*/

///////////////////////////////////////////////////////////////////////////////
// FxStateInstance : instance of a material "class"
//  with independent parameter values
///////////////////////////////////////////////////////////////////////////////

struct FxCachePermutation {

  void dump() const;
  uint64_t genIndex() const;

  uint32_t _rendering_model = "DEFERRED_PBR"_crcu;
  bool _stereo = false;
  bool _instanced = false;
  bool _skinned = false;
  fxtechnique_constptr_t _forced_technique = nullptr;

};

struct FxCachePermutationSet {
  void add(fxcachepermutation_constptr_t perm);
  std::unordered_set<fxcachepermutation_constptr_t> __permutations;
};

///////////////////////////////////////////////////////////////////////////////

struct FxStateInstance {

  FxStateInstance(const FxCachePermutation& config);

  int beginBlock(const RenderContextInstData& RCID);
  bool beginPass(const RenderContextInstData& RCID, int ipass);
  void endPass(const RenderContextInstData& RCID);
  void endBlock(const RenderContextInstData& RCID);

  void wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall);

  using varval_t = varmap::VarMap::value_type;
  using statelambda_t = std::function<void(const RenderContextInstData& RCID, int ipass)>;

  void addStateLambda(statelambda_t sl){_statelambdas.push_back(sl);}

  void bindParam(fxparam_constptr_t p, varval_t v);

  GfxMaterial* _material = nullptr;
  fxtechnique_constptr_t _technique = nullptr;
  const FxCachePermutation __permutation;
  std::unordered_map<fxparam_constptr_t, varval_t> _params;
  std::vector<statelambda_t> _statelambdas;
  fxparam_constptr_t _parInstanceMatrixMap = nullptr;
  fxparam_constptr_t _parInstanceIdMap     = nullptr;
  fxparam_constptr_t _parInstanceColorMap  = nullptr;
  svar64_t _impl;

};

struct FxStateInstanceCache {

  using cache_miss_fn_t = std::function<fxinstance_ptr_t(const FxCachePermutation&)>;
  fxinstance_ptr_t findfxinst(const RenderContextInstData& RCID) const;
  fxinstance_ptr_t findfxinst(const FxCachePermutation& permu) const;
  cache_miss_fn_t _on_miss;
  mutable std::unordered_map<uint64_t,fxinstance_ptr_t> _lut;
  svar64_t _impl;
};

template <typename MtlClass>
  struct FxStateInstanceCacheImpl{

  FxStateInstanceCacheImpl(){}

  fxinstancecache_constptr_t getCache(const MtlClass* material){

    fxinstancecache_constptr_t rval;

    auto it = _fxcachemap.find(material);
    if(it==_fxcachemap.end()){
      auto newcache = std::make_shared<FxStateInstanceCache>();
      newcache->_impl.set<const MtlClass*>(material);
      newcache->_on_miss = [=](const FxCachePermutation& permu) -> fxinstance_ptr_t {
        return _createFxStateInstance(permu,material);
      };
      rval = newcache;
      _fxcachemap[material] = newcache;
    }
    else{
      rval = it->second;
    }

    return rval;    
  }

  std::unordered_map<const MtlClass*,fxinstancecache_ptr_t> _fxcachemap;
};

} // namespace ork::lev2
