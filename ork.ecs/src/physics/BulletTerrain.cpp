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
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include "bullet_impl.h"
#include <rapidjson/document.h> // E.2-walk: hf_asset manifest parse
#include <fstream>
#include <sstream>
#include <atomic>
#include <filesystem>
#include <algorithm>
///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::ecs::BulletShapeTerrainData, "BulletShapeTerrainData");
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
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

  BulletTerrainImpl(const BulletShapeTerrainData& data);
  ~BulletTerrainImpl();

  btCollisionShape* init_bullet_shape(const ShapeCreateData& data);

  bool _statStamp(uint64_t& em, uint64_t& es, uint64_t& mm, uint64_t& ms) const;
  bool _needsReload() const;
  bool _loadHeightData();       // first load vs in-place reload keyed off _loadok
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
  return true;
}

void BulletTerrainImpl::_reloadIfChanged() {
  if (not _needsReload())
    return;
  _loadHeightData();
}

void BulletTerrainImpl::consumePendingReload() {
  if (_reload_pending.exchange(false))
    _reloadIfChanged();
}

///////////////////////////////////////////////////////////////////////////////

BulletTerrainImpl::~BulletTerrainImpl() {
  // both register (init_bullet_shape) and this destroy run on the update thread (component
  // activate/deactivate), same thread as the _onUpdate poll — no lock needed.
  if (_world)
    _world->_terrainReloadPolls.erase(this);
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
