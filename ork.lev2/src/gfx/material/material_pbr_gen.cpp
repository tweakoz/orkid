////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/file/path.h>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/image.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {

static logchannel_ptr_t logchan_pbrgen = logger()->configureChannel("PBRGEN", fvec3(0.8, 0.8, 0.5), true);

float roughness_power = 0.5f;
int _SALT() {
  // return rand();
  return 47;
}
bool force_pbrgen_spec = false;

/////////////////////////////////////////////////////////////////////////

static FxUniformBuffer* _getlightingDataBuffer(Context* context) {
  FxUniformBuffer* _buffer;
  uint64_t LOCK = lev2::GfxEnv::createLock();
  context->makeCurrentContext();
  std::vector<uint8_t> initial_bytes;
  initial_bytes.resize(16384);
  _buffer     = context->FXI()->createUniformBuffer(16384);
  auto mapped = context->FXI()->mapUniformBuffer(_buffer);
  mapped->unmap();
  lev2::GfxEnv::releaseLock(LOCK);
  return _buffer;
}

/////////////////////////////////////////////////////////////////////////

FxUniformBuffer* PBRMaterial::lightingDataBuffer(Context* targ) {
  static FxUniformBuffer* _buffer = _getlightingDataBuffer(targ);
  return _buffer;
}

/////////////////////////////////////////////////////////////////////////

static FxUniformBuffer* _getBoneDataBuffer(Context* context) {
  FxUniformBuffer* _buffer;
  uint64_t LOCK = lev2::GfxEnv::createLock();
  { //
    context->makeCurrentContext();
    _buffer     = context->FXI()->createUniformBuffer(65536);
    auto mapped = context->FXI()->mapUniformBuffer(_buffer);
    mapped->unmap();
  }
  lev2::GfxEnv::releaseLock(LOCK);
  return _buffer;
}

/////////////////////////////////////////////////////////////////////////

FxUniformBuffer* PBRMaterial::boneDataBuffer(Context* targ) {
  static FxUniformBuffer* _buffer = _getBoneDataBuffer(targ);
  return _buffer;
}

/////////////////////////////////////////////////////////////////////////

static texture_ptr_t _getbrdfintmap(Context* targ, std::string typname, uint64_t type) {
  texture_ptr_t _map;

  targ->makeCurrentContext();
  _map = std::make_shared<lev2::Texture>();

  uint64_t LOCK = lev2::GfxEnv::createLock();

  _map->_debugName = FormatString("brdfIntegrationMap:%s", typname.c_str());

#if defined(__APPLE__)
  constexpr int DIM = 512;
#elif defined(ORK_ARCHITECTURE_X86_64)
  constexpr int DIM = 512;
#else
  constexpr int DIM = 1024; // takes too long on arm
#endif

  ///////////////////////////////
  // dblock cache
  ///////////////////////////////
  auto brdfhasher = DataBlock::createHasher();
  brdfhasher->accumulateString(_map->_debugName); // identifier
  brdfhasher->accumulateItem<uint64_t>(type);         // version code
  brdfhasher->accumulateItem<float>(0.95);         // version code
  brdfhasher->accumulateItem<float>(DIM);         // dimension
  brdfhasher->finish();
  uint64_t brdfhash = brdfhasher->result();
  // logchan_pbrgen->log("brdfIntegrationMap hashkey<%zx>", brdfhash);
  datablock_ptr_t dblock = DataBlockCache::findDataBlock(brdfhash);
  if (dblock) {
    switch(type) {
      case "BLINN"_crcu: {
        logchan_pbrgen->log("brdfIntegrationMap BLINN loaded from cache");
        break;
      }
      case "GGX"_crcu: {
        logchan_pbrgen->log("brdfIntegrationMap GGX loaded from cache");
        break;
      }
      case "GGXVELVET"_crcu: {
        logchan_pbrgen->log("brdfIntegrationMap GGXVELVET loaded from cache");
        break;
      }
      case "GGXRIM"_crcu: {
        logchan_pbrgen->log("brdfIntegrationMap GGXRIM loaded from cache");
        break;
      }
      case "PHONG"_crcu: {
        logchan_pbrgen->log("brdfIntegrationMap PHONG loaded from cache");
        break;
      }
    }
    // loaded from cache
    // logchan_pbrgen->log("brdfIntegrationMap loaded from cache");
  } else { // recompute and cache
    // logchan_pbrgen->log("Begin Compute brdfIntegrationMap");
    dblock        = std::make_shared<DataBlock>();
    float* texels = dblock->allocateItems<float>(DIM * DIM * 4);
    auto group    = opq::createCompletionGroup(opq::concurrentQueue(), "BRDFMAPGEN");

    using gen_t     = std::function<dvec2(double, double)>;
    gen_t generator = [](double fx, double fy) -> dvec2 { return brdf::integrateGGX<4096>(fx, fy); };
    switch(type) {
      case "BLINN"_crcu: {
        printf( "GENERATING BLINN BRDF INTEGRATION MAP\n");
        generator = [](double fx, double fy) -> dvec2 { return brdf::integrateBlinn<4096>(fx, fy); };
        break;
      }
      case "GGX"_crcu: {
        printf( "GENERATING GGX BRDF INTEGRATION MAP\n");
        generator = [](double fx, double fy) -> dvec2 { return brdf::integrateGGX<4096>(fx, fy); };
        break;
      }
      case "GGXVELVET"_crcu: {
        printf( "GENERATING GGXVELVET BRDF INTEGRATION MAP\n");
        generator = [](double fx, double fy) -> dvec2 { return brdf::integrateGGXVelvet<4096>(fx, fy); };
        break;
      }
      case "GGXRIM"_crcu: {
        printf( "GENERATING GGXRIM BRDF INTEGRATION MAP\n");
        generator = [](double fx, double fy) -> dvec2 { return brdf::integrateGGXStrongRim<4096>(fx, fy); };
        break;
      }
      case "PHONG"_crcu: {
        printf( "GENERATING PHONG BRDF INTEGRATION MAP\n");
        generator = [](double fx, double fy) -> dvec2 { return brdf::integrateGGXStrongRim<4096>(fx, fy); };
        //generator = [](double fx, double fy) -> dvec2 { return brdf::integratePhongLike<4096>(fx, fy); };
        break;
      }
    }

    for (int y = 0; y < DIM; y++) {
      float fy  = float(y) / float(DIM - 1);
      int ybase = y * DIM;
      group->enqueue([=]() {
        for (int x = 0; x < DIM; x++) {
          float fx = float(x) / float(DIM - 1);
          dvec3 output = generator(fx, fy);
          int texidxbase         = (ybase + x) * 4;
          texels[texidxbase + 0] = float(output.x);
          texels[texidxbase + 1] = float(output.y);
          texels[texidxbase + 2] = float(output.z);
          texels[texidxbase + 3] = 1.0f;
        }
      });
    }
    group->join();
    logchan_pbrgen->log("End Compute brdfIntegrationMap");
    fflush(stdout);
    DataBlockCache::setDataBlock(brdfhash, dblock);
  }

  ///////////////////////////////
  // verify (debug)
  ///////////////////////////////

  if (1) {
    auto outpath = file::Path::temp_dir() / FormatString("brdftest%zx.exr",type);
    auto out     = ImageOutput::create(outpath.c_str());
    assert(out != nullptr);
    ImageSpec spec(DIM, DIM, 4, TypeDesc::FLOAT);
    out->open(outpath.c_str(), spec);
    out->write_image(TypeDesc::FLOAT, dblock->data());
    out->close();
  }

  ///////////////////////////////
  auto enq_op = [DIM,dblock,_map](Context* ctx) {
      TextureInitData tid;
      tid._w           = DIM;
      tid._h           = DIM;
      tid._src_format  = EBufferFormat::RGBA32F;
      tid._dst_format  = EBufferFormat::RGBA32F;
      tid._autogenmips = false;
      tid._data        = dblock->data();
      ctx->TXI()->initTextureFromData(_map.get(), tid);
  };
  GfxEnv::GetRef().enqueueDeferredContextOp(enq_op);
  lev2::GfxEnv::releaseLock(LOCK);
  return _map;
}

/////////////////////////////////////////////////////////////////////////

texture_ptr_t PBRMaterial::brdfIntegrationMap(Context* targ,std::string type) {
  uint64_t type_hash = CrcString(type.c_str()).hashed();

  static std::unordered_map<uint64_t,texture_ptr_t> _maps;

  auto it = _maps.find(type_hash);
  if( it != _maps.end() ){
    return it->second;
  }
  else{
    auto new_tex = _getbrdfintmap(targ,type,type_hash);
    _maps[type_hash] = new_tex;
    return new_tex;
  }
}

/////////////////////////////////////////////////////////////////////////


} // namespace ork::lev2
