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
#include <ork/kernel/orkpool.inl>
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

struct EnumeratedLights;
struct PointLight;
struct SpotLight;
struct LightManagerData;
struct LightManager;
struct ProbeSHProjector;
using probeshprojector_ptr_t = std::shared_ptr<ProbeSHProjector>;

using enumeratedlights_ptr_t = std::shared_ptr<EnumeratedLights>;
using enumeratedlights_constptr_t = std::shared_ptr<const EnumeratedLights>;
using pointlightlist_t    = std::vector<PointLight*>;
using spotlightlist_t     = std::vector<SpotLight*>;
using tex2pointlightmap_t = std::map<Texture*, pointlightlist_t>;
using tex2spotlightmap_t  = std::map<texturearraysliceref_ptr_t, spotlightlist_t>;
using lightprobeset_t = std::vector<lightprobe_ptr_t>;
using lightmanagerdata_ptr_t = std::shared_ptr<LightManagerData>;
using lightmanager_ptr_t = std::shared_ptr<LightManager>;

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
  // CONSTANT DEPTH BIAS IN WORLD METRES — the slack between an occluder and a
  // receiver under which the receiver stays lit. Authored in metres because the
  // sun's cascade bands each have their own fitted depth range (the ladder's
  // ranges differ by 3x and more), and an ndc constant is therefore a DIFFERENT
  // world slack in every band; the evaluator divides by the band's own range.
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
  // Selection rank among lights of the same kind. LightManager::enumerateInPass
  // sorts the enumerated directional list DESCENDING by this, so index 0 is the
  // authoritative sun/sky/cascade pick. Ties keep enumeration order.
  float _priority = 0.0f;
  // Which CELESTIAL BODY this light is, for the procedural sky's visible discs:
  // 0 none, 1 sun, 2 moon. Declared, never inferred — the priority rank is a
  // shadow/cascade selection order and _castsShadows flips with the night
  // policy, so neither one identifies a body.
  int _skyBody = 0;

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
  // Aliased read of the reflected LightData rank (same live-data contract as
  // intensity()). Null-tolerant because Dynamic*Light constructs its base with
  // a null LightData before installing its inline data.
  float priority() const {
    return _data ? _data->_priority : 0.0f;
  }
  // same null-tolerant live read as priority(); 0 (no body) for an inline-data light
  int skyBody() const {
    return _data ? _data->_skyBody : 0;
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
  // Non-owning back-ref to the scenegraph Node (LightNode) that owns
  // this Light, set by Layer::createLightNode. Lets the renderer's
  // light-enumeration pass consult the node's `_enabled` flag without
  // the lighting layer having to include scenegraph headers. Null when
  // the Light wasn't created via a scenegraph layer (e.g. headlights).
  sgnode_wkptr_t _sgnode;

  // True if no owning scenegraph node, else the node's `_enabled` bit.
  // Impl in gfx_lighting.cpp so Node's definition isn't needed here.
  bool enabled() const;

  const LightData* _data;
  xform_generator_t _xformgenerator;

  int miInFrustumID;
  bool _dynamic = false;
  bool _castsShadows = false;

  texturearraysliceref_ptr_t _cookieColor;
  texturearraysliceref_ptr_t _cookieDepth;
  rtgroup_ptr_t _depthRTG;
  pbr::radiancemaps_ptr_t _RadianceCookie;
};

///////////////////////////////////////////////////////////////////////////////

enum class LightProbeType : uint64_t {
  CrcEnum(REFLECTION),
  CrcEnum(SH_Radiance),
  CrcEnum(END)
};

enum class ProbeActivationMode : crc_enum_t {
  CrcEnum(ALWAYS),
  CrcEnum(BAKE_ONLY),
};

struct LightProbe {

  LightProbe();
  ~LightProbe();
  void resize(int dim);

  void exportEquirectangular(Context* ctx, const fquat& rot, const file::Path& path);

  LightProbeType _type = LightProbeType::REFLECTION;
  ProbeActivationMode _activationMode = ProbeActivationMode::ALWAYS;
  bool _active = true;
  bool _dirty = true;
  // PBR2 Phase 0 — when true, the forward compositor treats the probe
  // as perpetually dirty: re-renders the cubemap every frame so
  // reflections respond live to scene changes (sky swap, geometry
  // motion, etc.). Default false → probe renders once (or when
  // explicitly _markAllDirty'd by a bake button) and stays static.
  // Future: paired with autobake-at-tojson to bake static probes
  // offline; this flag stays as the opt-in for live updates.
  bool _dynamic = false;
  uint64_t _version = 0;
  std::string _name;
  fmtx4 _worldMatrix; // +y up, right handed
  rtgroup_ptr_t _cubeRenderRTG;
  rtgroup_ptr_t _equiRenderRTG;
  texture_ptr_t _cubeTexture;
  varmap::varmap_ptr_t _userdata;
  svar64_t _impl;

  // Pointers to live component data (no caching — always reads current values)
  const int* _pDim = nullptr;
  const int* _pSupersample = nullptr;
  const int* _pTemporalFrames = nullptr;
  const std::string* _pRenderLayer = nullptr;

  // Defaults used when no component data is bound
  int _dim = 0;
  std::string _renderLayer = "probe";
  int _supersample = 0;
  int _temporalFrames = 0;

  // Accessors — read from component data if bound, else from defaults
  int dim() const { return _pDim ? *_pDim : _dim; }
  int supersample() const { return _pSupersample ? *_pSupersample : _supersample; }
  int temporalFrames() const { return _pTemporalFrames ? *_pTemporalFrames : _temporalFrames; }
  const std::string& renderLayer() const { return _pRenderLayer ? *_pRenderLayer : _renderLayer; }

  // SKYLIGHT lane C — SH_Radiance probes only. _shSlot is the probe's index into
  // the projector's SH SSBO, claimed at RTG-allocation time and stable for the
  // probe's life; _shProjector is the (shared, node-owned) projector that last
  // wrote it — the only handle by which a captured probe's coefficients can be
  // read back. _shPendingProject carries a finished capture across to the NEXT
  // frame: a compute dispatch phase is submitted and waited BEFORE the frame's own
  // graphics command buffer is, so projecting the cube in the frame that rendered
  // it samples an image whose contents have not happened yet (measured: all-black).
  // Same 1-frame rule HZBBuilder lives by. All three stay inert for REFLECTION probes.
  int _shSlot            = -1;
  bool _shPendingProject = false;
  probeshprojector_ptr_t _shProjector;

  // TAA runtime state
  int _accumFrameCount = 0;
  int _accumWriteIdx = 0;
  rtgroup_ptr_t _ssaaRenderRTG;
  rtgroup_ptr_t _tempFaceRTG;
  rtgroup_ptr_t _accumFaceRTG[6][2];
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
    _shadowMapSize = 2048; // sun cascades default 2048² (SKYLIGHT L5)
  }

  drawable_ptr_t createDrawable() const final;

  // SKYLIGHT lane A — cascade shadow tunables (all reflected; A8 law).
  // Storage reserves kSunCascadeStorage cascades and the runtime count may use
  // all of them; the DEFAULT stays 4, so growing the storage costs a scene that
  // authors nothing exactly nothing (same ladder, same slices, same bytes).
  int _shadowCascadeCount  = 4;      // 2..kSunCascadeStorage
  float _shadowMaxDistance = 250.0f; // CASTER CEILING (meters ABOVE a band) — NOT the coverage
                                     //  radius: world-anchored bands set coverage (see below).
                                     //  The fit spends it as ceiling/sin(elevation) of toward-light
                                     //  extrusion, so the ceiling a scene buys is the same at every
                                     //  sun angle; spending it as raw along-light distance dropped
                                     //  every caster a low sun was actually casting from.
  float _pcfDither         = 1.0f;   // PCF kernel radius in shadow texels (also dither magnitude)
  // WORLD-ANCHORED BANDS. Cascades are nested world-space spheres centered on
  // the VIEWER POSITION, radii geometric from the base: radius[i] =
  // _shadowBandRadius * _shadowBandRatio^i (10/40/160/640m by default). Fitting
  // is view-INDEPENDENT by construction — no frustum, no camera forward — so a
  // pure rotation can never invalidate a fit, and each band's world texel size
  // (2*radius/mapdim) is a constant that a refit cannot change. A fifth band
  // continues the same ladder (2560m at the stock knobs) — km-scale coverage for
  // the haze march and distant ground, bought with no new radius knob.
  float _shadowBandRadius = 10.0f; // band-0 coverage radius (meters)
  float _shadowBandRatio  = 4.0f;  // radius multiplier per band outward
  // PER-BAND RESOLUTION (S2b). The array stays ONE allocation at the NEAR
  // band's dim (_shadowMapSize); an outer band is rendered into a VIEWPORT
  // sub-rect of its slice, dim[i] = _shadowMapSize / ratio^i floored at 256,
  // and the shader scales that band's uv (and its world texel size) to match.
  // 1 = OFF = every band at the full dim = exactly what the engine ships. The
  // near band is never stepped down, so the sharpest cascade is untouched and
  // the saving is entirely in the bands whose texels are already meters wide.
  float _shadowBandResRatio = 1.0f;
  // PER-SNAPSHOT LIGHT-SPACE JITTER (S2b) — sub-texel offset of each band's
  // SNAPPED ortho origin, amplitude in TEXELS, cycling a low-discrepancy 2D
  // sequence once per snapshot. Two snapshots blended by the flip crossfade
  // are then two different sub-texel samplings of the same penumbra, i.e.
  // temporal supersampling of the shadow edge. Requires
  // _shadowCrossfadeFrames > 0 and is FORCED to 0 without it: a hard swap
  // would turn the offset into visible per-snapshot crawl (the same failure
  // that got per-fragment kernel rotation removed from the shader). Never
  // touches texel SIZE — the constant-texel invariant is what makes the offset
  // a resample instead of a rescale — and the refresh gate never sees it (it
  // reads viewer position and light direction only).
  float _shadowJitterTexels = 0.0f;
  // Seconds between cascade SNAPSHOTS: the fit + cull + depth passes are held
  // between ticks and the maps are re-sampled as they stand. 0 = disabled =
  // every frame. This is THE cadence — not a ceiling, not a floor: no amount of
  // sun travel or viewer walking shortens it, because a scene that declares one
  // minute is declaring that a minute-old shadow is acceptable. The sun's own
  // live state (direction, color, cookie) is never held. Only a STRUCTURAL
  // change — caster flip (sun->moon) or a rig edit that moves the maps
  // themselves — refits early; see ForwardPbrNodeImpl::_update_sun_cascades.
  float _shadowSnapshotInterval = 0.0f;
  // REFRESH GATE — what makes the UNDECLARED cadence (interval 0) take a new
  // snapshot. Without it "no declared interval" means "refit every frame", and
  // a scene with a frozen sun and a standing viewer paid a full fit + cull +
  // four depth passes to redraw the maps it already had (measured: 1.4 ms/frame
  // in the forest).
  //  The gate reads exactly three things — the viewer's POSITION, the light's
  // DIRECTION and the CASTER SET — because those are the premises the fit is a
  // function of (bands are world spheres about the anchor; orientation is
  // deliberately not read, W7). NONE OF THEM MOVED MEANS NO REFIT: the held
  // snapshot is reused for as long as that stays true (owner ruling,
  // 2026-08-05), and the price is that a caster which moves WITHOUT changing
  // the set — wind in a canopy, a walker's limbs — keeps the shadow it had.
  // Frozen leaf-shimmer is accepted; a quarter-second re-fit clock that costs
  // ~1.9 ms on a paused frame is not.
  //  The ceiling is therefore OFF by default and exists only for a scene that
  // wants that shimmer back: above 0 it refits on a wall clock regardless of
  // the premises, at whatever rate is declared.
  //  A DECLARED interval (above) overrides all of it: the declaration is the
  // cadence, and a drift trigger under it would make the declared number
  // advisory (owner ruling, 2026-07-30). All three at 0 = gate disarmed = refit
  // every frame, the pre-gate path exactly — which is why the ceiling's OFF
  // value is only OFF while one of the other two is armed.
  float _shadowRefreshAngleDeg = 0.1f; // sun travel since the snapshot, degrees
  float _shadowRefreshDistance = 0.5f; // viewer travel from the snapshot anchor, meters
  float _shadowRefreshMaxSecs  = 0.0f; // wall-clock ceiling; 0 = none (see above)
  // SNAPSHOT AMORTIZATION + FLIP (S2a). Both 0 = the shipped path exactly: one
  // set of maps, every band rendered in the frame the snapshot is taken, and a
  // hard swap the moment it lands. Either one above 0 double-buffers the
  // cascade array (2x depth-array memory) so the LIVE maps stay sampled while
  // the next snapshot is drawn into the other set.
  //  BandsPerFrame — how many cascade bands ONE frame may render. 0 = unset =
  //   all of them, the shipped single-frame snapshot. A snapshot is always
  //   rendered against ONE frozen premise set and published as a whole: half a
  //   refit (band 0 new, band 3 old) is a geometrically inconsistent world.
  //  CrossfadeFrames — frames over which the shader blends the OLD snapshot's
  //   shadow factor into the new one on a flip (depth values cannot be lerped;
  //   the FACTORS can). 0 = hard swap, and the ARM: no frames, no fade.
  //  CrossfadeSecs — the window's wall-clock LENGTH once armed. A frame count
  //   is not a duration (12 frames = 24 ms in an unthrottled offscreen loop,
  //   200 ms at 60 Hz), so the fraction of time spent blending used to be a
  //   function of the frame rate. With this above 0 the blend ramps on the
  //   clock and the frame count only keeps it armed; 0 = ride the frames, the
  //   pre-clock behavior exactly. Sibling of
  //   SkyAtmosphereData::_iblCrossfadeMaxSecs, which bounds the IBL feed's
  //   window the same way.
  int _shadowSnapshotBandsPerFrame = 0;
  int _shadowCrossfadeFrames       = 0;
  float _shadowCrossfadeSecs       = 0.2f;
  // CULLSETS (both empty = the one implicit all-families set = the shipped
  // path, byte for byte). A cullset is a NAMED list of caster FAMILIES
  // (ShadowFamily tokens: terrain / instanced / other); each band subscribes to
  // exactly one. A set is culled ONCE against the union of only ITS bands' fit
  // radii, and its bands draw only its families — which is what keeps a 10 km
  // outer band from dragging a scattered canopy through every near band's depth
  // pass (one union volume + one survivor list put every instance in every
  // band's draw).
  //  Spelling — two flat strings so the whole rig rides the reflected .ecs the
  // author/player split demands, and so a new partition is a scene edit with no
  // C++ in it:
  //   _shadowCullSets     "near=terrain,instanced,other;far=terrain"
  //   _shadowBandCullSets "near,near,near,near,far"   (one name PER BAND)
  // The band list's length must equal the cascade count, every name must be a
  // declared set, and every family token must be a known one — all three are
  // hard errors, because a mis-spelled family that silently matched nothing
  // reads as a broken shadow, not as a typo.
  std::string _shadowCullSets;
  std::string _shadowBandCullSets;
  // CLOUD SHADOWS — the sun COOKIE, filled by the forward prologue by drawing
  // the scene's cloud-deck layer from a sun-aligned ortho camera (one shared
  // occlusion source: the decks' own transmittance, never a second model).
  //  Strength 0 (the default) DISARMS the whole path — no fill pass, no cookie
  // published, and the shader's enable branch keeps such a scene byte-identical
  // to one that never heard of decks.
  //  SOFTNESS is a mip-LOD bias, not a kernel: cloud shadows are big soft
  // patches and must read FUZZIER than any ground-object shadow in the same
  // frame (owner law), which is what a wider filter footprint buys.
  //  EXTINCTION is the beam's, shared by BOTH consumers (ground direct term and
  // sun/moon disc): one beam, one Beer-Lambert law. See _cloudExtinction.
  float _cloudShadowStrength = 0.0f;    // 0..1 — how much deck occlusion reaches the ground
  float _cloudShadowExtent   = 4000.0f; // ortho HALF-extent about the viewer (meters)
  float _cloudShadowSoftness = 2.0f;    // cookie mip-LOD bias (GROUND penumbra)
  float _cloudShadowDepth    = 30000.0f;// toward-light extrusion (must clear the deck altitude)
  int _cloudShadowMapSize    = 512;     // cookie resolution (mipped)
  // COOKIE CADENCE — how many frames one cookie fill serves (1 = refill every
  // frame). The fill draws every deck of the sky into a mipped ortho target and
  // regenerates its mip chain; the thing it captures is a slab of cloud
  // kilometers across, moving at cloud speed, sampled through a mip bias whose
  // whole job is to blur it. Nothing in that image can change meaningfully in
  // one frame.
  //  HOLDING IS FREE OF SKEW because the cookie is a WORLD-space field: the
  // published matrix travels with the texture it describes, so a held cookie
  // keeps painting the same shadow on the same ground while the viewer moves
  // (the only drift is the ortho window's anchor, and that window is kilometers
  // wide). The DISARM path is never held — a cookie must not outlive its decks.
  int _cloudShadowRefreshFrames = 4;

  // BEER-LAMBERT extinction of the direct beam: transmittance = exp(-tau * a),
  // with a the cookie's accumulated occlusion and tau THIS number. The cookie's
  // alpha is a coverage-like union of shell occlusions, so a = 1 is read as "one
  // full reference cloud stands in the beam" and tau is that reference cloud's
  // OPTICAL DEPTH.
  //  The default is the optical depth of a thin fair-weather cumulus from the
  // standard cloud relation tau = 3*LWP / (2*rho_w*r_e): LWP 50 g/m^2,
  // r_e 10 um, rho_w 1000 kg/m^3 -> tau = 0.15/0.02 = 7.5. A direct beam
  // through such a deck keeps exp(-7.5) = 0.05% of its light — a solid cumulus
  // puts the sun OUT, which is what a sky looks like. Halved coverage (a=0.5)
  // still keeps only 2.4%, while genuine veil (a=0.1) passes 47% and grades.
  //  0 = transparent (no extinction at any alpha); the STRENGTH knob, not this
  // one, is what disarms the path.
  float _cloudExtinction = 7.5f;
  // The DISC's own sample LOD. Same units as _cloudShadowSoftness (a mip bias),
  // deliberately a separate number with a much lower default: the ground term
  // needs a penumbra wide enough to read as a cloud shadow, while the disc
  // lookup is a single texel answering "is the beam blocked" — a transit edge
  // in life is crisp (the sun's 0.53deg disc against a cloud edge), so the only
  // filtering it wants is enough to stop single-texel aliasing as the deck
  // advects.
  float _cloudDiscSoftness = 0.5f;

  // SHADOW WEIGHT AGAINST THE IMAGE-BASED TERM. A shadow factor scales the
  // DIRECT beam; whether it may also pull the ambient/IBL term down is a
  // question about WHAT SOLID ANGLE the occluder covers, and the two occluders
  // here answer it differently:
  //   A CLOUD occludes a broad wedge of the SKY, so the sky light itself is
  // reduced under it. With the env term carrying ~90% of a daylit frame's
  // energy, a cookie that touches only the direct beam is invisible — which is
  // exactly what was measured before this knob existed.
  //   A TREE occludes the sun DISC and almost none of the sky dome, so a
  // cascade must keep its hands off the env term. That is an asserted invariant
  // (the shadow-attribution gate), which is why the cascade weight defaults to
  // OFF; nonzero is scene experimentation, not physics.
  //  Both act on BOTH env channels (diffuse SH irradiance and specular IBL) —
  // a cloud dims the reflection of the sky as much as the light off it — but
  // not on the same quantity: the cascade rides its shadow factor,
  // mix(1, factor, weight), while the cloud rides COVERAGE,
  // mix(1, 1 - strength*alpha, weight). The beam's transmittance is
  // exp(-tau*alpha) and saturates near-binary past alpha ~ 0.3, which is the
  // right answer for a disc that is or is not blocked and the wrong one for a
  // dome that is partly covered; coverage grades with the deck. The cloud
  // default is deliberately short of 1 — sky light reaches a shaded patch from
  // beyond the cloud too, so full black under a deck would be wrong. The
  // darkening is LINEAR in this number, and it is a look: scenes author it
  // (Scene.sun(cloud_ibl_weight=...)) rather than editing this default.
  float _cloudShadowIblWeight   = 0.65f;
  float _cascadeShadowIblWeight = 0.0f;
  // Cascade shadow FLOOR on the direct term: shadowed direct light keeps this
  // fraction instead of going to zero (mix(floor, 1, cascade) in fwdtools).
  // Lifts shadow brightness ABOVE the stock look — the IBL weight above can
  // only darken. 0 = stock. A look number; scenes author it (Scene.sun(
  // cascade_floor=...)).
  float _cascadeShadowFloor     = 0.0f;
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

  // Point the light along (tgt-eye): builds a world matrix whose zNormal()
  // (== direction()) is the travel direction of the sunlight. Follows the
  // SpotLight explicit-xform pattern: the generator lambda is installed ONCE
  // and reads _explicit_world thereafter (per-frame std::function replacement
  // races render-thread worldMatrix() calls → UAF).
  void lookAt(const fvec3& eye, const fvec3& tgt, const fvec3& up);

  DirectionalLight(const DirectionalLightData* pld);
  DirectionalLight(xform_generator_t mtx, const DirectionalLightData* dld = 0);

  fmtx4 _explicit_world;
  bool _xformgenerator_is_explicit = false;
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
  file::Path _cookiePath;

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

  // Set view/projection matrices directly (bypasses lookAt's perspective
  // projection, enabling orthographic shadow projection, off-axis frusta,
  // or caller-provided view matrices that sidestep lookAt's up-vector
  // singularity.)
  //
  // All three variants mark `_matrices_explicit = true` so callers that
  // would otherwise overwrite view/proj (ECS xformgenerator lambdas etc.)
  // leave the values alone. The two higher-level variants also record the
  // raw projection params in `_explicit_*` for debugging / round-trip.
  void setViewProj(const fmtx4& view, const fmtx4& proj, const fvec3& pos);
  void setOrthoViewProj(
      const fmtx4& view,
      float left, float right, float top, float bottom,
      float near, float far,
      const fvec3& pos);
  void setPerspectiveViewProj(
      const fmtx4& view,
      float fovy_rad, float aspect,
      float near, float far,
      const fvec3& pos);

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

  // Explicit-matrix override bookkeeping.
  //
  // Set by setViewProj / setOrthoViewProj / setPerspectiveViewProj; any
  // code that otherwise rebuilds view/proj from the light's transform
  // (e.g. ECS xformgenerator lambdas, lookAt) should check
  // `_matrices_explicit` and skip the recalc so the explicit matrices
  // stay intact. `_explicit_proj_type` and `_explicit_proj_*` record the
  // raw projection params the caller supplied (for debugging / round-trip
  // matrix rebuild). CUSTOM = matrix handed in pre-built, raw params not
  // tracked.
  enum class ExplicitProjType { NONE, ORTHO, PERSPECTIVE, CUSTOM };
  bool _matrices_explicit = false;
  ExplicitProjType _explicit_proj_type = ExplicitProjType::NONE;
  // Ortho params (meaningful iff _explicit_proj_type == ORTHO)
  float _explicit_ortho_left   = 0.0f;
  float _explicit_ortho_right  = 0.0f;
  float _explicit_ortho_top    = 0.0f;
  float _explicit_ortho_bottom = 0.0f;
  // Perspective params (meaningful iff _explicit_proj_type == PERSPECTIVE)
  float _explicit_persp_fovy_rad = 0.0f;
  float _explicit_persp_aspect   = 1.0f;
  // Near/far (meaningful for both ORTHO and PERSPECTIVE)
  float _explicit_near = 0.0f;
  float _explicit_far  = 0.0f;
  // Light world-position storage read by the xformgenerator lambda. Kept
  // as a plain member instead of lambda-captured so per-frame setters
  // don't need to replace `_xformgenerator` (std::function reassignment
  // races with render-thread worldMatrix() calls → UAF on the old
  // lambda's captures). Lambda captures `[this]` only and reads this.
  fvec3 _explicit_eye_pos = fvec3(0);
  bool _xformgenerator_is_explicit = false;

private:
  // Install `_xformgenerator` to read _explicit_eye_pos. Idempotent —
  // subsequent calls are no-ops so the std::function storage stays
  // stable across per-frame update/setter calls.
  void _installExplicitXformGenerator();
public:
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

struct EnumeratedLights {

  std::vector<Light*> _alllights;
  // SKYLIGHT lane A — directional bucket. The forward node's sun-cascade
  // pass picks THE sun (first active shadow-caster) from this list.
  std::vector<DirectionalLight*> _directionallights;
  pointlightlist_t _untexturedpointlights;
  tex2pointlightmap_t _tex2pointlightmap;
  spotlightlist_t _untexturedspotlights;
  tex2spotlightmap_t _tex2spotlightmap;
  tex2spotlightmap_t _tex2shadowedspotlightmap;
  tex2spotlightmap_t _tex2spotdecalmap;
  lightprobeset_t _lightprobes;

  int _num_active_untextured_pointlights = 0;
  int _num_active_texspotlights = 0;


};

///////////////////////////////////////////////////////////////////////////////

struct LightManager {

  LightCollector mcollector;

public:

  LightManager(lightmanagerdata_constptr_t lmd);

  void gpuInit(Context* ctx);

  GlobalLightContainer mGlobalStationaryLights; // non-moving, potentially animating color or texture (and => not lightmappable)
  LightContainer mGlobalMovingLights;           // moving lights
  lightprobeset_t _lightprobes;

  // PBR2 Phase 0 — find a registered probe by its (entity-name-derived)
  // _name. Linear scan over the few probes the scene declares (typically
  // 1-10). Called once at consumer activation time, not per-frame.
  lightprobe_ptr_t findProbeByName(const std::string& name) const;

  void enumerateInPass(const CompositingPassData& CPD, enumeratedlights_ptr_t out_lights) const;

  void QueueInstance(const LightMask& lgid, const fmtx4& mtx);

  size_t GetNumLightGroups() const;
  void Clear();

  void bindEnumeratedToStorageBuffer(Context* ctx, enumeratedlights_ptr_t enumerated_lights, FxShaderStorageBuffer* ssbo) const;  

  texturearraysliceref_ptr_t allocateDepthSlice();
  texturearraysliceref_ptr_t allocateColorSlice(const std::string& cookiePath);

  // SKYLIGHT lane A — dedicated sun-cascade depth array (Z32F). NOT the 256²
  // spot cookie array. Rebuilds (array + per-slice RTGs) when the requested
  // dim OR set count changes; the default 8×8 array keeps the sampler
  // descriptor valid in sunless scenes (shader never samples it — has_sun
  // gates).
  //
  // SETS (S2a) — the double buffer is a SLICE RANGE of this one array, not a
  // second texture: sets=2 allocates 2*bands slices and the shader picks a set
  // by adding a base to its band index, so the crossfade costs no extra sampler
  // and no extra descriptor. sets=1 is byte-identical to the single-buffered
  // allocation this shipped with.
  //
  // ALLOCATION FOLLOWS THE RUNTIME BAND COUNT, not the storage ceiling: a slice
  // is a full dim² Z32F surface (16 MB at the default 2048²), so sizing the
  // array by the ceiling would charge every 4-band scene for a fifth map it
  // never renders into. _sun_cascade_bands is therefore THE slice stride — set
  // base = set * _sun_cascade_bands — and every consumer must read it rather
  // than assume the ceiling.
  static constexpr int kSunCascadeStorage = 5; // per-set storage CEILING (L5); runtime count 2..5
  void ensureSunCascades(Context* ctx, int dim, int bands, int sets = 1);

  // SUN COOKIE fill target — one small MIPPED RGBA8 RTG the prologue draws the
  // cloud decks into (see ForwardPbrNodeImpl::_update_sun_cookie). Rebuilt only
  // on a dimension change; _sun_cookie points at its color texture once armed.
  void ensureSunCookie(Context* ctx, int dim);

  lightmanagerdata_constptr_t _data;
  texturearray_ptr_t _cookies_spot_color;
  texturearray_ptr_t _cookies_spot_depth;
  texturearray_ptr_t _cookies_spot_color_default;
  texturearray_ptr_t _cookies_spot_depth_default;
  texturearray_ptr_t _sun_shadow_cascades;
  texturearray_ptr_t _sun_shadow_cascades_default;
  // SUN COOKIE (cloud shadows) — a TRANSMITTANCE map projected from the sun's
  // plane onto the world. Unlike the spot cookies this is a single plain 2D
  // texture, because there is one sun and its cookie covers everything the
  // camera can reach rather than a per-light cone. The default is a 1x1 WHITE
  // texel (transmittance 1) so the sampler descriptor is always valid and an
  // un-armed cookie cannot darken anything even if a shader samples it.
  texture_ptr_t _sun_cookie;
  texture_ptr_t _sun_cookie_default;
  // World->cookie-uv projection and its dials, published by whoever owns the
  // deck (see _sunCookieMatrix in the forward prologue). _sun_cookie_strength
  // 0 disarms the whole path.
  fmtx4 _sun_cookie_matrix;
  float _sun_cookie_strength = 0.0f;
  float _sun_cookie_lod      = 0.0f; // GROUND sample bias
  // Beer-Lambert tau for the direct beam (both consumers) and the DISC's own,
  // sharper sample bias. Published together with the cookie so the ground and
  // the sky can never run different extinction laws for the same beam.
  float _sun_cookie_extinction = 0.0f;
  float _sun_cookie_disc_lod   = 0.0f;
  // the direction the cookie's light TRAVELS (== the cascade holder's
  // direction the frame it was filled). The disc occlusion consumer compares
  // it against the sky's sun/moon so it can only dim the body the cookie was
  // actually baked toward.
  fvec3 _sun_cookie_dir = fvec3(0, -1, 0);
  rtgroup_ptr_t _sun_cookie_rtg;
  int _sun_cookie_dim = 0;
  std::vector<texturearraysliceref_ptr_t> _sun_cascade_slices; // keep alive: RTG->_slice is a raw ptr
  std::vector<rtgroup_ptr_t> _sun_cascade_rtgs;                // indexed set*_sun_cascade_bands + band
  int _sun_cascade_dim   = 0;
  int _sun_cascade_sets  = 0;
  int _sun_cascade_bands = 0; // THE slice stride of the allocation above
  bool _needs_gpu_init = true;
  int _nextDepthSliceAlloc = 0;
  int _nextColorSliceAlloc = 0;
  std::map<std::string, texturearraysliceref_ptr_t> _colorCookieMap;
};

using lightmanager_ptr_t = std::shared_ptr<LightManager>;

///////////////////////////////////////////////////////////////////////////////
// SUN CASCADE CULLSETS — the resolved form of the two authored strings on
// DirectionalLightData. Built once per SNAPSHOT (not per frame) by the forward
// prologue; the band->set map drives the fit's per-set volume fold, the cull
// fan-out's family filter, and the depth pass's enqueue gate.
//
// UNAUTHORED = ONE SET named "all", every family, every band subscribed. That
// is the compatibility contract: one cull against one union volume and one
// draw per band, i.e. the pre-cullset engine exactly.
///////////////////////////////////////////////////////////////////////////////

struct SunCullSetPlan {
  // A band ladder cannot use more distinct sets than it has bands.
  static constexpr int kMaxSets = LightManager::kSunCascadeStorage;

  int _numSets   = 1;
  bool _authored = false; // false => the implicit all-families set (no filtering anywhere)
  std::string _names[kMaxSets];
  uint32_t _familyMask[kMaxSets]                 = {0};
  int _bandSet[LightManager::kSunCascadeStorage] = {0};
  // bands in DRAW ORDER, grouped by set: the snapshot culls a set once and then
  // draws all of that set's bands, so the number of shadow culls a snapshot
  // costs is the number of SETS and never the number of bands.
  int _drawOrder[LightManager::kSunCascadeStorage] = {0};
};

// Parse + VALIDATE the authored pair against a cascade count. Fails loud (named
// error, offending token quoted) on an unknown family token, an unknown set
// name, a band-list length mismatch, a duplicate set name, or more sets than
// bands. Empty/empty yields the implicit all-families plan.
SunCullSetPlan resolveSunCullSets(
    const std::string& sets_spec,  //
    const std::string& bands_spec, //
    int cascade_count);

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
