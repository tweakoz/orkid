////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/opq.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/pch.h>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>
#include <pkg/ent/ReferenceArchetype.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>

#include <ork/gfx/camera.h>
#include <ork/kernel/orklut.hpp>
#include <ork/math/collision_test.h>
#include <ork/stream/ResizableStringOutputStream.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::Drawable, "Drawable");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ModelDrawable, "ModelDrawable");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CallbackDrawable, "CallbackDrawable");

namespace ork::ent {

ork::MpMcBoundedQueue<RenderSyncToken> DrawableBuffer::mOfflineRenderSynchro;
ork::MpMcBoundedQueue<RenderSyncToken> DrawableBuffer::mOfflineUpdateSynchro;

ork::atomic<bool> DrawableBuffer::gbInsideClearAndSync;

Layer::Layer() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Drawable::Describe() { DrawableBuffer::gbInsideClearAndSync = false; }

void ModelDrawable::Describe() {}

void CallbackDrawable::Describe() {}

///////////////////////////////////////////////////////////////////////////////

void DrawableBuffer::Reset() {
  // AssertOnOpQ2( UpdateSerialOpQ() );

  miNumLayersUsed = 0;
  mLayers.clear();
  for (int il = 0; il < kmaxlayers; il++) {
    mRawLayers[il].Reset(*this);
  }
}

///////////////////////////////////////////////////////////////////////////////

void DrawableBufLayer::Reset(const DrawableBuffer& dB) {
  // AssertOnOpQ2( UpdateSerialOpQ() );
  miBufferIndex = dB.miBufferIndex;
  miItemIndex   = -1;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBufLayer* DrawableBuffer::MergeLayer(const PoolString& layername) {
  AssertOnOpQ2(UpdateSerialOpQ());
  DrawableBufLayer* player     = 0;
  LayerLut::const_iterator itL = mLayerLut.find(layername);
  if (itL != mLayerLut.end()) {
    player = itL->second;
  } else {
    OrkAssert(miNumLayersUsed < kmaxlayers);
    player = &mRawLayers[miNumLayersUsed++];
    mLayers.insert(layername);
    mLayerLut.AddSorted(layername, player);
  }
  return player;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBufItem& DrawableBufLayer::Queue(const DrawQueueXfData& xfdata, const Drawable* d) {
  AssertOnOpQ2(UpdateSerialOpQ());
  // mDrawBufItems.push_back(DrawableBufItem()); // replace std::vector with an array so we can amortize construction costs
  miItemIndex++;
  OrkAssert(miItemIndex < kmaxitems);
  DrawableBufItem& item = mDrawBufItems[miItemIndex];
  item.SetDrawable(d);
  item.mUserData0    = 0;
  item.mUserData1    = 0;
  item.mXfData       = xfdata;
  item.miBufferIndex = miBufferIndex;
  return item;
}

///////////////////////////////////////////////////////////////////////////////

DrawableBuffer::DrawableBuffer(int ibidx)
    : miBufferIndex(ibidx)
    , miNumLayersUsed(0) {}

DrawableBufLayer::DrawableBufLayer()
    : miItemIndex(-1)
    , miBufferIndex(-1) {}

///////////////////////////////////////////////////////////////////////////////

DrawableBuffer::~DrawableBuffer() {}

///////////////////////////////////////////////////////////////////////////////

const CameraData* DrawableBuffer::GetCameraData(int icam) const {
  int inumscenecameras = mCameraDataLUT.size();
  // printf( "NumSceneCameras<%d>\n", inumscenecameras );
  if (icam >= 0 && inumscenecameras) {
    icam                     = icam % inumscenecameras;
    auto& itCAM              = mCameraDataLUT.GetItemAtIndex(icam);
    const CameraData* pdata  = &itCAM.second;
    const lev2::Camera* pcam = pdata->getEditorCamera();
    // printf( "icam<%d> pdata<%p> pcam<%p>\n", icam, pdata, pcam );
    return pdata;
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

const CameraData* DrawableBuffer::GetCameraData(const PoolString& named) const {
  int inumscenecameras = mCameraDataLUT.size();
  auto itCAM           = mCameraDataLUT.find(named);
  if (itCAM != mCameraDataLUT.end()) {
    const CameraData* pdata = &itCAM->second;
    return pdata;
  }
  return 0;
}

/////////////////////////////////////////////////////////////////////
static concurrent_multi_buffer<DrawableBuffer, 2> gBuffers;
/////////////////////////////////////////////////////////////////////
const DrawableBuffer* DrawableBuffer::BeginDbRead(int lid) { return gBuffers.BeginRead(); }
/////////////////////
void DrawableBuffer::EndDbRead(const DrawableBuffer* db) { gBuffers.EndRead(db); }
/////////////////////////////////////////////////////////////////////
DrawableBuffer* DrawableBuffer::LockWriteBuffer(int lid) {
  AssertOnOpQ2(UpdateSerialOpQ());
  DrawableBuffer* wbuf = gBuffers.BeginWrite();
  return wbuf;
}
void DrawableBuffer::UnLockWriteBuffer(DrawableBuffer* db) {
  AssertOnOpQ2(UpdateSerialOpQ());
  gBuffers.EndWrite(db);
}
/////////////////////////////////////////////////////////////////////
// flush all renderer side data
//  sync until flushed
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::BeginClearAndSyncReaders() {
  AssertOnOpQ2(UpdateSerialOpQ());

  bool b = gbInsideClearAndSync.exchange(true);
  OrkAssert(b == false);
  // printf( "DrawableBuffer::BeginClearAndSyncReaders()\n");
  gBuffers.disable();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::EndClearAndSyncReaders() {
  AssertOnOpQ2(UpdateSerialOpQ());
  bool b = gbInsideClearAndSync.exchange(false);
  OrkAssert(b == true);
  ////////////////////
  // printf( "DrawableBuffer::EndClearAndSyncReaders()\n");
  gBuffers.enable();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::BeginClearAndSyncWriters() {
  // AssertOnOpQ2( UpdateSerialOpQ() );
  // printf( "DrawableBuffer::BeginClearAndSyncWriters()\n");
  gBuffers.disable();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::EndClearAndSyncWriters() {
  // AssertOnOpQ2( UpdateSerialOpQ() );
  ////////////////////
  // printf( "DrawableBuffer::EndClearAndSyncWriters()\n");
  gBuffers.enable();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::ClearAndSyncReaders() {
  AssertOnOpQ2(UpdateSerialOpQ());

  BeginClearAndSyncReaders();
  EndClearAndSyncReaders();
}
/////////////////////////////////////////////////////////////////////
void DrawableBuffer::ClearAndSyncWriters() {
  BeginClearAndSyncWriters();
  EndClearAndSyncWriters();
}
///////////////////////////////////////////////////////////////////////////////
Drawable::Drawable()
    : mDataA(nullptr)
    , mDataB(nullptr)
    , mEnabled(true) {
  AssertOnOpQ2(UpdateSerialOpQ());
  // printf( "Drawable<%p>::Drawable(Entity<%p>)\n", this, pent );
  fflush(stdout);
  // OrkAssert(mEntity);
}
Drawable::~Drawable() {
  AssertOnOpQ2(UpdateSerialOpQ());
  // printf( "Delete Drawable<%p>\n", this );
}

///////////////////////////////////////////////////////////////////////////////
ModelDrawable::ModelDrawable(Entity* pent)
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
  const lev2::XgmModel* Model         = mModelInst->GetXgmModel();
  bool IsSkinned                      = Model->IsSkinned();
  ork::lev2::XgmWorldPose* pworldpose = 0;
  if (IsSkinned) {
    mpWorldPose = new ork::lev2::XgmWorldPose(Model->RefSkel(), mModelInst->RefLocalPose());
  }
  Drawable::var_t ap;
  ap.Set(mpWorldPose);
  SetUserDataA(ap);
}

///////////////////////////////////////////////////////////////////////////////

void ModelDrawable::QueueToLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const {
  AssertOnOpQ2(UpdateSerialOpQ());

#if 1 // DRAWTHREADS
  const lev2::XgmModel* Model = mModelInst->GetXgmModel();
  bool IsSkinned              = Model->IsSkinned();

  DrawableBufItem& item = buffer.Queue(xfdata, this);

  // orkprintf( " ModelDrawable::QueueToBuffer() mdl<%p> IsSkinned<%d>\n", Model, int(IsSkinned) );

  if (IsSkinned) {
    ork::lev2::XgmWorldPose* pworldpose = GetUserDataA().Get<ork::lev2::XgmWorldPose*>();
    if (pworldpose) {
      const ork::lev2::XgmSkeleton& Skeleton = Model->RefSkel();

      const ork::lev2::XgmLocalPose& locpos = mModelInst->RefLocalPose();
      orkvector<fmtx4>& WorldMatrices       = pworldpose->GetMatrices();
      int inumch                            = locpos.NumJoints();
      for (int ich = 0; ich < inumch; ich++) {
        // orkprintf( " mdrwq2b setmtxblk ich<%d>\n", ich );
        const fmtx4& MatIBind    = Skeleton.RefInverseBindMatrix(ich);
        const fmtx4& MatJ        = Skeleton.RefJointMatrix(ich);
        const fmtx4& MatAnimJCat = locpos.RefLocalMatrix(ich);

        WorldMatrices[ich] = (MatIBind * MatAnimJCat);
      }
    }
  }
#endif
}

///////////////////////////////////////////////////////////////////////////////

void ModelDrawable::QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const {
#if 1 // DRAWTHREADS
  AssertOnOpQ2(MainThreadOpQ());
  const ork::lev2::RenderContextFrameData* fdata = renderer->GetTarget()->GetRenderContextFrameData();
  const lev2::XgmModel* Model                    = mModelInst->GetXgmModel();
  const CameraData* camdat                       = fdata->GetCameraData();
  OrkAssert(camdat != 0);

  const CameraCalcContext& ccctx = fdata->GetCameraCalcCtx();
  bool bvisicd                   = camdat->GetVisibilityCamDat() != 0;
  if (bvisicd) {
    camdat = camdat->GetVisibilityCamDat();
  }
  const Frustum& frus = bvisicd ? camdat->GetFrustum() : ccctx.mFrustum; // camdat->GetFrustum();

  const ork::fmtx4& matw = item.mXfData.mWorldMatrix;

  bool IsPickState = renderer->GetTarget()->FBI()->IsPickState();
  bool IsSkinned   = Model->IsSkinned();

  ork::fvec3 center_plus_offset = mOffset + Model->GetBoundingCenter();

  ork::fvec3 ctr = ork::fvec4(center_plus_offset * mfScale).Transform(matw);

  ork::fvec3 vwhd = Model->GetBoundingAA_WHD();
  float frad      = vwhd.GetX();
  if (vwhd.GetY() > frad)
    frad = vwhd.GetY();
  if (vwhd.GetZ() > frad)
    frad = vwhd.GetZ();
  frad *= 0.6f;

  bool bCenterInFrustum = frus.Contains(ctr);

  //////////////////////////////////////////////////////////////////////

  const ork::lev2::XgmWorldPose* pworldpose = GetUserDataA().Get<ork::lev2::XgmWorldPose*>();

  ork::fvec3 matw_trans;
  ork::fquat matw_rot;
  float matw_scale;

  matw.DecomposeMatrix(matw_trans, matw_rot, matw_scale);

  //////////////////////////////////////////////////////////////////////
  // generate coarse light mask

  ork::lev2::LightMask mdl_lmask;

  ork::lev2::LightManager* light_manager = fdata->GetLightManager();

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
  }

  //////////////////////////////////////////////////////////////////////

  int inumacc = 0;
  int inumrej = 0;

  int inummeshes = Model->GetNumMeshes();
  for (int imesh = 0; imesh < inummeshes; imesh++) {
    const lev2::XgmMesh& mesh = *Model->GetMesh(imesh);

    // if( 0 == strcmp(mesh.GetMeshName().c_str(),"fg_2_1_3_ground_SG_ground_GeoDaeId") )
    //{
    //	orkprintf( "yo\n" );
    //}

    if (mModelInst->IsMeshEnabled(imesh)) {
      int inumclusset = mesh.GetNumSubMeshes();

      for (int ics = 0; ics < inumclusset; ics++) {
        const lev2::XgmSubMesh& submesh   = *mesh.GetSubMesh(ics);
        const lev2::GfxMaterial* material = submesh.mpMaterial;

        int inumclus = submesh.miNumClusters;

        for (int ic = 0; ic < inumclus; ic++) {
          bool btest = true;

          const lev2::XgmCluster& cluster = submesh.RefCluster(ic);

          ork::lev2::LightMask lmask;

          if (IsSkinned) {
            lmask = mdl_lmask;

            float fdb = frus.mBottomPlane.GetPointDistance(ctr);
            float fdt = frus.mTopPlane.GetPointDistance(ctr);
            float fdl = frus.mLeftPlane.GetPointDistance(ctr);
            float fdr = frus.mRightPlane.GetPointDistance(ctr);
            float fdn = frus.mNearPlane.GetPointDistance(ctr);
            float fdf = frus.mFarPlane.GetPointDistance(ctr);

            const float kdist = -5.0f;
            btest             = (fdb > kdist) && (fdt > kdist) && (fdl > kdist) && (fdr > kdist) &&
                    (fdn > kdist)
                    //&&	(fdn<100.0f); // 50m actors
                    && (fdf > kdist);
            if (false == btest) {
            }
            btest = true; // todo fix culler
          } else {
            const Sphere& bsph = cluster.mBoundingSphere;

            float clussphrad = bsph.mRadius * matw_scale * mfScale;
            fvec3 clussphctr = ((bsph.mCenter + mOffset) * mfScale).Transform(matw);
            Sphere sph2(clussphctr, clussphrad);

            btest = true; // CollisionTester::FrustumSphereTest( frus, sph2 );

            ////////////////////////////////////////////////
            // per cluster light assignment
            ////////////////////////////////////////////////

            if (btest) {
              ////////////////////////////////////////////////
              // lighting sphere test
              ////////////////////////////////////////////////

              ork::fvec3 ctr = ork::fvec4(Model->GetBoundingCenter()).Transform(matw);
              for (size_t il = 0; il < inuml; il++) {
                ork::lev2::Light* plight = light_manager->mLightsInFrustum[il];
                OrkAssert(plight);
                bool baf = plight->AffectsSphere(sph2.mCenter, sph2.mRadius);
                if (baf) {
                  lmask.AddLight(plight);
                }
              }
            }
          }

          if (btest) {
            lev2::ModelRenderable& renderable = renderer->QueueModel();

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
            renderable.SetLightMask(lmask);
            renderable.SetScale(mfScale);
            renderable.SetRotate(mRotate);
            renderable.SetOffset(mOffset);
            renderable.SetWorldPose(pworldpose);

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
            // orkprintf( " ModelDrawable::QueueToRenderer() rable<%p> \n", & renderable );

            inumacc++;
          } else {
            inumrej++;
          }
        }
      }
    }
  }

#endif
  // orkprintf( "numacc %d numrej %d\n", inumacc, inumrej );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CallbackDrawable::CallbackDrawable(Entity* pent)
    : Drawable()
    , mSortKey(4)
    , mRenderCallback(0)
    , mQueueToLayerCallback(0)
    , mDataDestroyer(0) {}
///////////////////////////////////////////////////////////////////////////////
CallbackDrawable::~CallbackDrawable() {
  if (mDataDestroyer) {
    mDataDestroyer->Destroy();
  }
  mDataDestroyer        = 0;
  mQueueToLayerCallback = 0;
  mRenderCallback       = 0;
}
///////////////////////////////////////////////////////////////////////////////
// Multithreaded Renderer DB
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::QueueToLayer(const DrawQueueXfData& xfdata, DrawableBufLayer& buffer) const {
  AssertOnOpQ2(UpdateSerialOpQ());

  DrawableBufItem& cdb = buffer.Queue(xfdata, this);
  cdb.mUserData0       = GetUserDataA();
  if (mQueueToLayerCallback) {
    mQueueToLayerCallback(cdb);
  }
}
///////////////////////////////////////////////////////////////////////////////
//
///////////////////////////////////////////////////////////////////////////////
void CallbackDrawable::QueueToRenderer(const DrawableBufItem& item, lev2::Renderer* renderer) const {
  AssertOnOpQ2(MainThreadOpQ());

  lev2::CallbackRenderable& renderable = renderer->QueueCallback();
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
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} // namespace ork::ent
