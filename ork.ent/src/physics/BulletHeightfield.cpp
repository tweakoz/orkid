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
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
#include <BulletCollision/CollisionShapes/btHeightfieldTerrainShape.h>
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/util/endian.h>
#include <pkg/ent/bullet.h>
#include <pkg/ent/HeightFieldDrawable.h>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeHeightfieldData, "BulletShapeHeightfieldData");

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////



struct BulletHeightfieldImpl {
  int mGridSize = 0;
  bool _loadok = false;
  Entity* _entity = nullptr;
  btHeightfieldTerrainShape* _terrainShape = nullptr;

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

    printf("Load Heightmap<%s>\n", _hfd.HeightMapPath().c_str());

    _phyheightmap = std::make_shared<HeightMap>(0,0) ;

    _loadok = _phyheightmap->Load(_hfd.HeightMapPath());

    int idimx = _phyheightmap->GetGridSizeX();
    int idimz = _phyheightmap->GetGridSizeZ();
    printf("idimx<%d> idimz<%d>\n", idimx, idimz);
    // float aspect = float(idimz)/float(idimx);
    const float kworldsizeX = _hfd.WorldSize();
    const float kworldsizeZ = kworldsizeX;

    _phyheightmap->SetWorldSize(kworldsizeX, kworldsizeZ);
    _phyheightmap->SetWorldHeight(_hfd.WorldHeight());
  });

  _subscriber->_handler(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

BulletHeightfieldImpl::~BulletHeightfieldImpl(){

}

///////////////////////////////////////////////////////////////////////////////

btHeightfieldTerrainShape* BulletHeightfieldImpl::init_bullet_shape(const ShapeCreateData& data) {
  _entity = data.mEntity;

  if (false == _loadok)
    return nullptr;

  int idimx = _phyheightmap->GetGridSizeX();
  int idimz = _phyheightmap->GetGridSizeZ();

  float aspect = float(idimz) / float(idimx);
  const float kworldsizeX = _hfd.WorldSize();
  const float kworldsizeZ = kworldsizeX * aspect;

  auto world_controller = data.mWorld;
  const BulletSystemData& world_data = world_controller->GetWorldData();

  btVector3 grav = !world_data.GetGravity();

  //////////////////////////////////////////
  // hook it up to bullet if present
  //////////////////////////////////////////

  float ftoth = _phyheightmap->GetMaxHeight() - _phyheightmap->GetMinHeight();

  auto pdata = _phyheightmap->GetHeightData();

  _terrainShape = new btHeightfieldTerrainShape(idimx, idimz, // w,h
                                                (void*)pdata, // data
                                                ftoth,        // heightScale
                                                _phyheightmap->GetMinHeight(), _phyheightmap->GetMaxHeight(),
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
  reflect::RegisterProperty("HeightMap", &BulletShapeHeightfieldData::GetHeightMapName,
                            &BulletShapeHeightfieldData::SetHeightMapName);
  reflect::RegisterProperty("VizHeightMap", &BulletShapeHeightfieldData::GetVizHeightMapName,
                            &BulletShapeHeightfieldData::SetVizHeightMapName);
  reflect::RegisterProperty("WorldHeight", &BulletShapeHeightfieldData::mWorldHeight);
  reflect::RegisterProperty("WorldSize", &BulletShapeHeightfieldData::mWorldSize);
  reflect::RegisterProperty("SphericalLightMap", &BulletShapeHeightfieldData::GetTextureAccessor,
                            &BulletShapeHeightfieldData::SetTextureAccessor);
  reflect::RegisterProperty("VisualOffset", &BulletShapeHeightfieldData::mVisualOffset);

  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("HeightMap", "editor.class", "ged.factory.filelist");
  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("HeightMap", "editor.filetype", "png");

  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("VizHeightMap", "editor.class", "ged.factory.filelist");
  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("VizHeightMap", "editor.filetype", "png");

  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("WorldHeight", "editor.range.min", "0");
  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("WorldHeight", "editor.range.max", "10000");

  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("WorldSize", "editor.range.min", "1.0f");
  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("WorldSize", "editor.range.max", "20000.0");

  ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("SphericalLightMap", "editor.class", "ged.factory.assetlist");
  ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("SphericalLightMap", "editor.assettype", "lev2tex");
  ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>("SphericalLightMap", "editor.assetclass", "lev2tex");
}

///////////////////////////////////////////////////////////////////////////////
void BulletShapeHeightfieldData::SetTextureAccessor(ork::rtti::ICastable* const& tex) {
  _spherelightmap = tex ? ork::rtti::autocast(tex) : 0;
}
void BulletShapeHeightfieldData::GetTextureAccessor(ork::rtti::ICastable*& tex) const { tex = _spherelightmap; }
///////////////////////////////////////////////////////////////////////////////

BulletShapeHeightfieldData::BulletShapeHeightfieldData()
    : mHeightMapName("none"), mVizHeightMapName("non"), mWorldHeight(1000.0f), mWorldSize(1000.0f), _spherelightmap(nullptr) {

  mShapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval = new BulletShapeBaseInst(this);
    auto impl = std::make_shared<BulletHeightfieldImpl>(*this);
    rval->_impl.Set<std::shared_ptr<BulletHeightfieldImpl>>(impl);

    rval->mCollisionShape = impl->init_bullet_shape(data);

    ////////////////////////////////////////////////////////////////////
    // create drawable
    ////////////////////////////////////////////////////////////////////

    impl->_drawable = std::make_shared<HeightFieldDrawable>();
    impl->_drawable->_hfpath = this->VizHeightMapPath();
    impl->_drawable->_visualOffset = mVisualOffset;
    impl->_drawable->_worldHeight = this->WorldHeight();
    impl->_drawable->_worldSizeXZ = this->WorldSize();
    impl->_drawable->_sphericalenvmap = _spherelightmap;

    auto raw_drawable = impl->_drawable->create();
    raw_drawable->SetOwner(data.mEntity);
    data.mEntity->AddDrawable(AddPooledLiteral("Default"), raw_drawable);

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
void BulletShapeHeightfieldData::SetVizHeightMapName(file::Path const& lmap) { mVizHeightMapName = lmap; }
void BulletShapeHeightfieldData::GetVizHeightMapName(file::Path& lmap) const { lmap = mVizHeightMapName; }

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
