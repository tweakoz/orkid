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
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include "bullet_impl.h"
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

  std::shared_ptr<HeightMap> _phyheightmap;
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
  _subscriber = msgrouter::channel("bshdchanged")->subscribe([=](msgrouter::content_t c) {
    if (_curhfpath != _hfd._heightMapPath) {
      printf("Load Heightmap<%s>\n", _hfd._heightMapPath.c_str());
      _phyheightmap = std::make_shared<HeightMap>(0, 0);
      _loadok       = _phyheightmap->Load(_hfd._heightMapPath);
      _curhfpath    = _hfd._heightMapPath;
      int idimx     = _phyheightmap->GetGridSizeX();
      int idimz     = _phyheightmap->GetGridSizeZ();
      printf("idimx<%d> idimz<%d>\n", idimx, idimz);
      assert(idimx == idimz);
    }
    _phyheightmap->SetWorldSize(_hfd._worldSize, _hfd._worldSize);
    _phyheightmap->SetWorldHeight(_hfd._worldHeight);
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

  int idimx = _phyheightmap->GetGridSizeX();
  int idimz = _phyheightmap->GetGridSizeZ();

  float aspect            = float(idimz) / float(idimx);
  const float kworldsizeX = _hfd._worldSize;
  const float kworldsizeZ = kworldsizeX * aspect;

  auto world_controller              = data.mWorld;
  const BulletSystemData& world_data = world_controller->GetWorldData();

  btVector3 grav = orkv3tobtv3(world_data.GetGravity());

  //////////////////////////////////////////
  // hook it up to bullet if present
  //////////////////////////////////////////

  float ftoth = _phyheightmap->GetMaxHeight() - _phyheightmap->GetMinHeight();

  auto pdata = _phyheightmap->GetHeightData();

  _terrainShape = new btHeightfieldTerrainShape(
      idimx,
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

void BulletShapeTerrainData::describeX(object::ObjectClass* clazz) {
  ////////
  clazz->directProperty("HeightMap", &BulletShapeTerrainData::_heightMapPath)
    ->annotate<std::string>( "editor.filetype", "png")
    ->annotate<std::string>( "editor.factory", "ged.factory.filelist");
  ////////
  clazz->floatProperty("WorldHeight", float_range{0,10000}, &BulletShapeTerrainData::_worldHeight);
  clazz->floatProperty("WorldSize", float_range{1,20000}, &BulletShapeTerrainData::_worldSize);
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

bool BulletShapeTerrainData::postDeserialize(reflect::serdes::IDeserializer&) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeTerrainData::~BulletShapeTerrainData() {
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
///////////////////////////////////////////////////////////////////////////////
