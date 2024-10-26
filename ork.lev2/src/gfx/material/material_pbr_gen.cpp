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

static logchannel_ptr_t logchan_pbrgen = logger()->createChannel("PBRGEN", fvec3(0.8, 0.8, 0.5), true);

extern std::atomic<int> __FIND_IT;

float roughness_power = 1.0f;
int _SALT() {
  // return rand();
  return 19;
}

/////////////////////////////////////////////////////////////////////////

static FxShaderParamBuffer* _getPointLightDataBuffer(Context* context) {
  FxShaderParamBuffer* _buffer;

  uint64_t LOCK = lev2::GfxEnv::createLock();
  context->makeCurrentContext();

  std::vector<uint8_t> initial_bytes;
  initial_bytes.resize(16384);

  _buffer     = context->FXI()->createParamBuffer(16384);
  auto mapped = context->FXI()->mapParamBuffer(_buffer);
  // size_t base  = 0;
  // for (int i = 0; i < KMAXLIGHTSPERCHUNK; i++)
  // mapped->ref<fvec3>(base + i * sizeof(fvec4)) = fvec3(0, 0, 0);
  // base += KMAXLIGHTSPERCHUNK * sizeof(fvec4);
  // for (int i = 0; i < KMAXLIGHTSPERCHUNK; i++)
  // mapped->ref<fvec4>(base + i * sizeof(fvec4)) = fvec4();
  mapped->unmap();

  lev2::GfxEnv::releaseLock(LOCK);
  return _buffer;
}

/////////////////////////////////////////////////////////////////////////

FxShaderParamBuffer* PBRMaterial::pointLightDataBuffer(Context* targ) {
  static FxShaderParamBuffer* _buffer = _getPointLightDataBuffer(targ);
  return _buffer;
}

/////////////////////////////////////////////////////////////////////////

static FxShaderParamBuffer* _getBoneDataBuffer(Context* context) {
  FxShaderParamBuffer* _buffer;
  uint64_t LOCK = lev2::GfxEnv::createLock();
  { //
    context->makeCurrentContext();
    _buffer     = context->FXI()->createParamBuffer(65536);
    auto mapped = context->FXI()->mapParamBuffer(_buffer);
    mapped->unmap();
  }
  lev2::GfxEnv::releaseLock(LOCK);
  return _buffer;
}

/////////////////////////////////////////////////////////////////////////

FxShaderParamBuffer* PBRMaterial::boneDataBuffer(Context* targ) {
  static FxShaderParamBuffer* _buffer = _getBoneDataBuffer(targ);
  return _buffer;
}

/////////////////////////////////////////////////////////////////////////

static texture_ptr_t _getbrdfintmap(Context* targ) {
  texture_ptr_t _map;

  targ->makeCurrentContext();
  _map = std::make_shared<lev2::Texture>();

  uint64_t LOCK = lev2::GfxEnv::createLock();

  _map->_debugName = "brdfIntegrationMap";

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
  brdfhasher->accumulateItem<float>(1.0);         // version code
  brdfhasher->accumulateItem<float>(DIM);         // dimension
  brdfhasher->finish();
  uint64_t brdfhash = brdfhasher->result();
  // logchan_pbrgen->log("brdfIntegrationMap hashkey<%zx>", brdfhash);
  auto dblock = DataBlockCache::findDataBlock(brdfhash);
  if (dblock) {
    // loaded from cache
    // logchan_pbrgen->log("brdfIntegrationMap loaded from cache");
  } else { // recompute and cache
    // logchan_pbrgen->log("Begin Compute brdfIntegrationMap");
    dblock        = std::make_shared<DataBlock>();
    float* texels = dblock->allocateItems<float>(DIM * DIM * 4);
    auto group    = opq::createCompletionGroup(opq::concurrentQueue(), "BRDFMAPGEN");
    for (int y = 0; y < DIM; y++) {
      float fy  = float(y) / float(DIM - 1);
      int ybase = y * DIM;
      group->enqueue([=]() {
        for (int x = 0; x < DIM; x++) {
          float fx               = float(x) / float(DIM - 1);
          dvec3 output           = brdf::integrateGGX<4096>(fx, fy);
          int texidxbase         = (ybase + x) * 4;
          texels[texidxbase + 0] = float(output.x);
          texels[texidxbase + 1] = float(output.y);
          texels[texidxbase + 2] = float(output.z);
          texels[texidxbase + 3] = 1.0f;
        }
      });
    }
    group->join();
    // logchan_pbrgen->log("End Compute brdfIntegrationMap");
    fflush(stdout);
    DataBlockCache::setDataBlock(brdfhash, dblock);
  }

  ///////////////////////////////
  // verify (debug)
  ///////////////////////////////

  if (0) {
    auto outpath = file::Path::temp_dir() / "brdftest.exr";
    auto out     = ImageOutput::create(outpath.c_str());
    assert(out != nullptr);
    ImageSpec spec(DIM, DIM, 4, TypeDesc::FLOAT);
    out->open(outpath.c_str(), spec);
    out->write_image(TypeDesc::FLOAT, dblock->data());
    out->close();
  }

  ///////////////////////////////

  TextureInitData tid;
  tid._w           = DIM;
  tid._h           = DIM;
  tid._src_format  = EBufferFormat::RGBA32F;
  tid._dst_format  = EBufferFormat::RGBA32F;
  tid._autogenmips = true;
  tid._data        = dblock->data();

  targ->TXI()->initTextureFromData(_map.get(), tid);
  lev2::GfxEnv::releaseLock(LOCK);
  return _map;
}

/////////////////////////////////////////////////////////////////////////

texture_ptr_t PBRMaterial::brdfIntegrationMap(Context* targ) {
  static texture_ptr_t _map = _getbrdfintmap(targ);
  return _map;
}

/////////////////////////////////////////////////////////////////////////

static file::Path filterenv_shader_path() {
  return file::Path("orkshader://pbr_filterenv.glfx");
}
static uint32_t shader_hash() {
  return filterenv_shader_path().hashFileContents();
}
static file::Path this_path() {
  return file::Path("ork_lev2://src/gfx/material/material_pbr_gen.cpp");
}
static uint32_t this_hash() {
  if (this_path().doesPathExist()) {
    return this_path().hashFileContents();
  } else {
    return 0;
  }
}

/////////////////////////////////////////////////////////////////////////

texture_ptr_t PBRMaterial::filterSpecularEnvMap(texture_ptr_t rawenvmap, Context* targ, bool equirectangular) {
  targ->makeCurrentContext();
  auto txi = targ->TXI();
  auto fbi = targ->FBI();
  auto fxi = targ->FXI();
  auto gbi = targ->GBI();
  auto dwi = targ->DWI();
  int w    = rawenvmap->_width;
  int h    = rawenvmap->_height;
  ///////////////////////////////////////////////
  static std::shared_ptr<FreestyleMaterial> mtl;
  static const FxShaderParam* param_mvp    = nullptr;
  static const FxShaderParam* param_pfm    = nullptr;
  static const FxShaderParam* param_ruf    = nullptr;
  static const FxShaderParam* param_imgdim = nullptr;

  targ->debugPushGroup("PBRMaterial::filterSpecularEnvMap");
  __FIND_IT.store(1);

  if (not mtl) {
    mtl = std::make_shared<FreestyleMaterial>();
    OrkAssert(mtl.get() != nullptr);
    mtl->gpuInit(targ, filterenv_shader_path());
    // logchan_pbrgen->log("filterenv mtl<%p> tekFilterSpecMap<%p>", mtl.get(), tekFilterSpecMap);
    param_mvp    = mtl->param("mvp");
    param_pfm    = mtl->param("prefiltmap");
    param_ruf    = mtl->param("roughness");
    param_imgdim = mtl->param("imgdim");
  }
  const FxShaderTechnique* tekFilterSpecMap = nullptr;
  if (equirectangular)
    tekFilterSpecMap = mtl->technique("tek_filterSpecularMapEquirectangular");
  else
    tekFilterSpecMap = mtl->technique("tek_filterSpecularMapStandard");
  OrkAssert(tekFilterSpecMap != nullptr);
  ///////////////////////////////////////////////
  auto filtex                                                      = std::make_shared<FilteredEnvMap>();
  rawenvmap->_vars->makeValueForKey<filtenvmapptr_t>("filtenvmap") = filtex;
  ///////////////////////////////////////////////
  logchan_pbrgen->log(
      "filterenv-spec tex<%p:%s> hash<0x%zx> w<%d> h<%d> equirectangular<%d>",
      (void*)rawenvmap.get(),
      rawenvmap->_debugName.c_str(),
      rawenvmap->_contentHash,
      w,
      h,
      int(equirectangular));
  boost::Crc64 basehasher;
  basehasher.accumulateItem<int>(_SALT());
  basehasher.accumulateString("filterenv-spec-v0");
  basehasher.accumulateItem<uint32_t>(uint32_t(equirectangular));
  basehasher.accumulateItem<uint64_t>(rawenvmap->_contentHash);
  basehasher.accumulateItem<uint32_t>(shader_hash());
  basehasher.accumulateItem<uint32_t>(this_hash());
  basehasher.finish();
  uint64_t cmipchain_hashkey = basehasher.result();
  auto cmipchain_datablock   = DataBlockCache::findDataBlock(cmipchain_hashkey);
  ///////////////////////////////////////////////
  if (cmipchain_datablock) {
    // logchan_pbrgen->log("filterenv-spec tex<%p> loading precomputed!", rawenvmap.get());
  } else {
    auto RCFD = std::make_shared<RenderContextFrameData>(targ);

    ////////////////////////////////////
    // count mips
    ////////////////////////////////////

    int numpix = w * h;
    int imip   = 0;
    while ((w > 4) and (h > 4)) {
      numpix = w * h;
      w >>= 1;
      h >>= 1;
      imip++;
    }

    int nummips = imip;

    ////////////////////////////////////

    imip = 0;
    CompressedImageMipChain::miplevels_t compressed_levels;
    w                        = rawenvmap->_width;
    h                        = rawenvmap->_height;
    numpix                   = w * h;
    std::atomic<int> pending = 0;
    std::vector<compressedimg_ptr_t> cimgs;
    for (int imip = 0; imip < nummips; imip++) {

      auto outgroup = std::make_shared<RtGroup>(targ, w, h, MsaaSamples::MSAA_1X);
      auto outbuffr = outgroup->createRenderTarget(EBufferFormat::RGBA32F);
      auto captureb = std::make_shared<CaptureBuffer>();

      outgroup->_autoclear = true;
      filtex->_rtgroup     = outgroup;
      filtex->_rtbuffer    = outbuffr;
      outbuffr->_debugName = FormatString("filteredenvmap-specenv-mip%d", imip);

      fbi->PushRtGroup(outgroup.get());
      mtl->begin(tekFilterSpecMap, RCFD);
      ///////////////////////////////////////////////
      float roughness = float(imip) / float(nummips - 1);
      roughness       = powf(roughness, roughness_power);
      ///////////////////////////////////////////////
      logchan_pbrgen->log("filterenv imip<%d> nummips<%d> w<%d> h<%d> roughness<%g>", imip, nummips, w, h, roughness);
      logchan_pbrgen->log("filterenv imip<%d> outgroup<%p> outbuf<%p>", imip, outgroup.get(), outbuffr.get());
      ///////////////////////////////////////////////
      mtl->bindParamMatrix(param_mvp, fmtx4::Identity());
      mtl->bindParamCTex(param_pfm, rawenvmap.get());
      mtl->bindParamFloat(param_ruf, roughness);
      mtl->bindParamVec2(param_imgdim, fvec2(w, h));
      mtl->commit();
      dwi->quad2DEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
      ///////////////////////////////////////////////
      mtl->end(RCFD);
      fbi->PopRtGroup();

      fbi->capture(outbuffr.get(), captureb.get());

      if (1) {
        auto outpath = file::Path::temp_dir() / FormatString("filteredenv-specmap-mip%d.exr", imip);
        auto out     = ImageOutput::create(outpath.c_str());
        logchan_pbrgen->log("filterenv write dbgout<%s> <%p>", outpath.c_str(), out.get());
        OrkAssert(out != nullptr);
        ImageSpec spec(w, h, 4, TypeDesc::FLOAT);
        out->open(outpath.c_str(), spec);
        out->write_image(TypeDesc::FLOAT, captureb->_data);
        out->close();
      }

      pending.fetch_add(1);
      auto cimg = std::make_shared<CompressedImage>();
      cimgs.push_back(cimg);
      auto op = [=, &pending]() {
        Image im;
        im.initRGBA8WithNormalizedFloatBuffer(w, h, 4, (const float*)captureb->_data);
        im.compressDefault(*cimg);
        pending.fetch_sub(1);
      };
      opq::concurrentQueue()->enqueue(op);

      rawenvmap->_vars->makeValueForKey<rtgroup_ptr_t>(FormatString("alt-tex-specenv-group-mip%d", imip))   = std::move(outgroup);
      rawenvmap->_vars->makeValueForKey<rtbuffer_ptr_t>(FormatString("alt-tex-specenv-buffer-mip%d", imip)) = std::move(outbuffr);
      w >>= 1;
      h >>= 1;
      numpix = w * h;
    }
    while (pending.load() > 0) {
      usleep(1000);
    }
    for (auto cimg : cimgs) {
      compressed_levels.push_back(*cimg);
    }
    CompressedImageMipChain mipchain;
    mipchain.initWithPrecompressedMipLevels(compressed_levels);
    cmipchain_datablock = std::make_shared<DataBlock>();
    mipchain.writeXTX(cmipchain_datablock);
    DataBlockCache::setDataBlock(cmipchain_hashkey, cmipchain_datablock);
  }
  auto alt_tex        = std::make_shared<Texture>();
  alt_tex->_debugName = rawenvmap->_debugName + "[filtenvmap-processed-specular]";
  txi->LoadTexture(alt_tex, cmipchain_datablock);

  rawenvmap->_vars->makeValueForKey<texture_ptr_t>("alt-tex-specenv") = alt_tex;

  __FIND_IT.store(0);
  targ->debugPopGroup();

  return alt_tex;
}

/////////////////////////////////////////////////////////////////////////

texture_ptr_t PBRMaterial::filterDiffuseEnvMap(texture_ptr_t rawenvmap, Context* targ, bool equirectangular) {
  targ->makeCurrentContext();
  auto txi = targ->TXI();
  auto fbi = targ->FBI();
  auto fxi = targ->FXI();
  auto gbi = targ->GBI();
  auto dwi = targ->DWI();
  ///////////////////////////////////////////////
  static std::shared_ptr<FreestyleMaterial> mtl;
  static const FxShaderParam* param_mvp = nullptr;
  static const FxShaderParam* param_pfm = nullptr;
  static const FxShaderParam* param_ruf = nullptr;

  __FIND_IT.store(1);
  targ->debugPushGroup("PBRMaterial::filterDiffuseEnvMap");

  if (not mtl) {
    mtl = std::make_shared<FreestyleMaterial>();
    OrkAssert(mtl.get() != nullptr);
    mtl->gpuInit(targ, filterenv_shader_path());
    param_mvp = mtl->param("mvp");
    param_pfm = mtl->param("prefiltmap");
    param_ruf = mtl->param("roughness");
  }

  const FxShaderTechnique* tekFilterDiffMap = nullptr;
  if (equirectangular)
    tekFilterDiffMap = mtl->technique("tek_filterDiffuseMapEquirectangular");
  else
    tekFilterDiffMap = mtl->technique("tek_filterDiffuseMapStandard");
  OrkAssert(tekFilterDiffMap != nullptr);
  logchan_pbrgen->log("filterenv mtl<%p> tekFilterDiffMap<%p>", mtl.get(), tekFilterDiffMap);

  ///////////////////////////////////////////////
  auto filtex                                                      = std::make_shared<FilteredEnvMap>();
  rawenvmap->_vars->makeValueForKey<filtenvmapptr_t>("filtenvmap") = filtex;
  ///////////////////////////////////////////////
  logchan_pbrgen->log(
      "filterenv-diff tex<%p:%s> hash<0x%zx> equirectangular<%d>",
      (void*)rawenvmap.get(),
      rawenvmap->_debugName.c_str(),
      rawenvmap->_contentHash,
      int(equirectangular));
  boost::Crc64 basehasher;
  basehasher.accumulateString("filterenv-diff-v0");
  basehasher.accumulateItem<int>(_SALT());
  basehasher.accumulateItem<uint32_t>(uint32_t(equirectangular));
  basehasher.accumulateItem<uint64_t>(rawenvmap->_contentHash);
  basehasher.accumulateItem<uint32_t>(shader_hash());
  basehasher.accumulateItem<uint32_t>(this_hash());
  basehasher.finish();
  uint64_t cmipchain_hashkey = basehasher.result();
  auto cmipchain_datablock   = DataBlockCache::findDataBlock(cmipchain_hashkey);
  ///////////////////////////////////////////////
  if (cmipchain_datablock) {
    // logchan_pbrgen->log("filterenv-diff tex<%p> loading precomputed!", rawenvmap);
  } else {
    auto RCFD = std::make_shared<RenderContextFrameData>(targ);
    int w = rawenvmap->_width;
    int h = rawenvmap->_height;

    int numpix      = w * h;
    int imip        = 0;
    float roughness = 1.0f;
    std::map<int, std::shared_ptr<CaptureBuffer>> cap4mip;
    CompressedImageMipChain::miplevels_t compressed_levels;
    std::atomic<int> pending = 0;
    std::vector<compressedimg_ptr_t> cimgs;
    while (numpix != 0) {

      auto outgroup        = std::make_shared<RtGroup>(targ, w, h, MsaaSamples::MSAA_1X);
      auto outbuffr        = outgroup->createRenderTarget(EBufferFormat::RGBA32F);
      auto captureb        = std::make_shared<CaptureBuffer>();
      outgroup->_autoclear = true;

      filtex->_rtgroup     = outgroup;
      filtex->_rtbuffer    = outbuffr;
      outbuffr->_debugName = FormatString("filteredenvmap-diffenv-mip%d", imip);
      // outbuffer->

      /// logchan_pbrgen->log("filterenv imip<%d> w<%d> h<%d>", imip, w, h);
      // logchan_pbrgen->log("filterenv imip<%d> outgroup<%p> outbuf<%p>", imip, outgroup.get(), outbuffr.get());

      fbi->PushRtGroup(outgroup.get());
      mtl->begin(tekFilterDiffMap, RCFD);
      ///////////////////////////////////////////////
      mtl->bindParamMatrix(param_mvp, fmtx4::Identity());
      mtl->bindParamCTex(param_pfm, rawenvmap.get());
      mtl->bindParamFloat(param_ruf, roughness);
      mtl->commit();
      dwi->quad2DEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
      ///////////////////////////////////////////////
      mtl->end(RCFD);
      fbi->PopRtGroup();

      fbi->capture(outbuffr.get(), captureb.get());

      if (1) {
        auto outpath = file::Path::temp_dir() / FormatString("filteredenv-diffmap-mip%d.exr", imip);
        auto out     = ImageOutput::create(outpath.c_str());
        // logchan_pbrgen->log("filterenv write dbgout<%s> <%p>", outpath.c_str(), out.get());
        OrkAssert(out != nullptr);
        ImageSpec spec(w, h, 4, TypeDesc::FLOAT);
        out->open(outpath.c_str(), spec);
        out->write_image(TypeDesc::FLOAT, captureb->_data);
        out->close();
      }

      pending.fetch_add(1);
      auto cimg = std::make_shared<CompressedImage>();
      cimgs.push_back(cimg);
      auto op = [=, &pending]() {
        Image im;
        im.initRGBA8WithNormalizedFloatBuffer(w, h, 4, (const float*)captureb->_data);
        im.compressDefault(*cimg);
        pending.fetch_sub(1);
      };
      opq::concurrentQueue()->enqueue(op);

      rawenvmap->_vars->makeValueForKey<std::shared_ptr<RtGroup>>(FormatString("alt-tex-diffenv-group-mip%d", imip))   = outgroup;
      rawenvmap->_vars->makeValueForKey<std::shared_ptr<RtBuffer>>(FormatString("alt-tex-diffenv-buffer-mip%d", imip)) = outbuffr;

      cap4mip[imip] = captureb;
      w >>= 1;
      h >>= 1;
      roughness += 0.1f;
      numpix = w * h;
      imip++;
    }
    while (pending.load() > 0) {
      usleep(1000);
    }
    for (auto cimg : cimgs) {
      compressed_levels.push_back(*cimg);
    }

    CompressedImageMipChain cmipchain;
    cmipchain.initWithPrecompressedMipLevels(compressed_levels);
    cmipchain_datablock = std::make_shared<DataBlock>();
    cmipchain.writeXTX(cmipchain_datablock);
    DataBlockCache::setDataBlock(cmipchain_hashkey, cmipchain_datablock);
  }

  auto alt_tex        = std::make_shared<Texture>();
  alt_tex->_debugName = rawenvmap->_debugName + "[filtenvmap-processed-diffuse]";
  txi->LoadTexture(alt_tex, cmipchain_datablock);
  rawenvmap->_vars->makeValueForKey<texture_ptr_t>("alt-tex-diffenv") = alt_tex;

  __FIND_IT.store(0);
  targ->debugPopGroup();

  return alt_tex;
}

} // namespace ork::lev2
