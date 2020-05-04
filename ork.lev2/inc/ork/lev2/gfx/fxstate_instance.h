////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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

struct FxShaderTechniquePermA;
struct FxShaderTechniquePermutations;
struct FxStateInstance;
using fxinstance_ptr_t        = std::shared_ptr<FxStateInstance>;
using fxinstance_constptr_t   = std::shared_ptr<const FxStateInstance>;
using fxshadertechperma_ptr_t = std::shared_ptr<FxShaderTechniquePermA>;
using fxshadertechperms_ptr_t = std::shared_ptr<FxShaderTechniquePermutations>;

///////////////////////////////////////////////////////////////////////////////

struct FxShaderTechniquePermA {
  fxtechnique_constptr_t _rigid;
  fxtechnique_constptr_t _skinned;
  fxtechnique_constptr_t _rigid_instanced;
  fxtechnique_constptr_t _skinned_instanced;
};

struct FxShaderTechniquePermutations {
  enum PermBase { MONO = 0, STEREO, PICK };
  FxShaderTechniquePermutations();
  fxtechnique_constptr_t select(PermBase base, bool skinned, bool instanced);
  fxshadertechperma_ptr_t _mono, _stereo, _pick;
};

///////////////////////////////////////////////////////////////////////////////
// FxStateInstance : instance of a material "class"
//  with independent parameter values
///////////////////////////////////////////////////////////////////////////////

struct FxStateInstanceConfig {
  bool _instanced_primitive = false;
};

struct FxStateInstance : public std::enable_shared_from_this<FxStateInstance> {

  FxStateInstance(FxStateInstanceConfig& config);

  int beginBlock(const RenderContextInstData& RCID);
  bool beginPass(const RenderContextInstData& RCID, int ipass);
  void endPass(const RenderContextInstData& RCID);
  void endBlock(const RenderContextInstData& RCID);

  void wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall);

  using varval_t = varmap::VarMap::value_type;
  fxshadertechperms_ptr_t _teks;
  FxStateInstanceConfig _config;
  std::unordered_map<fxparam_constptr_t, varval_t> _params;
};

} // namespace ork::lev2
