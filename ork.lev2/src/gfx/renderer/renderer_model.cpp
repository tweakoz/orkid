////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>

namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
ModelDrawable::ModelDrawable(DrawableOwner* pent)
    : Drawable()
    , mModelInst(NULL)
    , mfScale(1.0f)
    , mRotate(0.0f, 0.0f, 0.0f)
    , mOffset(0.0f, 0.0f, 0.0f)
    , mpWorldPose(0)
    , mbShowBoundingSphere(false) {
  for (int i = 0; i < kMaxEngineParamFloats; i++)
    mEngineParamFloats[i] = 0.0f;
}
/////////////////////////////////////////////////////////////////////
ModelDrawable::~ModelDrawable() {
  if (mpWorldPose) {
    delete mpWorldPose;
    mpWorldPose = nullptr;
  }
}
void ModelDrawable::SetEngineParamFloat(int idx, float fv) {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  mEngineParamFloats[idx] = fv;
}

float ModelDrawable::GetEngineParamFloat(int idx) const {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  return mEngineParamFloats[idx];
}

///////////////////////////////////////////////////////////////////////////////

void ModelDrawable::SetModelInst(lev2::XgmModelInst* pModelInst) {
  mModelInst                          = pModelInst;
  const lev2::XgmModel* Model         = mModelInst->xgmModel();
  bool isSkinned                      = Model->isSkinned();
  ork::lev2::XgmWorldPose* pworldpose = 0;
  if (isSkinned) {
    mpWorldPose = new ork::lev2::XgmWorldPose(Model->skeleton());
  }
  Drawable::var_t ap;
  ap.Set(mpWorldPose);
  SetUserDataA(ap);
}

///////////////////////////////////////////////////////////////////////////////

void ModelDrawable::enqueueToRenderQueue(const DrawableBufItem& item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());
  auto RCFD          = renderer->GetTarget()->topRenderContextFrameData();
  const auto& topCPD = RCFD->topCPD();

  const lev2::XgmModel* Model = mModelInst->xgmModel();

  const auto& monofrustum = topCPD.monoCamFrustum();

  // TODO - resolve frustum in case of stereo camera

  const ork::fmtx4& matw = item.mXfData.mWorldMatrix;

  bool isPickState = renderer->GetTarget()->FBI()->isPickState();
  bool isSkinned   = Model->isSkinned();

  ork::fvec3 center_plus_offset = mOffset + Model->boundingCenter();

  ork::fvec3 ctr = ork::fvec4(center_plus_offset * mfScale).Transform(matw);

  ork::fvec3 vwhd = Model->boundingAA_WHD();
  float frad      = vwhd.GetX();
  if (vwhd.GetY() > frad)
    frad = vwhd.GetY();
  if (vwhd.GetZ() > frad)
    frad = vwhd.GetZ();
  frad *= 0.6f;

  bool bCenterInFrustum = monofrustum.contains(ctr);

  //////////////////////////////////////////////////////////////////////

  const ork::lev2::XgmWorldPose* pworldpose = GetUserDataA().Get<ork::lev2::XgmWorldPose*>();

  ork::fvec3 matw_trans;
  ork::fquat matw_rot;
  float matw_scale;

  matw.decompose(matw_trans, matw_rot, matw_scale);

  //////////////////////////////////////////////////////////////////////
  // generate coarse light mask

  /*
  ork::lev2::LightMask mdl_lmask;

  ork::lev2::LightManager* light_manager = RCFD->GetLightManager();

  size_t inuml = 0;

  if (light_manager) {
    inuml = light_manager->mLightsInFrustum.size();

    for (size_t il = 0; il < inuml; il++) {
      ork::lev2::Light* plight = light_manager->mLightsInFrustum[il];
      OrkAssert(plight);

      bool baf = plight->AffectsSphere(ctr, frad);
      if (baf) {
        mdl_lmask.AddLight(plight);
      }
    }
  }*/

  //////////////////////////////////////////////////////////////////////

  int inumacc = 0;
  int inumrej = 0;

  int inummeshes = Model->numMeshes();
  for (int imesh = 0; imesh < inummeshes; imesh++) {
    const lev2::XgmMesh& mesh = *Model->mesh(imesh);

    // if( 0 == strcmp(mesh.meshName().c_str(),"fg_2_1_3_ground_SG_ground_GeoDaeId") )
    //{
    //	orkprintf( "yo\n" );
    //}

    if (mModelInst->isMeshEnabled(imesh)) {
      int inumclusset = mesh.numSubMeshes();

      for (int ics = 0; ics < inumclusset; ics++) {
        const lev2::XgmSubMesh& submesh   = *mesh.subMesh(ics);
        const lev2::GfxMaterial* material = submesh.mpMaterial;

        int inumclus = submesh.miNumClusters;

        for (int ic = 0; ic < inumclus; ic++) {
          bool btest = true;

          const lev2::XgmCluster& cluster = submesh.cluster(ic);

          if (isSkinned) {

            float fdb = monofrustum._bottomPlane.pointDistance(ctr);
            float fdt = monofrustum._topPlane.pointDistance(ctr);
            float fdl = monofrustum._leftPlane.pointDistance(ctr);
            float fdr = monofrustum._rightPlane.pointDistance(ctr);
            float fdn = monofrustum._nearPlane.pointDistance(ctr);
            float fdf = monofrustum._farPlane.pointDistance(ctr);

            const float kdist = -5.0f;
            btest             = (fdb > kdist) && (fdt > kdist) && (fdl > kdist) && (fdr > kdist) &&
                    (fdn > kdist)
                    //&&	(fdn<100.0f); // 50m actors
                    && (fdf > kdist);
            if (false == btest) {
            }
            btest = true; // todo fix culler
          } else {        // Rigid
            const Sphere& bsph = cluster.mBoundingSphere;

            float clussphrad = bsph.mRadius * matw_scale * mfScale;
            fvec3 clussphctr = ((bsph.mCenter + mOffset) * mfScale).Transform(matw);
            Sphere sph2(clussphctr, clussphrad);

            btest = true; // CollisionTester::FrustumSphereTest( frus, sph2 );
          }

          if (btest) {
            lev2::ModelRenderable& renderable = renderer->enqueueModel();

            // if(mEngineParamFloats[0] < 1.0f && mEngineParamFloats[0] > 0.0f)
            //	orkprintf("mEngineParamFloats[0] = %g\n", mEngineParamFloats[0]);

            for (int i = 0; i < kMaxEngineParamFloats; i++)
              renderable.SetEngineParamFloat(i, mEngineParamFloats[i]);

            renderable.SetModelInst(mModelInst);
            renderable.SetObject(GetOwner());
            renderable.SetMesh(&mesh);
            renderable.SetSubMesh(&submesh);
            renderable.SetCluster(&cluster);
            renderable.SetModColor(renderer->GetTarget()->RefModColor());
            renderable.SetMatrix(matw);
            // renderable.SetLightMask(lmask);
            renderable.SetScale(mfScale);
            renderable.SetRotate(mRotate);
            renderable.SetOffset(mOffset);

            size_t umat = size_t(material);
            u32 imtla   = (umat & 0xff);
            u32 imtlb   = ((umat >> 8) & 0xff);
            u32 imtlc   = ((umat >> 16) & 0xff);
            u32 imtld   = ((umat >> 24) & 0xff);
            u32 imtl    = (imtla + imtlb + imtlc + imtld) & 0xff;

            int isortpass = (material->GetRenderQueueSortingData().miSortingPass + 16) & 0xff;
            int isortoffs = material->GetRenderQueueSortingData().miSortingOffset;

            int isortkey = (isortpass << 24) | (isortoffs << 16) | imtl;

            renderable.SetSortKey(isortkey);
            // orkprintf( " ModelDrawable::enqueueToRenderQueue() rable<%p> \n", & renderable );

            inumacc++;
          } else {
            inumrej++;
          }
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

ModelRenderable::ModelRenderable(IRenderer* renderer)
    : IRenderableDag()
    , mModelInst(0)
    , mSortKey(0)
    , mMaterialIndex(0)
    , mMaterialPassIndex(0)
    , mScale(1.0f)
    , mEdgeColor(-1)
    ///////////////////////////
    , mMesh(0)
    , mSubMesh(0)
    , mCluster(0)
    , mRotate(0.0f, 0.0f, 0.0f)
    , mOffset(0.0f, 0.0f, 0.0f) {
  for (int i = 0; i < kMaxEngineParamFloats; i++)
    mEngineParamFloats[i] = 0.0f;
}

///////////////////////////////////////////////////////////////////////////////

void ModelRenderable::SetEngineParamFloat(int idx, float fv) {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  mEngineParamFloats[idx] = fv;
}

///////////////////////////////////////////////////////////////////////////////

float ModelRenderable::GetEngineParamFloat(int idx) const {
  OrkAssert(idx >= 0 && idx < kMaxEngineParamFloats);

  return mEngineParamFloats[idx];
}

///////////////////////////////////////////////////////////////////////////////

void ModelRenderable::Render(const IRenderer* renderer) const {
  renderer->RenderModel(*this);
}

///////////////////////////////////////////////////////////////////////////////

bool ModelRenderable::CanGroup(const IRenderable* oth) const {
  auto pren = dynamic_cast<const ModelRenderable*>(oth);
  if (pren) {
    const lev2::XgmSubMesh* submesh = pren->subMesh();
    const GfxMaterial* mtl          = submesh->GetMaterial();
    const GfxMaterial* mtl2         = subMesh()->GetMaterial();
    return (mtl == mtl2);
  }
  return false;
}

} // namespace ork::lev2
