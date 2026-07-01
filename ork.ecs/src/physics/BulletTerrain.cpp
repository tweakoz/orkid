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
  float _resHeight = 1000.0f;   // resolved world height (manifest height_m when asset-wired)

  // E.2-walk: the collider's heightmap is loaded as a TRANSIENT lev2::Image (EXR via OIIO),
  // high-quality-resampled DOWN to _render_dimension when set; its channel-0 heights are copied into
  // _heightData and the Image is then freed (only _heightData persists). _heightData is kept ALIVE here
  // because btHeightfieldTerrainShape stores the data POINTER (it does not copy).
  std::vector<float> _heightData;
  int   _gridDim = 0;
  float _minH    = 0.0f;
  float _maxH    = 1.0f;
  hfdrawableinstptr_t _hfinstance;
  const BulletShapeTerrainData& _hfd;
  msgrouter::subscriber_t _subscriber;

  BulletTerrainImpl(const BulletShapeTerrainData& data);
  ~BulletTerrainImpl();

  btHeightfieldTerrainShape* init_bullet_shape(const ShapeCreateData& data);
};

using terrain_impl_ptr_t = std::shared_ptr<BulletTerrainImpl>;

///////////////////////////////////////////////////////////////////////////////

BulletTerrainImpl::BulletTerrainImpl(const BulletShapeTerrainData& data)
    : _hfd(data) {
  // E.2-walk: resolve the ASSET-WIRED form first. hf_asset names a baked HeightField —
  // the height image is the SAME normalized [0,1] EXR the chunk renderer consumes, and
  // worldSize/worldHeight come from the .terrain.json manifest, so physics collides with
  // exactly what renders (one declaration). Direct _heightMapPath/_worldSize/_worldHeight
  // remain the override when hf_asset is empty.
  _resPath   = _hfd._heightMapPath;
  _resSize   = _hfd._worldSize;
  _resHeight = _hfd._worldHeight;
  if (not _hfd._hf_asset.empty()) {
    std::string base = file::Path::expandPathString("<assetcache>/terrain/" + _hfd._hf_asset);
    _resPath         = file::Path((base + "/height.exr").c_str());
    std::string manifest_path = base + "/" + _hfd._hf_asset + ".terrain.json";
    std::ifstream mf(manifest_path);
    if (mf.good()) {
      std::stringstream mstrm;
      mstrm << mf.rdbuf();
      rapidjson::Document doc;
      doc.Parse(mstrm.str().c_str());
      OrkAssert(not doc.HasParseError());
      _resSize   = doc["scale"]["extent_m"].GetFloat();
      _resHeight = doc["scale"]["height_m"].GetFloat();
      printf("BulletShapeTerrain: hf_asset<%s> -> <%s> extent<%g> height<%g>\n",
             _hfd._hf_asset.c_str(), _resPath.c_str(), _resSize, _resHeight);
    } else {
      printf("BulletShapeTerrain: hf_asset<%s> manifest MISSING <%s> — the HeightField asset "
             "must materialize BEFORE the physics shape (declaration order = dependency order)\n",
             _hfd._hf_asset.c_str(), manifest_path.c_str());
      OrkAssert(false);
    }
  }
  _subscriber = msgrouter::channel("bshdchanged")->subscribe([=](msgrouter::content_t c) {
    if (_curhfpath != _resPath) {
      // load the heightmap as a lev2::Image (EXR -> R32F / RGBA32F via OIIO)
      auto img = lev2::Image::createFromFile(_resPath.c_str());
      OrkAssert(img and img->_width > 0 and img->_width == img->_height);
      int src_dim = int(img->_width);
      int rdim    = _hfd._render_dimension;
      // high-quality DOWNsample to the render grid (ringing-free TRIANGLE — no spurious collision bumps),
      // so the collider surface == the visible downsampled mesh (terrain render_dimension).
      if (rdim > 0 and rdim < src_dim) {
        auto ds = std::make_shared<lev2::Image>();
        ds->resampledOf(*img, rdim, rdim, lev2::Image::ResampleFilter::TRIANGLE);
        img = ds;
        printf("BulletShapeTerrain: collider resampled %d -> %d (Image::resampledOf TRIANGLE)\n", src_dim, rdim);
      }
      const bool f32 = (img->_bytesPerChannel == 4);
      OrkAssert(f32 or img->_bytesPerChannel == 1); // float EXR (R32F/RGBA32F) or 8-bit PNG (legacy direct path)
      _gridDim     = int(img->_width);
      const int nc = int(img->_numcomponents);
      _heightData.resize(size_t(_gridDim) * size_t(_gridDim));
      _minH = 1e30f;
      _maxH = -1e30f;
      for (int y = 0; y < _gridDim; y++)
        for (int x = 0; x < _gridDim; x++) {
          float h = f32 ? img->pixel32f(x, y)[0]                            // channel 0 = height
                        : (float(img->pixel8(x, y)[0]) * (1.0f / 255.0f));  // 8-bit -> normalized [0,1]
          _heightData[size_t(y) * _gridDim + x] = h;
          _minH                                 = std::min(_minH, h);
          _maxH                                 = std::max(_maxH, h);
        }
      img.reset(); // heights are copied into _heightData -> free the Image buffer now (only _heightData persists)
      _loadok    = true;
      _curhfpath = _resPath;
      printf("BulletShapeTerrain: Image heightmap<%s> dim<%d> nc<%d> min<%g> max<%g>\n",
             _resPath.c_str(), _gridDim, nc, _minH, _maxH);
    }
  });

  _subscriber->_handler(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

BulletTerrainImpl::~BulletTerrainImpl() {
}

///////////////////////////////////////////////////////////////////////////////

btHeightfieldTerrainShape* BulletTerrainImpl::init_bullet_shape(const ShapeCreateData& data) {
  _entity = data.mEntity;

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

  _terrainShape = new btHeightfieldTerrainShape(
      idimx,
      idimz,        // w,h
      (void*)pdata, // data
      ftoth,        // heightScale
      _minH,
      _maxH,
      1,         // upAxis,
      PHY_FLOAT, // usefloat heightDataType,
      true);     // flipQuadEdges );

  //_terrainShape->setUseDiamondSubdivision(true);
  _terrainShape->setUseZigzagSubdivision(true);

  float fworldsizeX = _resSize;
  float fworldsizeZ = _resSize; // square heightfield

  float scalex = fworldsizeX / float(idimx);
  float scalez = fworldsizeZ / float(idimz);
  float scaley = 1.0f;

  _terrainShape->setLocalScaling(btVector3(scalex, _resHeight, scalez));

  printf("_terrainShape<%p>\n", _terrainShape);

  return _terrainShape;
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
  // scale (extent_m/height_m) at shape creation; overrides the three direct props above.
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
