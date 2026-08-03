///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/rtti/downcast.h>
#include <ork/util/endian.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/pri.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/image.h> // E.2-walk: the collider loads + high-quality-resamples its heightmap as a lev2::Image
#include <ork/lev2/gfx/live_field_buffer.h> // S4: physics REQUIRES final — hold last-final while a live re-bake runs
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include "bullet_impl.h"
#include <BulletCollision/CollisionDispatch/btManifoldResult.h>       // W·M: gContactAddedCallback
#include <BulletCollision/CollisionDispatch/btCollisionObjectWrapper.h> // W·M: contact wrapper -> terrain transform
#include <rapidjson/document.h> // E.2-walk: hf_asset manifest parse
#include <fstream>
#include <sstream>
#include <atomic>
#include <filesystem>
#include <algorithm>
#include <unordered_set>
///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::ecs::BulletShapeTerrainData, "BulletShapeTerrainData");
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

// W·M SURFACE RESPONSE — physics leg (owner-adjudicated 2026-07-22).
// Immutable-after-load, per-collider surface-class weight field + the friction response row.
// Lives inside BulletTerrainImpl (stable address); the raw pointer is stashed on the
// heightfield shape's user pointer so the global contact-added hook can find it. Rebuilt in
// place on a deferred rebake-reload (update thread, before the step — never during a step),
// so the pointer stays valid across reloads.
struct TerrainFrictionSampler {
  bool _active   = false;
  int _dim       = 0;      // collider-grid dim (== _gridDim; W is resampled to match heights)
  float _extent  = 1000.0f;// world extent in meters (square heightfield)
  float _rows[4] = {0, 0, 0, 0}; // M[:,friction] deltas, one per RGBA class channel
  std::vector<fvec4> _weights;   // per-texel class weights, row-major [row*_dim + col]

  // per-contact friction DELTA at terrain-local (lx,lz) meters. Truncating nearest-texel
  // sampling (scatter-parity convention: u = x/E + 0.5) so physics samples the SAME texel the
  // material/scatter do. Residual (1-Σw) implicitly carries delta 0 (it is never a captured row).
  float deltaAt(float lx, float lz) const {
    if ((not _active) or _dim <= 0 or _weights.empty())
      return 0.0f;
    float u = lx / _extent + 0.5f;
    float v = lz / _extent + 0.5f;
    int col = int(u * float(_dim)); // trunc toward zero == floor for u>=0 (matches hfdflow_scatter)
    int row = int(v * float(_dim));
    col     = std::clamp(col, 0, _dim - 1);
    row     = std::clamp(row, 0, _dim - 1);
    const fvec4& W = _weights[size_t(row) * size_t(_dim) + size_t(col)];
    return W.x * _rows[0] + W.y * _rows[1] + W.z * _rows[2] + W.w * _rows[3];
  }
};

// Registry of live terrain friction samplers. A heightfield-vs-body contact does NOT report
// the terrain as a TERRAIN_SHAPE_PROXYTYPE at the manifold: bullet processes each contacted
// cell as an internal TRIANGLE child shape, so the leaf wrapper shape is a triangle carrying
// no user pointer. We therefore identify the terrain via the collision OBJECT's ROOT shape
// (the compound we build), whose user pointer we set to the sampler and register here. Membership
// disambiguates our pointer from any other shape user pointer. All lifecycle (register at shape
// build, erase at collider destroy) and reads (the hook, during step) run on the update thread —
// no lock needed.
static std::unordered_set<const void*>& _activeTerrainSamplers() {
  static std::unordered_set<const void*> s;
  return s;
}

// The global contact-added hook (bullet's gContactAddedCallback slot). Fires INSIDE
// stepSimulation for any manifold point whose body carries CF_CUSTOM_MATERIAL_CALLBACK —
// only terrain bodies with an active sampler get that flag, so this is terrain-only. Stateless:
// the per-terrain data rides on the terrain compound shape's user pointer (registered above), so
// multiple simulations (and multiple terrains) share one hook safely. Modifies the base combined
// friction in place by the W·M[:,friction] residual delta at the contact point.
static bool orkTerrainContactAddedCallback(
    btManifoldPoint& cp,
    const btCollisionObjectWrapper* w0,
    int /*partId0*/,
    int /*index0*/,
    const btCollisionObjectWrapper* w1,
    int /*partId1*/,
    int /*index1*/) {

  auto& reg = _activeTerrainSamplers();
  if (reg.empty())
    return false;
  // identify the terrain side via its rigid body's ROOT (compound) shape user pointer.
  const btCollisionObject* tobj      = nullptr;
  const TerrainFrictionSampler* samp = nullptr;
  auto probe = [&](const btCollisionObjectWrapper* w) {
    if (not w)
      return;
    auto obj = w->getCollisionObject();
    if (not obj)
      return;
    auto root      = obj->getCollisionShape(); // the body's root shape (compound for terrain)
    const void* up = root ? root->getUserPointer() : nullptr;
    if (up and reg.count(up)) {
      samp = reinterpret_cast<const TerrainFrictionSampler*>(up);
      tobj = obj;
    }
  };
  probe(w0);
  if (not samp)
    probe(w1);
  if (not samp or not samp->_active)
    return false; // not our terrain (or feature off): leave the base combined friction untouched

  // contact point -> terrain-local x,z via the terrain BODY transform (entity frame). The
  // compound child offset is y-only, so x,z are unaffected — inverse*worldpos yields centered
  // local meters in [-E/2, E/2], the frame u = x/E + 0.5 expects (scatter-parity).
  const btVector3& wpos = cp.getPositionWorldOnB();
  btVector3 lpos        = tobj->getWorldTransform().inverse() * wpos;
  float delta           = samp->deltaAt(float(lpos.x()), float(lpos.z()));

  float f              = cp.m_combinedFriction + delta;
  cp.m_combinedFriction = (f < 0.0f) ? 0.0f : f; // clamp: friction is non-negative
  return true;
}

///////////////////////////////////////////////////////////////////////////////

struct BulletTerrainImpl {
  int mGridSize                            = 0;
  bool _loadok                             = false;
  Entity* _entity                          = nullptr;
  btHeightfieldTerrainShape* _terrainShape = nullptr;
  file::Path _curhfpath;
  file::Path _resPath;          // E.2-walk: resolved height image (asset-wired or direct)
  float _resSize   = 1000.0f;   // resolved world extent (manifest extent_m when asset-wired)

  // E.2-walk: the collider's heightmap is loaded as a TRANSIENT lev2::Image (EXR via OIIO),
  // high-quality-resampled DOWN to _render_dimension when set; its channel-0 heights are copied into
  // _heightData and the Image is then freed (only _heightData persists). _heightData is kept ALIVE here
  // because btHeightfieldTerrainShape stores the data POINTER (it does not copy).
  std::vector<float> _heightData;
  int   _gridDim = 0;
  float _minH    = 0.0f;
  float _maxH    = 1.0f;
  // the ctor-time collision AABB span (PADDED beyond the observed [min,max]): the shape's
  // AABB is FIXED at creation while a rebake reload mutates heights in place — the pad
  // absorbs modest relief growth; _loadHeightData warns loudly when data escapes it.
  float _aabbMinH = 0.0f;
  float _aabbMaxH = 1.0f;
  hfdrawableinstptr_t _hfinstance;
  const BulletShapeTerrainData& _hfd;
  msgrouter::subscriber_t _subscriber;

  // REBAKE RELOAD: a same-path rebake overwrites height.exr + the .terrain.json manifest IN
  // PLACE, so a path-only reload gate never fires. Content-stamp both artifacts (cheap stat —
  // mtime+size of each; a different bake changes the EXR size and/or mtime, and rebakes are
  // seconds apart, so stat is robust here and far cheaper than hashing the field). The reload
  // gate becomes "path changed OR stamp changed".
  std::string _manifestPath;                 // "" when the direct (non-asset) heightmap form is used
  BulletSystem* _world = nullptr;            // set at init_bullet_shape; owns the deferred-reload poll
  std::atomic<bool> _reload_pending{false};  // set by the msgrouter handler (any thread), consumed on update thread
  bool     _stampValid    = false;
  uint64_t _stampExrMtime = 0, _stampExrSize = 0;
  uint64_t _stampManMtime = 0, _stampManSize = 0;
  bool     _holdLogged    = false; // S4 hold-last-final: log the hold ONCE per re-bake

  // W·M SURFACE RESPONSE — the class-weight field + friction response (stable address; the
  // heightfield shape's user pointer references it; rebuilt in place on reload).
  TerrainFrictionSampler _fricSampler;

  BulletTerrainImpl(const BulletShapeTerrainData& data);
  ~BulletTerrainImpl();

  btCollisionShape* init_bullet_shape(const ShapeCreateData& data);

  bool _statStamp(uint64_t& em, uint64_t& es, uint64_t& mm, uint64_t& ms) const;
  bool _needsReload() const;
  bool _loadHeightData();       // first load vs in-place reload keyed off _loadok
  void _loadSurfaceWeights();   // W·M: (re)load the RGBA class-weight EXR -> _fricSampler._weights
  void _reloadIfChanged();
  void consumePendingReload(); // update-thread consume of a deferred rebake reload
};

using terrain_impl_ptr_t = std::shared_ptr<BulletTerrainImpl>;

///////////////////////////////////////////////////////////////////////////////

BulletTerrainImpl::BulletTerrainImpl(const BulletShapeTerrainData& data)
    : _hfd(data) {
  // E.2-walk: resolve the ASSET-WIRED form first. hf_asset names a baked HeightField —
  // the height image is the SAME TRUE-METERS EXR the chunk renderer consumes, and
  // worldSize comes from the .terrain.json manifest, so physics collides with exactly
  // what renders (one declaration). Direct _heightMapPath/_worldSize remain the override
  // when hf_asset is empty.
  _resPath   = _hfd._heightMapPath;
  _resSize   = _hfd._worldSize;
  if (not _hfd._hf_asset.empty()) {
    std::string base = file::Path::expandPathString("<assetcache>/terrain/" + _hfd._hf_asset);
    _resPath         = file::Path((base + "/height.exr").c_str());
    std::string manifest_path = base + "/" + _hfd._hf_asset + ".terrain.json";
    _manifestPath             = manifest_path;
    std::ifstream mf(manifest_path);
    if (mf.good()) {
      std::stringstream mstrm;
      mstrm << mf.rdbuf();
      rapidjson::Document doc;
      doc.Parse(mstrm.str().c_str());
      OrkAssert(not doc.HasParseError());
      _resSize   = doc["scale"]["extent_m"].GetFloat();
      // heights are TRUE METERS in the EXR (manifest version 2 dropped scale.height_m).
      printf("BulletShapeTerrain: hf_asset<%s> -> <%s> extent<%g>\n",
             _hfd._hf_asset.c_str(), _resPath.c_str(), _resSize);
    } else {
      printf("BulletShapeTerrain: hf_asset<%s> manifest MISSING <%s> — the HeightField asset "
             "must materialize BEFORE the physics shape (declaration order = dependency order)\n",
             _hfd._hf_asset.c_str(), manifest_path.c_str());
      OrkAssert(false);
    }
  }
  // The "bshdchanged" handler only sets a flag: a REBAKE broadcasts this channel from the GPU
  // thread (HeightFieldGenData::materialize) while the sim update thread reads _heightData through
  // the btHeightfieldTerrainShape pointer — mutating here would race. The actual reload is deferred
  // to BulletSystem::_onUpdate (consumePendingReload), which runs on the update thread at a point
  // where nothing is stepping the sim. An atomic store is safe from any posting thread.
  _subscriber = msgrouter::channel("bshdchanged")->subscribe(
      [this](msgrouter::content_t) { _reload_pending.store(true); });

  // initial synchronous load (construction thread): init_bullet_shape needs _heightData + _loadok
  // immediately after. Not routed through the deferred flag.
  _reloadIfChanged();
}

///////////////////////////////////////////////////////////////////////////////

// (mtime,size) of height.exr and the manifest. Returns false if either is missing/unreadable —
// the caller treats that as "reload needed" so the loud-failure path can report it.
bool BulletTerrainImpl::_statStamp(uint64_t& em, uint64_t& es, uint64_t& mm, uint64_t& ms) const {
  namespace fs = std::filesystem;
  std::error_code ec;
  auto esz = fs::file_size(_resPath.c_str(), ec);
  if (ec)
    return false;
  auto ewt = fs::last_write_time(_resPath.c_str(), ec);
  if (ec)
    return false;
  em = uint64_t(ewt.time_since_epoch().count());
  es = uint64_t(esz);
  mm = 0;
  ms = 0;
  if (not _manifestPath.empty()) {
    auto msz = fs::file_size(_manifestPath, ec);
    if (ec)
      return false;
    auto mwt = fs::last_write_time(_manifestPath, ec);
    if (ec)
      return false;
    mm = uint64_t(mwt.time_since_epoch().count());
    ms = uint64_t(msz);
  }
  return true;
}

bool BulletTerrainImpl::_needsReload() const {
  if (_curhfpath != _resPath) // path changed (or first load: _curhfpath is empty)
    return true;
  uint64_t em, es, mm, ms;
  if (not _statStamp(em, es, mm, ms)) // missing/unreadable -> attempt reload (fails loudly, keeps old data)
    return true;
  if (not _stampValid)
    return true;
  return (em != _stampExrMtime) or (es != _stampExrSize) or (mm != _stampManMtime) or (ms != _stampManSize);
}

// Load height.exr into _heightData. On failure (missing/corrupt EXR, or a grid-dim change we
// cannot hot-swap) emit a NAMED error and return false WITHOUT touching the live data — the old
// heights stay valid (never zero/garbage). btHeightfieldTerrainShape holds _heightData.data() by
// POINTER, so a reload copies in place (grid dim verified unchanged) to keep that pointer stable.
bool BulletTerrainImpl::_loadHeightData() {
  const bool first  = not _loadok; // never successfully loaded -> fresh assign (sizes _heightData)
  const char* stage = first ? "LOAD" : "RELOAD";
  { // self-defend: never feed a missing path to createFromFile — report loudly, keep old data
    std::error_code ec;
    if (not std::filesystem::exists(_resPath.c_str(), ec) or ec) {
      printf("BulletShapeTerrain: %s FAILED — heightmap<%s> does not exist; retaining previous heights\n",
             stage, _resPath.c_str());
      return false;
    }
  }
  auto img = lev2::Image::createFromFile(_resPath.c_str());
  if (not(img and img->_width > 0 and img->_width == img->_height)) {
    printf("BulletShapeTerrain: %s FAILED — heightmap<%s> missing/corrupt (image load failed); "
           "retaining previous heights\n",
           stage, _resPath.c_str());
    return false;
  }
  int src_dim = int(img->_width);
  int rdim    = _hfd._render_dimension;
  // high-quality DOWNsample to the render grid (ringing-free TRIANGLE — no spurious collision bumps),
  // so the collider surface == the visible downsampled mesh (terrain render_dimension).
  if (rdim > 0 and rdim < src_dim) {
    auto ds = std::make_shared<lev2::Image>();
    ds->resampledOf(*img, rdim, rdim, lev2::Image::ResampleFilter::TRIANGLE);
    img = ds;
    if (first)
      printf("BulletShapeTerrain: collider resampled %d -> %d (Image::resampledOf TRIANGLE)\n", src_dim, rdim);
  }
  const bool f32 = (img->_bytesPerChannel == 4);
  if (not(f32 or img->_bytesPerChannel == 1)) { // float EXR (R32F/RGBA32F) or 8-bit PNG (legacy direct path)
    printf("BulletShapeTerrain: %s FAILED — heightmap<%s> unexpected bytesPerChannel<%d>; retaining previous heights\n",
           stage, _resPath.c_str(), int(img->_bytesPerChannel));
    return false;
  }
  const int newdim = int(img->_width);
  if (not first and newdim != _gridDim) {
    printf("BulletShapeTerrain: RELOAD FAILED — heightmap<%s> grid dim changed %d -> %d (hot-reload needs a "
           "stable grid; the shape is not rebuilt); retaining previous heights\n",
           _resPath.c_str(), _gridDim, newdim);
    return false;
  }
  const int nc = int(img->_numcomponents);
  std::vector<float> newheights(size_t(newdim) * size_t(newdim));
  float mn = 1e30f;
  float mx = -1e30f;
  for (int y = 0; y < newdim; y++)
    for (int x = 0; x < newdim; x++) {
      float h = f32 ? img->pixel32f(x, y)[0]                            // channel 0 = height in TRUE METERS (EXR)
                    : (float(img->pixel8(x, y)[0]) * (1.0f / 255.0f));  // legacy 8-bit direct-heightmap path
      newheights[size_t(y) * newdim + x] = h;
      mn                                 = std::min(mn, h);
      mx                                 = std::max(mx, h);
    }
  img.reset(); // heights are copied out -> free the Image buffer now (only _heightData persists)
  if (first) {
    _heightData = std::move(newheights);
    _gridDim    = newdim;
  } else {
    std::copy(newheights.begin(), newheights.end(), _heightData.begin()); // in place -> pointer stable
  }
  _minH      = mn;
  _maxH      = mx;
  // in-place reload guard: the live shape's AABB was fixed (padded) at ctor time — data
  // escaping it collides wrong (clipped AABB) AND shifts against the compound center.
  // Loud, not fatal: the visual reloads correctly; reopen the scene to rebuild physics.
  if (not first and (mn < _aabbMinH or mx > _aabbMaxH))
    printf("BulletShapeTerrain: RELOADED heights [%g..%g]m ESCAPE the ctor AABB [%g..%g]m — "
           "collision is wrong until the scene is reloaded\n", mn, mx, _aabbMinH, _aabbMaxH);
  _loadok    = true;
  _curhfpath = _resPath;
  _statStamp(_stampExrMtime, _stampExrSize, _stampManMtime, _stampManSize); // record what we just loaded
  _stampValid = true;
  printf("BulletShapeTerrain: %s heightmap<%s> dim<%d> nc<%d> min<%g> max<%g>\n",
         first ? "loaded" : "RELOADED", _resPath.c_str(), _gridDim, nc, _minH, _maxH);
  // W·M: (re)load the class-weight field onto the FRESH height grid (same grid, so texels align).
  // On reload this rebuilds _fricSampler._weights in place — the sampler's address (and thus the
  // heightfield shape's user pointer) is stable. Runs on the same thread as heights (ctor / the
  // deferred reload poll), never during a step.
  _loadSurfaceWeights();
  return true;
}

// W·M SURFACE RESPONSE — load the RGBA class-weight capture the material also consumes and
// resample it to the collider grid. Fail-soft-but-LOUD: any problem (feature off, non-asset
// form, missing/corrupt EXR, odd bit depth) leaves _fricSampler._active=false with a named
// message — friction falls back to the plain base value, never garbage.
void BulletTerrainImpl::_loadSurfaceWeights() {
  _fricSampler._active = false;
  _fricSampler._weights.clear();
  const std::string& chan = _hfd._surface_weights_channel;
  if (chan.empty())
    return; // feature OFF (byte-identical to the pre-W path)

  // response row M[:,friction] (per-RGBA-class friction delta); pad/truncate to the 4 class slots.
  for (int i = 0; i < 4; i++)
    _fricSampler._rows[i] = (i < int(_hfd._friction_rows.size())) ? _hfd._friction_rows[i] : 0.0f;
  _fricSampler._extent = _resSize;

  if (_hfd._hf_asset.empty()) {
    printf("BulletShapeTerrain: surface_weights<%s> requires the asset-wired (hf_asset) form; "
           "friction modulation DISABLED\n",
           chan.c_str());
    return;
  }
  std::string base  = file::Path::expandPathString("<assetcache>/terrain/" + _hfd._hf_asset);
  std::string wpath = base + "/" + chan + ".exr";
  {
    std::error_code ec;
    if (not std::filesystem::exists(wpath, ec) or ec) {
      printf("BulletShapeTerrain: surface_weights<%s> EXR MISSING <%s> — the material's class-weight "
             "capture must materialize BEFORE the collider (declaration order = dependency order); "
             "friction modulation DISABLED\n",
             chan.c_str(), wpath.c_str());
      return;
    }
  }
  auto img = lev2::Image::createFromFile(wpath.c_str());
  if (not(img and img->_width > 0 and img->_width == img->_height)) {
    printf("BulletShapeTerrain: surface_weights<%s> EXR <%s> missing/corrupt (load failed); "
           "friction modulation DISABLED\n",
           chan.c_str(), wpath.c_str());
    return;
  }
  // resample to the collider grid with the SAME ringing-free TRIANGLE the heights use, so the
  // weight texel aligns with the height texel a body actually rests on.
  int dim = _gridDim;
  if (dim > 0 and int(img->_width) != dim) {
    auto ds = std::make_shared<lev2::Image>();
    ds->resampledOf(*img, dim, dim, lev2::Image::ResampleFilter::TRIANGLE);
    img = ds;
  } else {
    dim = int(img->_width);
  }
  const bool f32 = (img->_bytesPerChannel == 4);
  const bool u8  = (img->_bytesPerChannel == 1);
  if (not(f32 or u8)) {
    printf("BulletShapeTerrain: surface_weights<%s> EXR <%s> unexpected bytesPerChannel<%d> "
           "(need f32 EXR or 8-bit); friction modulation DISABLED\n",
           chan.c_str(), wpath.c_str(), int(img->_bytesPerChannel));
    return;
  }
  const int nc = int(img->_numcomponents);
  _fricSampler._weights.assign(size_t(dim) * size_t(dim), fvec4(0, 0, 0, 0));
  const float u8scale = 1.0f / 255.0f;
  for (int y = 0; y < dim; y++)
    for (int x = 0; x < dim; x++) {
      fvec4 w(0, 0, 0, 0);
      if (f32) {
        const float* p = img->pixel32f(x, y);
        w.x            = p[0];
        w.y            = (nc > 1) ? p[1] : 0.0f;
        w.z            = (nc > 2) ? p[2] : 0.0f;
        w.w            = (nc > 3) ? p[3] : 0.0f;
      } else {
        const uint8_t* p = img->pixel8(x, y);
        w.x              = float(p[0]) * u8scale;
        w.y              = (nc > 1) ? float(p[1]) * u8scale : 0.0f;
        w.z              = (nc > 2) ? float(p[2]) * u8scale : 0.0f;
        w.w              = (nc > 3) ? float(p[3]) * u8scale : 0.0f;
      }
      _fricSampler._weights[size_t(y) * size_t(dim) + size_t(x)] = w;
    }
  img.reset();
  _fricSampler._dim    = dim;
  _fricSampler._active = true;
  printf("BulletShapeTerrain: surface_weights<%s> loaded <%s> dim<%d> nc<%d> rows[%g %g %g %g]\n",
         chan.c_str(), wpath.c_str(), dim, nc,
         _fricSampler._rows[0], _fricSampler._rows[1], _fricSampler._rows[2], _fricSampler._rows[3]);
}

void BulletTerrainImpl::_reloadIfChanged() {
  if (not _needsReload())
    return;
  _loadHeightData();
}

void BulletTerrainImpl::consumePendingReload() {
  if (not _reload_pending.load())
    return;
  // S4 HOLD-LAST-FINAL (JUL13 §S4 consumer law): physics REQUIRES final. While the
  // height product has an ACTIVE (armed, not-yet-final) live re-bake publisher, the
  // collider must NOT re-read the plane — it keeps colliding against the LAST FINAL
  // heights and re-polls next update tick. The reload consumes exactly once, at final
  // (the "RELOADED heightmap" log line is the gate-6 observable). An absent registry
  // entry (no S4 producer ever armed this path) behaves as "not live" — today's path.
  if (not lev2::s4ProgressiveDisabled()) {
    auto live = lev2::liveFieldFind(lev2::liveFieldCanonicalKey(_resPath.c_str()));
    if (live and live->isLive()) {
      if (not _holdLogged) {
        printf("BulletShapeTerrain: re-bake IN PROGRESS for <%s> — HOLDING last-final "
               "heights (S4); reload deferred to bake-final\n", _resPath.c_str());
        _holdLogged = true;
      }
      return; // keep _reload_pending set — consume on a later tick, at final
    }
  }
  _holdLogged = false;
  if (_reload_pending.exchange(false))
    _reloadIfChanged();
}

///////////////////////////////////////////////////////////////////////////////

BulletTerrainImpl::~BulletTerrainImpl() {
  // both register (init_bullet_shape) and this destroy run on the update thread (component
  // activate/deactivate), same thread as the _onUpdate poll — no lock needed.
  if (_world)
    _world->_terrainReloadPolls.erase(this);
  // W·M: drop this collider's sampler from the contact-hook registry (safe if never inserted).
  _activeTerrainSamplers().erase(&_fricSampler);
}

///////////////////////////////////////////////////////////////////////////////

btCollisionShape* BulletTerrainImpl::init_bullet_shape(const ShapeCreateData& data) {
  _entity = data.mEntity;

  // register the deferred-reload poll: BulletSystem::_onUpdate drains it on the update thread,
  // consuming any rebake notification the msgrouter handler flagged (possibly from the GPU thread).
  _world = data.mWorld;
  if (_world)
    _world->_terrainReloadPolls[this] = [this]() { consumePendingReload(); };

  if (false == _loadok)
    return nullptr;

  int idimx = _gridDim; // lev2::Image grid (== _render_dimension when downsampled, else the EXR res)
  int idimz = _gridDim;

  float aspect            = float(idimz) / float(idimx);
  const float kworldsizeX = _resSize;
  const float kworldsizeZ = kworldsizeX * aspect;

  auto world_controller              = data.mWorld;
  const BulletSystemData& world_data = world_controller->GetWorldData();

  btVector3 grav = orkv3tobtv3(world_data.GetGravity());

  //////////////////////////////////////////
  // hook it up to bullet (heights from the lev2::Image, kept alive in _heightData)
  //////////////////////////////////////////

  float ftoth = _maxH - _minH;

  auto pdata = _heightData.data();

  // PADDED AABB span: the shape AABB is fixed at ctor time but a rebake reload mutates the
  // height data in place — pad symmetrically so modest relief growth stays collidable (the
  // symmetric pad keeps the AABB center == (minH+maxH)/2, which the compound offset uses).
  float pad = std::max(100.0f, 0.25f * ftoth);
  _aabbMinH = _minH - pad;
  _aabbMaxH = _maxH + pad;

  _terrainShape = new btHeightfieldTerrainShape(
      idimx,
      idimz,        // w,h
      (void*)pdata, // data
      ftoth,        // heightScale (ignored for PHY_FLOAT)
      _aabbMinH,
      _aabbMaxH,
      1,         // upAxis,
      PHY_FLOAT, // usefloat heightDataType,
      true);     // flipQuadEdges );

  //_terrainShape->setUseDiamondSubdivision(true);
  _terrainShape->setUseZigzagSubdivision(true);

  // W·M SURFACE RESPONSE: when the class-weight field loaded, install the global contact-added
  // hook (idempotent — a plain function-pointer store). The sampler is bound to the terrain
  // COMPOUND shape's user pointer below (the manifold reports the body's root shape, not the
  // heightfield). The per-body CF_CUSTOM_MATERIAL_CALLBACK flag is raised system-side in
  // _onActivateComponent (it needs the rigid body).
  if (_fricSampler._active) {
    _fricSampler._extent = _resSize; // square heightfield extent (meters)
    gContactAddedCallback = orkTerrainContactAddedCallback;
    printf("BulletShapeTerrain: per-contact friction modulation ARMED (surface_weights<%s>)\n",
           _hfd._surface_weights_channel.c_str());
  }

  float fworldsizeX = _resSize;
  float fworldsizeZ = _resSize; // square heightfield

  float scalex = fworldsizeX / float(idimx);
  float scalez = fworldsizeZ / float(idimz);
  // heights are TRUE METERS end-to-end (baked EXR channel 0) — no vertical rescale.
  float scaley = 1.0f;

  _terrainShape->setLocalScaling(btVector3(scalex, scaley, scalez));

  // btHeightfieldTerrainShape is CENTERED on its (padded) AABB in local space — wrap it in a
  // compound whose child offset restores ABSOLUTE METERS: with the owning entity at y=0, world
  // y == the baked height (the entity no longer pre-offsets by half a height-scale constant).
  auto compound = new btCompoundShape();
  btTransform xf;
  xf.setIdentity();
  xf.setOrigin(btVector3(0.0f, (_aabbMinH + _aabbMaxH) * 0.5f, 0.0f));
  compound->addChildShape(xf, _terrainShape);

  // W·M: bind the sampler to the ROOT (compound) shape the manifold reports, and register it
  // so the contact-added hook can find + validate it (heightfield cell contacts surface as
  // triangle children with no user pointer, so the terrain must be identified body-side).
  if (_fricSampler._active) {
    compound->setUserPointer(&_fricSampler);
    _activeTerrainSamplers().insert(&_fricSampler);
  }

  printf("_terrainShape<%p> aabb[%g..%g]m\n", _terrainShape, _aabbMinH, _aabbMaxH);

  // SELF-DEFENSE: publish the terrain's world-Y surface span (true meters, entity at y=0) so the
  // character controller can name an underground spawn and recover a fall to walkable ground.
  if (_world)
    _world->_registerTerrainYSpan(_minH, _maxH);

  return compound;
}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeTerrainData::describeX(object::ObjectClass* clazz) {
  ////////
  clazz->directProperty("HeightMap", &BulletShapeTerrainData::_heightMapPath)
    ->annotate<std::string>( "editor.filetype", "png")
    ->annotate<std::string>( "editor.factory", "ged.factory.filelist");
  ////////
  clazz->floatProperty("WorldHeight", float_range{0,10000}, &BulletShapeTerrainData::_worldHeight);
  clazz->floatProperty("WorldSize", float_range{1,20000}, &BulletShapeTerrainData::_worldSize);
  // E.2-walk: the asset-wired form — resolves the baked HeightField artifact + manifest
  // scale (extent_m; heights are TRUE METERS) at shape creation; overrides the direct props above.
  clazz->directProperty("hf_asset", &BulletShapeTerrainData::_hf_asset);
  clazz->directProperty("render_dimension", &BulletShapeTerrainData::_render_dimension);
  // W·M SURFACE RESPONSE — physics leg. surface_weights_channel names the RGBA class-weight
  // capture (empty = feature off); friction_rows is M[:,friction] (per-class friction deltas).
  clazz->directProperty("surface_weights_channel", &BulletShapeTerrainData::_surface_weights_channel);
  clazz->directVectorProperty("friction_rows", &BulletShapeTerrainData::_friction_rows);
  //clazz->directProperty("VisualData", &BulletShapeTerrainData::_visualDataAccessor);
  ////////
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeTerrainData::BulletShapeTerrainData()
    : _heightMapPath("none") {

  _shapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval = new BulletShapeBaseInst(this);
    auto impl = std::make_shared<BulletTerrainImpl>(*this);
    rval->_impl.set<terrain_impl_ptr_t>(impl);

    rval->_collisionShape = impl->init_bullet_shape(data);
    // W·M: raise the flag so the system tags the terrain rigid body CF_CUSTOM_MATERIAL_CALLBACK.
    rval->_wantsCustomMaterialCallback = impl->_fricSampler._active;

    ////////////////////////////////////////////////////////////////////
    // create drawable
    ////////////////////////////////////////////////////////////////////
    //impl->_hfinstance               = _visualData.createInstance();
    //impl->_hfinstance->_worldHeight = this->WorldHeight();
    //impl->_hfinstance->_worldSizeXZ = this->_worldSize;
    //rval->_drawable = impl->_hfinstance->createCallbackDrawable();
    //rval->_drawable->SetOwner(data.mEntity);
    //data.mEntity->addDrawableToDefaultLayer(rval->_drawable);
    ////////////////////////////////////////////////////////////////////
    //msgrouter::channel("bshdchanged")->post(this);

    return rval;
  };

  _shapeFactory._invalidate = [](BulletShapeBaseData* data) {
    auto as_bshd = dynamic_cast<BulletShapeTerrainData*>(data);
    assert(as_bshd != nullptr);
    msgrouter::channel("bshdchanged")->post(nullptr);
  };
}

bool BulletShapeTerrainData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeTerrainData::~BulletShapeTerrainData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
