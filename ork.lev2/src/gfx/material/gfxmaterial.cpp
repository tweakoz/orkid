////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/application/application.h>
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
#include <ork/pch.h>

namespace ork {
static const std::string TexDestStrings[lev2::ETEXDEST_END + 2] = {
    "ETEXDEST_AMBIENT", "ETEXDEST_DIFFUSE", "ETEXDEST_SPECULAR", "ETEXDEST_BUMP", "ETEXDEST_END", ""};
template <> const EPropType PropType<lev2::ETextureDest>::meType   = EPROPTYPE_ENUM;
template <> const char* PropType<lev2::ETextureDest>::mstrTypeName = "GfxEnv::ETextureDest";
template <> lev2::ETextureDest PropType<lev2::ETextureDest>::FromString(const PropTypeString& String) {
  return PropType::FindValFromStrings<lev2::ETextureDest>(String.c_str(), TexDestStrings, lev2::ETEXDEST_END);
}
template <> void PropType<lev2::ETextureDest>::ToString(const lev2::ETextureDest& e, PropTypeString& tstr) {
  tstr.set(TexDestStrings[int(e)].c_str());
}
template <> void PropType<lev2::ETextureDest>::GetValueSet(const std::string*& ValueStrings, int& NumStrings) {
  NumStrings   = lev2::ETEXDEST_END + 1;
  ValueStrings = TexDestStrings;
}
} // namespace ork

/////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterial, "GfxMaterial")
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstApplicator, "MaterialInstApplicator")
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstItem, "MaterialInstItem")
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstItemMatrix, "MaterialInstItemMatrix")
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::MaterialInstItemMatrixBlock, "MaterialInstItemMatrixBlock")
ImplementReflectionX(ork::lev2::PBRMaterial, "PBRMaterial");



namespace ork {

namespace chunkfile {

XgmMaterialWriterContext::XgmMaterialWriterContext(Writer& w) : _writer(w) {}
XgmMaterialReaderContext::XgmMaterialReaderContext(Reader& r) : _reader(r) {}

}
namespace lev2 {


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
void MaterialInstApplicator::Describe() {}
void MaterialInstItem::Describe() {}
void MaterialInstItemMatrix::Describe() {}
void MaterialInstItemMatrixBlock::Describe() {}

void GfxMaterial::Describe() {}

/////////////////////////////////////////////////////////////////////////

RenderQueueSortingData::RenderQueueSortingData()
    : miSortingPass(4)
    , miSortingOffset(0)
    , mbTransparency(false) {}

/////////////////////////////////////////////////////////////////////////

TextureContext::TextureContext(const Texture* ptex, float repU, float repV)
    : mpTexture(ptex)
    , mfRepeatU(repU)
    , mfRepeatV(repV) {}

/////////////////////////////////////////////////////////////////////////

GfxMaterial::GfxMaterial()
    : miNumPasses(0)
    , mRenderContexInstData(0)
    , mMaterialName(AddPooledString("DefaultMaterial")) {
  PushDebug(false);
}

GfxMaterial::~GfxMaterial() {}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial::PushDebug(bool bdbg) { mDebug.push(bdbg); }
void GfxMaterial::PopDebug() { mDebug.pop(); }
bool GfxMaterial::IsDebug() { return mDebug.top(); }

/////////////////////////////////////////////////////////////////////////

void GfxMaterial::SetTexture(ETextureDest edest, const TextureContext& tex) { mTextureMap[edest] = tex; }

const TextureContext& GfxMaterial::GetTexture(ETextureDest edest) const { return mTextureMap[edest]; }

TextureContext& GfxMaterial::GetTexture(ETextureDest edest) { return mTextureMap[edest]; }

}} // namespace ork::lev2

/////////////////////////////////////////////////////////////////////////
