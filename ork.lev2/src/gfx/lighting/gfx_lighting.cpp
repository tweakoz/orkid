////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/kernel/Array.hpp>
#include <ork/kernel/opq.h>
#include <ork/kernel/fixedlut.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/renderer/irendertarget.h>
#include <ork/math/collision_test.h>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::lev2::LightManagerData, "LightManagerData");
ImplementReflectionX(ork::lev2::LightData, "LightData");
ImplementReflectionX(ork::lev2::PointLightData, "PointLightData");
ImplementReflectionX(ork::lev2::DirectionalLightData, "DirectionalLightData");
ImplementReflectionX(ork::lev2::AmbientLightData, "AmbientLightData");
ImplementReflectionX(ork::lev2::SpotLightData, "SpotLightData");

///////////////////////////////////////////////////////////////////////////////
namespace ork {

template class fixedlut<float, lev2::Light*, lev2::GlobalLightContainer::kmaxlights>;
template class ork::fixedvector<std::pair<U32, lev2::LightingGroup*>, lev2::LightCollector::kmaxonscreengroups>;
template class ork::fixedvector<lev2::LightingGroup, lev2::LightCollector::kmaxonscreengroups>;

namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

bool Light::isShadowCaster() const {
  return _data->IsShadowCaster();
}
float Light::distance(fvec3 pos) const {
  return (worldPosition() - pos).magnitude();
}

///////////////////////////////////////////////////////////////////////////////

void LightData::describeX(class_t* c) {

  c->directProperty("Color", &LightData::mColor)->annotate<ConstString>("editor.type", "color");

  c->directProperty("Intensity", &LightData::_intensity)
      ->annotate<float>("editor.range.min", 0)
      ->annotate<float>("editor.range.max", 10000);

  c->directProperty("ShadowCaster", &LightData::mbShadowCaster);
  c->directProperty("Decal", &LightData::_decal);

  c->floatProperty("ShadowBias", float_range{0.0, 0.01}, &LightData::mShadowBias)
      ->annotate<ConstString>("editor.range.log", "true");
  c->floatProperty("ShadowBlur", float_range{0.0, 1.0}, &LightData::mShadowBlur);
  c->directProperty("ShadowMapSize", &LightData::_shadowMapSize)
      ->annotate<int>("editor.range.min", 128)
      ->annotate<int>("editor.range.max", 4096);
  c->directProperty("ShadowSamples", &LightData::_shadowsamples)
      ->annotate<int>("editor.range.min", 1)
      ->annotate<int>("editor.range.max", 16);

  /*c->directProperty(
       "Cookie",
       &LightData::_cookie) //
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");*/
}

LightData::LightData()
    : mColor(1.0f, 0.0f, 0.0f)
    , _intensity(1)
    , mbShadowCaster(false)
    , _shadowsamples(1)
    , mShadowBlur(0.0f)
    , mShadowBias(0.002f) {
}

lev2::texture_ptr_t LightData::cookie() const {
  auto as_tex = std::dynamic_pointer_cast<TextureAsset>(_cookie);
  return as_tex ? as_tex->GetTexture() : nullptr;
}

Light::Light(const LightData* ld)
    : _data(ld)
    , mPriority(0.0f)
    , _dynamic(false) {
  /*if(ld){
    _cookieTexture = ld->cookie();
  }*/

  _xformgenerator = [] -> fmtx4 { return fmtx4(); };
}

Light::Light(xform_generator_t mtx, const LightData* ld)
    : _data(ld)
    , _xformgenerator(mtx)
    , mPriority(0.0f)
    , _dynamic(false) {
  /*if(ld){
    _cookieTexture = ld->cookie();
  }*/
}
Light::~Light() {
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void PointLightData::describeX(class_t* c) {

  c->floatProperty("Radius", float_range{1, 3000}, &PointLightData::_radius)->annotate<ConstString>("editor.range.log", "true");
  c->floatProperty("Falloff", float_range{1, 10}, &PointLightData::_falloff)->annotate<ConstString>("editor.range.log", "true");
}

drawable_ptr_t PointLightData::createDrawable() const {
  return std::make_shared<PointLight>(this);
}

pointlightdata_ptr_t PointLightData::instantiate() {
  return std::make_shared<PointLightData>();
}

///////////////////////////////////////////////////////////////////////////////

PointLight::PointLight(xform_generator_t mtx, const PointLightData* pld)
    : Light(mtx, pld)
    , _pldata(pld) {
}

///////////////////////////////////////////////////////////////////////////////

PointLight::PointLight(const PointLightData* pld)
    : Light(pld)
    , _pldata(pld) {
}

///////////////////////////////////////////////////////////////////////////////

bool PointLight::IsInFrustum(const Frustum& frustum) {
  const fvec3& wpos = worldPosition();

  float fd = frustum._nearPlane.pointDistance(wpos);

  if (fd > 200.0f)
    return false;

  return CollisionTester::FrustumSphereTest(frustum, Sphere(worldPosition(), radius()));
}

///////////////////////////////////////////////////////////

bool PointLight::AffectsSphere(const fvec3& center, float radius_) {
  float dist          = (worldPosition() - center).magnitude();
  float combinedradii = (radius_ + radius());
  return (dist < combinedradii);
}

///////////////////////////////////////////////////////////////////////////////

bool PointLight::AffectsAABox(const AABox& aab) {
  return CollisionTester::SphereAABoxTest(Sphere(worldPosition(), radius()), aab);
}

///////////////////////////////////////////////////////////////////////////////

bool PointLight::AffectsCircleXZ(const Circle& cirXZ) {
  fvec3 center(cirXZ.mCenter.x, worldPosition().y, cirXZ.mCenter.y);
  float dist          = (worldPosition() - center).magnitude();
  float combinedradii = (cirXZ.mRadius + radius());
  return (dist < combinedradii);
}

///////////////////////////////////////////////////////////////////////////////

DynamicPointLight::DynamicPointLight()
    : PointLight(nullptr) {
  _inlineData = std::make_shared<PointLightData>();
  _data       = _inlineData.get();
  _pldata     = _inlineData.get();
}
DynamicDirectionalLight::DynamicDirectionalLight()
    : DirectionalLight(nullptr) {
  _inlineData = std::make_shared<DirectionalLightData>();
  _data       = _inlineData.get();
  _dldata     = _inlineData.get();
}
DynamicSpotLight::DynamicSpotLight()
    : SpotLight(nullptr) {
  _inlineData = std::make_shared<SpotLightData>();
  _data       = _inlineData.get();
  _spdata     = _inlineData.get();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

DirectionalLight::DirectionalLight(xform_generator_t mtx, const DirectionalLightData* dld)
    : Light(mtx, dld) {
}

///////////////////////////////////////////////////////////////////////////////

DirectionalLight::DirectionalLight(const DirectionalLightData* dld)
    : Light(dld)
    , _dldata(dld) {
}

///////////////////////////////////////////////////////////////////////////////

void DirectionalLightData::describeX(class_t* c) {
}

drawable_ptr_t DirectionalLightData::createDrawable() const {
  return std::make_shared<DirectionalLight>(this);
}

///////////////////////////////////////////////////////////////////////////////

bool DirectionalLight::IsInFrustum(const Frustum& frustum) {
  return true; // directional lights are unbounded, hence always true
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

AmbientLight::AmbientLight(xform_generator_t mtx, const AmbientLightData* ald)
    : Light(mtx, ald)
    , mAld(ald) {
}

///////////////////////////////////////////////////////////////////////////////

AmbientLight::AmbientLight(const AmbientLightData* ald)
    : Light(ald)
    , mAld(ald) {
}

///////////////////////////////////////////////////////////////////////////////

void AmbientLightData::describeX(class_t* c) {
  c->floatProperty("AmbientShade", float_range{0, 1}, &AmbientLightData::mfAmbientShade);
  c->directProperty("HeadlightDir", &AmbientLightData::mvHeadlightDir);
}

drawable_ptr_t AmbientLightData::createDrawable() const {
  return std::make_shared<AmbientLight>(this);
}

///////////////////////////////////////////////////////////////////////////////

bool AmbientLight::IsInFrustum(const Frustum& frustum) {
  return true; // ambient lights are unbounded, hence always true
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

void SpotLightData::describeX(class_t* c) {
  c->floatProperty("Fovy", float_range{0, 180}, &SpotLightData::mFovy);
  c->floatProperty("Range", float_range{1, 1000}, &SpotLightData::mRange) //
      ->annotate<bool>("editor.range.log", true);
}

SpotLightData::SpotLightData()
    : mFovy(10.0f)
    , mRange(1.0f) {
}

drawable_ptr_t SpotLightData::createDrawable() const {
  return std::make_shared<SpotLight>(this);
}

///////////////////////////////////////////////////////////////////////////////

SpotLight::SpotLight(const SpotLightData* sld)
    : Light(sld)
    , _spdata(sld)
    , _shadowmapDim(0) {
}

SpotLight::SpotLight(xform_generator_t mtx, const SpotLightData* sld)
    : Light(mtx, sld)
    , _spdata(sld)
    , _shadowmapDim(0) {
}

float SpotLight::getFovy() const {
  return _spdata->GetFovy();
}
float SpotLight::getRange() const {
  return _spdata->GetRange();
}

///////////////////////////////////////////////////////////////////////////////

RtGroupRenderTarget* SpotLight::rendertarget(Context* ctx) {
  if (nullptr == _shadowIRT or (_spdata->shadowMapSize() != _shadowmapDim)) {
    _shadowmapDim         = _spdata->shadowMapSize();
    MsaaSamples msaasamps = intToMsaaEnum(_spdata->shadowSamples());
    _shadowRTG            = new RtGroup(ctx, _shadowmapDim, _shadowmapDim, msaasamps);
    _shadowIRT            = new RtGroupRenderTarget(_shadowRTG);
  }
  return _shadowIRT;
}

///////////////////////////////////////////////////////////////////////////////

bool SpotLight::IsInFrustum(const Frustum& frustum) {
  const auto& mtx = worldMatrix();
  fvec3 pos       = mtx.translation();
  fvec3 tgt       = pos + mtx.zNormal() * getRange();
  fvec3 up        = mtx.yNormal();
  float fovy      = 15.0f;

  // set(pos, tgt, up, fovy);

  return false; // CollisionTester::FrustumFrustumTest( frustum, mWorldSpaceLightFrustum );
}

///////////////////////////////////////////////////////////

void SpotLight::lookAt(const fvec3& pos, const fvec3& tgt, const fvec3& up) {
  float near = getRange() / 1000.0f;
  float far  = getRange();
  float fovy = getFovy();
  mProjectionMatrix.perspective(fovy, 1.0, near, far);
  mViewMatrix.lookAt(pos.x, pos.y, pos.z, tgt.x, tgt.y, tgt.z, up.x, up.y, up.z);
  mWorldSpaceLightFrustum.set(mViewMatrix, mProjectionMatrix);
  opq::assertOnQueue(opq::mainSerialQueue());
  _xformgenerator = [this, pos]() -> fmtx4 {
    fmtx4 rval;
    rval.setTranslation(pos);
    return rval;
  };
}

///////////////////////////////////////////////////////////

bool SpotLight::AffectsSphere(const fvec3& center, float radius) {
  return CollisionTester::FrustumSphereTest(mWorldSpaceLightFrustum, Sphere(center, radius));
}

///////////////////////////////////////////////////////////////////////////////

bool SpotLight::AffectsAABox(const AABox& aab) {
  return CollisionTester::Frustu_aaBoxTest(mWorldSpaceLightFrustum, aab);
}

///////////////////////////////////////////////////////////////////////////////

bool SpotLight::AffectsCircleXZ(const Circle& cirXZ) {
  return CollisionTester::FrustumCircleXZTest(mWorldSpaceLightFrustum, cirXZ);
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 SpotLight::shadowMatrix() const {
  return mProjectionMatrix * mViewMatrix;
}

///////////////////////////////////////////////////////////////////////////////

CameraData SpotLight::shadowCamDat() const {
  CameraData rval;
  fmtx4 matW   = worldMatrix();
  float fovy   = getFovy();
  float range  = getRange();
  float near   = range / 1000.0f;
  float far    = range;
  float aspect = 1.0;
  fvec3 wnx, wny, wnz, wpos;
  matW.toNormalVectors(wnx, wny, wnz);
  wpos      = matW.translation();
  fvec3 ctr = wpos + wnz * 0.01;

  rval.mEye       = wpos;
  rval.mTarget    = ctr;
  rval.mUp        = wny;
  rval._xnormal   = wnx;
  rval._ynormal   = wny;
  rval._znormal   = wnz;
  rval.mAper      = fovy;
  rval.mHorizAper = fovy;
  rval.mNear      = near;
  rval.mFar       = far;

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightContainer::AddLight(Light* plight) {
  float priority = plight->mPriority;
  _prioritizedLights[priority].insert(plight);
}

void LightContainer::RemoveLight(Light* plight) {
  auto it = _prioritizedLights.find(plight->mPriority);
  if (it != _prioritizedLights.end()) {
    it->second.erase(plight);
  }
}

LightContainer::LightContainer() {
}

void LightContainer::Clear() {
  _prioritizedLights.clear();
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

void LightManagerData::describeX(class_t* c) {
}

///////////////////////////////////////////////////////////////////////////////

void LightManager::enumerateInPass(const CompositingPassData& CPD, enumeratedlights_ptr_t out_lights) const {
  out_lights->_alllights.clear();
  ////////////////////////////////////////////////////////////
  for (GlobalLightContainer::map_type::const_iterator it = mGlobalStationaryLights.mPrioritizedLights.begin();
       it != mGlobalStationaryLights.mPrioritizedLights.end();
       it++) {
    Light* plight = it->second;

    if (true) { // plight->IsInFrustum(frustum)) {
      size_t idx = out_lights->_alllights.size();

      plight->miInFrustumID = 1 << idx;
      out_lights->_alllights.push_back(plight);
    } else {
      plight->miInFrustumID = -1;
    }
  }
  ////////////////////////////////////////////////////////////
  for (auto pri_item : mGlobalMovingLights._prioritizedLights) {
    for (auto item : pri_item.second) {
      Light* plight = item;
      if (true) { // plight->IsInFrustum(frustum)) {
        size_t idx = out_lights->_alllights.size();

        plight->miInFrustumID = 1 << idx;
        out_lights->_alllights.push_back(plight);
      } else {
        plight->miInFrustumID = -1;
      }
    }
  }

  ////////////////////////////////////////////////////////////
  // categorize
  ////////////////////////////////////////////////////////////

  out_lights->_lightprobes = _lightprobes;

  out_lights->_untexturedpointlights.clear();
  out_lights->_untexturedspotlights.clear();
  out_lights->_tex2pointlightmap.clear();
  out_lights->_tex2spotlightmap.clear();
  out_lights->_tex2spotdecalmap.clear();
  out_lights->_tex2shadowedspotlightmap.clear();

  for (auto l : out_lights->_alllights) {
    if (l->isShadowCaster()) {
      if (auto as_spot = dynamic_cast<lev2::SpotLight*>(l)) {
        auto cookie = as_spot->_cookieColor;
        if (cookie) {
          out_lights->_tex2shadowedspotlightmap[cookie].push_back(as_spot);
          OrkAssert(false);
        }
      }
    } else if (auto as_point = dynamic_cast<lev2::PointLight*>(l)) {
      // auto cookie = as_point->_cookieTexture;
      // if (cookie)
      // out_lights->_tex2pointlightmap[cookie.get()].push_back(as_point);
      // else
      out_lights->_untexturedpointlights.push_back(as_point);
    } else if (auto as_spot = dynamic_cast<lev2::SpotLight*>(l)) {
      auto cookie = as_spot->_cookieColor;
      bool decal  = as_spot->decal();
      if (decal) {
        if (cookie) {
          out_lights->_tex2spotdecalmap[cookie].push_back(as_spot);
        }
        OrkAssert(false);
      } else {
        if (cookie) {
          out_lights->_tex2spotlightmap[cookie].push_back(as_spot);
        } else {
          out_lights->_untexturedspotlights.push_back(as_spot);
        }
      }
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

void LightManager::gpuInit(Context* ctx) {
  if(_needs_gpu_init){
    printf("LightManager::gpuInit this=%p color_array=%p depth_array=%p\n",
          (void*)this, (void*)_cookies_spot_color_default.get(), (void*)_cookies_spot_depth_default.get());
    ctx->TXI()->updateTextureArray(_cookies_spot_color_default.get());
    ctx->TXI()->updateTextureArray(_cookies_spot_depth_default.get());
    _needs_gpu_init = false;
  }
  else {
    printf("LightManager::gpuInit this=%p already initialized\n", (void*)this);
  }
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
    : _manager(nullptr)
    , mLightMap(0)
    , mDPEnvMap(0) {
}

///////////////////////////////////////////////////////////////////////////////

LightManager::LightManager(lightmanagerdata_constptr_t lmd)
    : _data(lmd) {
  _cookies_spot_color_default                   = std::make_shared<TextureArray>();
  _cookies_spot_depth_default                   = std::make_shared<TextureArray>();
  _cookies_spot_color_default->_tex->_debugName = "cookies_spot_color";
  _cookies_spot_depth_default->_tex->_debugName = "cookies_spot_depth";

  _cookies_spot_color_default->resize(256, 256, 1, EBufferFormat::RGBA8);
  _cookies_spot_depth_default->resize(256, 256, 1, EBufferFormat::Z32F);

  // Set depth texture to use clamp-to-edge for proper shadow mapping
  _cookies_spot_depth_default->_tex->mTexSampleMode._texAddrModeS = TextureAddressMode::CLAMP;
  _cookies_spot_depth_default->_tex->mTexSampleMode._texAddrModeT = TextureAddressMode::CLAMP;
  _cookies_spot_depth_default->_tex->mTexSampleMode._texAddrModeR = TextureAddressMode::CLAMP;

  // Color cookies might also benefit from clamping to avoid wrapping artifacts
  _cookies_spot_color_default->_tex->mTexSampleMode._texAddrModeS = TextureAddressMode::CLAMP;
  _cookies_spot_color_default->_tex->mTexSampleMode._texAddrModeT = TextureAddressMode::CLAMP;
  _cookies_spot_color_default->_tex->mTexSampleMode._texAddrModeR = TextureAddressMode::CLAMP;

  _cookies_spot_color = _cookies_spot_color_default;
  _cookies_spot_depth = _cookies_spot_depth_default;
}

///////////////////////////////////////////////////////////////////////////////

HeadLightManager::HeadLightManager(RenderContextFrameData& FrameData)
    : mHeadLight([this]() -> fmtx4 { return mHeadLightMatrix; }, &mHeadLightData) {

  _managerdata = std::make_shared<LightManagerData>();
  _manager     = std::make_shared<LightManager>(_managerdata);
  auto cdata   = FrameData.topCPD().cameraMatrices();
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
    mHeadLightManager._alllights.push_back(& mHeadLight);*/
}

///////////////////////////////////////////////////////////////////////////////

void LightManager::bindEnumeratedToStorageBuffer( Context* ctx,                             //
                                                  enumeratedlights_ptr_t enumerated_lights, //
                                                  FxShaderStorageBuffer* ssbo ) const {            //
  constexpr size_t kmaxlights = 64; // must match storage_interface storage_fwd_lighting array size
  auto FXI = ctx->FXI();
  ///////////////////////////////////////////////////////////////////////////
  // build lighting UBO
  ///////////////////////////////////////////////////////////////////////////

  auto pl_mapped = FXI->mapStorageBuffer(ssbo, 0, ssbo->_length);

  size_t i32_stride  = sizeof(int32_t);
  size_t f32_stride  = sizeof(float);
  size_t vec4_stride = sizeof(fvec4);
  size_t mat4_stride = sizeof(fmtx4);

  size_t base_color      = 0;
  size_t base_sizbias    = base_color + vec4_stride * kmaxlights;
  size_t base_position   = base_sizbias + vec4_stride * kmaxlights;
  size_t base_shmtx      = base_position + vec4_stride * kmaxlights;
  size_t base_lighttexid = base_shmtx + mat4_stride * kmaxlights;

  if (0) {
    printf("base_color<%zu>\n", base_color);
    printf("base_sizbias<%zu>\n", base_sizbias);
    printf("base_position<%zu>\n", base_position);
    printf("base_shmtx<%zu>\n", base_shmtx);
  }
  // 16*(16+16+8) = 16*40 = 640

  //////////////////////////////////////////
  // Untextured point lights
  //////////////////////////////////////////

  size_t index = 0;
  for (auto light : enumerated_lights->_untexturedpointlights) {
    auto C                                                    = fvec4(light->color(), light->intensity());
    auto P                                                    = light->worldPosition();
    float R                                                   = light->radius();
    size_t v4_offset                                          = index * vec4_stride;
    pl_mapped->ref<fvec4>(base_color + v4_offset)             = C;
    pl_mapped->ref<fvec4>(base_sizbias + v4_offset)           = fvec4(R, 0, 0, 1);
    pl_mapped->ref<fvec4>(base_position + v4_offset)          = P;
    pl_mapped->ref<fmtx4>(base_shmtx + (index * mat4_stride)) = fmtx4();
    index++;
  }
  enumerated_lights->_num_active_untextured_pointlights = enumerated_lights->_untexturedpointlights.size(); 

  //////////////////////////////////////////
  // Textured spot lights
  //////////////////////////////////////////

  for (int i = 0; i < kmaxlights; i++) {
    pl_mapped->ref<uint32_t>(base_lighttexid + (i * i32_stride)) = i;
  }

  //////////////////////////////////////////
  // Textured spot lights
  //////////////////////////////////////////

  enumerated_lights->_num_active_texspotlights = 0;
  for (auto item : enumerated_lights->_tex2spotlightmap) {
    for (auto light : item.second) {
      auto irr = light->_RadianceCookie;

      auto C    = fvec4(light->color(), light->intensity());
      auto P    = light->worldMatrix().translation();
      float R   = light->_spdata->GetRange();
      float B   = light->shadowDepthBias();
      float SMS = light->_spdata->shadowMapSize();

      if (0) {
        printf("C<%zu> <%g %g %g %g>\n", index, C.x, C.y, C.z, C.w);
        printf("P<%zu> <%g %g %g>\n", index, P.x, P.y, P.z);
        printf("R<%zu> <%f> B<%f> SMS<%f>\n", index, R, B, SMS);
      }

      size_t v4_offset                                          = index * vec4_stride;
      pl_mapped->ref<fvec4>(base_color + v4_offset)             = C;
      pl_mapped->ref<fvec4>(base_sizbias + v4_offset)           = fvec4(R, B, SMS, 1);
      pl_mapped->ref<fvec4>(base_position + v4_offset)          = P;
      pl_mapped->ref<fmtx4>(base_shmtx + (index * mat4_stride)) = light->shadowMatrix();
      size_t texid_addr                                         = base_lighttexid + (index * i32_stride);
      // printf( "TEXID ADDR<%zu> ID<%d>\n", tex_addr, num_texspotlights );

      int cookie_index = light->_cookieColor->_slice;
      // cookie_index = rand() % 8;
      pl_mapped->ref<uint32_t>(texid_addr) = uint32_t(cookie_index);
      index++;
      enumerated_lights->_num_active_texspotlights++;
    }
  }
  OrkAssert(index <= kmaxlights);
  // printf( "texlistsize<%d>\n", texlist.size() );
  pl_mapped->unmap();

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

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
