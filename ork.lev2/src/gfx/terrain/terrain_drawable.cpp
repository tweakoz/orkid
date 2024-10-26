////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/util/crc64.h>
#include <ork/math/polar.h>
#include <ork/pch.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/msgrouter.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/terrain/terrain_drawable.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/meshutil/rigid_primitive.inl>
#include <ork/lev2/gfx/meshutil/clusterizer.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/kernel/datacache.h>
#include <ork/reflect/properties/registerX.inl>
///////////////////////////////////////////////////////////////////////////////
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

using vertex_type = VtxV12;

enum PatchType {
  PT_A = 0,
  PT_BT,
  PT_BB,
  PT_BL,
  PT_BR,
  PT_C,
};

///////////////////////////////////////////////////////////////////////////////

struct Patch {
  PatchType _type;
  int _lod;
  int _x, _z;
};

///////////////////////////////////////////////////////////////////////////////

struct SectorLodInfo {

  // LOD0 is the inner LOD
  // LODX is all outer LODs (combined)
  static int _g__num_triangles;

  ////////////////////////////////////////
  void addTriangle(
      const meshutil::vertex& vtxa, //
      const meshutil::vertex& vtxb, //
      const meshutil::vertex& vtxc) {
    meshutil::XgmClusterTri tri{vtxa, vtxb, vtxc};
    _clusterizer.addTriangle(tri, _meshflags);
    _g__num_triangles++;
  }
  ////////////////////////////////////////
  void buildClusters(AABox& aabb);
  void buildPrimitives(chunkfile::OutputStream* hdrstream, chunkfile::OutputStream* geostream);
  void gpuLoadGeometry(Context* context, chunkfile::InputStream* hdrstream, chunkfile::InputStream* geostream);
  ////////////////////////////////////////

  std::vector<Patch> _patches;
  meshutil::XgmClusterizerStd _clusterizer;
  meshutil::MeshConfigurationFlags _meshflags;

  using rigidprim_t = meshutil::RigidPrimitive<vertex_type>;
  rigidprim_t _primitive;

  bool _islod0 = false;
};

int SectorLodInfo::_g__num_triangles = 0;

///////////////////////////////////////////////////////////////////////////////

struct SectorInfo {
  SectorLodInfo _lod0;
  SectorLodInfo _lodX;
};

///////////////////////////////////////////////////////////////////////////////

struct TerrainRenderImpl {

  TerrainRenderImpl(terraindrawableinst_ptr_t hfdrw);
  ~TerrainRenderImpl();
  void gpuUpdate(Context* context);
  void render(const RenderContextInstData& RCID);

  void recomputeGeometry(chunkfile::OutputStream* hdrstream, chunkfile::OutputStream* geostream);
  void gpuLoadGeometry(Context* context, datablock_ptr_t dblock);
  void gpuInitGeometry(Context* context);
  void gpuInitTextures(Context* context);

  datablock_ptr_t recomputeTextures(Context* context);
  void reloadCachedTextures(Context* context, datablock_ptr_t dblock);

  terraindrawableinst_ptr_t _hfinstance;
  hfptr_t _heightfield;

  bool _gpuDataDirty                          = true;
  FreestyleMaterial* _terrainMaterial         = nullptr;
  const FxShaderTechnique* _tekBasic          = nullptr;
  const FxShaderTechnique* _tekDefGbuf1       = nullptr;
  const FxShaderTechnique* _tekStereo         = nullptr;
  const FxShaderTechnique* _tekDefGbuf1Stereo = nullptr;
  const FxShaderTechnique* _tekPick           = nullptr;

  const FxShaderParam* _parMatVPL       = nullptr;
  const FxShaderParam* _parMatVPC       = nullptr;
  const FxShaderParam* _parMatVPR       = nullptr;
  const FxShaderParam* _parCamPos       = nullptr;
  const FxShaderParam* _parTexA         = nullptr;
  const FxShaderParam* _parTexB         = nullptr;
  const FxShaderParam* _parTexEnv       = nullptr;
  const FxShaderParam* _parModColor     = nullptr;
  const FxShaderParam* _parTime         = nullptr;
  const FxShaderParam* _parTestXXX      = nullptr;
  const FxShaderParam* _parFogColor     = nullptr;
  const FxShaderParam* _parGrass        = nullptr;
  const FxShaderParam* _parSnow         = nullptr;
  const FxShaderParam* _parRock1        = nullptr;
  const FxShaderParam* _parRock2        = nullptr;
  const FxShaderParam* _parGblendYscale = nullptr;
  const FxShaderParam* _parGblendYbias  = nullptr;
  const FxShaderParam* _parGblendStepLo = nullptr;
  const FxShaderParam* _parGblendStepHi = nullptr;
  Texture* _heightmapTextureA           = nullptr;
  Texture* _heightmapTextureB           = nullptr;
  fvec3 _aabbmin;
  fvec3 _aabbmax;
  AABox _aabox;
  SectorInfo _sector[8];
  lev2::textureassetptr_t _sphericalenvmap = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

TerrainRenderImpl::TerrainRenderImpl(terraindrawableinst_ptr_t hfinst) {
  _hfinstance  = hfinst;
  _heightfield = std::make_shared<HeightMap>(0, 0);
}

///////////////////////////////////////////////////////////////////////////////

TerrainRenderImpl::~TerrainRenderImpl() {
  if (_heightmapTextureA) {
    delete _heightmapTextureA;
  }
  if (_heightmapTextureB) {
    delete _heightmapTextureB;
  }
}

///////////////////////////////////////////////////////////////////////////////
datablock_ptr_t TerrainRenderImpl::recomputeTextures(Context* context) {

  ork::Timer timer;
  timer.Start();

  datablock_ptr_t dblock = std::make_shared<DataBlock>();

  int MIPW = _heightfield->GetGridSizeX();
  int MIPH = _heightfield->GetGridSizeZ();

  dblock->addItem<int>(MIPW);
  dblock->addItem<int>(MIPH);

  auto chainA = new MipChain(MIPW, MIPH, EBufferFormat::RGBA32F, ETEXTYPE_2D);
  auto chainB = new MipChain(MIPW, MIPH, EBufferFormat::RGBA32F, ETEXTYPE_2D);

  auto mipA0      = chainA->_levels[0];
  auto mipB0      = chainB->_levels[0];
  auto pfloattexA = (float*)mipA0->_data;
  auto pfloattexB = (float*)mipB0->_data;
  assert(pfloattexA != nullptr);
  assert(pfloattexB != nullptr);

  fvec2 origin(0, 0);

  auto heightdata = (float*)_heightfield->GetHeightData();

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

  size_t miplen = sizeof(fvec4) * MIPW * MIPH;

  dblock->reserve(miplen * 2);

  for (ssize_t z = 0; z < MIPH; z++) {
    ssize_t zz = z - (MIPH >> 1);
    float fzz  = float(zz) / float(MIPH >> 1); // -1 .. 1
    for (ssize_t x = 0; x < MIPW; x++) {
      ssize_t xx = x - (MIPW >> 1);
      float fxx  = float(xx) / float(MIPW >> 1);
      fvec2 pos2d(fxx, fzz);
      float d                   = (pos2d - origin).magnitude();
      float dpow                = powf(d, 3);
      size_t index              = z * MIPW + x;
      float h                   = heightdata[index];
      size_t pixelbase          = index * 4;
      pfloattexA[pixelbase + 0] = float(xx);
      pfloattexA[pixelbase + 1] = float(h);
      pfloattexA[pixelbase + 2] = float(zz);
      pfloattexB[pixelbase + 0] = float(h);
      ///////////////////
      // compute normal
      ///////////////////
      if (x > 0 and z > 0) {
        size_t xxm1       = x - 1;
        float fxxm1       = float(xxm1);
        size_t zzm1       = z - 1;
        float fzzm1       = float(zzm1);
        size_t index_dxm1 = (z * MIPW) + xxm1;
        size_t index_dzm1 = (zzm1 * MIPW) + x;
        float hd0         = heightdata[index] * 1500;
        float hdx         = heightdata[index_dxm1] * 1500;
        float hdz         = heightdata[index_dzm1] * 1500;
        float fzz         = float(zz);
        fvec3 pos3d(x * 2, hd0, z * 2);
        fvec3 pos3d_dx(xxm1 * 2, hdx, z * 2);
        fvec3 pos3d_dz(x * 2, hdz, zzm1 * 2);

        fvec3 e01                 = (pos3d_dx - pos3d).normalized();
        fvec3 e02                 = (pos3d_dz - pos3d).normalized();
        auto n                    = e02.crossWith(e01).normalized();
        pfloattexB[pixelbase + 1] = debugmip ? 0.0f : float(n.x); // r x
        pfloattexB[pixelbase + 2] = debugmip ? 0.0f : float(n.y); // g y
        pfloattexB[pixelbase + 3] = debugmip ? 0.0f : float(n.z); // b z

      } // if(x>0 and z>0){
    }   // for( size_t x=0; x<MIPW; x++ ){
  }     // for( size_t z=0; z<MIPH; z++ ){

  dblock->addData(pfloattexA, miplen);
  dblock->addData(pfloattexB, miplen);

  /////////////////////////////
  // compute mips
  /////////////////////////////

  int levindex = 0;
  MIPW >>= 1;
  MIPH >>= 1;
  while (MIPW >= 2 and MIPH >= 2) {
    size_t miplen = sizeof(fvec4) * MIPW * MIPH;
    assert((levindex + 1) < chainA->_levels.size());
    auto prevlevA = chainA->_levels[levindex];
    auto nextlevA = chainA->_levels[levindex + 1];
    auto prevlevB = chainB->_levels[levindex];
    auto nextlevB = chainB->_levels[levindex + 1];
    // printf("levindex<%d> prevlev<%p> nextlev<%p>\n", levindex, prevlevA.get(), nextlevA.get());
    auto prevbasA = (float*)prevlevA->_data;
    auto nextbasA = (float*)nextlevA->_data;
    auto prevbasB = (float*)prevlevB->_data;
    auto nextbasB = (float*)nextlevB->_data;
    ////////////////////////////////////////////////
    constexpr float kdiv9 = 1.0f / 9.0f;
    int MAXW              = (MIPW * 2 - 1);
    int MAXH              = (MIPH * 2 - 1);
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
          auto& dm       = mipdebugcolors[levindex];
          dest_sampleB.y = dm.x;
          dest_sampleB.z = dm.y;
          dest_sampleB.w = dm.z;
        }
      }
    }
    ////////////////////////////////////////////////
    dblock->addData(nextbasA, miplen);
    dblock->addData(nextbasB, miplen);
    ////////////////////////////////////////////////
    MIPW >>= 1;
    MIPH >>= 1;
    levindex++;
  }

  ////////////////////////////////////////////////////////////////

  _heightmapTextureA = context->TXI()->createFromMipChain(chainA);
  _heightmapTextureB = context->TXI()->createFromMipChain(chainB);

  delete chainA;
  delete chainB;

  float runtime = timer.SecsSinceStart();
  // printf( "recomputeTextures runtime<%g>\n", runtime );
  // printf( "recomputeTextures dblocklen<%zu>\n", dblock->_data.GetSize() );

  return dblock;
}

///////////////////////////////////////////////////////////////////////////////

void TerrainRenderImpl::reloadCachedTextures(Context* context, datablock_ptr_t dblock) {
  chunkfile::InputStream istr(dblock->data(), dblock->length());
  int MIPW, MIPH;
  istr.GetItem<int>(MIPW);
  istr.GetItem<int>(MIPH);
  assert(MIPW == _heightfield->GetGridSizeX());
  assert(MIPH == _heightfield->GetGridSizeZ());
  auto chainA = new MipChain(MIPW, MIPH, EBufferFormat::RGBA32F, ETEXTYPE_2D);
  auto chainB = new MipChain(MIPW, MIPH, EBufferFormat::RGBA32F, ETEXTYPE_2D);
  // printf( "reloadCachedTextures ostr.len<%zu> nmips<%zu>\n", ostr.GetSize(), chainA->_levels.size() );
  int levindex = 0;
  while (MIPW >= 2 and MIPH >= 2) {
    int CHECKMIPW, CHECKMIPH;
    size_t levlen = sizeof(fvec4) * MIPW * MIPH;
    auto pa       = (const float*)istr.GetCurrent();
    auto leva     = chainA->_levels[levindex];
    auto levb     = chainB->_levels[levindex];
    memcpy(leva->_data, pa, levlen);
    istr.advance(levlen);
    auto pb = (const float*)istr.GetCurrent();
    memcpy(levb->_data, pb, levlen);
    istr.advance(levlen);
    // printf( "reloadmip lev<%d> w<%d> h<%d> pa<%p> pb<%p>\n", levindex, MIPW, MIPH, pa, pb );
    MIPW >>= 1;
    MIPH >>= 1;
    levindex++;
  }
  assert(istr.midx == istr.GetLength());
  _heightmapTextureA = context->TXI()->createFromMipChain(chainA);
  _heightmapTextureB = context->TXI()->createFromMipChain(chainB);
  delete chainA;
  delete chainB;
}

////////////////////////////////////////

void SectorLodInfo::buildClusters(AABox& aabb) {

  _clusterizer.Begin();

  for (auto p : _patches) {
    int x   = p._x;
    int z   = p._z;
    int lod = p._lod;

    /////////////////////////////////////////////
    // match patches destined for specific LODs
    /////////////////////////////////////////////

    if (lod != 0 and _islod0)            // is patch destined for LOD0 ?
      continue;                          // nope..
    if (lod == 0 and (false == _islod0)) // is patch destined for LODX ?
      continue;                          // nope..

    /////////////////////////////////////////////

    int step = 1 << lod;

    fvec3 p0(x, lod, z);
    fvec3 p1(x + step, lod, z);
    fvec3 p2(x + step, lod, z + step);
    fvec3 p3(x, lod, z + step);

    aabb.Grow(p0);
    aabb.Grow(p1);
    aabb.Grow(p2);
    aabb.Grow(p3);

    uint32_t c0 = 0xff000000;
    uint32_t c1 = 0xff0000ff;
    uint32_t c2 = 0xff00ffff;
    uint32_t c3 = 0xff00ff00;

    // here we should have the patches,
    //  just need to create the prims

    switch (p._type) {
      case PT_A: //
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

    c0 = 0x00000000;

    auto v0   = meshutil::vertex(p0, fvec3(), fvec3(), fvec2(), c0);
    auto v1   = meshutil::vertex(p1, fvec3(), fvec3(), fvec2(), c0);
    auto v2   = meshutil::vertex(p2, fvec3(), fvec3(), fvec2(), c0);
    auto v3   = meshutil::vertex(p3, fvec3(), fvec3(), fvec2(), c0);
    auto vc   = meshutil::vertex((p0 + p1 + p2 + p3) * 0.25, fvec3(), fvec3(), fvec2(), c0);
    auto vc01 = meshutil::vertex((p0 + p1) * 0.5, fvec3(), fvec3(), fvec2(), c0);
    auto vc12 = meshutil::vertex((p1 + p2) * 0.5, fvec3(), fvec3(), fvec2(), c0);
    auto vc23 = meshutil::vertex((p2 + p3) * 0.5, fvec3(), fvec3(), fvec2(), c0);
    auto vc30 = meshutil::vertex((p3 + p0) * 0.5, fvec3(), fvec3(), fvec2(), c0);

    switch (p._type) {
      case PT_A: {
        addTriangle(v0, vc01, vc);
        addTriangle(vc, vc01, v1);
        addTriangle(vc, v1, vc12);
        addTriangle(vc, vc12, v2);
        addTriangle(vc, v2, vc23);
        addTriangle(vc, vc23, v3);
        addTriangle(vc, v3, vc30);
        addTriangle(vc, vc30, v0);
        break;
      }
      case PT_BT: {
        addTriangle(vc, v0, v1);
        addTriangle(vc, v1, v2);
        addTriangle(vc, v2, vc23);
        addTriangle(vc, vc23, v3);
        addTriangle(vc, v3, v0);
        break;
      }
      case PT_BB: {
        addTriangle(v0, vc01, vc);
        addTriangle(vc, vc01, v1);
        addTriangle(vc, v1, v2);
        addTriangle(vc, v2, v3);
        addTriangle(vc, v3, v0);
        break;
      }
      case PT_BR: {
        addTriangle(vc, v0, v1);
        addTriangle(vc, v1, vc12);
        addTriangle(vc, vc12, v2);
        addTriangle(vc, v2, v3);
        addTriangle(vc, v3, v0);
        break;
      }
      case PT_BL: {
        addTriangle(vc, v0, v1);
        addTriangle(vc, v1, v2);
        addTriangle(vc, v2, v3);
        addTriangle(vc, v3, vc30);
        addTriangle(vc, vc30, v0);
        break;
      }
      case PT_C: {
        addTriangle(vc, v0, v1);
        addTriangle(vc, v1, v2);
        addTriangle(vc, v2, v3);
        addTriangle(vc, v3, v0);
        break;
      }
    }
  } // for (auto p : _patches) {
  _clusterizer.End();
}

///////////////////////////////////////////////////////////////////////////////

void TerrainRenderImpl::recomputeGeometry(chunkfile::OutputStream* hdrstream, chunkfile::OutputStream* geostream) {

  ork::Timer timer;
  timer.Start();

  ////////////////////////////////////////////////////////////////

  auto bbctr = (_aabbmin + _aabbmax) * 0.5f;
  auto bbdim = (_aabbmax - _aabbmin);

  // printf("IGLX<%d> IGLZ<%d> kworldsizeXZ<%f %f>\n", iglX, iglZ);
  // printf("bbmin<%f %f %f>\n", _aabbmin.x, _aabbmin.y, _aabbmin.z);
  // printf("bbmax<%f %f %f>\n", _aabbmax.x, _aabbmax.y, _aabbmax.z);
  // printf("bbctr<%f %f %f>\n", bbctr.x, bbctr.y, bbctr.z);
  // printf("bbdim<%f %f %f>\n", bbdim.x, bbdim.y, bbdim.z);

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
      p._x    = x;
      p._z    = z;
      p._lod  = lod;
      if (lod == 0)
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
      p._x    = x;
      p._z    = z;
      p._lod  = lod;
      if (lod == 0)
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
        p._x    = x;
        p._z    = z;
        p._lod  = lod;
        if (lod == 0)
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
    p._x    = x;
    p._z    = z;
    p._lod  = lod;
    if (lod == 0)
      _sector[sectorID(x, z)]._lod0._patches.push_back(p);
    else
      _sector[sectorID(x, z)]._lodX._patches.push_back(p);
  };

  ////////////////////////////////////////////

  struct Iter {
    int _lod    = -1;
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

  // printf("Generating Patches..\n");

  int iprevouterd2 = 0;

  for (auto iter : iters) {

    assert(iter._acount >= 4);       // at least one inner
    assert((iter._acount & 1) == 0); // must also be even

    int lod  = iter._lod;
    int step = 1 << lod;

    int newouterd2 = iprevouterd2 + (iter._acount * step / 2);

    int sectdim   = step;
    int sectdimp1 = sectdim + step;
    int a_start   = -(iter._acount << lod) / 2;
    int a_end     = a_start + (iter._acount - 1) * step;
    int a_z       = a_start;

    if (0 == lod) {
      patch_block(
          PT_A,
          lod, // Full Sector
          -newouterd2 + step,
          -newouterd2 + step,
          +newouterd2 - step,
          +newouterd2 - step);
    } else {
      patch_block(
          PT_A,
          lod, // Top Sector
          -newouterd2 + step,
          -newouterd2 + step,
          +newouterd2 - step,
          -iprevouterd2);

      patch_block(
          PT_A,
          lod, // Left Sector
          -newouterd2 + step,
          -iprevouterd2,
          -iprevouterd2,
          +iprevouterd2);

      patch_block(
          PT_A,
          lod, // Right Sector
          iprevouterd2,
          -iprevouterd2,
          newouterd2 - step,
          +iprevouterd2);

      patch_block(
          PT_A,
          lod, // Bottom Sector
          -newouterd2 + step,
          iprevouterd2,
          +newouterd2 - step,
          newouterd2 - step);
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
  // create 1 PrimGroup per 45 degree arc
  ////////////////////////////////////////////////////////////////

  for (int i = 0; i < 8; i++) {
    auto& sector         = _sector[i];
    sector._lod0._islod0 = true;
    sector._lodX._islod0 = false;
  }

  ////////////////////////////////////////////
  // find/create vertexbuffers / primitives
  ////////////////////////////////////////////

  _aabox.BeginGrow();
  for (int i = 0; i < 8; i++) {
    auto& sector = _sector[i];
    sector._lod0.buildClusters(_aabox);
    sector._lodX.buildClusters(_aabox);
    sector._lod0.buildPrimitives(hdrstream, geostream);
    sector._lodX.buildPrimitives(hdrstream, geostream);
  } // for each sector
  _aabox.EndGrow();

  auto geomin = _aabox.Min();
  auto geomax = _aabox.Max();
  auto geosiz = _aabox.GetSize();
  _aabbmin    = geomin;
  _aabbmax    = geomax;
  // printf("geomin<%f %f %f>\n", geomin.x, geomin.y, geomin.z);
  // printf("geomax<%f %f %f>\n", geomax.x, geomax.y, geomax.z);
  // printf("geosiz<%f %f %f>\n", geosiz.x, geosiz.y, geosiz.z);

  printf("TERRAIN-NUMTRIANGLES<%d>\n", SectorLodInfo::_g__num_triangles);

  float runtime = timer.SecsSinceStart();
}

///////////////////////////////////////////////////////////////////////////////

void SectorLodInfo::buildPrimitives(chunkfile::OutputStream* hdrstream, chunkfile::OutputStream* geostream) {
  lev2::ContextDummy DummyTarget;
  size_t inumclus = _clusterizer.GetNumClusters();
  XgmSubMesh xgmsubmesh; // hack for today..
  for (size_t icluster = 0; icluster < inumclus; icluster++) {
    auto clusterbuilder = _clusterizer.GetCluster(icluster);
    clusterbuilder->buildVertexBuffer(DummyTarget, vertex_type::meFormat);
    auto xgmcluster = std::make_shared<XgmCluster>();
    xgmsubmesh._clusters.push_back(xgmcluster);
    buildXgmCluster(DummyTarget, xgmcluster, clusterbuilder, false);
  }
  _primitive.writeToChunks(xgmsubmesh, hdrstream, geostream);
}

///////////////////////////////////////////////////////////////////////////////

void SectorLodInfo::gpuLoadGeometry(
    Context* ctx, //
    chunkfile::InputStream* hdrstream,
    chunkfile::InputStream* geostream) {
  _primitive.gpuLoadFromChunks(ctx, hdrstream, geostream);
}
///////////////////////////////////////////////////////////////////////////////

void TerrainRenderImpl::gpuLoadGeometry(Context* context, datablock_ptr_t dblock) {
  printf("TerrainRenderImpl::gpuLoadGeometry dblock len<0x%zx>\n", dblock->length());
  //////////////////////////////////////////
  chunkfile::DefaultLoadAllocator allocator;
  chunkfile::Reader chunkreader(dblock, allocator);
  OrkAssert(chunkreader._chunkfiletype == "tergeom");
  if (chunkreader.IsOk()) {
    auto hdrstream = chunkreader.GetStream("header");
    auto geostream = chunkreader.GetStream("geometry");
    printf("hdrstream len<0x%zx>\n", hdrstream->GetLength());
    printf("geostream len<0x%zx>\n", geostream->GetLength());
    ////////////////////////////////////////////////////////////////
    for (int i = 0; i < 8; i++) {
      auto& sector = _sector[i];
      sector._lod0.gpuLoadGeometry(context, hdrstream, geostream);
      sector._lodX.gpuLoadGeometry(context, hdrstream, geostream);
    } // for each sector
  }
  ////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

void TerrainRenderImpl::gpuInitGeometry(Context* context) {
  auto geometry_hasher = DataBlock::createHasher();
  geometry_hasher->accumulateItem<uint64_t>(_heightfield->_hash);
  geometry_hasher->accumulateItem<float>(_hfinstance->_worldSizeXZ);
  geometry_hasher->accumulateItem<float>(_hfinstance->_worldHeight);
  geometry_hasher->accumulateString("geometry-v0");
  geometry_hasher->finish();
  uint64_t hashkey = geometry_hasher->result();

  auto dblock = DataBlockCache::findDataBlock(hashkey);

  if (dblock) {
    printf("Read geometrycache hash<0x%llx>\n", hashkey);
  } else {
    chunkfile::Writer chunkwriter("tergeom");
    auto hdrstream = chunkwriter.AddStream("header");
    auto geostream = chunkwriter.AddStream("geometry");
    recomputeGeometry(hdrstream, geostream);
    dblock = std::make_shared<DataBlock>();
    chunkwriter.writeToDataBlock(dblock);
    printf("Writing geometrycache hash<0x%llx>\n", hashkey);
    DataBlockCache::setDataBlock(hashkey, dblock);
  }
  gpuLoadGeometry(context, dblock);
}

///////////////////////////////////////////////////////////////////////////////

void TerrainRenderImpl::gpuInitTextures(Context* context) {
  const int iglX           = _heightfield->GetGridSizeX();
  const int iglZ           = _heightfield->GetGridSizeZ();
  const int terrain_ngrids = iglX * iglZ;

  auto texture_hasher = DataBlock::createHasher();
  texture_hasher->accumulateItem<uint64_t>(_heightfield->_hash);
  texture_hasher->accumulateItem<float>(_hfinstance->_worldSizeXZ);
  texture_hasher->accumulateItem<float>(_hfinstance->_worldHeight);
  texture_hasher->accumulateString("texture-v0");
  texture_hasher->finish();
  uint64_t texture_hashkey = texture_hasher->result();

  auto dblock = DataBlockCache::findDataBlock(texture_hashkey);

  if (dblock) {
    reloadCachedTextures(context, dblock);
  } else {
    dblock = recomputeTextures(context);
    DataBlockCache::setDataBlock(texture_hashkey, dblock);
  }
}

///////////////////////////////////////////////////////////////////////////////

void TerrainRenderImpl::gpuUpdate(Context* context) {
  if (false == _gpuDataDirty)
    return;

  if (nullptr == _terrainMaterial) {
    _terrainMaterial = new FreestyleMaterial;
    _terrainMaterial->gpuInit(context, "orkshader://terrain");
    _tekBasic          = _terrainMaterial->technique("terrain");
    _tekStereo         = _terrainMaterial->technique("terrain_stereo");
    _tekPick           = _terrainMaterial->technique("pick");
    _tekDefGbuf1       = _terrainMaterial->technique("terrain_gbuf1");
    _tekDefGbuf1Stereo = _terrainMaterial->technique("terrain_gbuf1_stereo");

    _parMatVPL   = _terrainMaterial->param("MatMVPL");
    _parMatVPC   = _terrainMaterial->param("MatMVPC");
    _parMatVPR   = _terrainMaterial->param("MatMVPR");
    _parCamPos   = _terrainMaterial->param("CamPos");
    _parTexA     = _terrainMaterial->param("HFAMap");
    _parTexB     = _terrainMaterial->param("HFBMap");
    _parTexEnv   = _terrainMaterial->param("EnvMap");
    _parModColor = _terrainMaterial->param("ModColor");
    _parTime     = _terrainMaterial->param("Time");
    _parTestXXX  = _terrainMaterial->param("testxxx");

    _parFogColor     = _terrainMaterial->param("FogColor");
    _parGrass        = _terrainMaterial->param("GrassColor");
    _parSnow         = _terrainMaterial->param("SnowColor");
    _parRock1        = _terrainMaterial->param("Rock1Color");
    _parRock2        = _terrainMaterial->param("Rock2Color");
    _parGblendYscale = _terrainMaterial->param("GBlendYScale");
    _parGblendYbias  = _terrainMaterial->param("GBlendYBias");
    _parGblendStepLo = _terrainMaterial->param("GBlendStepLo");
    _parGblendStepHi = _terrainMaterial->param("GBlendStepHi");
  }

  bool _loadok = _heightfield->Load(_hfinstance->hfpath());
  _heightfield->SetWorldSize(_hfinstance->_worldSizeXZ, _hfinstance->_worldSizeXZ);
  _heightfield->SetWorldHeight(_hfinstance->_worldHeight);

  // orkprintf("ComputingGeometry hashkey<0x%llx>\n", hashkey );

  const int iglX           = _heightfield->GetGridSizeX();
  const int iglZ           = _heightfield->GetGridSizeZ();
  const int terrain_ngrids = iglX * iglZ;

  if (0 == iglX)
    return;

  ////////////////////////////////////////////////////////////////
  // create and fill in gpu data
  ////////////////////////////////////////////////////////////////

  gpuInitTextures(context);
  gpuInitGeometry(context);

  _gpuDataDirty = false;
}
///////////////////////////////////////////////////////////////////////////////

void TerrainRenderImpl::render(const RenderContextInstData& RCID) {

  auto raw_drawable         = _hfinstance->_rawdrawable;
  const IRenderer* renderer = RCID.GetRenderer();
  Context* targ             = renderer->GetTarget();
  auto RCFD                 = targ->topRenderContextFrameData();
  const auto& CPD           = RCFD->topCPD();
  bool stereo1pass          = CPD.isStereoOnePass();
  bool bpick                = CPD.isPicking();
  auto mtxi                 = targ->MTXI();
  auto fxi                  = targ->FXI();
  auto gbi                  = targ->GBI();
  ///////////////////////////////////////////////////////////////////
  assert(raw_drawable != nullptr);
  ///////////////////////////////////////////////////////////////////
  // update
  ///////////////////////////////////////////////////////////////////
  gpuUpdate(targ);
  const int iglX           = _heightfield->GetGridSizeX();
  const int iglZ           = _heightfield->GetGridSizeZ();
  const int terrain_ngrids = iglX * iglZ;
  if (terrain_ngrids < 1024)
    return;
  ///////////////////////////////////////////////////////////////////
  // render
  ///////////////////////////////////////////////////////////////////
  //////////////////////////
  fmtx4 viz_offset;
  viz_offset.setTranslation(_hfinstance->_visualOffset);
  //////////////////////////
  // color
  //////////////////////////
  fvec4 color = fcolor4::White();
  if (bpick) {
    //auto pickbuf    = targ->FBI()->currentPickBuffer();
    //uint64_t pickid = pickbuf->AssignPickId(raw_drawable->GetOwner());
    //color.setRGBAU64(pickid);
  } else if (false) { // is_sel ){
    color = fcolor4::Red();
  }
  //////////////////////////
  // env texture
  //////////////////////////
  Texture* ColorTex = nullptr;
  if (_sphericalenvmap && _sphericalenvmap->GetTexture())
    ColorTex = _sphericalenvmap->GetTexture().get();
  //////////////////////////
  //////////////////////////
  fmtx4 MVPL, MVPC, MVPR;
  //////////////////////////

  fvec3 campos_mono = CPD.monoCamPos(viz_offset);
  fvec3 znormal     = CPD.monoCamZnormal();

  if (stereo1pass and not bpick) {
    auto stcams = CPD._stereoCameraMatrices;
    MVPL        = stcams->MVPL(viz_offset);
    MVPR        = stcams->MVPR(viz_offset);
    MVPC        = stcams->MVPMONO(viz_offset);
  } else {
    auto mcams             = CPD._cameraMatrices;
    const fmtx4& PMTX_mono = mcams->_pmatrix;
    const fmtx4& VMTX_mono = mcams->_vmatrix;
    auto MV_mono           = fmtx4::multiply_ltor(viz_offset,VMTX_mono);
    auto MVP               = fmtx4::multiply_ltor(MV_mono,PMTX_mono);
    MVPL                   = MVP;
    MVPC                   = MVP;
    MVPR                   = MVP;
  }

  ///////////////////////////////////////////////////////////////////
  // render
  ///////////////////////////////////////////////////////////////////

  auto instance    = _hfinstance;
  const auto& HFDD = instance->_data;

  // auto range = _aabbmax - _aabbmin;

  auto tek_viz  = stereo1pass ? _tekDefGbuf1Stereo : _tekDefGbuf1;
  auto tek_pick = _tekPick;

  _terrainMaterial->_rasterstate.SetCullTest(ECullTest::OFF);

  _terrainMaterial->begin(bpick ? tek_pick : tek_viz, RCFD);
  _terrainMaterial->bindParamMatrix(_parMatVPL, MVPL);
  _terrainMaterial->bindParamMatrix(_parMatVPC, MVPC);
  _terrainMaterial->bindParamMatrix(_parMatVPR, MVPR);
  _terrainMaterial->bindParamCTex(_parTexA, _heightmapTextureA);
  _terrainMaterial->bindParamCTex(_parTexB, _heightmapTextureB);
  _terrainMaterial->bindParamVec3(_parCamPos, campos_mono);
  _terrainMaterial->bindParamVec4(_parModColor, color);
  _terrainMaterial->bindParamFloat(_parTime, 0.0f);

  _terrainMaterial->bindParamFloat(_parTestXXX, HFDD->_testxxx);

  _terrainMaterial->bindParamVec3(_parFogColor, fvec3(0, 0, 0));
  _terrainMaterial->bindParamVec3(_parGrass, HFDD->_grass);
  _terrainMaterial->bindParamVec3(_parSnow, HFDD->_snow);
  _terrainMaterial->bindParamVec3(_parRock1, HFDD->_rock1);
  _terrainMaterial->bindParamVec3(_parRock2, HFDD->_rock2);

  _terrainMaterial->bindParamFloat(_parGblendYscale, HFDD->_gblend_yscale);
  _terrainMaterial->bindParamFloat(_parGblendYbias, HFDD->_gblend_ybias);
  _terrainMaterial->bindParamFloat(_parGblendStepLo, HFDD->_gblend_steplo);
  _terrainMaterial->bindParamFloat(_parGblendStepHi, HFDD->_gblend_stephi);

  ////////////////////////////////
  // render L0
  ////////////////////////////////
  for (int isector = 0; isector < 8; isector++) {
    auto& sector = _sector[isector];
    sector._lod0._primitive.renderEML(targ);
  }
  ////////////////////////////////
  // render LX
  ////////////////////////////////
  for (int isector = 0; isector < 8; isector++) {
    auto& sector = _sector[isector];
    sector._lodX._primitive.renderEML(targ);
  }
  _terrainMaterial->end(RCFD);
}

///////////////////////////////////////////////////////////////////////////////

static void _RenderHeightfield(RenderContextInstData& RCID) {
  auto renderable = dynamic_cast<const CallbackRenderable*>(RCID._irenderable);
  renderable->GetDrawableDataA().getShared<TerrainRenderImpl>()->render(RCID);
}

///////////////////////////////////////////////////////////////////////////////

/*void TerrainDrawableData::describeX(class_t* c) {
  c->directProperty("Offset", &TerrainDrawableData::_visualOffset);
  c->directProperty("FogColor", &TerrainDrawableData::_fogcolor);
  c->directProperty("GrassColor", &TerrainDrawableData::_grass);
  c->directProperty("SnowColor", &TerrainDrawableData::_snow);
  c->directProperty("rock1Color", &TerrainDrawableData::_rock1);
  c->directProperty("Rock2Color", &TerrainDrawableData::_rock2);
  ////////////////////////////////////////////////////////////////////////
  c->accessorProperty("HeightMap", &TerrainDrawableData::_readHmapPath, &TerrainDrawableData::_writeHmapPath)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.filetype", "png");
  ////////////////////////////////////////////////////////////////////////
  c->directProperty("SphericalEnvMap", &TerrainDrawableData::_sphericalenvmapasset)
      ->annotate<ConstString>("editor.class", "ged.factory.assetlist")
      ->annotate<ConstString>("editor.assettype", "lev2tex")
      ->annotate<ConstString>("editor.assetclass", "lev2tex");
  ////////////////////////////////////////////////////////////////////////
  c->floatProperty("TestXXX", float_range{-100, 100}, &TerrainDrawableData::_testxxx);
  c->floatProperty("GBlendYScale", float_range{-1, 1}, &TerrainDrawableData::_gblend_yscale);
  c->floatProperty("GBlendYBias", float_range{-5000, 5000}, &TerrainDrawableData::_gblend_ybias);
  c->floatProperty("GBlendStepLo", float_range{0, 1}, &TerrainDrawableData::_gblend_steplo);
  c->floatProperty("GBlendStepHi", float_range{0, 1}, &TerrainDrawableData::_gblend_stephi);
  ////////////////////////////////////////////////////////////////////////
}
*/

TerrainDrawableData::TerrainDrawableData()
    : _hfpath("none") {
}
TerrainDrawableData::~TerrainDrawableData() {
}

///////////////////////////////////////////////////////////////////////////////
/*
callback_drawable_ptr_t TerrainDrawableInst::createCallbackDrawable() {

  auto impl = _impl.makeShared<TerrainRenderImpl>(this);

  _rawdrawable = std::make_shared<CallbackDrawable>(nullptr);
  _rawdrawable->SetRenderCallback(_RenderHeightfield);
  _rawdrawable->SetUserDataA(impl);
  _rawdrawable->SetSortKey(1000);
  return _rawdrawable;
}*/

drawable_ptr_t TerrainDrawableData::createDrawable() const {
  auto inst = std::make_shared<TerrainDrawableInst>(this);
  auto impl = inst->_impl.template makeShared<TerrainRenderImpl>(inst);
  auto drawable = std::make_shared<CallbackDrawable>(nullptr);
  inst->_rawdrawable = drawable;
  drawable->SetRenderCallback(_RenderHeightfield);
  drawable->SetUserDataA(impl);
  drawable->SetUserDataB(inst);
  drawable->_sortkey = 10;
  return drawable;

}

///////////////////////////////////////////////////////////////////////////////
Texture* TerrainDrawableData::envtex() const {
  auto as_texasset = std::dynamic_pointer_cast<TextureAsset>(_sphericalenvmapasset);
  return as_texasset ? as_texasset->GetTexture().get() : nullptr;
}
///////////////////////////////////////////////////////////////////////////////
static int count = 0;
void TerrainDrawableData::_writeHmapPath(file::Path const& hmap) {
  _hfpath = hmap;
  printf("set _hfpath<%s> count<%d>\n", _hfpath.c_str(), count++);
  if (_hfpath == "none") {
  }
}
void TerrainDrawableData::_readHmapPath(file::Path& hmap) const {
  hmap = _hfpath;
}

///////////////////////////////////////////////////////////////////////////////

TerrainDrawableInst::TerrainDrawableInst(const TerrainDrawableData* data)
    : _data(data) {
  _visualOffset = _data->_visualOffset;
}

file::Path TerrainDrawableInst::hfpath() const {
  return _data->_hfpath;
}


///////////////////////////////////////////////////////////////////////////////

TerrainDrawableInst::~TerrainDrawableInst() {
  _rawdrawable->SetRenderCallback(nullptr);
  _rawdrawable->SetUserDataA(nullptr);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
