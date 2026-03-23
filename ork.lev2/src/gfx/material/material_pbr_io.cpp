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
#include <ork/util/crc.h>
#include <ork/file/path.h>
#include <ork/file/chunkfile.inl>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/image.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <OpenImageIO/imageio.h>
#include <ork/kernel/datacache.h>
#include <ork/reflect/properties/registerX.inl>
//
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>
#include <ork/util/logger.h>

OIIO_NAMESPACE_USING

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_pbr_io = logger()->configureChannel("PBRMtlIO", fvec3(0.8, 0.8, 0.1), false);
///////////////////////////////////////////////////////////////////////////////

material_ptr_t PBRMaterial::_xgmReader( chunkfile::XgmMaterialReaderContext& ctx ){
    auto targ             = ctx._varmap->typedValueForKey<Context*>("gfxtarget").value();
    auto txi              = targ->TXI();
    const auto& embtexmap = ctx._varmap->typedValueForKey<embtexmap_t>("embtexmap").value();

    for (auto item : embtexmap) {
      logchan_pbr_io->log("embtex<%s>", item.first.c_str());
    }

    int istring = 0;

    ctx._inputStream->GetItem(istring);
    auto materialname = ctx._reader.GetString(istring);

    ctx._inputStream->GetItem(istring);
    auto texbasename = ctx._reader.GetString(istring);
    auto mtl         = std::make_shared<PBRMaterial>();
    mtl->_vars->makeValueForKey<bool>("from_xgm") = true;
    mtl->mMaterialName = materialname;
    logchan_pbr_io->log("read.xgm: materialName<%s>", materialname);
    ctx._inputStream->GetItem(istring);
    auto begintextures = ctx._reader.GetString(istring);
    OrkAssert(0 == strcmp(begintextures, "begintextures"));
    bool done = false;
    while (false == done) {
      ctx._inputStream->GetItem(istring);
      auto token = ctx._reader.GetString(istring);
      if (0 == strcmp(token, "endtextures"))
        done = true;
      else {
        ctx._inputStream->GetItem(istring);
        auto texname = ctx._reader.GetString(istring);
        logchan_pbr_io->log("read.xgm: find tex channel<%s> texname<%s> .. ", token, texname);
        auto itt = embtexmap.find(texname);
        OrkAssert(itt != embtexmap.end());
        auto embtex = itt->second;
        logchan_pbr_io->log("read.xgm: embtex<%p> data<%p> len<%zu>", embtex, embtex->_srcdata, embtex->_srcdatalen);
        auto image = std::make_shared<lev2::Image>();
        // crashes here...
        auto datablock = std::make_shared<DataBlock>(embtex->_srcdata, embtex->_srcdatalen);
        image->initFromDataBlock(datablock);
        //bool ok        = txi->LoadTexture(tex, datablock);
        //OrkAssert(ok);
        logchan_pbr_io->log(" embtex<%p> datablock<%p> len<%zu>", embtex, datablock.get(), datablock->length());
        logchan_pbr_io->log(" token<%s>", token);
        if (0 == strcmp(token, "colormap")) {
          mtl->_colorMapName = texname;
          mtl->_image_color = image;
        }
        if (0 == strcmp(token, "normalmap")) {
          mtl->_normalMapName = texname;
          mtl->_image_normal = image;
        }
        if (0 == strcmp(token, "mtlrufmap")) {
          mtl->_mtlRufMapName = texname;
          mtl->_image_mtlruf = image;
        }
        if (0 == strcmp(token, "emissivemap")) {
          mtl->_emissiveMapName = texname;
          mtl->_image_emissive = image;
        }
        if (0 == strcmp(token, "amboccmap")) {
          //mtl->_texAmbOcc     = tex;
          //mtl->_amboccMapName = texname;
          printf("amboccmap<%s>\n", texname );
        }
      }
    }

    mtl->assignImages( targ,                  //
                       mtl->_image_color,     //
                       mtl->_image_normal,    //
                       mtl->_image_mtlruf,    //
                       mtl->_image_emissive,  //
                       nullptr,  //
                       true);

    ctx._inputStream->GetItem<float>(mtl->_metallicFactor);
    ctx._inputStream->GetItem<float>(mtl->_roughnessFactor);
    ctx._inputStream->GetItem<fvec4>(mtl->_baseColor);
    ctx._inputStream->GetItem<float>(mtl->_alphaCutoff);
    ctx._inputStream->GetItem<int>(mtl->_alphaMode);
    ctx._inputStream->GetItem<bool>(mtl->_doubleSided);
    size_t num_lightmaps = 0;
    ctx._inputStream->GetItem<size_t>(num_lightmaps);
    mtl->_modifiers = std::make_shared<XgmModelAssetMaterialModifiers>();
    for(size_t i=0; i<num_lightmaps; i++){
      std::string key;
      std::string val;
      ctx._inputStream->GetItem(istring);
      key = ctx._reader.GetString(istring);
      uint64_t hash = 0;
      ctx._inputStream->GetItem<uint64_t>(hash);
      mtl->_modifiers->_lightmap_hashes[key] = hash;
      logchan_pbr_io->log("read.xgm:  lightmap<%s> -> 0x%lx", key.c_str(), hash );
      auto datablock = DataBlockCache::findDataBlock(hash);
      auto image = std::make_shared<lev2::Image>();
      image->initFromDataBlock(datablock);
      mtl->_lightmap_image_assets[key] = image;      
    }
    mtl->assignLightmaps(targ);

    if (auto try_ov = ctx._varmap->typedValueForKey<std::string>("override.shader.gbuf")) {
      const auto& ov_val = try_ov.value();
      if (ov_val == "normalviz") {
        mtl->_variant = "normalviz"_crcu;
      }
    }

    return mtl;
}

///////////////////////////////////////////////////////////////////////////////

void PBRMaterial::_xgmWriter( chunkfile::XgmMaterialWriterContext& ctx ) {
    auto pbrmtl = std::static_pointer_cast<const PBRMaterial>(ctx._material);

    int istring = ctx._writer.stringIndex(pbrmtl->mMaterialName.c_str());
    ctx._outputStream->AddItem(istring);

    istring = ctx._writer.stringIndex(pbrmtl->_textureBaseName.c_str());
    ctx._outputStream->AddItem(istring);

    auto dotex = [&](std::string channelname, std::string texname) {
      // logchan_pbr_io->log("write.xgm: tex channel<%s> texname<%s>", channelname.c_str(), texname.c_str());
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
    dotex("mtlrufmap", pbrmtl->_mtlRufMapName);
    istring = ctx._writer.stringIndex("endtextures");
    ctx._outputStream->AddItem(istring);

    ctx._outputStream->AddItem<float>(pbrmtl->_metallicFactor);
    ctx._outputStream->AddItem<float>(pbrmtl->_roughnessFactor);
    ctx._outputStream->AddItem<fvec4>(pbrmtl->_baseColor);
    ctx._outputStream->AddItem<float>(pbrmtl->_alphaCutoff);
    ctx._outputStream->AddItem<int>(pbrmtl->_alphaMode);
    ctx._outputStream->AddItem<bool>(pbrmtl->_doubleSided);
    //////////////////////////////////
    // save lightmaps
    //////////////////////////////////

    if (pbrmtl->_modifiers) {
      size_t num_lightmaps = pbrmtl->_modifiers->_lightmap_paths.size();
      ctx._outputStream->AddItem<size_t>(num_lightmaps);
      for(auto item : pbrmtl->_modifiers->_lightmap_paths){
        auto key = item.first;
        auto val = item.second;
        logchan_pbr_io->log("Write.xgm: lightmap<%s> val<%s>", key.c_str(), val.c_str());
        auto datablock = DataBlock::createFromPath(val);
        OrkAssert(datablock);
        //auto loadreq         = std::make_shared<asset::LoadRequest>(datablock);
        //auto asset_lightmap  = asset::AssetManager<lev2::TextureAsset>::load(loadreq);
        uint64_t hash = datablock->hash();
        DataBlockCache::setDataBlock(hash, datablock);
        istring = ctx._writer.stringIndex(key.c_str());
        ctx._outputStream->AddItem(istring);
        ctx._outputStream->AddItem(hash);
      }
    }
    else{
      ctx._outputStream->AddItem<size_t>(0);
    }

    //////////////////////////////////
    // logchan_pbr_io->log("write.xgm: _metallicFactor<%g>", pbrmtl->_metallicFactor);
    // logchan_pbr_io->log("write.xgm: _roughnessFactor<%g>", pbrmtl->_roughnessFactor);
    // logchan_pbr_io->log(
    //   "write.xgm: _baseColor<%g %g %g %g>", //
    // pbrmtl->_baseColor.x,                 //
    // pbrmtl->_baseColor.y,                 //
    // pbrmtl->_baseColor.z,                 //
    // pbrmtl->_baseColor.w);  
}

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::lev2 {
