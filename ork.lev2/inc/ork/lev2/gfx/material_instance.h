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
// GfxMaterialInstance : instance of a material "class"
//  with independent parameter values
///////////////////////////////////////////////////////////////////////////////

struct GfxMaterialInstance : public std::enable_shared_from_this<GfxMaterialInstance> {
  // GfxMaterialInstance(material_ptr_t mtl);
  int beginBlock(const RenderContextInstData& RCID);
  bool beginPass(const RenderContextInstData& RCID, int ipass);
  void endPass(const RenderContextInstData& RCID);
  void endBlock(const RenderContextInstData& RCID);

  void wrappedDrawCall(const RenderContextInstData& RCID, void_lambda_t drawcall);
  void setInstanceMvpParams(std::string monocam, std::string stereocamL, std::string stereocamR);

  using varval_t = varmap::VarMap::value_type;
  // material_ptr_t _material;
  fxtechnique_constptr_t _monoTek   = nullptr;
  fxtechnique_constptr_t _pickTek   = nullptr;
  fxtechnique_constptr_t _stereoTek = nullptr;
  std::unordered_map<fxparam_constptr_t, varval_t> _params;
};

} // namespace ork::lev2
