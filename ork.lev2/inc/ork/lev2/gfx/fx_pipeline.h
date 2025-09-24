////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <functional>
#include <unordered_map>
#include <ork/lev2/gfx/gfxenv_enum.h> // For ETextureDest
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/renderer/rendercontext.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/rasterstate.h>
#include <ork/kernel/varmap.inl>

namespace ork::lev2 {

struct FxPipeline;
using fxpipeline_ptr_t      = std::shared_ptr<FxPipeline>;
using pipelineance_constptr_t = std::shared_ptr<const FxPipeline>;

///////////////////////////////////////////////////////////////////////////////

/*enum FxStateBasePermutation { MONO = 0, //
                              STEREO, // 
                              PICK, // 
                              COUNT };*/

///////////////////////////////////////////////////////////////////////////////
// FxPipeline : instance of a material "class"
//  with independent parameter values
///////////////////////////////////////////////////////////////////////////////

struct FxPipelinePermutation {

  void dump() const;
  uint64_t genIndex() const;

  uint32_t _rendering_model = "FORWARD_PBR"_crcu;
  bool _stereo = false;
  bool _instanced = false;
  bool _skinned = false;
  bool _is_picking = false;
  bool _has_vtxcolors = false;
  bool _vr_mono = false;
  
  fxtechnique_constptr_t _forced_technique = nullptr;

};

struct FxPipelinePermutationSet {
  void add(fxpipelinepermutation_constptr_t perm);
  std::unordered_set<fxpipelinepermutation_constptr_t> __permutations;
};

///////////////////////////////////////////////////////////////////////////////
struct FxPipelineNamedParamProviders;
using fxpipelinenamedparamproviders_ptr_t = std::shared_ptr<FxPipelineNamedParamProviders>;

struct FxPipelineProviderContext {
  FxPipelineProviderContext(const RenderContextInstData& rcid, 
                            const CompositingPassData& topCPD, 
                            FxInterface* fxi);
  const RenderContextInstData& _rcid;
  const RenderContextFrameData* _rcfd;
  FxInterface* _fxi;
  const CompositingPassData& _topCPD;
};
struct FxPipelineNamedParamProviders {

  static fxpipelinenamedparamproviders_ptr_t instance();
  FxPipelineNamedParamProviders();

  using provider_fn_t = std::function<void(const FxPipelineProviderContext&, fxparam_constptr_t)>;
  std::unordered_map<uint64_t,provider_fn_t> _providers;
};


///////////////////////////////////////////////////////////////////////////////

struct FxPipeline {

  using varval_t = varmap::VarMap::value_type;
  using statelambda_t = std::function<void(const RenderContextInstData& RCID)>;
  using varval_generator_t = std::function<varval_t()>;

  FxPipeline(const FxPipelinePermutation& config);
  void dump() const;

  int beginBlock(const RenderContextInstData& RCID);
  void endBlock(const RenderContextInstData& RCID);
  void bindParam(fxparam_constptr_t p, varval_t v);
  void bindUniformBuffer(fxuniformblock_constptr_t p, varval_t v);
  void bindStorage(fxparamstorageblock_constptr_t p, varval_t v);
  void wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall);

  void _set_typed_param(const RenderContextInstData& RCID, fxparam_constptr_t p, varval_t val);
  void _set_storage(const RenderContextInstData& RCID, fxparamstorageblock_constptr_t p, varval_t val);
  void addStateLambda(statelambda_t sl){_statelambdas.push_back(sl);}

  GfxMaterial* _material_ptr = nullptr;
  rasterstate_ptr_t _rasterstate = nullptr;
  material_ptr_t _sharedMaterial = nullptr;
  fxtechnique_constptr_t _technique = nullptr;
  const FxPipelinePermutation __permutation;
  std::unordered_map<fxparam_constptr_t, varval_t> _params;
  std::unordered_map<fxuniformblock_constptr_t, varval_t> _uniformbuffers;
  std::unordered_map<fxparamstorageblock_constptr_t, varval_t> _storages;
  std::vector<statelambda_t> _statelambdas;
  fxparam_constptr_t _parInstanceMatrixMap = nullptr;
  fxparam_constptr_t _parInstanceIdMap     = nullptr;
  fxparam_constptr_t _parInstanceColorMap  = nullptr;
  svar64_t _impl;

  bool _debugBreak = false;
  bool _debugPrint = false;
  std::string _debugName;
  mutable std::string _debugText;
  varmap::varmap_ptr_t _vars;

};

struct FxPipelineCache {

  using cache_miss_fn_t = std::function<fxpipeline_ptr_t(const FxPipelinePermutation&)>;
  fxpipeline_ptr_t findPipeline(const RenderContextInstData& RCID) const;
  fxpipeline_ptr_t findPipeline(const FxPipelinePermutation& permu) const;
  cache_miss_fn_t _on_miss;
  mutable std::unordered_map<uint64_t,fxpipeline_ptr_t> _lut;
  svar64_t _impl;
};

template <typename MtlClass>
  struct FxPipelineCacheImpl{

  FxPipelineCacheImpl(){}

  fxpipelinecache_constptr_t getCache(const MtlClass* material){

    fxpipelinecache_constptr_t rval;

    auto it = _fxcachemap.find(material);
    if(it==_fxcachemap.end()){
      auto newcache = std::make_shared<FxPipelineCache>();
      newcache->_impl.set<const MtlClass*>(material);
      newcache->_on_miss = [=](const FxPipelinePermutation& permu) -> fxpipeline_ptr_t {
        return MtlClass::_createFxPipeline(permu,material);
      };
      rval = newcache;
      _fxcachemap[material] = newcache;
    }
    else{
      rval = it->second;
    }

    return rval;    
  }

  std::unordered_map<const MtlClass*,fxpipelinecache_ptr_t> _fxcachemap;
};

} // namespace ork::lev2
