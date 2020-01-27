////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
///////////////////////////////////////////////////////////////////////////////

#include <ork/math/plane.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/box.h>
#include <ork/math/sphere.h>
#include <ork/math/frustum.h>
#include <ork/kernel/orklut.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/Array.h>

#include <ork/config/config.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {

// class TextureAsset;
// class Texture;
class RenderContextFrameData;
class FxShader;
class FxShaderParam;
class FxShaderParamBlock;
class Context;
class CompositingPassData;

///////////////////////////////////////////////////////////////////////////////

inline int countbits(U32 v) {
  v     = v - ((v >> 1) & 0x55555555);                      // reuse input as temporary
  v     = (v & 0x33333333) + ((v >> 2) & 0x33333333);       // temp
  int c = (((v + (v >> 4) & 0xF0F0F0F) * 0x1010101) >> 24); // count
  return c;
}

///////////////////////////////////////////////////////////////////////////////

enum ELightType {
  ELIGHTTYPE_DIRECTIONAL = 0,
  ELIGHTTYPE_SPOT,
  ELIGHTTYPE_POINT,
  ELIGHTTYPE_AMBIENT,
};

///////////////////////////////////////////////////////////////////////////////

class LightData : public ork::Object {
  RttiDeclareAbstract(LightData, ork::Object);

  fvec3 mColor;
  bool mbShadowCaster;
  float mShadowSamples;
  float mShadowBlur;
  float mShadowBias;
  bool mbSpecular;

public:
  float GetShadowBias() const {
    return mShadowBias;
  }
  float GetShadowSamples() const {
    return mShadowSamples;
  }
  float GetShadowBlur() const {
    return mShadowBlur;
  }
  bool GetSpecular() const {
    return mbSpecular;
  }
  bool IsShadowCaster() const {
    return mbShadowCaster;
  }

  const fvec3& GetColor() const {
    return mColor;
  }
  void SetColor(const fvec3& clr) {
    mColor = clr;
  }

  LightData()
      : mColor(1.0f, 0.0f, 0.0f)
      , mbSpecular(false)
      , mbShadowCaster(false)
      , mShadowSamples(1.0f)
      , mShadowBlur(0.0f)
      , mShadowBias(0.2f) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct Light {

  Light(const fmtx4& mtx, const LightData* ld = 0)
      : _worldMatrix(mtx)
      , _data(ld)
      , _dynamic(false)
      , mPriority(0.0f) {
  }
  virtual ~Light() {
  }

  bool isShadowCaster() const;
  virtual bool IsInFrustum(const Frustum& frustum)              = 0;
  virtual bool AffectsSphere(const fvec3& center, float radius) = 0;
  virtual bool AffectsAABox(const AABox& aab)                   = 0;
  virtual bool AffectsCircleXZ(const Circle& cir)               = 0;
  virtual ELightType LightType() const                          = 0;

  const fvec3& color() const {
    return _data->GetColor();
  }
  const fmtx4& worldMatrix() const {
    return _worldMatrix;
  }
  fvec3 worldPosition() const {
    return _worldMatrix.GetTranslation();
  }
  fvec3 direction() const {
    return _worldMatrix.GetZNormal();
  }

  const LightData* _data;
  const fmtx4& _worldMatrix;

  float mPriority;
  int miInFrustumID;
  bool _dynamic;
};

///////////////////////////////////////////////////////////////////////////////

struct PointLightData : public LightData {
  DeclareConcreteX(PointLightData, LightData);

public:
  float _radius;
  float _falloff;
  textureassetptr_t _equirectTexture = nullptr;

public:
  float radius() const {
    return _radius;
  }
  float falloff() const {
    return _falloff;
  }

  PointLightData()
      : _radius(1.0f)
      , _falloff(1.0f)
      , _equirectTexture(nullptr) {
  }

private:
  void _readEquiMap(ork::rtti::ICastable*& model) const;
  void _writeEquiMap(ork::rtti::ICastable* const& model);
};

///////////////////////////////////////////////////////////////////////////////

struct PointLight : public Light {

  bool IsInFrustum(const Frustum& frustum) override;
  bool AffectsSphere(const fvec3& center, float radius) override;
  bool AffectsAABox(const AABox& aab) override;
  bool AffectsCircleXZ(const Circle& cir) override;
  ELightType LightType() const override {
    return ELIGHTTYPE_POINT;
  }

  float falloff() const {
    return _pldata->falloff();
  }
  float radius() const {
    return _pldata->radius();
  }
  Texture* texture() const {
    return _pldata->_equirectTexture ? _pldata->_equirectTexture->GetTexture() : nullptr;
  }

  PointLight(const fmtx4& mtx, const PointLightData* pld = 0);

  const PointLightData* _pldata;
};

///////////////////////////////////////////////////////////////////////////////

class DirectionalLightData : public LightData {
  RttiDeclareConcrete(DirectionalLightData, LightData);

public:
  DirectionalLightData() {
  }
};

///////////////////////////////////////////////////////////////////////////////

class DirectionalLight : public Light {

  const DirectionalLightData* mDld;

public:
  bool IsInFrustum(const Frustum& frustum) override;
  bool AffectsSphere(const fvec3& center, float radius) override {
    return true;
  }
  bool AffectsCircleXZ(const Circle& cir) override {
    return true;
  }
  bool AffectsAABox(const AABox& aab) override {
    return true;
  }
  ELightType LightType() const override {
    return ELIGHTTYPE_DIRECTIONAL;
  }

  DirectionalLight(const fmtx4& mtx, const DirectionalLightData* dld = 0);
};

///////////////////////////////////////////////////////////////////////////////

class AmbientLightData : public LightData {
  RttiDeclareConcrete(AmbientLightData, LightData);

  float mfAmbientShade;
  fvec3 mvHeadlightDir;

public:
  AmbientLightData()
      : mfAmbientShade(0.0f)
      , mvHeadlightDir(0.0f, 0.5f, 1.0f) {
  }
  float GetAmbientShade() const {
    return mfAmbientShade;
  }
  void SetAmbientShade(float fv) {
    mfAmbientShade = fv;
  }
  const fvec3& GetHeadlightDir() const {
    return mvHeadlightDir;
  }
  void SetHeadlightDir(const fvec3& dir) {
    mvHeadlightDir = dir;
  }
};

///////////////////////////////////////////////////////////////////////////////

class AmbientLight : public Light {

  const AmbientLightData* mAld;

public:
  bool IsInFrustum(const Frustum& frustum) override;
  bool AffectsSphere(const fvec3& center, float radius) override {
    return true;
  }
  bool AffectsCircleXZ(const Circle& cir) override {
    return true;
  }
  bool AffectsAABox(const AABox& aab) override {
    return true;
  }
  ELightType LightType() const override {
    return ELIGHTTYPE_AMBIENT;
  }
  float GetAmbientShade() const {
    return mAld->GetAmbientShade();
  }
  const fvec3& GetHeadlightDir() const {
    return mAld->GetHeadlightDir();
  }

  AmbientLight(const fmtx4& mtx, const AmbientLightData* dld = 0);
};

///////////////////////////////////////////////////////////////////////////////

class SpotLightData : public LightData {
  RttiDeclareConcrete(SpotLightData, LightData);

  float mFovy;
  float mRange;
  lev2::TextureAsset* mTexture;

  void SetTextureAccessor(ork::rtti::ICastable* const& tex);
  void GetTextureAccessor(ork::rtti::ICastable*& tex) const;

public:
  float GetFovy() const {
    return mFovy;
  }
  float GetRange() const {
    return mRange;
  }

  SpotLightData()
      : mFovy(10.0f)
      , mRange(1.0f)
      , mTexture(0) {
  }
};

///////////////////////////////////////////////////////////////////////////////

class SpotLight : public Light {

  const SpotLightData* mSld;

public:
  fmtx4 mProjectionMatrix;
  fmtx4 mViewMatrix;
  Frustum mWorldSpaceLightFrustum;
  // float			mFovy;
  // float			mRange;
  lev2::TextureAsset* mTexture;

  bool IsInFrustum(const Frustum& frustum) override;
  bool AffectsSphere(const fvec3& center, float radius) override;
  bool AffectsAABox(const AABox& aab) override;
  bool AffectsCircleXZ(const Circle& cir) override;
  ELightType LightType() const override {
    return ELIGHTTYPE_SPOT;
  }

  void Set(const fvec3& pos, const fvec3& target, const fvec3& up, float fovy);

  void SetTexture(lev2::TextureAsset* ptex) {
    mTexture = ptex;
  }
  lev2::TextureAsset* GetTexture() const {
    return mTexture;
  }

  float GetFovy() const {
    return mSld->GetFovy();
  }
  float GetRange() const {
    return mSld->GetRange();
  }

  SpotLight(const fmtx4& mtx, const SpotLightData* sld = 0);
};

///////////////////////////////////////////////////////////////////////////////

struct LightContainer {
  static const int kmaxlights = 8;

  typedef fixedlut<float, Light*, kmaxlights> map_type;

  map_type mPrioritizedLights;

  void AddLight(Light* plight);
  void RemoveLight(Light* plight);

  LightContainer();
  void Clear();
};

struct GlobalLightContainer {
  static const int kmaxlights = 256;

  typedef fixedlut<float, Light*, kmaxlights> map_type;

  map_type mPrioritizedLights;

  void AddLight(Light* plight);
  void RemoveLight(Light* plight);

  GlobalLightContainer();
  void Clear();
};

///////////////////////////////////////////////////////////////////////////////

struct LightMask {
  U32 mMask;

  LightMask()
      : mMask(0) {
  }

  void SetMask(U32 mask) {
    mMask = mask;
  }
  void AddLight(const Light* plight);
  int GetNumLights() const {
    return countbits(mMask);
  }
};

///////////////////////////////////////////////////////////////////////////////

class LightManager;

struct LightingGroup {
  static const int kmaxinst = 32;

  LightMask mLightMask;
  ork::fixedvector<fmtx4, kmaxinst> mInstances;
  LightManager* mLightManager;
  Texture* mLightMap;
  Texture* mDPEnvMap;

  size_t GetNumLights() const;
  size_t GetNumMatrices() const;
  const fmtx4* GetMatrices() const;
  int GetLightId(int idx) const;

  LightingGroup();
};

///////////////////////////////////////////////////////////////////////////////

class LightManagerData : public ork::Object {
  RttiDeclareConcrete(LightManagerData, ork::Object);

public:
};

///////////////////////////////////////////////////////////////////////////////

class LightCollector {
public:
  static const int kmaxonscreengroups = 32;
  static const int kmaxflagwords      = kmaxonscreengroups >> 5;

private:
  // typedef fixedmap<U32,LightingGroup*,kmaxonscreengroups>	ActiveMapType;
  // typedef orklut< U32,LightingGroup*, allocator_fixedpool< std::pair<U32,LightingGroup*>,kmaxonscreengroups > >	ActiveMapType;
  typedef ork::fixedlut<U32, LightingGroup*, kmaxonscreengroups> ActiveMapType;

  fixed_pool<LightingGroup, kmaxonscreengroups> mGroups;
  ActiveMapType mActiveMap;

  LightManager* mManager;

public:
  // const LightingGroup& GetActiveGroup( int idx ) const;
  size_t GetNumGroups() const;
  void SetManager(LightManager* mgr);
  void Clear();
  LightCollector();
  ~LightCollector();
  void QueueInstance(const LightMask& lmask, const fmtx4& mtx);
};

///////////////////////////////////////////////////////////////////////////////

struct EnumeratedLights {
  std::vector<Light*> _enumeratedLights;
};

///////////////////////////////////////////////////////////////////////////////

class LightManager {
  const LightManagerData& mLmd;

  LightCollector mcollector;

public:
  LightManager(const LightManagerData& lmd)
      : mLmd(lmd) {
  }

  GlobalLightContainer mGlobalStationaryLights; // non-moving, potentially animating color or texture (and => not lightmappable)
  LightContainer mGlobalMovingLights;           // moving lights

  void enumerateInPass(const CompositingPassData& CPD, EnumeratedLights& out) const;

  void QueueInstance(const LightMask& lgid, const fmtx4& mtx);

  size_t GetNumLightGroups() const;
  void Clear();
};

struct HeadLightManager {
  ork::fmtx4 mHeadLightMatrix;
  LightingGroup mHeadLightGroup;
  AmbientLightData mHeadLightData;
  AmbientLight mHeadLight;
  LightManagerData mHeadLightManagerData;
  LightManager mHeadLightManager;

  HeadLightManager(RenderContextFrameData& FrameData);
};

/*
///////////////////////
// usage scenario
///////////////////////

Drawables will be preattached to any statically bound lights,
however some lights are dynamically created, destroyed and moved



///////////////////////
///////////////////////
*/

}} // namespace ork::lev2
