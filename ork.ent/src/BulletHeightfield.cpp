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
#include <ork/math/polar.h>
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
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::BulletShapeHeightfieldData, "BulletShapeHeightfieldData");

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////

typedef SVtxV12C4T16 vertex_type;

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

struct SectorLodInfo {
  std::vector<Patch> _patches;
  size_t triangle_count = 0;
  size_t vertex_count = 0;
  VtxWriter<vertex_type> vwriter;
  TerVtxBuffersType* _vtxbuflist;
  bool _islod0 = false;

  size_t countTriangles(){
    triangle_count = 0;
    for (auto p : _patches) {

      // printf("p<%d %d> t<%d>\n", p._x, p._z, p._type);
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
    return triangle_count;
  }
  void createVB(){
      vertex_count = _patches.size() * 6;
      _vtxbuflist = new TerVtxBuffersType;
      auto vbuf = new StaticVertexBuffer<vertex_type>(vertex_count, 0, EPRIM_POINTS);
      vbuf->Reset();
      _vtxbuflist->push_back(vbuf);

      printf("sector<%p> triangle_count<%zu>\n", this, triangle_count);
      printf("sector<%p> vertex_count<%zu>\n", this, vertex_count);
  }
};

struct SectorInfo {
  SectorLodInfo _lod0;
  SectorLodInfo _lodX;
};

struct BulletHeightfieldImpl {
  int mGridSize = 0;
  bool _loadok = false;
  bool _initViz = true;
  Entity* _entity = nullptr;
  GfxMaterial3DSolid* mTerrainMtl = nullptr;
  Texture* _heightmapTextureA = nullptr;
  Texture* _heightmapTextureB = nullptr;
  btHeightfieldTerrainShape* _terrainShape = nullptr;

  fvec3 _aabbmin;
  fvec3 _aabbmax;

  HeightMap _heightmap;
  HeightMap _vizheightmap;
  const BulletShapeHeightfieldData& _hfd;
  msgrouter::subscriber_t _subscriber;
  SectorInfo _sector[8];

  BulletHeightfieldImpl(const BulletShapeHeightfieldData& data);
  ~BulletHeightfieldImpl();

  void init_visgeom(GfxTarget* ptarg);
  btHeightfieldTerrainShape* init_bullet_shape(const ShapeCreateData& data);
};

///////////////////////////////////////////////////////////////////////////////

BulletHeightfieldImpl::BulletHeightfieldImpl(const BulletShapeHeightfieldData& data)
    : _hfd(data), _heightmap(0, 0), _vizheightmap(0, 0) {
  _subscriber = msgrouter::channel("bshdchanged")->subscribe([=](msgrouter::content_t c) {
    // auto bshd = c.Get<BulletShapeHeightfieldData*>();
    this->_initViz = true;

    printf("Load Heightmap<%s>\n", _hfd.HeightMapPath().c_str());

    _loadok = _heightmap.Load(_hfd.HeightMapPath());
    _loadok &= _vizheightmap.Load(_hfd.VizHeightMapPath());

    int idimx = _heightmap.GetGridSizeX();
    int idimz = _heightmap.GetGridSizeZ();
    printf("idimx<%d> idimz<%d>\n", idimx, idimz);
    // float aspect = float(idimz)/float(idimx);
    const float kworldsizeX = _hfd.WorldSize();
    const float kworldsizeZ = kworldsizeX;

    _heightmap.SetWorldSize(kworldsizeX, kworldsizeZ);
    _heightmap.SetWorldHeight(_hfd.WorldHeight());
    _vizheightmap.SetWorldSize(kworldsizeX, kworldsizeZ);
    _vizheightmap.SetWorldHeight(_hfd.WorldHeight());
  });

  _subscriber->_handler(nullptr);
}

BulletHeightfieldImpl::~BulletHeightfieldImpl() {

  if (_heightmapTextureA) {
    delete _heightmapTextureA;
  }
  if (_heightmapTextureB) {
    delete _heightmapTextureB;
  }
}
///////////////////////////////////////////////////////////////////////////////

btHeightfieldTerrainShape* BulletHeightfieldImpl::init_bullet_shape(const ShapeCreateData& data) {
  _entity = data.mEntity;

  if (false == _loadok)
    return nullptr;

  int idimx = _heightmap.GetGridSizeX();
  int idimz = _heightmap.GetGridSizeZ();

  float aspect = float(idimz) / float(idimx);
  const float kworldsizeX = _hfd.WorldSize();
  const float kworldsizeZ = kworldsizeX * aspect;

  auto world_controller = data.mWorld;
  const BulletSystemData& world_data = world_controller->GetWorldData();

  btVector3 grav = !world_data.GetGravity();

  //////////////////////////////////////////
  // hook it up to bullet if present
  //////////////////////////////////////////

  float ftoth = _heightmap.GetMaxHeight() - _heightmap.GetMinHeight();

  auto pdata = _heightmap.GetHeightData();

  _terrainShape = new btHeightfieldTerrainShape(idimx, idimz, // w,h
                                                (void*)pdata, // data
                                                ftoth,        // heightScale
                                                _heightmap.GetMinHeight(), _heightmap.GetMaxHeight(),
                                                1,         // upAxis,
                                                PHY_FLOAT, // usefloat heightDataType,
                                                true);     // flipQuadEdges );

  //_terrainShape->setUseDiamondSubdivision(true);
  _terrainShape->setUseZigzagSubdivision(true);

  float fworldsizeX = _heightmap.GetWorldSizeX();
  float fworldsizeZ = _heightmap.GetWorldSizeZ();

  float scalex = fworldsizeX / float(idimx);
  float scalez = fworldsizeZ / float(idimz);
  float scaley = 1.0f;

  _terrainShape->setLocalScaling(btVector3(scalex, _heightmap.GetWorldHeight(), scalez));

  printf("_terrainShape<%p>\n", _terrainShape);

  return _terrainShape;
}

///////////////////////////////////////////////////////////////////////////////

void BulletHeightfieldImpl::init_visgeom(GfxTarget* ptarg) {
  if (false == _initViz)
    return;

  _initViz = false;

  auto sphmap = _hfd.GetSphereMap();
  auto sphmaptex = (sphmap != nullptr) ? sphmap->GetTexture() : nullptr;

  mTerrainMtl = new GfxMaterial3DSolid(ptarg, "orkshader://terrain", "terrain1");
  mTerrainMtl->SetColorMode(GfxMaterial3DSolid::EMODE_USER);
  mTerrainMtl->_enablePick = true;

  orkprintf("ComputingGeometry\n");

  ////////////////////////////////////////////////////////////////

  const float kworldsizeX = _heightmap.GetWorldSizeX();
  const float kworldsizeZ = _heightmap.GetWorldSizeZ();

  ////////////////////////////////////////////////////////////////
  // create and fill in gpu texture
  ////////////////////////////////////////////////////////////////

  const int iglX = _vizheightmap.GetGridSizeX();
  const int iglZ = _vizheightmap.GetGridSizeZ();
  const int terrain_ngrids = iglX * iglZ;

  if (0 == iglX)
    return;

  int MIPW = iglX;
  int MIPH = iglZ;

  auto chainA = new MipChain(MIPW, MIPH, EBUFFMT_RGBA128, ETEXTYPE_2D);
  auto chainB = new MipChain(MIPW, MIPH, EBUFFMT_RGBA128, ETEXTYPE_2D);

  auto mipA0 = chainA->_levels[0];
  auto mipB0 = chainB->_levels[0];
  auto pfloattexA = (float*)mipA0->_data;
  auto pfloattexB = (float*)mipB0->_data;
  assert(pfloattexA != nullptr);
  assert(pfloattexB != nullptr);

  fvec2 origin(0, 0);

  auto heightdata = (float*)_vizheightmap.GetHeightData();

  const bool debugmip = false;

  fvec3 mipdebugcolors[12] = {

      fvec3(0, 0, 0), // 0
      fvec3(0, 0, 1), // 1
      fvec3(0, 1, 0), // 2
      fvec3(0, 1, 1), // 3
      fvec3(1, 0, 0), // 4
      fvec3(1, 0, 1), // 5
      fvec3(1, 1, 0), // 6
      fvec3(1, 1, 1), // 7
      fvec3(0, 0, 0), // 0
      fvec3(0, 0, 1), // 1
      fvec3(0, 1, 0), // 2
      fvec3(0, 1, 1), // 3
  };

  for (ssize_t z = 0; z < MIPH; z++) {
    ssize_t zz = z - (MIPH >> 1);
    float fzz = float(zz) / float(MIPH >> 1); // -1 .. 1
    for (ssize_t x = 0; x < MIPW; x++) {
      ssize_t xx = x - (MIPW >> 1);
      float fxx = float(xx) / float(MIPW >> 1);
      fvec2 pos2d(fxx, fzz);
      float d = (pos2d - origin).Mag();
      float dpow = powf(d, 3);
      size_t index = z * MIPW + x;
      float h = heightdata[index];
      size_t pixelbase = index * 4;
      pfloattexA[pixelbase + 0] = float(xx);
      pfloattexA[pixelbase + 1] = float(h);
      pfloattexA[pixelbase + 2] = float(zz);
      pfloattexB[pixelbase + 0] = float(h);
      ///////////////////
      // compute normal
      ///////////////////
      if (x > 0 and z > 0) {
        size_t xxm1 = x - 1;
        float fxxm1 = float(xxm1);
        size_t zzm1 = z - 1;
        float fzzm1 = float(zzm1);
        size_t index_dxm1 = (z * MIPW) + xxm1;
        size_t index_dzm1 = (zzm1 * MIPW) + x;
        float hd0 = heightdata[index] * 1500;
        float hdx = heightdata[index_dxm1] * 1500;
        float hdz = heightdata[index_dzm1] * 1500;
        float fzz = float(zz);
        fvec3 pos3d(x * 2, hd0, z * 2);
        fvec3 pos3d_dx(xxm1 * 2, hdx, z * 2);
        fvec3 pos3d_dz(x * 2, hdz, zzm1 * 2);

        fvec3 e01 = (pos3d_dx - pos3d).Normal();
        fvec3 e02 = (pos3d_dz - pos3d).Normal();
        auto n = e02.Cross(e01).Normal();
        pfloattexB[pixelbase + 1] = debugmip ? 0.0f : float(n.x); // r x
        pfloattexB[pixelbase + 2] = debugmip ? 0.0f : float(n.y); // g y
        pfloattexB[pixelbase + 3] = debugmip ? 0.0f : float(n.z); // b z

      } // if(x>0 and z>0){
    }   // for( size_t x=0; x<MIPW; x++ ){
  }     // for( size_t z=0; z<MIPH; z++ ){

  /////////////////////////////
  // compute mips
  /////////////////////////////

  int levindex = 0;
  MIPW >>= 1;
  MIPH >>= 1;
  while (MIPW >= 2 and MIPH >= 2) {
    assert((levindex + 1) < chainA->_levels.size());
    auto prevlevA = chainA->_levels[levindex];
    auto nextlevA = chainA->_levels[levindex + 1];
    auto prevlevB = chainB->_levels[levindex];
    auto nextlevB = chainB->_levels[levindex + 1];
    printf("levindex<%d> prevlev<%p> nextlev<%p>\n", levindex, prevlevA.get(), nextlevA.get());
    auto prevbasA = (float*)prevlevA->_data;
    auto nextbasA = (float*)nextlevA->_data;
    auto prevbasB = (float*)prevlevB->_data;
    auto nextbasB = (float*)nextlevB->_data;
    ////////////////////////////////////////////////
    constexpr float kdiv9 = 1.0f / 9.0f;
    int MAXW = (MIPW * 2 - 1);
    int MAXH = (MIPH * 2 - 1);
    for (int y = 0; y < MIPH; y++) {
      for (int x = 0; x < MIPW; x++) {
        ///////////////////////////////////////////
        int plx = x * 2;
        int ply = y * 2;
        ///////////////////////////////////////////
        int plxm1 = std::clamp(plx - 1, 0, MAXW);
        int plxp1 = std::clamp(plx + 1, 0, MAXW);
        int plyp1 = std::clamp(ply + 1, 0, MAXH);
        int plym1 = std::clamp(ply - 1, 0, MAXH);
        ///////////////////////////////////////////
        auto& dest_sampleA = nextlevA->sample<fvec4>(x, y);
        auto& dest_sampleB = nextlevB->sample<fvec4>(x, y);
        ///////////////////////////////////////////
        dest_sampleA = prevlevA->sample<fvec4>(plxm1, plym1);
        dest_sampleA += prevlevA->sample<fvec4>(plx, plym1);
        dest_sampleA += prevlevA->sample<fvec4>(plxp1, plym1);
        dest_sampleA += prevlevA->sample<fvec4>(plxm1, ply);
        dest_sampleA += prevlevA->sample<fvec4>(plx, ply);
        dest_sampleA += prevlevA->sample<fvec4>(plxp1, ply);
        dest_sampleA += prevlevA->sample<fvec4>(plxm1, plyp1);
        dest_sampleA += prevlevA->sample<fvec4>(plx, plyp1);
        dest_sampleA += prevlevA->sample<fvec4>(plxp1, plyp1);
        ///////////////////////////////////////////
        dest_sampleB = prevlevB->sample<fvec4>(plxm1, plym1);
        dest_sampleB += prevlevB->sample<fvec4>(plx, plym1);
        dest_sampleB += prevlevB->sample<fvec4>(plxp1, plym1);
        dest_sampleB += prevlevB->sample<fvec4>(plxm1, ply);
        dest_sampleB += prevlevB->sample<fvec4>(plx, ply);
        dest_sampleB += prevlevB->sample<fvec4>(plxp1, ply);
        dest_sampleB += prevlevB->sample<fvec4>(plxm1, plyp1);
        dest_sampleB += prevlevB->sample<fvec4>(plx, plyp1);
        dest_sampleB += prevlevB->sample<fvec4>(plxp1, plyp1);
        ///////////////////////////////////////////
        dest_sampleA.x *= kdiv9;
        dest_sampleA.y *= kdiv9;
        dest_sampleA.z *= kdiv9;
        dest_sampleA.w *= kdiv9;
        //
        dest_sampleB.x *= kdiv9;
        dest_sampleB.y *= kdiv9;
        dest_sampleB.z *= kdiv9;
        dest_sampleB.w *= kdiv9;
        ///////////////////////////////////////////
        if (debugmip) {
          auto& dm = mipdebugcolors[levindex];
          dest_sampleB.y = dm.x;
          dest_sampleB.z = dm.y;
          dest_sampleB.w = dm.z;
        }
      }
    }
    ////////////////////////////////////////////////
    MIPW >>= 1;
    MIPH >>= 1;
    levindex++;
  }
  ////////////////////////////////////////////////////////////////

  _heightmapTextureA = ptarg->TXI()->createFromMipChain(chainA);
  _heightmapTextureB = ptarg->TXI()->createFromMipChain(chainB);

  delete chainA;
  delete chainB;

  ////////////////////////////////////////////////////////////////

  auto bbctr = (_aabbmin + _aabbmax) * 0.5f;
  auto bbdim = (_aabbmax - _aabbmin);

  printf("IGLX<%d> IGLZ<%d> kworldsizeXZ<%f %f>\n", iglX, iglZ, kworldsizeX, kworldsizeZ);
  printf("bbmin<%f %f %f>\n", _aabbmin.GetX(), _aabbmin.GetY(), _aabbmin.GetZ());
  printf("bbmax<%f %f %f>\n", _aabbmax.GetX(), _aabbmax.GetY(), _aabbmax.GetZ());
  printf("bbctr<%f %f %f>\n", bbctr.GetX(), bbctr.GetY(), bbctr.GetZ());
  printf("bbdim<%f %f %f>\n", bbdim.GetX(), bbdim.GetY(), bbdim.GetZ());

  auto sectorID = [&](int x, int z) -> int {
    int rval = 0;
    if (x >= 0) {
      if (z >= 0) {
        if (x >= z) {
          // A
          rval = 0;
        } else {
          // B
          rval = 1;
        }
      } else {
        if (x >= (-z)) {
          // C
          rval = 7;
        } else {
          // D
          rval = 6;
        }
      }
    } else { // x<0
      if (z >= 0) {
        if ((-x) > z) {
          // E
          rval = 3;
        } else {
          // F
          rval = 2;
        }
      } else {
        if ((-x) > (-z)) {
          // G
          rval = 4;
        } else {
          // H
          rval = 5;
        }
      }
    }
    return rval;
  };

  ////////////////////////////////////////////

  auto patch_row = [&](PatchType t, int lod, int x1, int x2, int z) {
    int step = 1 << lod;
    for (int x = x1; x < x2; x += step) {
      Patch p;
      p._type = t;
      p._x = x;
      p._z = z;
      p._lod = lod;
      if( lod == 0)
        _sector[sectorID(x, z)]._lod0._patches.push_back(p);
      else
        _sector[sectorID(x, z)]._lodX._patches.push_back(p);
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
      if( lod == 0)
        _sector[sectorID(x, z)]._lod0._patches.push_back(p);
      else
        _sector[sectorID(x, z)]._lodX._patches.push_back(p);
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
        if( lod == 0)
          _sector[sectorID(x, z)]._lod0._patches.push_back(p);
        else
          _sector[sectorID(x, z)]._lodX._patches.push_back(p);
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
    if( lod == 0)
      _sector[sectorID(x, z)]._lod0._patches.push_back(p);
    else
      _sector[sectorID(x, z)]._lodX._patches.push_back(p);
  };

  ////////////////////////////////////////////

  struct Iter {
    int _lod = -1;
    int _acount = 0;
  };

  std::vector<Iter> iters;

  iters.push_back(Iter{0, 256}); // 256*2 = 512m
  iters.push_back(Iter{1, 128}); // 128*4 = 512m   tot(1024m)
  iters.push_back(Iter{2, 64});  // 64*8 = 512  tot(1536)
  iters.push_back(Iter{3, 32});  // 32*16 = 512m tot(2048m) - 2.56mi
  iters.push_back(Iter{4, 16});  // 16*32 = 512m tot(2560m) - 2.56mi
  iters.push_back(Iter{5, 8});   // 8*64 = 512m tot(3072m) - 2.56mi
  // iters.push_back(Iter{4, 128});
  // iters.push_back(Iter{5, 128});

  printf( "Generating Patches..\n");

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
                  -newouterd2 + step, -newouterd2 + step, +newouterd2 - step, +newouterd2 - step);
    } else {
      patch_block(PT_A, lod, // Top Sector
                  -newouterd2 + step, -newouterd2 + step, +newouterd2 - step, -iprevouterd2);

      patch_block(PT_A, lod, // Left Sector
                  -newouterd2 + step, -iprevouterd2, -iprevouterd2, +iprevouterd2);

      patch_block(PT_A, lod, // Right Sector
                  iprevouterd2, -iprevouterd2, newouterd2 - step, +iprevouterd2);

      patch_block(PT_A, lod, // Bottom Sector
                  -newouterd2 + step, iprevouterd2, +newouterd2 - step, newouterd2 - step);
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

  ////////////////////////////////////////////////////////////////
  // create 1 vertex buffer per 45 degree arc
  ////////////////////////////////////////////////////////////////

  for (int i = 0; i < 8; i++) {
    auto& sector = _sector[i];
    sector._lod0.countTriangles();
    sector._lodX.countTriangles();
    sector._lod0.createVB();
    sector._lodX.createVB();
    sector._lod0._islod0 = true;
    sector._lodX._islod0 = false;
  }

  ////////////////////////////////////////////
  // find/create vertexbuffers
  ////////////////////////////////////////////

  AABox aab;
  aab.BeginGrow();

  ////////////////////////////////////////////

  auto onlod = [&](SectorInfo& info, bool do_lod0){
    auto& linfo = do_lod0 ? info._lod0 : info._lodX;
    auto vbuf = (*linfo._vtxbuflist)[0];
    linfo.vwriter.Lock(ptarg, vbuf, linfo.vertex_count);
    ////////////////////////////////////////////
    linfo.triangle_count = 0;
    auto& sector_patches = linfo._patches;
    for (auto p : sector_patches) {
      int x = p._x;
      int z = p._z;
      int lod = p._lod;

      if( lod!=0 and do_lod0 )
        continue;
      if( lod==0 and (false==do_lod0) )
        continue;

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
          linfo.triangle_count += 8;
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
      auto vc = vertex_type((p0 + p1 + p2 + p3) * 0.25, fvec2(), c0);

      switch (p._type) {
        case PT_A: //
          linfo.triangle_count += 2;
          linfo.vwriter.AddVertex(v0);
          linfo.vwriter.AddVertex(v1);
          linfo.vwriter.AddVertex(v2);
          linfo.vwriter.AddVertex(v0);
          linfo.vwriter.AddVertex(v2);
          linfo.vwriter.AddVertex(v3);
          break;
        case PT_BT:
        case PT_BB:
          linfo.triangle_count += 5;
          break;
        case PT_BR:
          linfo.triangle_count += 5;
          break;
        case PT_BL:
          break;
        case PT_C:
          linfo.triangle_count += 4;
          break;
      }
    }
    ////////////////////////////////////////////
    linfo.vwriter.UnLock(ptarg);

  };

  ////////////////////////////////////////////
  for (int i = 0; i < 8; i++) {
    auto& sector = _sector[i];
    onlod(sector,true);
    onlod(sector,false);
  } // for each sector
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

void FastRender(const RenderContextInstData& rcidata, BulletHeightfieldImpl* htri) {

  const Renderer* renderer = rcidata.GetRenderer();
  const ent::Entity* pent = htri->_entity;
  const auto& hfd = htri->_hfd;
  auto sphmap = hfd.GetSphereMap();

  GfxTarget* ptarg = renderer->GetTarget();

  auto framedata = ptarg->GetRenderContextFrameData();

  const fmtx4& PMTX = framedata->GetCameraCalcCtx().mPMatrix;
  const fmtx4& VMTX = framedata->GetCameraCalcCtx().mVMatrix;
  auto MMTX = pent->GetEffectiveMatrix();

  bool bpick = ptarg->FBI()->IsPickState();

  //////////////////////////
  htri->init_visgeom(ptarg);
  //////////////////////////
  fmtx4 inv_view;
  inv_view.inverseOf(VMTX);
  fvec3 campos = inv_view.GetTranslation();
  auto znormal = inv_view.GetZNormal().Normal();
  campos.y = 0;
  fmtx4 follow;
  follow.SetTranslation(hfd.GetVisualOffset());
  //////////////////////////
  ptarg->MTXI()->PushPMatrix(PMTX);
  ptarg->MTXI()->PushVMatrix(VMTX);
  ptarg->MTXI()->PushMMatrix(follow);
  {
    const int iglX = htri->_vizheightmap.GetGridSizeX();
    const int iglZ = htri->_vizheightmap.GetGridSizeZ();

    const int terrain_ngrids = iglX * iglZ;

    if (terrain_ngrids >= 1024) {

      ///////////////////////////////////////////////////////////////////
      // render
      ///////////////////////////////////////////////////////////////////

      Texture* ColorTex = nullptr;
      if (sphmap && sphmap->GetTexture())
        ColorTex = sphmap->GetTexture();

      auto material = htri->mTerrainMtl;

      material->SetColorMode(GfxMaterial3DSolid::EMODE_USER);
      material->SetTexture(ColorTex);
      material->SetTexture2(htri->_heightmapTextureA);
      material->SetTexture3(htri->_heightmapTextureB);

      auto range = htri->_aabbmax - htri->_aabbmin;
      material->SetUser0(htri->_aabbmin);
      material->SetUser1(range);
      material->SetUser2(fvec4(hfd.WorldHeight(), 0, 0, 0));
      material->mRasterState.SetCullTest(ECULLTEST_PASS_BACK);
      ptarg->PushMaterial(material);
      int ivbidx = 0;

      fvec4 color = fcolor4::White();

      if (bpick) {
        auto pickbuf = ptarg->FBI()->GetCurrentPickBuffer();
        uint64_t pickid = pickbuf->AssignPickId((Object*)&pent->GetEntData());
        color.SetRGBAU64(pickid);
      } else if (false) { // is_sel ){
        color = fcolor4::Red();
      }

      ptarg->PushModColor(color);
      {
        int inumpasses = htri->mTerrainMtl->BeginBlock(ptarg, rcidata);
        bool bDRAW = htri->mTerrainMtl->BeginPass(ptarg, 0);
        if (bDRAW) {

          if( true ) { //abs(znormal.y) > 0.8 ){ // looking up or down
            ////////////////////////////////
            // render L0
            ////////////////////////////////
            for (int isector = 0; isector < 8; isector++) {
              auto& sector = htri->_sector[isector];
              auto L0 = sector._lod0;
              auto vbufs = L0._vtxbuflist;
              int inumvb = vbufs->size();
              for (int ivb = 0; ivb < inumvb; ivb++) {
                auto vertex_buf = (*vbufs)[ivb];
                ptarg->GBI()->DrawPrimitiveEML(*vertex_buf, EPRIM_TRIANGLES);
              }
            }
            ////////////////////////////////
            // render LX
            ////////////////////////////////
            for (int isector = 0; isector < 8; isector++) {
              auto& sector = htri->_sector[isector];
              auto LX = sector._lodX;
              auto vbufs = LX._vtxbuflist;
              int inumvb = vbufs->size();
              for (int ivb = 0; ivb < inumvb; ivb++) {
                auto vertex_buf = (*vbufs)[ivb];
                ptarg->GBI()->DrawPrimitiveEML(*vertex_buf, EPRIM_TRIANGLES);
              }
            }
          }
          else { // sector based culling (WIP)

            // lod0 - inner sectors - draw all
            for (int sectID=0; sectID<8; sectID++) {
              auto& sector = htri->_sector[sectID];
              auto L0 = sector._lod0;
              auto vbufs = L0._vtxbuflist;
              int inumvb = vbufs->size();
              for (int ivb = 0; ivb < inumvb; ivb++) {
                auto vertex_buf = (*vbufs)[ivb];
                ptarg->GBI()->DrawPrimitiveEML(*vertex_buf, EPRIM_TRIANGLES);
              }

            }

            // lodXouter - inner sectors - draw visible
            auto zn_xz = znormal.GetXZ().Normal();
            float angle = 8.0*((PI*0.5)+rect2pol_ang(zn_xz.x, zn_xz.y))/(PI*2.0);
            //printf( "znormal<%g %g> angle<%g>\n", zn_xz.x, zn_xz.y, angle );
            int basesector = int(floor(angle))+2;
            for (int soff = 6; soff < 10; soff++) {
              int sectID = (basesector+soff)&7;
              auto& sector = htri->_sector[sectID];
              auto LX = sector._lodX;
              auto vbufs = LX._vtxbuflist;
              int inumvb = vbufs->size();
              for (int ivb = 0; ivb < inumvb; ivb++) {
                auto vertex_buf = (*vbufs)[ivb];
                ptarg->GBI()->DrawPrimitiveEML(*vertex_buf, EPRIM_TRIANGLES);
              }

            }
          }
          htri->mTerrainMtl->EndPass(ptarg);
          htri->mTerrainMtl->EndBlock(ptarg);
        }
      }
      ptarg->PopModColor();
      ptarg->PopMaterial();
    }
  }
  ptarg->MTXI()->PopMMatrix();
  ptarg->MTXI()->PopVMatrix();
  ptarg->MTXI()->PopPMatrix();
  // htrc.UnLockVisMap();
}

///////////////////////////////////////////////////////////////////////////////

static void RenderHeightfield(RenderContextInstData& rcid, GfxTarget* targ, const CallbackRenderable* pren) {
  auto data = pren->GetDrawableDataA().Get<BulletHeightfieldImpl*>();
  if (data)
    FastRender(rcid, data);
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
  mSphereLightMap = tex ? ork::rtti::autocast(tex) : 0;
}
void BulletShapeHeightfieldData::GetTextureAccessor(ork::rtti::ICastable*& tex) const { tex = mSphereLightMap; }
///////////////////////////////////////////////////////////////////////////////

BulletShapeHeightfieldData::BulletShapeHeightfieldData()
    : mHeightMapName("none"), mVizHeightMapName("non"), mWorldHeight(1000.0f), mWorldSize(1000.0f), mSphereLightMap(nullptr) {

  mShapeFactory._createShape = [=](const ShapeCreateData& data) -> BulletShapeBaseInst* {
    auto rval = new BulletShapeBaseInst(this);
    auto geo = std::make_shared<BulletHeightfieldImpl>(*this);
    rval->_impl.Set<std::shared_ptr<BulletHeightfieldImpl>>(geo);

    rval->mCollisionShape = geo->init_bullet_shape(data);

    auto pdrw = OrkNew ent::CallbackDrawable(data.mEntity);
    data.mEntity->AddDrawable(AddPooledLiteral("Default"), pdrw);

    pdrw->SetRenderCallback(RenderHeightfield);
    pdrw->SetOwner(&data.mEntity->GetEntData());
    pdrw->SetUserDataA((BulletHeightfieldImpl*)geo.get());
    pdrw->SetSortKey(1000);

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
