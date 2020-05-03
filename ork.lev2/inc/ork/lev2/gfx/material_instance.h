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

struct GfxMaterialInstance;
using materialinst_ptr_t      = std::shared_ptr<GfxMaterialInstance>;
using materialinst_constptr_t = std::shared_ptr<const GfxMaterialInstance>;

///////////////////////////////////////////////////////////////////////////////

struct FxShaderTechniquePermA {
  fxtechnique_constptr_t _rigid;
  fxtechnique_constptr_t _skinned;
  fxtechnique_constptr_t _rigid_instanced;
  fxtechnique_constptr_t _skinned_instanced;
};
using fxshadertechperma_ptr_t = std::shared_ptr<FxShaderTechniquePermA>;

struct FxShaderTechniquePermutations {
  enum PermBase { MONO = 0, STEREO, PICK };
  FxShaderTechniquePermutations();
  fxtechnique_constptr_t select(PermBase base, bool skinned, bool instanced);
  fxshadertechperma_ptr_t _mono, _stereo, _pick;
};

///////////////////////////////////////////////////////////////////////////////
// GfxMaterialInstance : instance of a material "class"
//  with independent parameter values
///////////////////////////////////////////////////////////////////////////////

struct GfxMaterialInstance : public std::enable_shared_from_this<GfxMaterialInstance> {
  int beginBlock(const RenderContextInstData& RCID);
  bool beginPass(const RenderContextInstData& RCID, int ipass);
  void endPass(const RenderContextInstData& RCID);
  void endBlock(const RenderContextInstData& RCID);

  void wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall);

  using varval_t = varmap::VarMap::value_type;
  FxShaderTechniquePermutations _teks;
  std::unordered_map<fxparam_constptr_t, varval_t> _params;
};

} // namespace ork::lev2
