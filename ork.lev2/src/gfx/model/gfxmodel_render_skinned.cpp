////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/orklut.hpp>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/kernel/string/deco.inl>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>

namespace ork::lev2 {

void XgmModel::RenderSkinned(
    const XgmModelInst* minst,
    const fcolor4& ModColor,
    const fmtx4& WorldMat,
    ork::lev2::Context* context,
    const RenderContextInstData& RCID,
    const RenderContextInstModelData& mdlctx) const {

  static constexpr size_t kMatrixBlockSize = 1024;
  static auto matrix_block                 = new fmtx4[kMatrixBlockSize];

  auto fxcache  = RCID._pipeline_cache;
  auto pipeline = fxcache->findPipeline(RCID);
  auto pmat     = pipeline->_material;

  auto R           = RCID.GetRenderer();
  auto RCFD        = context->topRenderContextFrameData();
  const auto& CPD  = RCFD->topCPD();
  bool stereo1pass = CPD.isStereoOnePass();

  ///////////////////////////////////
  // apply local pose to world pose
  ///////////////////////////////////

  auto localpose = minst->_localPose;
  minst->_worldPose->apply(WorldMat, localpose);

  if (0) {
    fvec3 c1(1, .8, .8);
    fvec3 c2(.8, .8, 1);
    deco::printe(c1, "LocalPose (post-concat)", true);
    deco::prints(localpose->dumpc(c1), true);
    deco::printe(c2, "WorldPose (post-concat)", true);
    deco::prints(minst->_worldPose->dumpc(c2), true);
  }

  ///////////////////////////////////
  // Draw Skinned Mesh
  ///////////////////////////////////


  if (1) // draw mesh
  {
    const XgmSkeleton& Skeleton = skeleton();

    context->debugPushGroup("RenderSkinnedMesh");
    context->MTXI()->PushMMatrix(fmtx4());
    if (CPD.isPicking()) {
      context->PushModColor(fvec4(1,1,0,1));
    }
    else{
      context->PushModColor(ModColor);
    }

    {
      const XgmMesh& XgmMesh       = *mdlctx.mMesh;
      auto cluster                 = mdlctx._cluster;
      const XgmSubMesh& XgmClusSet = *mdlctx.mSubMesh;

      bool bmatpushed = false;

      auto mtl = XgmClusSet.GetMaterial();

      if (mtl != nullptr) {

        mtl->gpuUpdate(context);

        auto fxcache = RCID._pipeline_cache;
        OrkAssert(fxcache);
        auto pipeline = fxcache->findPipeline(RCID);
        OrkAssert(pipeline);

        if (pipeline->_debugBreak) {
          // OrkBreak();
        }

        pipeline->wrappedDrawCall(RCID, [&]() {
          size_t inumjoints = cluster->mJoints.size();

          OrkAssert(miBonesPerCluster <= kMatrixBlockSize);

          for (size_t ijointreg = 0; ijointreg < inumjoints; ijointreg++) {
            const std::string& JointName = cluster->mJoints[ijointreg];
            int JointSkelIndex           = cluster->mJointSkelIndices[ijointreg];
            const fmtx4& finalmtx        = minst->_worldPose->_world_bindrela_matrices[JointSkelIndex];
            //////////////////////////////////////
            matrix_block[ijointreg] = finalmtx;
          }

          //////////////////////////////////////////////////////
          // apply bones
          //////////////////////////////////////////////////////

          MaterialInstItemMatrixBlock mtxblockitem;
          mtxblockitem.SetNumMatrices(inumjoints);
          mtxblockitem.SetMatrixBlock(matrix_block);
          mtl->BindMaterialInstItem(&mtxblockitem);
          { mtxblockitem.mApplicator->ApplyToTarget(context); }
          mtl->UnBindMaterialInstItem(&mtxblockitem);

          //////////////////////////////////////////////////////
          auto vtxbuffer = cluster->GetVertexBuffer();
          if (vtxbuffer) {
            int inumprim = cluster->numPrimGroups();
            for (int iprim = 0; iprim < inumprim; iprim++) {
              auto primgroup = cluster->primgroup(iprim);
              auto idxbuffer = primgroup->GetIndexBuffer();
              context->GBI()->DrawIndexedPrimitiveEML(*vtxbuffer, *idxbuffer, primgroup->GetPrimType());
            }
          }
          //////////////////////////////////////////////////////
        });
      }
      context->PopModColor();
      context->MTXI()->PopMMatrix();
      context->debugPopGroup();
    }
  }
}

} // namespace ork::lev2
