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
#include <ork/lev2/gfx/renderer.h>
#include <ork/util/endian.h>
#include <pkg/ent/bullet.h>

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeHeightfieldData,
                             "BulletShapeHeightfieldData");
///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

struct BulletHeightfieldImpl {
  int mGridSize = 0;
  bool _loadok = false;
  bool _initViz = true;
  Entity* _entity = nullptr;
  lev2::GfxMaterial3DSolid* mTerrainMtl = nullptr;
  lev2::Texture* _heightmapTexture = nullptr;
  btHeightfieldTerrainShape* _terrainShape = nullptr;

  fvec3 _aabbmin;
  fvec3 _aabbmax;

  orkmap<int, TerVtxBuffersType *> vtxbufmap;
  HeightMap _heightmap;
  const BulletShapeHeightfieldData &_hfd;
  msgrouter::subscriber_t _subscriber;

  BulletHeightfieldImpl(const BulletShapeHeightfieldData &data);

  void init_visgeom(lev2::GfxTarget *ptarg);
  btHeightfieldTerrainShape *init_bullet_shape(const ShapeCreateData &data);
};

///////////////////////////////////////////////////////////////////////////////

BulletHeightfieldImpl::BulletHeightfieldImpl(
    const BulletShapeHeightfieldData &data)
    : _hfd(data)
    , _heightmap(0, 0){
  _subscriber =
      msgrouter::channel("bshdchanged")->subscribe([=](msgrouter::content_t c) {
        // auto bshd = c.Get<BulletShapeHeightfieldData*>();
        this->_initViz = true;

        printf("Load Heightmap<%s>\n", _hfd.HeightMapPath().c_str());

        _loadok = _heightmap.Load(_hfd.HeightMapPath());

        int idimx = _heightmap.GetGridSizeX();
        int idimz = _heightmap.GetGridSizeZ();
        printf("idimx<%d> idimz<%d>\n", idimx, idimz);
        // float aspect = float(idimz)/float(idimx);
        const float kworldsizeX = _hfd.WorldSize();
        const float kworldsizeZ = kworldsizeX;

        _heightmap.SetWorldSize(kworldsizeX, kworldsizeZ);
        _heightmap.SetWorldHeight(_hfd.WorldHeight());
      });

  _subscriber->_handler(nullptr);
}

///////////////////////////////////////////////////////////////////////////////

btHeightfieldTerrainShape *
BulletHeightfieldImpl::init_bullet_shape(const ShapeCreateData &data) {
  _entity = data.mEntity;

  if (false == _loadok)
    return nullptr;

  int idimx = _heightmap.GetGridSizeX();
  int idimz = _heightmap.GetGridSizeZ();

  float aspect = float(idimz) / float(idimx);
  const float kworldsizeX = _hfd.WorldSize();
  const float kworldsizeZ = kworldsizeX * aspect;

  auto world_controller = data.mWorld;
  const BulletSystemData &world_data =
      world_controller->GetWorldData();

  btVector3 grav = !world_data.GetGravity();

  //////////////////////////////////////////
  // hook it up to bullet if present
  //////////////////////////////////////////

  float ftoth = _heightmap.GetMaxHeight() - _heightmap.GetMinHeight();

  auto pdata = _heightmap.GetHeightData();

  _terrainShape =
      new btHeightfieldTerrainShape(idimx, idimz,  // w,h
                                    (void *)pdata, // data
                                    ftoth,
                                    1,      // upAxis,
                                    true,   // usefloat heightDataType,
                                    false); // flipQuadEdges );

  _terrainShape->setUseDiamondSubdivision(true);

  float fworldsizeX = _heightmap.GetWorldSizeX();
  float fworldsizeZ = _heightmap.GetWorldSizeZ();

  float scalex = fworldsizeX / float(idimx);
  float scalez = fworldsizeZ / float(idimz);
  float scaley = 1.0f;

  _terrainShape->setLocalScaling(
      btVector3(scalex, _heightmap.GetWorldHeight(), scalez));

  return _terrainShape;
}

///////////////////////////////////////////////////////////////////////////////

void BulletHeightfieldImpl::init_visgeom(lev2::GfxTarget *ptarg) {
  if (false == _initViz)
    return;

  _initViz = false;

  auto sphmap = _hfd.GetSphereMap();
  auto sphmaptex = (sphmap != nullptr) ? sphmap->GetTexture() : nullptr;

  mTerrainMtl =
      new lev2::GfxMaterial3DSolid(ptarg, "orkshader://terrain", "terrain1");
  mTerrainMtl->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);


  orkprintf("ComputingGeometry\n");

  vtxbufmap.clear();

  const int iglX = _heightmap.GetGridSizeX();
  const int iglZ = _heightmap.GetGridSizeZ();

  const float kworldsizeX = _heightmap.GetWorldSizeX();
  const float kworldsizeZ = _heightmap.GetWorldSizeZ();

  const int terrain_ngrids = iglX * iglZ;

  ////////////////////////////////////////////////////////////////
  // create and fill in gpu texture
  ////////////////////////////////////////////////////////////////

  _heightmapTexture = lev2::Texture::CreateBlank(iglX,iglZ,lev2::EBUFFMT_F32);
  auto pfloattex = (float*) _heightmapTexture->GetTexData();
  assert(pfloattex!=nullptr);

  fvec2 origin(0,0);

  auto heightdata = (float*) _heightmap.GetHeightData();

  for( int z=0; z<iglZ; z++ ){
    int zz = z-(iglZ>>1);
    float fzz = float(zz)/float(iglZ>>1); // -1 .. 1
    for( int x=0; x<iglX; x++ ){
      int xx = x-(iglX>>1);
      float fxx = float(xx)/float(iglX>>1); // -1 .. 1
      fvec2 pos2d(fxx,fzz);
      float d = (pos2d-origin).Mag();
      float dpow = powf(d,3);
      size_t index = z*iglX+x;
      float h = heightdata[index];
      pfloattex[index]=float(h);
    }

  }

  ptarg->TXI()->initTextureFromData(_heightmapTexture,true);

  ////////////////////////////////////////////////////////////////

  auto bbctr = (_aabbmin + _aabbmax) * 0.5f;
  auto bbdim = (_aabbmax - _aabbmin);

  printf("IGLX<%d> IGLZ<%d> kworldsizeXZ<%f %f>\n", iglX, iglZ, kworldsizeX,
         kworldsizeZ);
  printf("bbmin<%f %f %f>\n", _aabbmin.GetX(), _aabbmin.GetY(),
         _aabbmin.GetZ());
  printf("bbmax<%f %f %f>\n", _aabbmax.GetX(), _aabbmax.GetY(),
         _aabbmax.GetZ());
  printf("bbctr<%f %f %f>\n", bbctr.GetX(), bbctr.GetY(), bbctr.GetZ());
  printf("bbdim<%f %f %f>\n", bbdim.GetX(), bbdim.GetY(), bbdim.GetZ());

  AABox aab;
  aab.BeginGrow();

  enum PatchType {
    PT_A = 0,
    PT_BT,
    PT_BB,
    PT_BL,
    PT_BR,
    PT_C,
  };

  struct Patch {
    PatchType _type;
    int _lod;
    int _x, _z;
  };

  std::vector<Patch> _patches;

  ////////////////////////////////////////////

  auto patch_row = [&](PatchType t, int lod, int x1, int x2, int z) {
    int step = 1 << lod;
    for (int x = x1; x < x2; x += step) {
      Patch p;
      p._type = t;
      p._x = x;
      p._z = z;
      p._lod = lod;
      _patches.push_back(p);
    }
  };

  ////////////////////////////////////////////

  auto patch_column = [&](PatchType t, int lod, int x, int z1, int z2) {
    int step = 1 << lod;
    for (int z = z1; z < z2; z += step) {
      Patch p;
      p._type = t;
      p._x = x;
      p._z = z;
      p._lod = lod;
      _patches.push_back(p);
    }
  };

  ////////////////////////////////////////////

  auto patch_block = [&](PatchType t, int lod, int x1, int z1, int x2, int z2) {
    if (x1 == x2)
      x2++;
    if (z1 == z2)
      z2++;

    int step = 1 << lod;

    for (int x = x1; x < x2; x += step) {
      for (int z = z1; z < z2; z += step) {
        Patch p;
        p._type = t;
        p._x = x;
        p._z = z;
        p._lod = lod;
        _patches.push_back(p);
      }
    }
  };

  ////////////////////////////////////////////

  auto single_patch = [&](PatchType t, int lod, int x, int z) {
    Patch p;
    p._type = t;
    p._x = x;
    p._z = z;
    p._lod = lod;
    _patches.push_back(p);
  };

  ////////////////////////////////////////////

  struct Iter {
    int _lod = -1;
    int _acount = 0;
  };

  std::vector<Iter> iters;

  iters.push_back(Iter{0, 256});  // 256*2 = 512m
  iters.push_back(Iter{1, 128});  // 128*4 = 512m   tot(1024m)
  iters.push_back(Iter{2, 128});  // 128*8 = 1024m  tot(2048)
  iters.push_back(Iter{3, 128});  // 128*16 = 2048m tot(4096m) - 2.56mi
  //iters.push_back(Iter{4, 128});
  //iters.push_back(Iter{5, 128});


  int iprevouterd2 = 0;

  for (auto iter : iters) {

    assert(iter._acount >= 4);       // at least one inner
    assert((iter._acount & 1) == 0); // must also be even

    int lod = iter._lod;
    int step = 1 << lod;

    int newouterd2 = iprevouterd2 + (iter._acount * step / 2);

    int sectdim = step;
    int sectdimp1 = sectdim + step;
    int a_start = -(iter._acount << lod) / 2;
    int a_end = a_start + (iter._acount - 1) * step;
    int a_z = a_start;

    if (0 == lod) {
      patch_block(PT_A, lod, // Full Sector
                  -newouterd2+step, -newouterd2 + step,
                  +newouterd2-step, +newouterd2 - step);
    } else {
      patch_block(PT_A, lod, // Top Sector
                  -newouterd2 + step, -newouterd2 + step,
                  +newouterd2-step, -iprevouterd2);

      patch_block(PT_A, lod, // Left Sector
                  -newouterd2 + step, -iprevouterd2,
                  -iprevouterd2, +iprevouterd2);

      patch_block(PT_A, lod, // Right Sector
                  iprevouterd2, -iprevouterd2,
                  newouterd2-step, +iprevouterd2);

      patch_block(PT_A, lod, // Bottom Sector
                  -newouterd2 + step, iprevouterd2,
                  +newouterd2-step, newouterd2-step);
    }

    int bx0 = -newouterd2;
    int bx1 = newouterd2 - step;
    patch_row(PT_BT, lod, bx0 + step, bx1, -newouterd2);
    patch_row(PT_BB, lod, bx0 + step, bx1, +newouterd2 - step);
    patch_column(PT_BL, lod, -newouterd2, bx0, bx1);
    patch_column(PT_BR, lod, +newouterd2 - step, bx0, bx1);

    single_patch(PT_C, lod, bx0, bx0);
    single_patch(PT_C, lod, bx1, bx0);
    single_patch(PT_C, lod, bx1, bx1);
    single_patch(PT_C, lod, bx0, bx1);

    iprevouterd2 = newouterd2;
  }

  size_t triangle_count = 0;

  for (auto p : _patches) {

    //printf("p<%d %d> t<%d>\n", p._x, p._z, p._type);
    switch (p._type) {
    case PT_A: //
      triangle_count += 8;
      break;
    case PT_BT:
    case PT_BL:
    case PT_BB:
    case PT_BR:
      triangle_count += 5;
      break;
    case PT_C:
      triangle_count += 4;
      break;
    }
  }

  size_t vertex_count = _patches.size() * 6;

  printf("triangle_count<%zu>\n", triangle_count);
  printf("vertex_count<%zu>\n", vertex_count);

  ////////////////////////////////////////////
  // find/create vertexbuffers
  ////////////////////////////////////////////

  typedef lev2::SVtxV12C4T16 vertex_type;

  TerVtxBuffersType *vertexbuffers = 0;

  auto itv = vtxbufmap.find(terrain_ngrids);

  if (itv == vtxbufmap.end()) // init index buffer for this terrain size
  {
    vertexbuffers = OrkNew TerVtxBuffersType;
    vtxbufmap[terrain_ngrids] = vertexbuffers;

    auto vbuf = new lev2::StaticVertexBuffer<vertex_type>(
        vertex_count, 0, lev2::EPRIM_POINTS);
    vertexbuffers->push_back(vbuf);
  } else {
    vertexbuffers = itv->second;
  }

  auto vbuf = (*vertexbuffers)[0];
  vbuf->Reset();
  ////////////////////////////////////////////
  lev2::VtxWriter<vertex_type> vwriter;
  vwriter.Lock(ptarg, vbuf, vertex_count);
  ////////////////////////////////////////////
  triangle_count = 0;
  for (auto p : _patches) {
    int x = p._x;
    int z = p._z;
    int lod = p._lod;
    int step = 1 << lod;

    fvec3 p0(x, lod, z);
    fvec3 p1(x + step, lod, z);
    fvec3 p2(x + step, lod, z + step);
    fvec3 p3(x, lod, z + step);

    aab.Grow(p0);
    aab.Grow(p1);
    aab.Grow(p2);
    aab.Grow(p3);

    uint32_t c0 = 0xff000000;
    uint32_t c1 = 0xff0000ff;
    uint32_t c2 = 0xff00ffff;
    uint32_t c3 = 0xff00ff00;

    switch (p._type) {
    case PT_A: //
      triangle_count += 8;
      c0 = 0xff0000ff;
      break;
    case PT_BT:
    case PT_BB:
      c0 = 0xff800080;
      break;
    case PT_BL:
    case PT_BR:
      c0 = 0xff00ff00;
      break;
      break;
    case PT_C:
      c0 = 0xff808080;
      break;
    }

    p0.y = lod;
    p1.y = lod;
    p2.y = lod;
    p3.y = lod;


    auto v0 = vertex_type(p0, fvec2(), c0);
    auto v1 = vertex_type(p1, fvec2(), c0);
    auto v2 = vertex_type(p2, fvec2(), c0);
    auto v3 = vertex_type(p3, fvec2(), c0);
    auto vc = vertex_type((p0+p1+p2+p3)*0.25, fvec2(), c0);

    switch (p._type) {
    case PT_A: //
      triangle_count += 2;
      vwriter.AddVertex(v0);
      vwriter.AddVertex(v1);
      vwriter.AddVertex(v2);
      vwriter.AddVertex(v0);
      vwriter.AddVertex(v2);
      vwriter.AddVertex(v3);
      break;
    case PT_BT:
    case PT_BB:
      triangle_count += 5;
      break;
    case PT_BR:
      triangle_count += 5;
      break;
      case PT_BL:
      break;
    case PT_C:
      triangle_count += 4;
      break;
    }
  }
  ////////////////////////////////////////////
  vwriter.UnLock(ptarg);
  ////////////////////////////////////////////

  aab.EndGrow();
  auto geomin = aab.Min();
  auto geomax = aab.Max();
  auto geosiz = aab.GetSize();

  _aabbmin = geomin;
  _aabbmax = geomax;


  printf("geomin<%f %f %f>\n", geomin.GetX(), geomin.GetY(), geomin.GetZ());
  printf("geomax<%f %f %f>\n", geomax.GetX(), geomax.GetY(), geomax.GetZ());
  printf("geosiz<%f %f %f>\n", geosiz.GetX(), geosiz.GetY(), geosiz.GetZ());
}
///////////////////////////////////////////////////////////////////////////////

void FastRender(const lev2::RenderContextInstData &rcidata,
                BulletHeightfieldImpl *htri) {

  const lev2::Renderer *renderer = rcidata.GetRenderer();
  const ent::Entity *pent = htri->_entity;
  const auto &hfd = htri->_hfd;
  auto sphmap = hfd.GetSphereMap();

  lev2::GfxTarget *ptarg = renderer->GetTarget();

  auto framedata = ptarg->GetRenderContextFrameData();

  const fmtx4& PMTX = framedata->GetCameraCalcCtx().mPMatrix;
  const fmtx4& VMTX = framedata->GetCameraCalcCtx().mVMatrix;
  auto MMTX = pent->GetEffectiveMatrix();

  bool bpick = ptarg->FBI()->IsPickState();

  //////////////////////////
  htri->init_visgeom(ptarg);
  //////////////////////////
  fmtx4 inv_view;
  inv_view.GEMSInverse(VMTX);
  fvec3 campos = inv_view.GetTranslation();
  campos.y = 0;
  fmtx4 follow;
  //follow.SetTranslation( campos );
  //////////////////////////
  ptarg->MTXI()->PushPMatrix(PMTX);
  ptarg->MTXI()->PushVMatrix(VMTX);
  ptarg->MTXI()->PushMMatrix(follow);
  {
    const int iglX = htri->_heightmap.GetGridSizeX();
    const int iglZ = htri->_heightmap.GetGridSizeZ();

    const int terrain_ngrids = iglX * iglZ;

    if (terrain_ngrids >= 1024) {
      const auto &vb_map = htri->vtxbufmap;
      const auto &itv = vb_map.find(terrain_ngrids);

      if ((itv != vb_map.end())) {
        auto vbsptr = itv->second;
        auto &vertexbuffers = *vbsptr;

        ///////////////////////////////////////////////////////////////////
        // render
        ///////////////////////////////////////////////////////////////////

        lev2::Texture *ColorTex = nullptr;
        if (sphmap && sphmap->GetTexture())
          ColorTex = sphmap->GetTexture();

        auto material = htri->mTerrainMtl;

        material->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_USER);
        material->SetTexture(ColorTex);
        material->SetTexture2(htri->_heightmapTexture);

        auto range = htri->_aabbmax-htri->_aabbmin;
        material->SetUser0(htri->_aabbmin);
        material->SetUser1(range);
        material->SetUser2(fvec4(hfd.WorldHeight(),0,0,0));

        ptarg->PushMaterial(material);
        int ivbidx = 0;

        CColor4 color = CColor4::White();

        if (bpick) {
          auto pickbuf = ptarg->FBI()->GetCurrentPickBuffer();
          color = pickbuf->AssignPickId((Object *)&pent->GetEntData());
        } else if (false) { // is_sel ){
          color = CColor4::Red();
        }

        ptarg->PushModColor(color);
        {
          int inumvb = vertexbuffers.size();
          int inumpasses = htri->mTerrainMtl->BeginBlock(ptarg, rcidata);
          bool bDRAW = htri->mTerrainMtl->BeginPass(ptarg, 0);
          if (bDRAW) {
            for (int ivb = 0; ivb < inumvb; ivb++) {
              auto vertex_buf = vertexbuffers[ivb];
              ptarg->GBI()->DrawPrimitiveEML(*vertex_buf,
                                             lev2::EPRIM_TRIANGLES);
            }
            htri->mTerrainMtl->EndPass(ptarg);
            htri->mTerrainMtl->EndBlock(ptarg);
          }
        }
        ptarg->PopModColor();
        ptarg->PopMaterial();
      }
    }
  }
  ptarg->MTXI()->PopMMatrix();
  ptarg->MTXI()->PopVMatrix();
  ptarg->MTXI()->PopPMatrix();
  // htrc.UnLockVisMap();
}

///////////////////////////////////////////////////////////////////////////////

static void RenderHeightfield(lev2::RenderContextInstData &rcid,
                              lev2::GfxTarget *targ,
                              const lev2::CallbackRenderable *pren) {
  auto data = pren->GetDrawableDataA().Get<BulletHeightfieldImpl *>();
  if (data)
    FastRender(rcid, data);
}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeHeightfieldData::Describe() {
  reflect::RegisterProperty("HeightMap",
                            &BulletShapeHeightfieldData::GetHeightMapName,
                            &BulletShapeHeightfieldData::SetHeightMapName);
  reflect::RegisterProperty("WorldHeight",
                            &BulletShapeHeightfieldData::mWorldHeight);
  reflect::RegisterProperty("WorldSize",
                            &BulletShapeHeightfieldData::mWorldSize);
  reflect::RegisterProperty("SphericalLightMap",
                            &BulletShapeHeightfieldData::GetTextureAccessor,
                            &BulletShapeHeightfieldData::SetTextureAccessor);
  reflect::RegisterProperty("VisualOffset",
                            &BulletShapeHeightfieldData::mVisualOffset);

  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "HeightMap", "editor.class", "ged.factory.filelist");
  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "HeightMap", "editor.filetype", "png");

  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "WorldHeight", "editor.range.min", "0.0");
  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "WorldHeight", "editor.range.max", "0.25");

  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "WorldSize", "editor.range.min", "1.0f");
  reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "WorldSize", "editor.range.max", "20000.0");

  ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "SphericalLightMap", "editor.class", "ged.factory.assetlist");
  ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "SphericalLightMap", "editor.assettype", "lev2tex");
  ork::reflect::AnnotatePropertyForEditor<BulletShapeHeightfieldData>(
      "SphericalLightMap", "editor.assetclass", "lev2tex");
}

///////////////////////////////////////////////////////////////////////////////
void BulletShapeHeightfieldData::SetTextureAccessor(
    ork::rtti::ICastable *const &tex) {
  mSphereLightMap = tex ? ork::rtti::autocast(tex) : 0;
}
///////////////////////////////////////////////////////////////////////////////
void BulletShapeHeightfieldData::GetTextureAccessor(
    ork::rtti::ICastable *&tex) const {
  tex = mSphereLightMap;
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeHeightfieldData::BulletShapeHeightfieldData()
    : mHeightMapName("none"), mWorldHeight(1000.0f), mWorldSize(1000.0f),
      mSphereLightMap(nullptr) {

  mShapeFactory._createShape =
      [=](const ShapeCreateData &data) -> BulletShapeBaseInst * {
    auto rval = new BulletShapeBaseInst(this);
    auto geo = std::make_shared<BulletHeightfieldImpl>(*this);
    rval->_impl.Set<std::shared_ptr<BulletHeightfieldImpl>>(geo);

    rval->mCollisionShape = geo->init_bullet_shape(data);

    auto pdrw = OrkNew ent::CallbackDrawable(data.mEntity);
    data.mEntity->AddDrawable(AddPooledLiteral("Default"), pdrw);

    pdrw->SetRenderCallback(RenderHeightfield);
    pdrw->SetOwner(&data.mEntity->GetEntData());
    pdrw->SetUserDataA((BulletHeightfieldImpl *)geo.get());
    pdrw->SetSortKey(1000);

    msgrouter::channel("bshdchanged")->post(this);

    return rval;
  };

  mShapeFactory._invalidate = [](BulletShapeBaseData *data) {
    auto as_bshd = dynamic_cast<BulletShapeHeightfieldData *>(data);
    assert(as_bshd != nullptr);
    msgrouter::channel("bshdchanged")->post(nullptr);
  };
}

bool BulletShapeHeightfieldData::PostDeserialize(reflect::IDeserializer &) {
  return true;
}

///////////////////////////////////////////////////////////////////////////////

BulletShapeHeightfieldData::~BulletShapeHeightfieldData() {}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeHeightfieldData::SetHeightMapName(file::Path const &lmap) {
  mHeightMapName = lmap;
  // mbDirty = true;
}

///////////////////////////////////////////////////////////////////////////////

void BulletShapeHeightfieldData::GetHeightMapName(file::Path &lmap) const {
  lmap = mHeightMapName;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ent
///////////////////////////////////////////////////////////////////////////////
