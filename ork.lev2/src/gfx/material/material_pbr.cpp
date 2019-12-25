////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.inl>

OIIO_NAMESPACE_USING

ImplementReflectionX(ork::lev2::PBRMaterial, "PBRMaterial");

namespace ork::lev2 {

/////////////////////////////////////////////////////////////////////////

Texture* PBRMaterial::brdfIntegrationMap(GfxTarget* targ) {

  static Texture* _map = nullptr;

  if (nullptr == _map) {

    _map              = new lev2::Texture;
    _map->_debugName  = "brdfIntegrationMap";
    constexpr int DIM = 1024;
    _map->_width      = DIM;
    _map->_height     = DIM;
    _map->_texFormat  = EBUFFMT_RGBA32F;

    ///////////////////////////////
    // dblock cache
    ///////////////////////////////

    boost::Crc64 brdfhasher;
    brdfhasher.accumulateString(_map->_debugName); // identifier
    brdfhasher.accumulateItem<float>(1.0);         // version code
    brdfhasher.accumulateItem<float>(DIM);         // dimension
    brdfhasher.finish();
    uint64_t brdfhash = brdfhasher.result();
    printf("brdfIntegrationMap hashkey<%zx>\n", brdfhash);
    auto dblock = DataBlockCache::findDataBlock(brdfhash);
    if (dblock) {
      // loaded from cache
      printf("brdfIntegrationMap loaded from cache\n");
    } else { // recompute and cache
      printf("Begin Compute brdfIntegrationMap\n");
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
      printf("End Compute brdfIntegrationMap\n");
      fflush(stdout);
      DataBlockCache::setDataBlock(brdfhash, dblock);
    }

    _map->_data = dblock->data();

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

    targ->TXI()->initTextureFromData(_map, false);
  }
  return _map;
}

/////////////////////////////////////////////////////////////////////////

Texture* PBRMaterial::filterEnvMap(Texture* rawenvmap, GfxTarget* targ) {
  auto txi = targ->TXI();
  auto fbi = targ->FBI();
  auto fxi = targ->FXI();
  ///////////////////////////////////////////////
  static std::shared_ptr<FreestyleMaterial> mtl;
  static const FxShaderTechnique* tek   = nullptr;
  static const FxShaderParam* param_mvp = nullptr;
  static const FxShaderParam* param_pfm = nullptr;
  static const FxShaderParam* param_ruf = nullptr;

  targ->debugPushGroup("PBRMaterial::filterEnvMap");
  if (not mtl) {
    mtl = std::make_shared<FreestyleMaterial>();
    OrkAssert(mtl.get() != nullptr);
    mtl->gpuInit(targ, "orkshader://pbr_filterenv");
    tek = mtl->technique("tek_yo");
    OrkAssert(tek != nullptr);
    printf("filterenv mtl<%p> tek<%p>\n", mtl.get(), tek);
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
  auto targ_buf   = fbi->GetThisBuffer();
  float roughness = 0.0f;
  std::map<int, std::shared_ptr<CaptureBuffer>> cap4mip;
  auto mipchain        = new MipChain(w, h, EBUFFMT_RGBA32F, ETEXTYPE_2D);
  mipchain->_debugName = "filtenvmap-processed";
  while (numpix != 0) {

    auto outgroup = std::make_shared<RtGroup>(targ, w, h, 1);
    auto outbuffr = std::make_shared<RtBuffer>(outgroup.get(), lev2::ETGTTYPE_MRT0, lev2::EBUFFMT_RGBA32F, w, h);
    auto captureb = std::make_shared<CaptureBuffer>();

    filtex->_rtgroup     = outgroup;
    filtex->_rtbuffer    = outbuffr;
    outbuffr->_debugName = FormatString("filteredenvmap-mip%d", imip);
    // outbuffer->
    outgroup->SetMrt(0, outbuffr.get());

    printf("filterenv imip<%d> w<%d> h<%d>\n", imip, w, h);
    printf("filterenv imip<%d> outgroup<%p> outbuf<%p>\n", imip, outgroup.get(), outbuffr.get());

    fbi->PushRtGroup(outgroup.get());
    fbi->BeginFrame();
    fbi->Clear(fvec4(0, 0, 0, 0), 1);
    mtl->bindTechnique(tek);
    mtl->begin(RCFD);
    ///////////////////////////////////////////////
    mtl->bindParamMatrix(param_mvp, fmtx4::Identity);
    mtl->bindParamCTex(param_pfm, rawenvmap);
    mtl->bindParamFloat(param_ruf, roughness);
    mtl->commit();
    targ_buf->Render2dQuadEML(fvec4(-1, -1, 2, 2), fvec4(0, 0, 1, 1), fvec4(0, 0, 0, 0));
    ///////////////////////////////////////////////
    mtl->end(RCFD);
    fbi->EndFrame();
    fbi->PopRtGroup();

    fbi->capture(*outgroup.get(), 0, captureb.get());

    auto mipchain_level = mipchain->_levels[imip];
    memcpy(mipchain_level->_data, captureb->_data, w * h * 4 * sizeof(float));

    if (1) {
      auto outpath = file::Path::temp_dir() / FormatString("filteredenvmip%d.exr", imip);
      auto out     = ImageOutput::create(outpath.c_str());
      printf("filterenv write dbgout<%s> <%p>\n", outpath.c_str(), out.get());
      OrkAssert(out != nullptr);
      ImageSpec spec(w, h, 4, TypeDesc::FLOAT);
      out->open(outpath.c_str(), spec);
      out->write_image(TypeDesc::FLOAT, captureb->_data);
      out->close();
    }

    rawenvmap->_varmap.makeValueForKey<std::shared_ptr<RtGroup>>(FormatString("alt-tex-group-mip%d", imip))   = outgroup;
    rawenvmap->_varmap.makeValueForKey<std::shared_ptr<RtBuffer>>(FormatString("alt-tex-buffer-mip%d", imip)) = outbuffr;

    cap4mip[imip] = captureb;
    w >>= 1;
    h >>= 1;
    roughness += 0.1f;
    numpix = w * h;
    imip++;
  }

  auto alt_tex                                            = txi->createFromMipChain(mipchain);
  rawenvmap->_varmap.makeValueForKey<Texture*>("alt-tex") = alt_tex;

  targ->debugPopGroup();

  return rawenvmap;
}

/////////////////////////////////////////////////////////////////////////

PbrMatrixBlockApplicator* PbrMatrixBlockApplicator::getApplicator() {
  static PbrMatrixBlockApplicator* _gapplicator = new PbrMatrixBlockApplicator;
  return _gapplicator;
}

void PBRMaterial::describeX(class_t* c) {

  /////////////////////////////////////////////////////////////////

  chunkfile::materialreader_t reader = [](chunkfile::XgmMaterialReaderContext& ctx) -> ork::lev2::GfxMaterial* {
    auto targ             = ctx._varmap.typedValueForKey<GfxTarget*>("gfxtarget").value();
    auto txi              = targ->TXI();
    const auto& embtexmap = ctx._varmap.typedValueForKey<embtexmap_t>("embtexmap").value();

    int istring = 0;

    ctx._inputStream->GetItem(istring);
    auto materialname = ctx._reader.GetString(istring);

    ctx._inputStream->GetItem(istring);
    auto texbasename = ctx._reader.GetString(istring);
    auto mtl         = new PBRMaterial;
    mtl->SetName(AddPooledString(materialname));
    printf("materialName<%s>\n", materialname);
    ctx._inputStream->GetItem(istring);
    auto begintextures = ctx._reader.GetString(istring);
    assert(0 == strcmp(begintextures, "begintextures"));
    bool done = false;
    while (false == done) {
      ctx._inputStream->GetItem(istring);
      auto token = ctx._reader.GetString(istring);
      if (0 == strcmp(token, "endtextures"))
        done = true;
      else {
        ctx._inputStream->GetItem(istring);
        auto texname = ctx._reader.GetString(istring);
        auto itt     = embtexmap.find(texname);
        assert(itt != embtexmap.end());
        auto embtex = itt->second;
        printf("got tex channel<%s> name<%s> embtex<%p>\n", token, texname, embtex);
        auto tex       = new lev2::Texture;
        auto datablock = std::make_shared<DataBlock>(embtex->_srcdata, embtex->_srcdatalen);
        bool ok        = txi->LoadTexture(tex, datablock);
        assert(ok);
        if (0 == strcmp(token, "colormap")) {
          mtl->_texColor = tex;
        }
        if (0 == strcmp(token, "normalmap")) {
          mtl->_texNormal = tex;
        }
        if (0 == strcmp(token, "metalmap")) {
          mtl->_texRoughAndMetal = tex;
        }
      }
    }
    return mtl;
  };

  /////////////////////////////////////////////////////////////////

  chunkfile::materialwriter_t writer = [](chunkfile::XgmMaterialWriterContext& ctx) {
    auto pbrmtl = static_cast<const PBRMaterial*>(ctx._material);

    int istring = ctx._writer.stringIndex(pbrmtl->mMaterialName.c_str());
    ctx._outputStream->AddItem(istring);

    istring = ctx._writer.stringIndex(pbrmtl->_textureBaseName.c_str());
    ctx._outputStream->AddItem(istring);

    auto dotex = [&](std::string channelname, std::string texname) {
      if (texname.length()) {
        istring = ctx._writer.stringIndex(channelname.c_str());
        ctx._outputStream->AddItem(istring);
        istring = ctx._writer.stringIndex(texname.c_str());
        ctx._outputStream->AddItem(istring);
      }
    };
    istring = ctx._writer.stringIndex("begintextures");
    ctx._outputStream->AddItem(istring);
    dotex("colormap", pbrmtl->_colorMapName);
    dotex("normalmap", pbrmtl->_normalMapName);
    dotex("amboccmap", pbrmtl->_amboccMapName);
    dotex("emissivemap", pbrmtl->_emissiveMapName);
    dotex("roughmap", pbrmtl->_roughMapName);
    dotex("metalmap", pbrmtl->_metalMapName);
    istring = ctx._writer.stringIndex("endtextures");
    ctx._outputStream->AddItem(istring);
  };

  /////////////////////////////////////////////////////////////////

  c->annotate("xgm.writer", writer);
  c->annotate("xgm.reader", reader);
}
} // namespace ork::lev2
