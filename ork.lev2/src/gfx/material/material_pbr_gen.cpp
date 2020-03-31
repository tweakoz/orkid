////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
#include <ork/lev2/gfx/material_freestyle.inl>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/image.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {

/////////////////////////////////////////////////////////////////////////

Texture* PBRMaterial::brdfIntegrationMap(Context* targ) {
  targ->makeCurrentContext();

  static Texture* _map = nullptr;

  if (nullptr == _map) {

    _map              = new lev2::Texture;
    _map->_debugName  = "brdfIntegrationMap";
    constexpr int DIM = 1024;

    ///////////////////////////////
    // dblock cache
    ///////////////////////////////

    boost::Crc64 brdfhasher;
    brdfhasher.accumulateString(_map->_debugName); // identifier
    brdfhasher.accumulateItem<float>(1.0);         // version code
    brdfhasher.accumulateItem<float>(DIM);         // dimension
    brdfhasher.finish();
    uint64_t brdfhash = brdfhasher.result();
    // printf("brdfIntegrationMap hashkey<%zx>\n", brdfhash);
    auto dblock = DataBlockCache::findDataBlock(brdfhash);
    if (dblock) {
      // loaded from cache
      // printf("brdfIntegrationMap loaded from cache\n");
    } else { // recompute and cache
      // printf("Begin Compute brdfIntegrationMap\n");
      dblock        = std::make_shared<DataBlock>();
      float* texels = dblock->allocateItems<float>(DIM * DIM * 4);
      auto group    = opq::createCompletionGroup(opq::concurrentQueue());
      for (int y = 0; y < DIM; y++) {
        float fy  = float(y) / float(DIM - 1);
        int ybase = y * DIM;
        group->enqueue([=]() {
          for (int x = 0; x < DIM; x++) {
            float fx               = float(x) / float(DIM - 1);
            dvec3 output           = brdf::integrateGGX<1024>(fx, fy);
            int texidxbase         = (ybase + x) * 4;
            texels[texidxbase + 0] = float(output.x);
            texels[texidxbase + 1] = float(output.y);
            texels[texidxbase + 2] = float(output.z);
            texels[texidxbase + 3] = 1.0f;
          }
        });
      }
      group->join();
      // printf("End Compute brdfIntegrationMap\n");
      fflush(stdout);
      DataBlockCache::setDataBlock(brdfhash, dblock);
    }

    ///////////////////////////////
    // verify (debug)
    ///////////////////////////////

    if (1) {
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
    tid._format      = EBufferFormat::RGBA32F;
    tid._autogenmips = true;
    tid._data        = dblock->data();

    targ->TXI()->initTextureFromData(_map, tid);
  }
  return _map;
}

/////////////////////////////////////////////////////////////////////////

Texture* PBRMaterial::filterSpecularEnvMap(Texture* rawenvmap, Context* targ) {
  targ->makeCurrentContext();
  auto txi = targ->TXI();
  auto fbi = targ->FBI();
  auto fxi = targ->FXI();
  auto gbi = targ->GBI();
  auto dwi = targ->DWI();
  ///////////////////////////////////////////////
  static std::shared_ptr<FreestyleMaterial> mtl;
  static const FxShaderTechnique* tekFilterSpecMap = nullptr;
  static const FxShaderParam* param_mvp            = nullptr;
  static const FxShaderParam* param_pfm            = nullptr;
  static const FxShaderParam* param_ruf            = nullptr;
  targ->debugPushGroup("PBRMaterial::filterSpecularEnvMap");
  if (not mtl) {
    mtl = std::make_shared<FreestyleMaterial>();
    OrkAssert(mtl.get() != nullptr);
    mtl->gpuInit(targ, "orkshader://pbr_filterenv");
    tekFilterSpecMap = mtl->technique("tek_filterSpecularMap");
    OrkAssert(tekFilterSpecMap != nullptr);
    printf("filterenv mtl<%p> tekFilterSpecMap<%p>\n", mtl.get(), tekFilterSpecMap);
    param_mvp = mtl->param("mvp");
    param_pfm = mtl->param("prefiltmap");
    param_ruf = mtl->param("roughness");
  }
  ///////////////////////////////////////////////
  auto filtex                                                       = std::make_shared<FilteredEnvMap>();
  rawenvmap->_varmap.makeValueForKey<filtenvmapptr_t>("filtenvmap") = filtex;
  ///////////////////////////////////////////////
  RenderContextFrameData RCFD(targ);
  int w = rawenvmap->_width;
  int h = rawenvmap->_height;

  int numpix      = w * h;
  int imip        = 0;
  float roughness = 0.0f;
  CompressedImageMipChain::miplevels_t compressed_levels;
  while (numpix != 0) {

    auto outgroup = std::make_shared<RtGroup>(targ, w, h, 1);
    auto outbuffr = std::make_shared<RtBuffer>(lev2::ERTGSLOT0, lev2::EBufferFormat::RGBA32F, w, h);
    auto captureb = std::make_shared<CaptureBuffer>();

    outgroup->_autoclear = true;
    filtex->_rtgroup     = outgroup;
    filtex->_rtbuffer    = outbuffr;
    outbuffr->_debugName = FormatString("filteredenvmap-specenv-mip%d", imip);
    outgroup->SetMrt(0, outbuffr.get());

    printf("filterenv imip<%d> w<%d> h<%d>\n", imip, w, h);
    printf("filterenv imip<%d> outgroup<%p> outbuf<%p>\n", imip, outgroup.get(), outbuffr.get());

    fbi->PushRtGroup(outgroup.get());
    mtl->bindTechnique(tekFilterSpecMap);
    mtl->begin(RCFD);
    ///////////////////////////////////////////////
    mtl->bindParamMatrix(param_mvp, fmtx4::Identity);
    mtl->bindParamCTex(param_pfm, rawenvmap);
    mtl->bindParamFloat(param_ruf, roughness);
    mtl->commit();
    dwi->quad2DEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
    ///////////////////////////////////////////////
    mtl->end(RCFD);
    fbi->PopRtGroup();

    fbi->capture(*outgroup.get(), 0, captureb.get());

    if (1) {
      auto outpath = file::Path::temp_dir() / FormatString("filteredenv-specmap-mip%d.exr", imip);
      auto out     = ImageOutput::create(outpath.c_str());
      printf("filterenv write dbgout<%s> <%p>\n", outpath.c_str(), out.get());
      OrkAssert(out != nullptr);
      ImageSpec spec(w, h, 4, TypeDesc::FLOAT);
      out->open(outpath.c_str(), spec);
      out->write_image(TypeDesc::FLOAT, captureb->_data);
      out->close();
    }

    Image im;
    im.initWithNormalizedFloatBuffer(w, h, 4, (const float*)captureb->_data);
    CompressedImage cim;
    im.compressBC7(cim);
    compressed_levels.push_back(cim);

    rawenvmap->_varmap.makeValueForKey<std::shared_ptr<RtGroup>>(FormatString("alt-tex-specenv-group-mip%d", imip))   = outgroup;
    rawenvmap->_varmap.makeValueForKey<std::shared_ptr<RtBuffer>>(FormatString("alt-tex-specenv-buffer-mip%d", imip)) = outbuffr;
    w >>= 1;
    h >>= 1;
    roughness += 0.1f;
    numpix = w * h;
    imip++;
  }

  CompressedImageMipChain mipchain;
  mipchain.initWithPrecompressedMipLevels(compressed_levels);
  auto xtx_datablock = std::make_shared<DataBlock>();
  mipchain.writeXTX(xtx_datablock);

  auto alt_tex        = new Texture;
  alt_tex->_debugName = "filtenvmap-processed-specular";
  txi->LoadTexture(alt_tex, xtx_datablock);

  rawenvmap->_varmap.makeValueForKey<Texture*>("alt-tex-specenv") = alt_tex;

  targ->debugPopGroup();

  return alt_tex;
}

/////////////////////////////////////////////////////////////////////////

Texture* PBRMaterial::filterDiffuseEnvMap(Texture* rawenvmap, Context* targ) {
  targ->makeCurrentContext();
  auto txi = targ->TXI();
  auto fbi = targ->FBI();
  auto fxi = targ->FXI();
  auto gbi = targ->GBI();
  auto dwi = targ->DWI();
  ///////////////////////////////////////////////
  static std::shared_ptr<FreestyleMaterial> mtl;
  static const FxShaderTechnique* tekFilterDiffMap = nullptr;
  static const FxShaderParam* param_mvp            = nullptr;
  static const FxShaderParam* param_pfm            = nullptr;
  static const FxShaderParam* param_ruf            = nullptr;

  targ->debugPushGroup("PBRMaterial::filterDiffuseEnvMap");
  if (not mtl) {
    mtl = std::make_shared<FreestyleMaterial>();
    OrkAssert(mtl.get() != nullptr);
    mtl->gpuInit(targ, "orkshader://pbr_filterenv");
    tekFilterDiffMap = mtl->technique("tek_filterDiffuseMap");
    OrkAssert(tekFilterDiffMap != nullptr);
    // printf("filterenv mtl<%p> tekFilterDiffMap<%p>\n", mtl.get(), tekFilterDiffMap);
    param_mvp = mtl->param("mvp");
    param_pfm = mtl->param("prefiltmap");
    param_ruf = mtl->param("roughness");
  }
  ///////////////////////////////////////////////
  auto filtex                                                       = std::make_shared<FilteredEnvMap>();
  rawenvmap->_varmap.makeValueForKey<filtenvmapptr_t>("filtenvmap") = filtex;
  ///////////////////////////////////////////////
  RenderContextFrameData RCFD(targ);
  int w = rawenvmap->_width;
  int h = rawenvmap->_height;

  int numpix      = w * h;
  int imip        = 0;
  float roughness = 1.0f;
  std::map<int, std::shared_ptr<CaptureBuffer>> cap4mip;
  CompressedImageMipChain::miplevels_t compressed_levels;
  while (numpix != 0) {

    auto outgroup        = std::make_shared<RtGroup>(targ, w, h, 1);
    auto outbuffr        = std::make_shared<RtBuffer>(lev2::ERTGSLOT0, lev2::EBufferFormat::RGBA32F, w, h);
    auto captureb        = std::make_shared<CaptureBuffer>();
    outgroup->_autoclear = true;

    filtex->_rtgroup     = outgroup;
    filtex->_rtbuffer    = outbuffr;
    outbuffr->_debugName = FormatString("filteredenvmap-diffenv-mip%d", imip);
    // outbuffer->
    outgroup->SetMrt(0, outbuffr.get());

    /// printf("filterenv imip<%d> w<%d> h<%d>\n", imip, w, h);
    // printf("filterenv imip<%d> outgroup<%p> outbuf<%p>\n", imip, outgroup.get(), outbuffr.get());

    fbi->PushRtGroup(outgroup.get());
    mtl->bindTechnique(tekFilterDiffMap);
    mtl->begin(RCFD);
    ///////////////////////////////////////////////
    mtl->bindParamMatrix(param_mvp, fmtx4::Identity);
    mtl->bindParamCTex(param_pfm, rawenvmap);
    mtl->bindParamFloat(param_ruf, roughness);
    mtl->commit();
    dwi->quad2DEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
    ///////////////////////////////////////////////
    mtl->end(RCFD);
    fbi->PopRtGroup();

    fbi->capture(*outgroup.get(), 0, captureb.get());

    if (1) {
      auto outpath = file::Path::temp_dir() / FormatString("filteredenv-diffmap-mip%d.exr", imip);
      auto out     = ImageOutput::create(outpath.c_str());
      // printf("filterenv write dbgout<%s> <%p>\n", outpath.c_str(), out.get());
      OrkAssert(out != nullptr);
      ImageSpec spec(w, h, 4, TypeDesc::FLOAT);
      out->open(outpath.c_str(), spec);
      out->write_image(TypeDesc::FLOAT, captureb->_data);
      out->close();
    }

    Image im;
    im.initWithNormalizedFloatBuffer(w, h, 4, (const float*)captureb->_data);
    CompressedImage cim;
    im.compressBC7(cim);
    compressed_levels.push_back(cim);

    rawenvmap->_varmap.makeValueForKey<std::shared_ptr<RtGroup>>(FormatString("alt-tex-diffenv-group-mip%d", imip))   = outgroup;
    rawenvmap->_varmap.makeValueForKey<std::shared_ptr<RtBuffer>>(FormatString("alt-tex-diffenv-buffer-mip%d", imip)) = outbuffr;

    cap4mip[imip] = captureb;
    w >>= 1;
    h >>= 1;
    roughness += 0.1f;
    numpix = w * h;
    imip++;
  }

  CompressedImageMipChain mipchain;
  mipchain.initWithPrecompressedMipLevels(compressed_levels);
  auto xtx_datablock = std::make_shared<DataBlock>();
  mipchain.writeXTX(xtx_datablock);

  auto alt_tex        = new Texture;
  alt_tex->_debugName = "filtenvmap-processed-diffuse";
  txi->LoadTexture(alt_tex, xtx_datablock);
  rawenvmap->_varmap.makeValueForKey<Texture*>("alt-tex-diffenv") = alt_tex;

  targ->debugPopGroup();

  return alt_tex;
}

} // namespace ork::lev2
