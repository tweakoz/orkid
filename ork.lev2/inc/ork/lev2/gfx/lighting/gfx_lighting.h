////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/renderer/renderable.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
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

struct LightData : public DrawableData {
  DeclareAbstractX(LightData, DrawableData);

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

  lev2::texture_ptr_t cookie() const;

  bool decal() const {
    return _decal;
  }

  int shadowMapSize() const {
    return _shadowMapSize;
  }

  fvec3 mColor;
  float _intensity = 1.0f;

  bool mbShadowCaster;
  int _shadowsamples;
  float mShadowBlur;
  float mShadowBias;
  asset::asset_ptr_t _cookie;
  bool _decal        = false;
  int _shadowMapSize = 1024;
};

///////////////////////////////////////////////////////////////////////////////

typedef std::function<fmtx4()> xform_generator_t;

struct Light : public Drawable {

  Light(const LightData* ld);
  Light(xform_generator_t mtx, const LightData* ld = 0);
  virtual ~Light();

  bool isShadowCaster() const;
  virtual bool IsInFrustum(const Frustum& frustum)              = 0;
  virtual bool AffectsSphere(const fvec3& center, float radius) = 0;
  virtual bool AffectsAABox(const AABox& aab)                   = 0;
  virtual bool AffectsCircleXZ(const Circle& cir)               = 0;
  virtual ELightType LightType() const                          = 0;

  float intensity() const {
    return _data->_intensity;
  }
  const fvec3& color() const {
    return _data->GetColor();
  }
  fmtx4 worldMatrix() const {
    return _xformgenerator();
  }
  fvec3 worldPosition() const {
    return worldMatrix().translation();
  }
  fvec3 direction() const {
    return worldMatrix().zNormal();
  }
  float distance(fvec3 pos) const;
  texture_ptr_t cookie() const {
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
  bool _dynamic = false;
  bool _castsShadows = false;

  texture_ptr_t _cookieTexture;
  rtgroup_ptr_t _depthRTG;
  texture_ptr_t _depthTexture;
  pbr::irradiancemaps_ptr_t _irradianceCookie;
};

///////////////////////////////////////////////////////////////////////////////

enum class LightProbeType {
  REFLECTION,
  SH_IRRADIANCE,
  END
};

struct LightProbe {

  LightProbe();
  ~LightProbe();
  void resize(int dim);

  LightProbeType _type = LightProbeType::REFLECTION;
  int _dim = 0;
  bool _dirty = true;
  uint64_t _version = 0;
  std::string _name;
  fmtx4 _worldMatrix; // +y up, right handed
  rtgroup_ptr_t _cubeRenderRTG;
  texture_ptr_t _cubeTexture;
  varmap::varmap_ptr_t _userdata;
};

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

  static pointlightdata_ptr_t instantiate();

  drawable_ptr_t createDrawable() const final;

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

  PointLight(const PointLightData* pld);
  PointLight(xform_generator_t mtx, const PointLightData* pld = 0);

  const PointLightData* _pldata;
};

struct DynamicPointLight : public PointLight {

  DynamicPointLight();

  pointlightdata_ptr_t _inlineData;

};


///////////////////////////////////////////////////////////////////////////////

struct DirectionalLightData : public LightData {
  DeclareConcreteX(DirectionalLightData, LightData);

public:
  DirectionalLightData() {
  }

  drawable_ptr_t createDrawable() const final;

};

///////////////////////////////////////////////////////////////////////////////

struct DirectionalLight : public Light {

  const DirectionalLightData* _dldata;

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

  DirectionalLight(const DirectionalLightData* pld);
  DirectionalLight(xform_generator_t mtx, const DirectionalLightData* dld = 0);
};

using directionallight_ptr_t = std::shared_ptr<DirectionalLight>;

struct DynamicDirectionalLight : public DirectionalLight {

  DynamicDirectionalLight();

  directionallightdata_ptr_t _inlineData;

};

///////////////////////////////////////////////////////////////////////////////

struct AmbientLightData : public LightData {
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

  drawable_ptr_t createDrawable() const final;

};

///////////////////////////////////////////////////////////////////////////////

struct AmbientLight : public Light {

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

  AmbientLight(const AmbientLightData* pld);
  AmbientLight(xform_generator_t mtx, const AmbientLightData* dld = 0);
};

///////////////////////////////////////////////////////////////////////////////

struct SpotLightData : public LightData {
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

  drawable_ptr_t createDrawable() const final;

};


///////////////////////////////////////////////////////////////////////////////

struct SpotLight : public Light {

public:
  bool IsInFrustum(const Frustum& frustum) override;
  bool AffectsSphere(const fvec3& center, float radius) override;
  bool AffectsAABox(const AABox& aab) override;
  bool AffectsCircleXZ(const Circle& cir) override;
  ELightType LightType() const override {
    return ELIGHTTYPE_SPOT;
  }

  void lookAt(const fvec3& pos, const fvec3& target, const fvec3& up);

  float getFovy() const;
  float getRange() const;

  RtGroupRenderTarget* rendertarget(Context* ctx);
  fmtx4 shadowMatrix() const;
  CameraData shadowCamDat() const;

  SpotLight(const SpotLightData* pld);
  SpotLight(xform_generator_t mtx, const SpotLightData* sld = 0);

  RtGroup* _shadowRTG             = nullptr;
  RtGroupRenderTarget* _shadowIRT = nullptr;
  const SpotLightData* _spdata       = nullptr;

  float _fovy = 0.0f;
  fmtx4 mProjectionMatrix;
  fmtx4 mViewMatrix;
  Frustum mWorldSpaceLightFrustum;
  int _shadowmapDim;
};

struct DynamicSpotLight : public SpotLight {

  DynamicSpotLight();

  spotlightdata_ptr_t _inlineData;

};


///////////////////////////////////////////////////////////////////////////////

struct LightContainer {

  using light_list_t = std::unordered_set<Light*>;

  using map_type = std::unordered_map<float, light_list_t> ;

  map_type _prioritizedLights;

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

struct LightingGroup {
  static const int kmaxinst = 32;

  LightMask mLightMask;
  ork::fixedvector<fmtx4, kmaxinst> mInstances;
  lightmanager_ptr_t _manager;
  Texture* mLightMap;
  Texture* mDPEnvMap;

  size_t GetNumLights() const;
  size_t GetNumMatrices() const;
  const fmtx4* GetMatrices() const;
  int GetLightId(int idx) const;

  LightingGroup();
};

///////////////////////////////////////////////////////////////////////////////

struct LightManagerData : public ork::Object {
  DeclareConcreteX(LightManagerData, ork::Object);

public:
};

using lightmanagerdata_ptr_t = std::shared_ptr<LightManagerData>;

///////////////////////////////////////////////////////////////////////////////

struct LightCollector {
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

struct EnumeratedLights;

using enumeratedlights_ptr_t = std::shared_ptr<EnumeratedLights>;
using enumeratedlights_constptr_t = std::shared_ptr<const EnumeratedLights>;
using pointlightlist_t    = std::vector<PointLight*>;
using spotlightlist_t     = std::vector<SpotLight*>;
using tex2pointlightmap_t = std::map<Texture*, pointlightlist_t>;
using tex2spotlightmap_t  = std::map<Texture*, spotlightlist_t>;
using lightprobeset_t = std::vector<lightprobe_ptr_t>;

struct EnumeratedLights {
  std::vector<Light*> _alllights;
  pointlightlist_t _untexturedpointlights;
  tex2pointlightmap_t _tex2pointlightmap;
  spotlightlist_t _untexturedspotlights;
  tex2spotlightmap_t _tex2spotlightmap;
  tex2spotlightmap_t _tex2shadowedspotlightmap;
  tex2spotlightmap_t _tex2spotdecalmap;
  lightprobeset_t _lightprobes;
};

///////////////////////////////////////////////////////////////////////////////

struct LightManager {

  LightCollector mcollector;

public:
  LightManager(lightmanagerdata_constptr_t lmd)
      : _data(lmd) {
  }

  GlobalLightContainer mGlobalStationaryLights; // non-moving, potentially animating color or texture (and => not lightmappable)
  LightContainer mGlobalMovingLights;           // moving lights
  lightprobeset_t _lightprobes;

  void enumerateInPass(const CompositingPassData& CPD, enumeratedlights_ptr_t out_lights) const;

  void QueueInstance(const LightMask& lgid, const fmtx4& mtx);

  size_t GetNumLightGroups() const;
  void Clear();

  lightmanagerdata_constptr_t _data;

};

using lightmanager_ptr_t = std::shared_ptr<LightManager>;

struct HeadLightManager {
  ork::fmtx4 mHeadLightMatrix;
  LightingGroup mHeadLightGroup;
  AmbientLightData mHeadLightData;
  AmbientLight mHeadLight;
  lightmanagerdata_ptr_t _managerdata;
  lightmanager_ptr_t _manager;

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
