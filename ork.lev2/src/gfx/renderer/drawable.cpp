////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>

#include <ork/kernel/orklut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/math/collision_test.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/kernel/string/deco.inl>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::Drawable, "Drawable");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::ModelDrawable, "ModelDrawable");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CallbackDrawable, "CallbackDrawable");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::DrawableOwner, "DrawableOwner");
namespace ork::lev2 {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Drawable::Describe() {
  DrawableBuffer::gbInsideClearAndSync = false;
}

void ModelDrawable::Describe() {
}

void CallbackDrawable::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

void Drawable::enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  DrawableBufItem& item = buffer.Queue(xfdata, this);
}

///////////////////////////////////////////////////////////////////////////////
Drawable::Drawable()
    : mDataA(nullptr)
    , mDataB(nullptr)
    , mEnabled(true) {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  fflush(stdout);
}
Drawable::~Drawable() {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());
  // printf( "Delete Drawable<%p>\n", this );
}

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

          ork::lev2::LightMask lmask;

          if (isSkinned) {
            // lmask = mdl_lmask;

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

            ////////////////////////////////////////////////
            // per cluster light assignment
            ////////////////////////////////////////////////

            /*if (btest) {
              ////////////////////////////////////////////////
              // lighting sphere test
              ////////////////////////////////////////////////

              ork::fvec3 ctr = ork::fvec4(Model->boundingCenter()).Transform(matw);
              for (size_t il = 0; il < inuml; il++) {
                ork::lev2::Light* plight = light_manager->mLightsInFrustum[il];
                OrkAssert(plight);
                bool baf = plight->AffectsSphere(sph2.mCenter, sph2.mRadius);
                if (baf) {
                  lmask.AddLight(plight);
                }
              }
            }*/
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
///////////////////////////////////////////////////////////////////////////////
CallbackDrawable::CallbackDrawable(DrawableOwner* pent)
    : Drawable()
    , mSortKey(4)
    , mRenderCallback(0)
    , _enqueueOnLayerCallback(0)
    , mDataDestroyer(0) {
}
///////////////////////////////////////////////////////////////////////////////
CallbackDrawable::~CallbackDrawable() {
  if (mDataDestroyer) {
    mDataDestroyer->Destroy();
  }
  mDataDestroyer          = 0;
  _enqueueOnLayerCallback = 0;
  mRenderCallback         = 0;
}
///////////////////////////////////////////////////////////////////////////////
// Multithreaded Renderer DB
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::enqueueOnLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const {
  // ork::opq::assertOnQueue2(opq::updateSerialQueue());

  DrawableBufItem& cdb = buffer.Queue(xfdata, this);
  cdb.mUserData0       = GetUserDataA();
  if (_enqueueOnLayerCallback) {
    _enqueueOnLayerCallback(cdb);
  }
  if (_enqueueOnLayerLambda) {
    _enqueueOnLayerLambda(cdb);
  }
}
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::enqueueToRenderQueue(const DrawableBufItem& item, lev2::IRenderer* renderer) const {
  ork::opq::assertOnQueue2(opq::mainSerialQueue());

  lev2::CallbackRenderable& renderable = renderer->enqueueCallback();
  renderable.SetMatrix(item.mXfData.mWorldMatrix);
  renderable.SetObject(GetOwner());
  renderable.SetRenderCallback(mRenderCallback);
  renderable.SetSortKey(mSortKey);
  renderable.SetDrawableDataA(GetUserDataA());
  renderable.SetDrawableDataB(GetUserDataB());
  renderable.SetUserData0(item.mUserData0);
  renderable.SetUserData1(item.mUserData1);
  renderable.SetModColor(renderer->GetTarget()->RefModColor());
}

///////////////////////////////////////////////////////////////////////////////

void DrawableOwner::Describe() {
}
DrawableOwner::DrawableOwner() {
}
DrawableOwner::~DrawableOwner() {
}
///////////////////////////////////////////////////////////////////////////////
void DrawableOwner::_addDrawable(const PoolString& layername, drawable_ptr_t pdrw) {
  DrawableVector* pldrawables = GetDrawables(layername);
  if (nullptr == pldrawables) {
    pldrawables = new DrawableVector;
    mLayerMap.AddSorted(layername, pldrawables);
  }
  pldrawables->push_back(pdrw);
}

DrawableOwner::DrawableVector* DrawableOwner::GetDrawables(const PoolString& layer) {
  DrawableVector* pldrawables = 0;

  LayerMap::const_iterator itL = mLayerMap.find(layer);
  if (itL != mLayerMap.end()) {
    pldrawables = itL->second;
  }
  return pldrawables;
}
const DrawableOwner::DrawableVector* DrawableOwner::GetDrawables(const PoolString& layer) const {
  const DrawableVector* pldrawables = 0;

  LayerMap::const_iterator itL = mLayerMap.find(layer);
  if (itL != mLayerMap.end()) {
    pldrawables = itL->second;
  }
  return pldrawables;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::lev2
