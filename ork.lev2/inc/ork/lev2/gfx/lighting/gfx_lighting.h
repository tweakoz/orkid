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
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {

class RtGroupRenderTarget;
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

struct LightData : public ork::Object {
  DeclareAbstractX(LightData, ork::Object);

public:
  float GetShadowBias() const {
    return mShadowBias;
  }
  int shadowSamples() const {
    return _shadowsamples;
  }
  float GetShadowBlur() const {
    return mShadowBlur;
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

  LightData();

  lev2::Texture* cookie() const;

  bool decal() const {
    return _decal;
  }

  int shadowMapSize() const {
    return _shadowMapSize;
  }

  fvec3 mColor;
  bool mbShadowCaster;
  int _shadowsamples;
  float mShadowBlur;
  float mShadowBias;
  asset::asset_ptr_t _cookie;
  bool _decal        = false;
  int _shadowMapSize = 1024;
};
using lightdata_ptr_t      = std::shared_ptr<LightData>;
using lightdata_constptr_t = std::shared_ptr<const LightData>;

///////////////////////////////////////////////////////////////////////////////

typedef std::function<fmtx4()> xform_generator_t;

struct Light {

  Light(xform_generator_t mtx, const LightData* ld = 0)
      : _xformgenerator(mtx)
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
  fmtx4 worldMatrix() const {
    return _xformgenerator();
  }
  fvec3 worldPosition() const {
    return worldMatrix().GetTranslation();
  }
  fvec3 direction() const {
    return worldMatrix().GetZNormal();
  }
  float distance(fvec3 pos) const;
  Texture* cookie() const {
    return _data->cookie();
  }
  bool decal() const {
    return _data->decal();
  }
  float shadowDepthBias() const {
    return _data->GetShadowBias();
  }
  const LightData* _data;
  xform_generator_t _xformgenerator;

  float mPriority;
  int miInFrustumID;
  bool _dynamic;
};

using light_ptr_t      = std::shared_ptr<Light>;
using light_constptr_t = std::shared_ptr<const Light>;

///////////////////////////////////////////////////////////////////////////////

struct PointLightData : public LightData {
  DeclareConcreteX(PointLightData, LightData);

public:
  float radius() const {
    return _radius;
  }
  float falloff() const {
    return _falloff;
  }

  float _radius;
  float _falloff;

  PointLightData()
      : _radius(1.0f)
      , _falloff(1.0f) {
  }
};

using pointlightdata_ptr_t      = std::shared_ptr<PointLightData>;
using pointlightdata_constptr_t = std::shared_ptr<const PointLightData>;

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

  PointLight(xform_generator_t mtx, const PointLightData* pld = 0);

  const PointLightData* _pldata;
};

using pointlight_ptr_t      = std::shared_ptr<PointLight>;
using pointlight_constptr_t = std::shared_ptr<const PointLight>;

///////////////////////////////////////////////////////////////////////////////

class DirectionalLightData : public LightData {
  DeclareConcreteX(DirectionalLightData, LightData);

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

  DirectionalLight(xform_generator_t mtx, const DirectionalLightData* dld = 0);
};

///////////////////////////////////////////////////////////////////////////////

class AmbientLightData : public LightData {
  DeclareConcreteX(AmbientLightData, LightData);

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

  AmbientLight(xform_generator_t mtx, const AmbientLightData* dld = 0);
};

///////////////////////////////////////////////////////////////////////////////

class SpotLightData : public LightData {
  DeclareConcreteX(SpotLightData, LightData);

  float mFovy;
  float mRange;

public:
  float GetFovy() const {
    return mFovy;
  }
  float GetRange() const {
    return mRange;
  }

  SpotLightData();
};

using spotlightdata_ptr_t      = std::shared_ptr<SpotLightData>;
using spotlightdata_constptr_t = std::shared_ptr<const SpotLightData>;

///////////////////////////////////////////////////////////////////////////////

class SpotLight : public Light {

public:
  bool IsInFrustum(const Frustum& frustum) override;
  bool AffectsSphere(const fvec3& center, float radius) override;
  bool AffectsAABox(const AABox& aab) override;
  bool AffectsCircleXZ(const Circle& cir) override;
  ELightType LightType() const override {
    return ELIGHTTYPE_SPOT;
  }

  void set(const fvec3& pos, const fvec3& target, const fvec3& up, float fovy);

  float GetFovy() const {
    return _SLD->GetFovy();
  }
  float GetRange() const {
    return _SLD->GetRange();
  }

  RtGroupRenderTarget* rendertarget(Context* ctx);
  fmtx4 shadowMatrix() const;
  CameraData shadowCamDat() const;

  SpotLight(xform_generator_t mtx, const SpotLightData* sld = 0);

  fmtx4 mProjectionMatrix;
  fmtx4 mViewMatrix;
  Frustum mWorldSpaceLightFrustum;
  RtGroup* _shadowRTG;
  RtGroupRenderTarget* _shadowIRT;
  int _shadowmapDim;
  const SpotLightData* _SLD;
};

using spotlight_ptr_t      = std::shared_ptr<SpotLight>;
using spotlight_constptr_t = std::shared_ptr<const SpotLight>;

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
  DeclareConcreteX(LightManagerData, ork::Object);

public:
};

using lightmanagerdata_ptr_t = std::shared_ptr<LightManagerData>;

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

using lightmanager_ptr_t = std::shared_ptr<LightManager>;

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
