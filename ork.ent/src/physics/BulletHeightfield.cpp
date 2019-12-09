///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/util/endian.h>
#include <pkg/ent/HeightFieldDrawable.h>
#include <pkg/ent/bullet.h>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeHeightfieldData, "BulletShapeHeightfieldData");

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

struct BulletHeightfieldImpl {
  int mGridSize                            = 0;
  bool _loadok                             = false;
  Entity* _entity                          = nullptr;
  btHeightfieldTerrainShape* _terrainShape = nullptr;
  file::Path _curhfpath;

  std::shared_ptr<HeightMap> _phyheightmap;
  hfdrawableptr_t _drawable;
  const BulletShapeHeightfieldData& _hfd;
  msgrouter::subscriber_t _subscriber;

  BulletHeightfieldImpl(const BulletShapeHeightfieldData& data);
  ~BulletHeightfieldImpl();

  btHeightfieldTerrainShape* init_bullet_shape(const ShapeCreateData& data);
};

///////////////////////////////////////////////////////////////////////////////

BulletHeightfieldImpl::BulletHeightfieldImpl(const BulletShapeHeightfieldData& data)
    : _hfd(data) {
  _subscriber = msgrouter::channel("bshdchanged")->subscribe([=](msgrouter::content_t c) {
    if (_curhfpath != _hfd.HeightMapPath()) {
      printf("Load Heightmap<%s>\n", _hfd.HeightMapPath().c_str());
      _phyheightmap = std::make_shared<HeightMap>(0, 0);
      _loadok       = _phyheightmap->Load(_hfd.HeightMapPath());
      _curhfpath    = _hfd.HeightMapPath();
      int idimx = _phyheightmap->GetGridSizeX();
      int idimz = _phyheightmap->GetGridSizeZ();
      printf("idimx<%d> idimz<%d>\n", idimx, idimz);
      assert(idimx==idimz);
    }
    _phyheightmap->SetWorldSize(_hfd.WorldSize(), _hfd.WorldSize());
    _phyheightmap->SetWorldHeight(_hfd.WorldHeight());
  });

  _subscriber->_handler(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

BulletHeightfieldImpl::~BulletHeightfieldImpl() {}

///////////////////////////////////////////////////////////////////////////////

btHeightfieldTerrainShape* BulletHeightfieldImpl::init_bullet_shape(const ShapeCreateData& data) {
  _entity = data.mEntity;

  if (false == _loadok)
    return nullptr;

  int idimx = _phyheightmap->GetGridSizeX();
  int idimz = _phyheightmap->GetGridSizeZ();

  float aspect            = float(idimz) / float(idimx);
  const float kworldsizeX = _hfd.WorldSize();
  const float kworldsizeZ = kworldsizeX * aspect;

  auto world_controller              = data.mWorld;
  const BulletSystemData& world_data = world_controller->GetWorldData();

  btVector3 grav = !world_data.GetGravity();

  //////////////////////////////////////////
  // hook it up to bullet if present
  //////////////////////////////////////////

  float ftoth = _phyheightmap->GetMaxHeight() - _phyheightmap->GetMinHeight();

  auto pdata = _phyheightmap->GetHeightData();

  _terrainShape = new btHeightfieldTerrainShape(idimx,
                                                idimz,        // w,h
                                                (void*)pdata, // data
                                                ftoth,        // heightScale
                                                _phyheightmap->GetMinHeight(),
                                                _phyheightmap->GetMaxHeight(),
                                                1,         // upAxis,
                                                PHY_FLOAT, // usefloat heightDataType,
                                                true);     // flipQuadEdges );

  //_terrainShape->setUseDiamondSubdivision(true);
  _terrainShape->setUseZigzagSubdivision(true);

  float fworldsizeX = _phyheightmap->GetWorldSizeX();
  float fworldsizeZ = _phyheightmap->GetWorldSizeZ();

  float scalex = fworldsizeX / float(idimx);
  float scalez = fworldsizeZ / float(idimz);
  float scaley = 1.0f;

  _terrainShape->setLocalScaling(btVector3(scalex, _phyheightmap->GetWorldHeight(), scalez));

  printf("_terrainShape<%p>\n", _terrainShape);

  return _terrainShape;
}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeHeightfieldData::Describe() {
  reflect::RegisterProperty(
      "HeightMap", &BulletShapeHeightfieldData::GetHeightMapName, &BulletShapeHeightfieldData::SetHeightMapName);
  reflect::RegisterProperty("WorldHeight", &BulletShapeHeightfieldData::mWorldHeight);
  reflect::RegisterProperty("WorldSize", &BulletShapeHeightfieldData::mWorldSize);

  reflect::RegisterProperty("VisualData",&BulletShapeHeightfieldData::_visualDataAccessor);

  reflect::annotatePropertyForEditor<BulletShapeHeightfieldData>("HeightMap", "editor.class", "ged.factory.filelist");
  reflect::annotatePropertyForEditor<BulletShapeHeightfieldData>("HeightMap", "editor.filetype", "png");

  reflect::annotatePropertyForEditor<BulletShapeHeightfieldData>("WorldHeight", "editor.range.min", "0");
  reflect::annotatePropertyForEditor<BulletShapeHeightfieldData>("WorldHeight", "editor.range.max", "10000");

  reflect::annotatePropertyForEditor<BulletShapeHeightfieldData>("WorldSize", "editor.range.min", "1.0f");
  reflect::annotatePropertyForEditor<BulletShapeHeightfieldData>("WorldSize", "editor.range.max", "20000.0");
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeHeightfieldData::BulletShapeHeightfieldData()
    : mHeightMapName("none")
    , mWorldHeight(1000.0f)
    , mWorldSize(1000.0f) {

  mShapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval = new BulletShapeBaseInst(this);
    auto impl = std::make_shared<BulletHeightfieldImpl>(*this);
    rval->_impl.Set<std::shared_ptr<BulletHeightfieldImpl>>(impl);

    rval->mCollisionShape = impl->init_bullet_shape(data);

    ////////////////////////////////////////////////////////////////////
    // create drawable
    ////////////////////////////////////////////////////////////////////

    impl->_drawable                   = _visualData.createDrawable();
    impl->_drawable->_worldHeight     = this->WorldHeight();
    impl->_drawable->_worldSizeXZ     = this->WorldSize();

    auto raw_drawable = impl->_drawable->create();
    raw_drawable->SetOwner(data.mEntity);
    data.mEntity->addDrawableToDefaultLayer(raw_drawable);

    ////////////////////////////////////////////////////////////////////

    msgrouter::channel("bshdchanged")->post(this);

    return rval;
  };

  mShapeFactory._invalidate = [](BulletShapeBaseData* data) {
    auto as_bshd = dynamic_cast<BulletShapeHeightfieldData*>(data);
    assert(as_bshd != nullptr);
    msgrouter::channel("bshdchanged")->post(nullptr);
  };
}

bool BulletShapeHeightfieldData::PostDeserialize(reflect::IDeserializer&) { return true; }

///////////////////////////////////////////////////////////////////////////////

BulletShapeHeightfieldData::~BulletShapeHeightfieldData() {}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeHeightfieldData::SetHeightMapName(file::Path const& lmap) { mHeightMapName = lmap; }
void BulletShapeHeightfieldData::GetHeightMapName(file::Path& lmap) const { lmap = mHeightMapName; }

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
