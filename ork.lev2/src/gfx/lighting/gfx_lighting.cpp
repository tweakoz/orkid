////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/kernel/fixedlut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/math/collision_test.h>
#include <ork/reflect/RegisterProperty.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::Light, "Light");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::PointLight, "PointLight");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::DirectionalLight, "DirectionalLight");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AmbientLight, "AmbientLight");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::SpotLight, "SpotLight");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::LightManagerData, "LightManagerData");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::LightData, "LightData");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::PointLightData, "PointLightData");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::DirectionalLightData, "DirectionalLightData");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::AmbientLightData, "AmbientLightData");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::SpotLightData, "SpotLightData");

///////////////////////////////////////////////////////////////////////////////
namespace ork {

template class fixedlut<float, lev2::Light*, lev2::LightContainer::kmaxlights>;
template class fixedlut<float, lev2::Light*, lev2::GlobalLightContainer::kmaxlights>;
template class ork::fixedvector<std::pair<U32, lev2::LightingGroup*>, lev2::LightCollector::kmaxonscreengroups>;
template class ork::fixedvector<lev2::LightingGroup, lev2::LightCollector::kmaxonscreengroups>;

namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

void Light::Describe() {
}

bool Light::isShadowCaster() const {
  return mLd->IsShadowCaster();
}

///////////////////////////////////////////////////////////////////////////////

void LightData::Describe() {
  ork::reflect::RegisterProperty("Color", &LightData::mColor);
  ork::reflect::RegisterProperty("AffectsSpecular", &LightData::mbSpecular);
  ork::reflect::RegisterProperty("ShadowSamples", &LightData::mShadowSamples);
  ork::reflect::RegisterProperty("ShadowBlur", &LightData::mShadowBlur);
  ork::reflect::RegisterProperty("ShadowBias", &LightData::mShadowBias);
  ork::reflect::RegisterProperty("ShadowCaster", &LightData::mbShadowCaster);

  ork::reflect::annotatePropertyForEditor<LightData>("ShadowBias", "editor.range.min", "0.0");
  ork::reflect::annotatePropertyForEditor<LightData>("ShadowBias", "editor.range.max", "2.0");
  ork::reflect::annotatePropertyForEditor<LightData>("ShadowBias", "editor.range.log", "true");
  ork::reflect::annotatePropertyForEditor<LightData>("ShadowSamples", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<LightData>("ShadowSamples", "editor.range.max", "256.0");
  ork::reflect::annotatePropertyForEditor<LightData>("ShadowBlur", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<LightData>("ShadowBlur", "editor.range.max", "1.0");
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void PointLight::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

void PointLightData::Describe() {
  ork::reflect::RegisterProperty("Radius", &PointLightData::_radius);
  ork::reflect::annotatePropertyForEditor<PointLightData>("Radius", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<PointLightData>("Radius", "editor.range.max", "3000.00");
  ork::reflect::annotatePropertyForEditor<PointLightData>("Radius", "editor.range.log", "true");

  ork::reflect::RegisterProperty("Falloff", &PointLightData::_falloff);
  ork::reflect::annotatePropertyForEditor<PointLightData>("Falloff", "editor.range.min", "0");
  ork::reflect::annotatePropertyForEditor<PointLightData>("Falloff", "editor.range.max", "10.00");
  ork::reflect::annotatePropertyForEditor<PointLightData>("Falloff", "editor.range.log", "true");
}

///////////////////////////////////////////////////////////////////////////////

PointLight::PointLight(const fmtx4& mtx, const PointLightData* pld)
    : Light(mtx, pld)
    , mPld(pld) {
}

///////////////////////////////////////////////////////////////////////////////

bool PointLight::IsInFrustum(const Frustum& frustum) {
  const fvec3& wpos = GetWorldPosition();

  float fd = frustum.mNearPlane.GetPointDistance(wpos);

  if (fd > 200.0f)
    return false;

  return CollisionTester::FrustumSphereTest(frustum, Sphere(GetWorldPosition(), GetRadius()));
}

///////////////////////////////////////////////////////////

bool PointLight::AffectsSphere(const fvec3& center, float radius) {
  float dist          = (GetWorldPosition() - center).Mag();
  float combinedradii = (radius + GetRadius());

  //	orkprintf( "PointLight::AffectsSphere point<%f %f %f> center<%f %f %f>\n",
  //				mWorldPosition.GetX(), mWorldPosition.GetY(), mWorldPosition.GetZ(),
  //				center.GetX(), center.GetY(), center.GetZ() );

  // float crsq = combinedradii; //(combinedradii*combinedradii);
  return (dist < combinedradii);
}

///////////////////////////////////////////////////////////////////////////////

bool PointLight::AffectsAABox(const AABox& aab) {
  return CollisionTester::SphereAABoxTest(Sphere(GetWorldPosition(), GetRadius()), aab);
}

///////////////////////////////////////////////////////////////////////////////

bool PointLight::AffectsCircleXZ(const Circle& cirXZ) {
  fvec3 center(cirXZ.mCenter.GetX(), GetWorldPosition().GetY(), cirXZ.mCenter.GetY());
  float dist          = (GetWorldPosition() - center).Mag();
  float combinedradii = (cirXZ.mRadius + GetRadius());

  //	orkprintf( "PointLight::AffectsSphere point<%f %f %f> center<%f %f %f>\n",
  //				mWorldPosition.GetX(), mWorldPosition.GetY(), mWorldPosition.GetZ(),
  //				center.GetX(), center.GetY(), center.GetZ() );

  // float crsq = combinedradii; //(combinedradii*combinedradii);
  return (dist < combinedradii);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void DirectionalLight::Describe() {
}

DirectionalLight::DirectionalLight(const fmtx4& mtx, const DirectionalLightData* dld)
    : Light(mtx, dld) {
}

///////////////////////////////////////////////////////////////////////////////

void DirectionalLightData::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

bool DirectionalLight::IsInFrustum(const Frustum& frustum) {
  return true; // directional lights are unbounded, hence always true
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void AmbientLight::Describe() {
}

AmbientLight::AmbientLight(const fmtx4& mtx, const AmbientLightData* dld)
    : Light(mtx, dld)
    , mAld(dld) {
}

///////////////////////////////////////////////////////////////////////////////

void AmbientLightData::Describe() {
  ork::reflect::RegisterProperty("AmbientShade", &AmbientLightData::mfAmbientShade);
  ork::reflect::annotatePropertyForEditor<AmbientLightData>("AmbientShade", "editor.range.min", "0.0");
  ork::reflect::annotatePropertyForEditor<AmbientLightData>("AmbientShade", "editor.range.max", "1.0");
  ork::reflect::RegisterProperty("HeadlightDir", &AmbientLightData::mvHeadlightDir);
}

///////////////////////////////////////////////////////////////////////////////

bool AmbientLight::IsInFrustum(const Frustum& frustum) {
  return true; // ambient lights are unbounded, hence always true
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void SpotLight::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

void SpotLightData::Describe() {
  ork::reflect::RegisterProperty("Fovy", &SpotLightData::mFovy);
  ork::reflect::RegisterProperty("Range", &SpotLightData::mRange);
  ork::reflect::RegisterProperty("Texture", &SpotLightData::GetTextureAccessor, &SpotLightData::SetTextureAccessor);

  ork::reflect::annotatePropertyForEditor<SpotLightData>("Texture", "editor.class", "ged.factory.assetlist");
  ork::reflect::annotatePropertyForEditor<SpotLightData>("Texture", "editor.assettype", "lev2tex");

  ork::reflect::annotatePropertyForEditor<SpotLightData>("Fovy", "editor.range.min", "0.0");
  ork::reflect::annotatePropertyForEditor<SpotLightData>("Fovy", "editor.range.max", "180.0");

  ork::reflect::annotatePropertyForEditor<SpotLightData>("Range", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<SpotLightData>("Range", "editor.range.max", "1000.00");
  ork::reflect::annotatePropertyForEditor<SpotLightData>("Range", "editor.range.log", "true");
}

///////////////////////////////////////////////////////////////////////////////

void SpotLightData::SetTextureAccessor(ork::rtti::ICastable* const& tex) {
  mTexture = tex ? ork::rtti::autocast(tex) : 0;
}

///////////////////////////////////////////////////////////////////////////////

void SpotLightData::GetTextureAccessor(ork::rtti::ICastable*& tex) const {
  tex = mTexture;
}

///////////////////////////////////////////////////////////////////////////////

SpotLight::SpotLight(const fmtx4& mtx, const SpotLightData* sld)
    : Light(mtx, sld)
    , mSld(sld)
    , mTexture(0) {
}

///////////////////////////////////////////////////////////////////////////////

bool SpotLight::IsInFrustum(const Frustum& frustum) {
  fvec3 pos  = GetMatrix().GetTranslation();
  fvec3 tgt  = pos + GetMatrix().GetZNormal() * GetRange();
  fvec3 up   = GetMatrix().GetYNormal();
  float fovy = 15.0f;

  Set(pos, tgt, up, fovy);

  return false; // CollisionTester::FrustumFrustumTest( frustum, mWorldSpaceLightFrustum );
}

///////////////////////////////////////////////////////////

void SpotLight::Set(const fvec3& pos, const fvec3& tgt, const fvec3& up, float fovy) {
  // mFovy = fovy;

  // mWorldSpaceDirection = (tgt-pos);

  // mRange = mWorldSpaceDirection.Mag();

  // mWorldSpaceDirection.Normalize();

  mProjectionMatrix.Perspective(GetFovy(), 1.0, GetRange() / float(1000.0f), GetRange());
  mViewMatrix.LookAt(pos.GetX(), pos.GetY(), pos.GetZ(), tgt.GetX(), tgt.GetY(), tgt.GetZ(), up.GetX(), up.GetY(), up.GetZ());
  // mFovy = fovy;

  mWorldSpaceLightFrustum.Set(mViewMatrix, mProjectionMatrix);

  // SetPosition( pos );
}

///////////////////////////////////////////////////////////

bool SpotLight::AffectsSphere(const fvec3& center, float radius) {
  return CollisionTester::FrustumSphereTest(mWorldSpaceLightFrustum, Sphere(center, radius));
}

///////////////////////////////////////////////////////////////////////////////

bool SpotLight::AffectsAABox(const AABox& aab) {
  return CollisionTester::FrustumAABoxTest(mWorldSpaceLightFrustum, aab);
}

///////////////////////////////////////////////////////////////////////////////

bool SpotLight::AffectsCircleXZ(const Circle& cirXZ) {
  return CollisionTester::FrustumCircleXZTest(mWorldSpaceLightFrustum, cirXZ);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightContainer::AddLight(Light* plight) {
  if (mPrioritizedLights.size() < map_type::kimax) {
    mPrioritizedLights.AddSorted(plight->mPriority, plight);
  }
}

void LightContainer::RemoveLight(Light* plight) {
  map_type::iterator it = mPrioritizedLights.find(plight->mPriority);
  if (it != mPrioritizedLights.end()) {
    mPrioritizedLights.erase(it);
  }
}

LightContainer::LightContainer()
    : mPrioritizedLights(EKEYPOLICY_MULTILUT) {
}

void LightContainer::Clear() {
  mPrioritizedLights.clear();
}

///////////////////////////////////////////////////////////////////////////////

void GlobalLightContainer::AddLight(Light* plight) {
  if (mPrioritizedLights.size() < map_type::kimax) {
    mPrioritizedLights.AddSorted(plight->mPriority, plight);
  }
}

void GlobalLightContainer::RemoveLight(Light* plight) {
  map_type::iterator it = mPrioritizedLights.find(plight->mPriority);
  if (it != mPrioritizedLights.end()) {
    mPrioritizedLights.erase(it);
  }
}

GlobalLightContainer::GlobalLightContainer()
    : mPrioritizedLights(EKEYPOLICY_MULTILUT) {
}

void GlobalLightContainer::Clear() {
  mPrioritizedLights.clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

// const LightingGroup& LightCollector::GetActiveGroup( int idx ) const
//{
//	return mGroups[ idx ];
//}

size_t LightCollector::GetNumGroups() const {
  return mGroups.size();
}

void LightCollector::SetManager(LightManager* mgr) {
  mManager = mgr;
}

void LightCollector::Clear() {
  mGroups.clear();
  /*for( int i=0; i<kmaxonscreengroups; i++ )
  {
      mGroups[i].mLightManager = mManager;
      mGroups[i].mInstances.clear();
  }*/
  mActiveMap.clear();
}

LightCollector::LightCollector()
    : mManager(0) {
  // for( int i=0; i<kmaxonscreengroups; i++ )
  //{
  //	mGroups[i].mLightMask.SetMask(i);
  //}
}

LightCollector::~LightCollector() {
  static size_t imax = 0;

  size_t isize = mActiveMap.size();

  if (isize > imax) {
    imax = isize;
  }

  /// printf( "lc maxgroups<%d>\n", imax );
}

void LightCollector::QueueInstance(const LightMask& lmask, const fmtx4& mtx) {
  U32 uval = lmask.mMask;

  ActiveMapType::const_iterator it = mActiveMap.find(uval);

  if (it != mActiveMap.end()) {
    LightingGroup* pgrp = it->second;

    pgrp->mInstances.push_back(mtx);
  } else {
    size_t index        = mGroups.size();
    LightingGroup* pgrp = mGroups.allocate();

    pgrp->mLightMask.SetMask(uval);
    mActiveMap.AddSorted(uval, pgrp);

    pgrp->mInstances.push_back(mtx);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightManagerData::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

void LightManager::enumerateInPass(const CompositingPassData& CPD, EnumeratedLights& out) const {
  out._enumeratedLights.clear();
  ////////////////////////////////////////////////////////////
  for (GlobalLightContainer::map_type::const_iterator it = mGlobalStationaryLights.mPrioritizedLights.begin();
       it != mGlobalStationaryLights.mPrioritizedLights.end();
       it++) {
    Light* plight = it->second;

    if (true) { // plight->IsInFrustum(frustum)) {
      size_t idx = out._enumeratedLights.size();

      plight->miInFrustumID = 1 << idx;
      out._enumeratedLights.push_back(plight);
    } else {
      plight->miInFrustumID = -1;
    }
  }
  ////////////////////////////////////////////////////////////
  for (LightContainer::map_type::const_iterator it = mGlobalMovingLights.mPrioritizedLights.begin();
       it != mGlobalMovingLights.mPrioritizedLights.end();
       it++) {
    Light* plight = it->second;

    if (true) { // plight->IsInFrustum(frustum)) {
      size_t idx = out._enumeratedLights.size();

      plight->miInFrustumID = 1 << idx;
      out._enumeratedLights.push_back(plight);
    } else {
      plight->miInFrustumID = -1;
    }
  }

  ////////////////////////////////////////////////////////////
  // mcollector.SetManager(this);
  // mcollector.Clear();
}

///////////////////////////////////////////////////////////

void LightManager::QueueInstance(const LightMask& lmask, const fmtx4& mtx) {
  mcollector.QueueInstance(lmask, mtx);
}

///////////////////////////////////////////////////////////

size_t LightManager::GetNumLightGroups() const {
  return mcollector.GetNumGroups();
}

///////////////////////////////////////////////////////////

void LightManager::Clear() {
  // mGlobalStationaryLights.Clear();
  mGlobalMovingLights.Clear();

  mcollector.Clear();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightMask::AddLight(const Light* plight) {
  mMask |= plight->miInFrustumID;
}

///////////////////////////////////////////////////////////

size_t LightingGroup::GetNumLights() const {
  return size_t(countbits(mLightMask.mMask));
}

///////////////////////////////////////////////////////////

size_t LightingGroup::GetNumMatrices() const {
  return mInstances.size();
}

///////////////////////////////////////////////////////////

const fmtx4* LightingGroup::GetMatrices() const {
  return &mInstances[0];
}

///////////////////////////////////////////////////////////////////////////////

int LightingGroup::GetLightId(int idx) const {
  U32 mask = mLightMask.mMask;

  int ilightid = -1;

  U32 umask = 1;
  for (int b = 0; b < 32; b++) {
    if (mask & umask) {
      ilightid = b;
      idx--;
      if (0 == idx)
        return ilightid;
    }
    umask <<= 1;
  }

  return ilightid;
}

///////////////////////////////////////////////////////////////////////////////

LightingGroup::LightingGroup()
    : mLightManager(0)
    , mLightMap(0)
    , mDPEnvMap(0) {
}

///////////////////////////////////////////////////////////////////////////////

HeadLightManager::HeadLightManager(RenderContextFrameData& FrameData)
    : mHeadLight(mHeadLightMatrix, &mHeadLightData)
    , mHeadLightManager(mHeadLightManagerData) {
  auto cdata = FrameData.topCPD().cameraMatrices();
  /*
  auto camvd = cdata->computeMatrices();
    ork::fvec3 vZ = cdata->xNormal();
    ork::fvec3 vY = cdata->yNormal();
    ork::fvec3 vP = cdata->GetFrustum().mNearCorners[0];
    mHeadLightMatrix = cdata->GetIVMatrix();
    mHeadLightData.SetColor( fvec3(1.3f,1.3f,1.5f) );
    mHeadLightData.SetAmbientShade(0.757f);
    mHeadLightData.SetHeadlightDir(fvec3(0.0f,0.5f,1.0f));
    mHeadLight.miInFrustumID = 1;
    mHeadLightGroup.mLightMask.AddLight( & mHeadLight );
    mHeadLightGroup.mLightManager = FrameData.GetLightManager();
    mHeadLightMatrix.SetTranslation( vP );
    mHeadLightManager.mGlobalMovingLights.AddLight( & mHeadLight );
    mHeadLightManager._enumeratedLights.push_back(& mHeadLight);*/
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

LightingFxInterface::LightingFxInterface()
    : mbHasLightingInterface(false)
    , mpShader(0)
    , hAmbientLight(0)
    , hNumDirectionalLights(0)
    , hDirectionalLightDirs(0)
    , hDirectionalLightColors(0)
    , hDirectionalLightPos(0)
    , hDirectionalAttenA(0)
    , hDirectionalAttenK(0)
    , hLightMode(0) {
  mCurrentLightingGroup = 0;
}

void LightingFxInterface::ApplyLighting(Context* pTarg, int iPass) {
  /*
  ////////////////////////////////
  if (false == mbHasLightingInterface) {
    return;
  }
  if (nullptr == mpShader) {
    return;
  }

  const RenderContextInstData* rdata   = pTarg->GetRenderContextInstData();
  const RenderContextFrameData* rfdata = pTarg->topRenderContextFrameData();
  auto cammatrices                     = rfdata->topCPD().cameraMatrices();

  const lev2::LightingGroup* lgroup = rdata->GetLightingGroup();

  if (lgroup) {
    static int gLightStateChanged    = 0;
    static int gLightStateNotChanged = 0;

    if (lgroup != mCurrentLightingGroup) {
      gLightStateChanged++;

      static const int kmaxl = 8;

      static fvec4 DirLightColors[kmaxl];
      static fvec4 DirLightDirs[kmaxl];
      static fvec4 DirLightPos[kmaxl];
      static fvec4 DirLightAtnA[kmaxl];
      static fvec4 DirLightAtnK[kmaxl];
      static float LightMode[kmaxl];

      int inumdl = 0;

      int inuml = (int)lgroup->GetNumLights();

      inuml = std::min(inuml, kmaxl);

      for (int il = 0; il < kmaxl; il++) {
        LightMode[il] = 0.0f;
      }

      for (int il = 0; il < inuml; il++) {
        int ilightid = lgroup->GetLightId(il);

        Light* plight = lgroup->mLightManager->_enumeratedLights[ilightid];

        fvec3 LightColor = plight->GetColor();
        // float fimag = 1.0f;
        // if( (LightColor.GetX()<0.0f) || (LightColor.GetY()<0.0f) || (LightColor.GetZ()<0.0f) )
        //{
        //	fimag = -1.0f;
        //}
        // float fmag = LightColor.Mag();
        // LightColor = LightColor*(1.0f/fmag);
        // fmag *= fimag;

        if ((kmaxl - inumdl) > 0)
          switch (plight->LightType()) {
            case lev2::ELIGHTTYPE_SPOT: {
              auto pspotlight = (lev2::SpotLight*)plight;

              DirLightDirs[inumdl]   = pspotlight->GetDirection();
              DirLightColors[inumdl] = LightColor;
              DirLightPos[inumdl]    = pspotlight->GetWorldPosition();

              float frang = pspotlight->GetRange();
              float fovy  = pspotlight->GetFovy() * DTOR;

              DirLightAtnA[inumdl] = fvec3(0.0f, 0.0f, fovy);
              DirLightAtnK[inumdl] = fvec3(0.0f, 0.0f, 10.0f / frang);
              inumdl++;
              break;
            }
            case lev2::ELIGHTTYPE_POINT: {

              auto ppointlight = (lev2::PointLight*)plight;

              DirLightDirs[inumdl]   = ppointlight->GetDirection();
              DirLightColors[inumdl] = LightColor;
              DirLightPos[inumdl]    = ppointlight->GetWorldPosition();

              float frang   = ppointlight->GetRadius();
              float falloff = ppointlight->GetFalloff();

              // float flin = 2.0ffalloff

              DirLightAtnA[inumdl] = fvec3(0.0f, 1.0f, 0.0f);
              DirLightAtnK[inumdl] = fvec3(1.0f, 0.0f, falloff);

              LightMode[inumdl] = 1.0f;

              inumdl++;

              break;
            }
            case lev2::ELIGHTTYPE_DIRECTIONAL: {
              auto pdirlight = (lev2::DirectionalLight*)plight;

              DirLightDirs[inumdl]   = pdirlight->GetDirection();
              DirLightColors[inumdl] = LightColor;
              DirLightPos[inumdl]    = pdirlight->GetWorldPosition();

              DirLightAtnA[inumdl] = fvec3(0.0f, 1.0f, 0.0f);
              DirLightAtnK[inumdl] = fvec3(1.0f, 0.0f, 0.0f);

              inumdl++;
              break;
            }
            case lev2::ELIGHTTYPE_AMBIENT: {
              auto phedlight          = (lev2::AmbientLight*)plight;
              const ork::fmtx4& mativ = pTarg->MTXI()->RefVITGMatrix();

              const auto& camdat = cammatrices->_camdat;

              ork::fvec4 veye = ork::fvec4(camdat.GetEye());

              auto hldir = phedlight->GetHeadlightDir();

              ork::fvec3 vzdir = hldir.x * camdat.xNormal() + hldir.y * camdat.yNormal() + hldir.z * camdat.zNormal();

              vzdir.Normalize();

              DirLightDirs[inumdl]   = vzdir;
              DirLightColors[inumdl] = LightColor;
              DirLightPos[inumdl]    = veye;

              ork::fvec4 vzn = vzdir;
              vzn.SetW(0.0f);
              fvec3 viewz = vzn.Transform(pTarg->MTXI()->RefVMatrix());

              float fambientshade = phedlight->GetAmbientShade();
              float fa0           = (1.0f - fambientshade);
              float fa1           = fambientshade;

              // orkprintf( "veye<%f %f %f> veyev<%f %f %f>\n", veye.GetX(), veye.GetY(), veye.GetZ(), veyev.GetX(), veyev.GetY(),
              // veyev.GetZ() );
              DirLightAtnA[inumdl] = fvec3(fa0, fa1, 0.0f);
              DirLightAtnK[inumdl] = fvec3(1.0f, 0.0f, 0.0f);

              inumdl++;
              break;
            }
          }
      }

      pTarg->FXI()->BindParamInt(mpShader, hNumDirectionalLights, inumdl);
      pTarg->FXI()->BindParamVect4Array(mpShader, hDirectionalLightColors, DirLightColors, inumdl);
      pTarg->FXI()->BindParamVect4Array(mpShader, hDirectionalLightDirs, DirLightDirs, inumdl);
      pTarg->FXI()->BindParamVect4Array(mpShader, hDirectionalLightPos, DirLightPos, inumdl);
      pTarg->FXI()->BindParamVect4Array(mpShader, hDirectionalAttenA, DirLightAtnA, inumdl);
      pTarg->FXI()->BindParamVect4Array(mpShader, hDirectionalAttenK, DirLightAtnK, inumdl);
      pTarg->FXI()->BindParamFloatArray(mpShader, hLightMode, LightMode, inumdl);
    } else {
      gLightStateNotChanged++;
    }
  } else {
    pTarg->FXI()->BindParamInt(mpShader, hNumDirectionalLights, 0);
  }
  */
}

void LightingFxInterface::Init(FxShader* pshader) {
  mpShader = pshader;

  FxInterface* pfxi       = GfxEnv::GetRef().GetLoaderTarget()->FXI();
  hAmbientLight           = pfxi->parameter(pshader, "AmbientLight");
  hNumDirectionalLights   = pfxi->parameter(pshader, "NumDirectionalLights");
  hDirectionalLightDirs   = pfxi->parameter(pshader, "DirectionalLightDir");
  hDirectionalLightColors = pfxi->parameter(pshader, "DirectionalLightColor");
  hDirectionalLightPos    = pfxi->parameter(pshader, "DirectionalLightPos");
  hDirectionalAttenA      = pfxi->parameter(pshader, "DirectionalAttenA");
  hDirectionalAttenK      = pfxi->parameter(pshader, "DirectionalAttenK");
  hLightMode              = pfxi->parameter(pshader, "LightMode");

  mbHasLightingInterface = true;

  if (0 == hAmbientLight)
    mbHasLightingInterface = false;
  if (0 == hNumDirectionalLights)
    mbHasLightingInterface = false;
  if (0 == hDirectionalLightDirs)
    mbHasLightingInterface = false;
  if (0 == hDirectionalLightColors)
    mbHasLightingInterface = false;
  if (0 == hDirectionalLightPos)
    mbHasLightingInterface = false;
  if (0 == hDirectionalAttenA)
    mbHasLightingInterface = false;
  if (0 == hDirectionalAttenK)
    mbHasLightingInterface = false;
  if (0 == hLightMode)
    mbHasLightingInterface = false;
}

/////////////////////////////////////
/////////////////////////////////////
// global lighting alg
/////////////////////////////////////
/////////////////////////////////////
// .	enumerate set of ONSCREEN static lights via scenegraph
// .	enumerate set of ONSCREEN dynamic lights via scenegraph
// .	cull light sets based on a policy (example - no more than 64 lights on screen at once)
//////////////
// .	when queing a renderable, identify its statically (precomputed) linked lights
// .    determine which dynamic lights affect the renderable
// .	cull the renderables lights based on a policy (example - use the 3 lights with the highest priority)
// .	map the renderable's culled light set to a specific lighting group, creating the group if necessary
// .        some of these light groups could potentially be precomputed (if no dynamics allowed in the group)
//////////////
// OR
// .   for each renderable in renderqueue
// .       bind lightgroup (with redundant state change checking)
// .	   render the renderable
//////////////

} // namespace lev2
} // namespace ork
