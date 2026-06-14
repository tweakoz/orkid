////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

/////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::GfxMaterial, "GfxMaterial")

namespace ork {

namespace chunkfile {

XgmMaterialWriterContext::XgmMaterialWriterContext(Writer& w)
    : _writer(w) {
}
XgmMaterialReaderContext::XgmMaterialReaderContext(Reader& r)
    : _reader(r) {
    _varmap = std::make_shared<varmap::VarMap>();
}

} // namespace chunkfile
namespace lev2 {

void GfxMaterial::Describe() {
}

/////////////////////////////////////////////////////////////////////////

fxpipelinecache_constptr_t GfxMaterial::pipelineCache(fxpipelinepermutation_set_constptr_t perms) const{
  return _doFxPipelineCache(perms);
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial::bindParam(fxparam_constptr_t p, varval_t v){
  OrkAssert(p!=nullptr);
  _bound_params[p] = v;
  _bound_params_stamp++; // pipelines re-overlay on their next beginBlock
}

/////////////////////////////////////////////////////////////////////////

RenderQueueSortingData::RenderQueueSortingData()
    : miSortingPass(4)
    , miSortingOffset(0)
    , mbTransparency(false) {
}

/////////////////////////////////////////////////////////////////////////

TextureContext::TextureContext(const Texture* ptex, float repU, float repV)
    : mpTexture(ptex)
    , mfRepeatU(repU)
    , mfRepeatV(repV) {
}

/////////////////////////////////////////////////////////////////////////

GfxMaterial::GfxMaterial()
    : mMaterialName("DefaultMaterial") {
  _rasterstate = std::make_shared<RasterState>();
  PushDebug(false);
}

GfxMaterial::~GfxMaterial() {
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial::PushDebug(bool bdbg) {
  mDebug.push(bdbg);
}
void GfxMaterial::PopDebug() {
  mDebug.pop();
}
bool GfxMaterial::IsDebug() {
  return mDebug.top();
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial::cloneStateFrom(const GfxMaterial& oth) {

  _rasterstate = oth._rasterstate->clone();
  miNumPasses = oth.miNumPasses;
  mMaterialName = oth.mMaterialName + ".clone";

  mTextureMap[ETEXDEST_AMBIENT]  = oth.mTextureMap[ETEXDEST_AMBIENT];
  mTextureMap[ETEXDEST_DIFFUSE]  = oth.mTextureMap[ETEXDEST_DIFFUSE];
  mTextureMap[ETEXDEST_SPECULAR] = oth.mTextureMap[ETEXDEST_SPECULAR];
  mTextureMap[ETEXDEST_BUMP]     = oth.mTextureMap[ETEXDEST_BUMP];

  mfFogStart                     = oth.mfFogStart;
  mfFogRange                     = oth.mfFogRange;

  mSortingData                   = oth.mSortingData;
  mDebug                        = oth.mDebug;
  _doinit                        = oth._doinit;
  mfParticleSize                 = oth.mfParticleSize;

  _varmap                        = oth._varmap;
  _bound_params                  = oth._bound_params;
  _bound_params_stamp            = oth._bound_params_stamp;
  _state_lambdas                 = oth._state_lambdas;

  _variant                      = oth._variant;
}

/////////////////////////////////////////////////////////////////////////

void GfxMaterial::SetTexture(ETextureDest edest, const TextureContext& tex) {
  mTextureMap[edest] = tex;
}

const TextureContext& GfxMaterial::GetTexture(ETextureDest edest) const {
  return mTextureMap[edest];
}

TextureContext& GfxMaterial::GetTexture(ETextureDest edest) {
  return mTextureMap[edest];
}

/////////////////////////////////////////////////////////////////////////
} // namespace lev2
} // namespace ork

/////////////////////////////////////////////////////////////////////////
