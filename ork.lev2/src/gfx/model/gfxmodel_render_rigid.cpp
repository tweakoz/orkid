////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/kernel/environment.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////

void XgmModel::RenderRigid(
    const fcolor4& ModColor,
    const fmtx4& WorldMat,
    ork::lev2::Context* context,
    const RenderContextInstData& RCID,
    const RenderContextInstModelData& mdlctx) const {

  auto R                       = RCID.GetRenderer();
  auto RCFD                    = context->topRenderContextFrameData();
  const auto& CPD              = RCFD->topCPD();
  bool stereo1pass             = CPD.isStereoOnePass();
  const XgmMesh& XgmMesh       = *mdlctx.mMesh;
  auto cluster                 = mdlctx._cluster;
  const XgmSubMesh& XgmClusSet = *mdlctx.mSubMesh;
  int inummesh                 = numMeshes();
  int inumclusset              = XgmMesh.numSubMeshes();

  auto fxcache = RCID._pipeline_cache;
  OrkAssert(fxcache);
  auto pipeline = fxcache->findPipeline(RCID);
  OrkAssert(pipeline);

  context->debugPushGroup(
      FormatString("XgmModel::RenderRigid stereo1pass<%d> inummesh<%d> inumclusset<%d>", int(stereo1pass), inummesh, inumclusset));

  context->MTXI()->SetMMatrix(WorldMat);
  context->PushModColor(ModColor);
  {
    //////////////////////////////////////////////
    // pipeline wrapped draw call
    //////////////////////////////////////////////
    pipeline->wrappedDrawCall(RCID, [&]() {
      auto vtxbuffer = cluster->_vertexBuffer;
      int inumprim   = cluster->numPrimGroups();
      for (int iprim = 0; iprim < inumprim; iprim++) {
        auto primgroup = cluster->primgroup(iprim);
        auto idxbuffer = primgroup->GetIndexBuffer();
        context->GBI()->DrawIndexedPrimitiveEML(*vtxbuffer, *idxbuffer, primgroup->GetPrimType());
      }
    });
    //////////////////////////////////////////////
  }
  context->PopModColor();
  context->debugPopGroup();
}

} // namespace ork::lev2
