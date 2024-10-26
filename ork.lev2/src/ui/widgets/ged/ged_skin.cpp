////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/ui/ged/ged.h>
#include <ork/lev2/ui/ged/ged_node.h>
#include <ork/lev2/ui/ged/ged_skin.h>
#include <ork/lev2/ui/ged/ged_container.h>
#include <ork/lev2/ui/ged/ged_surface.h>
#include <ork/kernel/core_interface.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/math/misc_math.h>

////////////////////////////////////////////////////////////////
namespace ork::lev2::ged {
////////////////////////////////////////////////////////////////

texture_ptr_t createDownArrowTexture(lev2::Context* ctx){
  auto texture = std::make_shared<lev2::Texture>();
  texture->_debugName = "ged_downarrow";
  TextureInitData tid;
  tid._w     = 9;
  tid._h     = 9;
  tid._src_format    = EBufferFormat::RGBA8;
  tid._dst_format    = EBufferFormat::RGBA8;
  tid._autogenmips = false;
  uint32_t X = 0xFFFFFFFF;
  uint32_t O = 0x00000000;
  uint32_t data[] = {
    X, X, X, X, X, X, X, X, X,
    X, O, O, O, O, O, O, O, X,
    O, X, O, O, O, O, O, X, O,
    O, X, O, O, O, O, O, X, O,
    O, O, X, O, O, O, X, O, O,
    O, O, X, O, O, O, X, O, O,
    O, O, O, X, O, X, O, O, O,
    O, O, O, X, O, X, O, O, O,
    O, O, O, O, X, O, O, O, O,
  };
  tid._data = (const void*) data;
  ctx->TXI()->initTextureFromData(texture.get(), tid);
  return texture;
}

texture_ptr_t createUpArrowTexture(lev2::Context* ctx){
  auto texture = std::make_shared<lev2::Texture>();
  texture->_debugName = "ged_uparrow";
  TextureInitData tid;
  tid._w     = 9;
  tid._h     = 9;
  tid._src_format    = EBufferFormat::RGBA8;
  tid._dst_format    = EBufferFormat::RGBA8;
  tid._autogenmips = false;
  uint32_t X = 0xFFFFFFFF;
  uint32_t O = 0x00000000;
  uint32_t data[] = {

    O, O, O, O, X, O, O, O, O,
    O, O, O, X, X, X, O, O, O,
    O, O, X, X, O, X, X, O, O,
    O, X, X, O, O, O, X, X, O,
    X, X, O, O, X, O, O, X, X,
    O, O, O, X, X, X, O, O, O,
    O, O, X, X, O, X, X, O, O,
    O, X, X, O, O, O, X, X, O,
    X, X, O, O, X, O, O, X, X,

  };
  tid._data = (const void*) data;
  ctx->TXI()->initTextureFromData(texture.get(), tid);
  return texture;
}

texture_ptr_t createRightArrowTexture(lev2::Context* ctx){
  auto texture = std::make_shared<lev2::Texture>();
  texture->_debugName = "ged_rightarrow";
  TextureInitData tid;
  tid._w     = 9;
  tid._h     = 9;
  tid._src_format    = EBufferFormat::RGBA8;
  tid._dst_format    = EBufferFormat::RGBA8;
  tid._autogenmips = false;
  uint32_t X = 0xFFFFFFFF;
  uint32_t O = 0x00000000;
  uint32_t data[] = {
    X, X, O, O, O, O, O, O, O,
    X, O, X, X, O, O, O, O, O,
    X, O, O, O, X, X, O, O, O,
    X, O, O, O, O, O, X, X, O,
    X, O, O, O, O, O, O, O, X,
    X, O, O, O, O, O, X, X, O,
    X, O, O, O, X, X, O, O, O,
    X, O, X, X, O, O, O, O, O,
    X, X, O, O, O, O, O, O, O,
  };
  tid._data = (const void*) data;
  ctx->TXI()->initTextureFromData(texture.get(), tid);
  return texture;
}

////////////////////////////////////////////////////////////////

texture_ptr_t createReplaceObjTexture(lev2::Context* ctx){
  auto texture = std::make_shared<lev2::Texture>();
  texture->_debugName = "ged_rightarrow";
  TextureInitData tid;
  tid._w     = 9;
  tid._h     = 9;
  tid._src_format    = EBufferFormat::RGBA8;
  tid._dst_format    = EBufferFormat::RGBA8;
  tid._autogenmips = false;
  uint32_t X = 0xFFFFFFFF;
  uint32_t O = 0x00000000;
  uint32_t data[] = {
    O, O, X, X, X, X, X, O, O,
    O, X, X, O, O, O, X, X, O,
    X, X, O, O, O, O, O, X, X,
    X, O, O, O, O, O, O, O, X,
    X, O, O, O, O, O, O, O, X,
    X, O, O, O, O, O, O, O, X,
    X, X, O, O, O, O, O, X, X,
    O, X, X, O, O, O, X, X, O,
    O, O, X, X, X, X, X, O, O,
  };
  tid._data = (const void*) data;
  ctx->TXI()->initTextureFromData(texture.get(), tid);
  return texture;
}

////////////////////////////////////////////////////////////////

texture_ptr_t createGedSpawnTexture(lev2::Context* ctx){
  auto texture = std::make_shared<lev2::Texture>();
  texture->_debugName = "ged_spawm";
  TextureInitData tid;
  tid._w     = 9;
  tid._h     = 9;
  tid._src_format    = EBufferFormat::RGBA8;
  tid._dst_format    = EBufferFormat::RGBA8;
  tid._autogenmips = false;
  uint32_t X = 0xFFFFFFFF;
  uint32_t O = 0x00000000;
  uint32_t data[] = {
    X, X, X, X, X, X, X, X, X,
    X, O, O, O, O, O, O, O, X,
    X, O, X, X, X, X, X, O, X,
    X, O, X, X, X, X, X, O, X,
    X, O, X, X, X, X, X, O, X,
    X, O, X, X, X, X, X, O, X,
    X, O, X, X, X, X, X, O, X,
    X, O, O, O, O, O, O, O, X,
    X, X, X, X, X, X, X, X, X,
  };
  tid._data = (const void*) data;
  ctx->TXI()->initTextureFromData(texture.get(), tid);
  return texture;
}
////////////////////////////////////////////////////////////////

std::set<void*>& GedSkin::GetObjSet() {
  static std::set<void*> gObjSet;
  return gObjSet;
}
void GedSkin::ClearObjSet() {
  GetObjSet().clear();
}
void GedSkin::AddToObjSet(void* pobj) {
  GetObjSet().insert(pobj);
}
bool GedSkin::IsObjInSet(void* pobj) {
  bool rval = false;
  rval      = (GetObjSet().find(pobj) != GetObjSet().end());
  return rval;
}

void GedSkin::pushCustomColor(fcolor3 color) {
  _colorStack.push(color);
}
void GedSkin::popCustomColor() {
  _colorStack.pop();
}

////////////////////////////////////////////////////////////////

GedSkin::GedSkin(ork::lev2::Context* ctx)
    : _scrollY(0)
    , _gedVP(nullptr)
    , _font(nullptr)
    , _char_w(0)
    , _char_h(0) {

  ////////////////////////////////////////////////////////
  _textures["downarrow"_crcu] = createDownArrowTexture(ctx);
  _textures["uparrow"_crcu] = createUpArrowTexture(ctx);
  _textures["rightarrow"_crcu] = createRightArrowTexture(ctx);
  _textures["replaceobj"_crcu] = createReplaceObjTexture(ctx);
  //_textures["spawnnewged"_crcu] = createGedSpawnTexture(ctx);
  ////////////////////////////////////////////////////////

    _timer.Start();
}

void GedSkin::gpuInit(lev2::Context* ctx) {
  _material = std::make_shared<FreestyleMaterial>();
  _material->gpuInit(ctx, "orkshader://ui2");
  _tekpick       = _material->technique("ui_picking");
  _tekvtxcolor   = _material->technique("ui_vtxcolor");
  _tektexcolor   = _material->technique("ui_texcolor");
  _tekvtxpick    = _material->technique("ui_vtxpicking");
  _tekmodcolor   = _material->technique("ui_modcolor");
  _tekcolorwheel = _material->technique("ui_colorwheel");
  _parmvp        = _material->param("mvp");
  _parmodcolor   = _material->param("modcolor");
  _parobjid      = _material->param("objid");
  _partime       = _material->param("time");
  _partexture    = _material->param("ColorMap");
  _material->dump();
}

//'ork::tool::ged::GedSkin::PrimContainer *' to
//'ork::tool::ged::GedSkin::PrimContainer *'

void GedSkin::AddPrim(const GedPrim& cb) {
  int isort                   = calcsort(cb.miSortKey);
  PrimContainers::iterator it = mPrimContainers.find(isort);
  if (it == mPrimContainers.end()) {
    PrimContainer* pcontainer = mPrimContainerPool.allocate();
    OrkAssert(pcontainer != 0);
    it = mPrimContainers.AddSorted(isort, pcontainer);
  }

  PrimContainer* pctr = it->second;
  if (cb._renderLambda) {
    GedPrim* pooledprim = pctr->mPrimPool.allocate();
    *pooledprim         = cb;
    pctr->mCustomPrims.push_back(pooledprim);
  } else
    switch (cb.meType) {
      case PrimitiveType::LINES: {
        GedPrim* pooledprim = pctr->mPrimPool.allocate();
        *pooledprim         = cb;
        pctr->mLinePrims.push_back(pooledprim);
        break;
      }
      case PrimitiveType::QUADS: {
        GedPrim* pooledprim = pctr->mPrimPool.allocate();
        *pooledprim         = cb;
        if(pooledprim->_texture){
          auto& grptex = pctr->mTexQuadPrims[pooledprim->_texture];
          grptex.push_back(pooledprim);
        }
        else{
          pctr->mQuadPrims.push_back(pooledprim);
        }
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
}

void GedSkin::PrimContainer::clear() {
  mPrimPool.clear();
  mLinePrims.clear();
  mQuadPrims.clear();
  for( auto& item : mTexQuadPrims ){
    item.second.clear();
  }
  mCustomPrims.clear();
}

void GedSkin::clear() {
  for (int i = 0; i < int(mPrimContainerPool.capacity()); i++) {
    mPrimContainerPool.direct_access(i).clear();
  }
  mPrimContainerPool.clear();
  mPrimContainers.clear();
}

void GedSkin::DrawTexBox( GedObject* pnode, int ix, int iy, texture_ptr_t tex, fvec4 color ){
  GedPrim prim;
  prim.ix1 = ix;
  prim.ix2 = ix + tex->_width - 1;
  prim.iy1 = iy;
  prim.iy2 = iy + tex->_height - 1;
  fvec4 uobj = _gedVP->AssignPickId(pnode);
  if (_is_pickmode) {
    AddToObjSet((void*)pnode);
  }

  prim._ucolor   = _is_pickmode ? uobj : color; 
  prim.meType    = PrimitiveType::QUADS;
  prim.miSortKey = calcsort(4);
  prim._texture = tex;
  AddPrim(prim);

}

///////////////////////////////////////////////////////////////////

void GedSkin::DrawTexBoxCrc(GedObject* pnode, int ix, int iy, uint32_t crc, ESTYLE ic ){
  auto it = _textures.find(crc);
  if( it != _textures.end() ){
    auto color = GetStyleColor(pnode, ic);
    DrawTexBox( pnode, ix, iy, it->second, color );
  }
}

///////////////////////////////////////////////////////////////////

void GedSkin::DrawColorBox(GedObject* pnode, int ix, int iy, int iw, int ih, fvec4 color, int isort) {
  GedPrim prim;
  prim.ix1 = ix;
  prim.ix2 = ix + iw;
  prim.iy1 = iy;
  prim.iy2 = iy + ih;

  fvec4 uobj = _gedVP->AssignPickId(pnode);

  if (_is_pickmode) {
    AddToObjSet((void*)pnode);
    // printf( "insert obj<%p>\n", (void*) pnode );
  }

  prim._ucolor   = _is_pickmode ? uobj : color; // Default Outline
  prim.meType    = PrimitiveType::QUADS;
  prim.miSortKey = calcsort(isort);
  AddPrim(prim);
}

////////////////////////////////////////////////////////////////

orkvector<GedSkin*> instantiateSkins(ork::lev2::Context* ctx) {
  orkvector<GedSkin*> skins;
  auto skin0 = new GedSkin0(ctx);
  auto skin1 = new GedSkin1(ctx);
  skins.push_back(skin0);
  skins.push_back(skin1);
  return skins;
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ged
////////////////////////////////////////////////////////////////
