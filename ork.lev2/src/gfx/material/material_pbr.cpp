////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
#include <ork/file/path.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/lev2/gfx/camera/uicam.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/material_pbr.inl>
#include <ork/gfx/brdf.inl>
#include <ork/pch.h>
#include <OpenImageIO/imageio.h>

OIIO_NAMESPACE_USING

ImplementReflectionX(ork::lev2::PBRMaterial, "PBRMaterial");

namespace ork::lev2 {

  /////////////////////////////////////////////////////////////////////////

  Texture* PBRMaterial::brdfIntegrationMap(GfxTarget* targ) {

    static Texture* _map = nullptr;

    if( nullptr == _map ){

      printf( "Begin Compute brdfIntegrationMap\n");

      // todo: use datablock cache
      
      _map = new lev2::Texture;
      _map->_debugName = "brdfIntegrationMap";
      constexpr int DIM = 1024;
      _map->_width = DIM;
      _map->_height = DIM;
      _map->_texFormat = EBUFFMT_RGBA32F;


      auto texels = (float*) malloc(DIM*DIM*4*sizeof(float));
      for( int y=0; y<DIM; y++ ){
        float fy = float(y)/float(DIM-1);
        int ybase = y*DIM;
        for( int x=0; x<DIM; x++ ){
          float fx = float(x)/float(DIM-1);
          dvec3 output = brdf::integrateGGX<1024>(fx,fy);
          int texidxbase = (ybase+x)*4;
          texels[texidxbase+0] = float(output.x);
          texels[texidxbase+1] = float(output.y);
          texels[texidxbase+2] = float(output.z);
          texels[texidxbase+3] = 1.0f;
        }
      }
      targ->TXI()->initTextureFromData(_map,false);

      auto outpath = file::Path::temp_dir()/"brdftest.exr";
      auto out = ImageOutput::create(outpath.c_str());
      assert(out!=nullptr);
      ImageSpec spec(DIM, DIM, 4, TypeDesc::FLOAT);
      out->open(outpath.c_str(), spec);
      out->write_image(TypeDesc::FLOAT, texels);
      out->close();


      free((void*)texels);
      printf( "End Compute brdfIntegrationMap\n");
      OrkAssert(false);
    }
    return _map;
  }

  /////////////////////////////////////////////////////////////////////////

  PbrMatrixBlockApplicator* PbrMatrixBlockApplicator::getApplicator() {
    static PbrMatrixBlockApplicator* _gapplicator = new PbrMatrixBlockApplicator;
    return _gapplicator;
  }

  void PBRMaterial::describeX(class_t* c) {

      /////////////////////////////////////////////////////////////////

      chunkfile::materialreader_t reader = [](chunkfile::XgmMaterialReaderContext& ctx)->ork::lev2::GfxMaterial*{

        auto targ = ctx._varmap.typedValueForKey<GfxTarget*>("gfxtarget").value();
        auto txi = targ->TXI();
        const auto& embtexmap = ctx._varmap.typedValueForKey<embtexmap_t>("embtexmap").value();

        int istring = 0;

        ctx._inputStream->GetItem(istring);
        auto materialname = ctx._reader.GetString(istring);

        ctx._inputStream->GetItem(istring);
        auto texbasename = ctx._reader.GetString(istring);
        auto mtl = new PBRMaterial;
        mtl->SetName(AddPooledString(materialname));
        printf( "materialName<%s>\n", materialname );
        ctx._inputStream->GetItem(istring);
        auto begintextures = ctx._reader.GetString(istring);
        assert(0==strcmp(begintextures,"begintextures"));
        bool done = false;
        while( false==done ){
          ctx._inputStream->GetItem(istring);
          auto token = ctx._reader.GetString(istring);
          if( 0 == strcmp(token,"endtextures"))
            done = true;
          else {
            ctx._inputStream->GetItem(istring);
            auto texname = ctx._reader.GetString(istring);
            auto itt = embtexmap.find(texname);
            assert(itt!=embtexmap.end());
            auto embtex = itt->second;
            printf( "got tex channel<%s> name<%s> embtex<%p>\n", token, texname, embtex );
            auto tex = new lev2::Texture;
            auto datablock = std::make_shared<DataBlock>(embtex->_srcdata,embtex->_srcdatalen);
            bool ok = txi->LoadTexture(tex,datablock);
            assert(ok);
            if( 0 == strcmp(token,"colormap")){
              mtl->_texColor = tex;
            }
            if( 0 == strcmp(token,"normalmap")){
              mtl->_texNormal = tex;
            }
            if( 0 == strcmp(token,"metalmap")){
              mtl->_texRoughAndMetal = tex;
            }
          }

        }
        return mtl;
      };

      /////////////////////////////////////////////////////////////////

      chunkfile::materialwriter_t writer = [](chunkfile::XgmMaterialWriterContext& ctx){
        auto pbrmtl = static_cast<const PBRMaterial*>(ctx._material);

        int istring = ctx._writer.stringIndex(pbrmtl->mMaterialName.c_str());
        ctx._outputStream->AddItem(istring);

        istring = ctx._writer.stringIndex(pbrmtl->_textureBaseName.c_str());
        ctx._outputStream->AddItem(istring);

        auto dotex = [&](std::string channelname, std::string texname){
          if( texname.length() ) {
            istring = ctx._writer.stringIndex(channelname.c_str());
            ctx._outputStream->AddItem(istring);
            istring = ctx._writer.stringIndex(texname.c_str());
            ctx._outputStream->AddItem(istring);
          }
        };
        istring = ctx._writer.stringIndex("begintextures");
        ctx._outputStream->AddItem(istring);
        dotex( "colormap", pbrmtl->_colorMapName );
        dotex( "normalmap", pbrmtl->_normalMapName );
        dotex( "amboccmap", pbrmtl->_amboccMapName );
        dotex( "emissivemap", pbrmtl->_emissiveMapName );
        dotex( "roughmap", pbrmtl->_roughMapName );
        dotex( "metalmap", pbrmtl->_metalMapName );
        istring = ctx._writer.stringIndex("endtextures");
        ctx._outputStream->AddItem(istring);

      };

      /////////////////////////////////////////////////////////////////

      c->annotate("xgm.writer",writer);
      c->annotate("xgm.reader",reader);
  }
} // namespace ork::lev2 {
