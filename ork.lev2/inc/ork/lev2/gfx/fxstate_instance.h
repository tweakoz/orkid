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

struct FxStateInstanceConfig {

  void dump() const;

  ERenderModelID _rendering_model = ERenderModelID::DEFERRED_PBR;
  bool _stereo = false;
  bool _instanced = false;
  bool _skinned = false;
};

///////////////////////////////////////////////////////////////////////////////

struct FxStateInstance {

  FxStateInstance(FxStateInstanceConfig& config);

  int beginBlock(const RenderContextInstData& RCID);
  bool beginPass(const RenderContextInstData& RCID, int ipass);
  void endPass(const RenderContextInstData& RCID);
  void endBlock(const RenderContextInstData& RCID);

  void wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall);

  using varval_t = varmap::VarMap::value_type;
  using statelambda_t = std::function<void(const RenderContextInstData& RCID, int ipass)>;

  void addStateLambda(statelambda_t sl){_statelambdas.push_back(sl);}
  GfxMaterial* _material = nullptr;
  fxtechnique_constptr_t _technique = nullptr;
  FxStateInstanceConfig _config;
  std::unordered_map<fxparam_constptr_t, varval_t> _params;
  std::vector<statelambda_t> _statelambdas;
  fxparam_constptr_t _parInstanceMatrixMap = nullptr;
  fxparam_constptr_t _parInstanceIdMap     = nullptr;
  fxparam_constptr_t _parInstanceColorMap  = nullptr;
  svar64_t _impl;

};

struct FxStateInstanceLut {

  static uint64_t genIndex(const FxStateInstanceConfig& config);
  fxinstance_ptr_t findfxinst(const RenderContextInstData& RCID) const;
  void assignfxinst(const FxStateInstanceConfig& config, fxinstance_ptr_t fxi);
  std::unordered_map<uint64_t,fxinstance_ptr_t> _lut;
};

} // namespace ork::lev2
