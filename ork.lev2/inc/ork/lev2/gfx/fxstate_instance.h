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

struct FxStateInstance;
using fxinstance_ptr_t      = std::shared_ptr<FxStateInstance>;
using fxinstance_constptr_t = std::shared_ptr<const FxStateInstance>;

///////////////////////////////////////////////////////////////////////////////

enum FxStateBasePermutation { MONO = 0, STEREO, PICK };

///////////////////////////////////////////////////////////////////////////////
// FxStateInstance : instance of a material "class"
//  with independent parameter values
///////////////////////////////////////////////////////////////////////////////

struct FxStateInstanceConfig {
  FxStateBasePermutation _base_perm = MONO;
  bool _instanced_primitive         = false;
  bool _skinned                     = false;
};

struct FxStateInstance : public std::enable_shared_from_this<FxStateInstance> {

  FxStateInstance(FxStateInstanceConfig& config);

  int beginBlock(const RenderContextInstData& RCID);
  bool beginPass(const RenderContextInstData& RCID, int ipass);
  void endPass(const RenderContextInstData& RCID);
  void endBlock(const RenderContextInstData& RCID);

  void wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall);

  using varval_t                    = varmap::VarMap::value_type;
  fxtechnique_constptr_t _technique = nullptr;
  FxStateInstanceConfig _config;
  std::unordered_map<fxparam_constptr_t, varval_t> _params;
  fxparam_constptr_t _parInstanceMatrixMap = nullptr;
  fxparam_constptr_t _parInstanceIdMap     = nullptr;
  fxparam_constptr_t _parInstanceColorMap  = nullptr;
};

} // namespace ork::lev2
