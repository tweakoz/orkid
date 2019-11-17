////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/shadman.h>

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

struct PBRMaterial : public GfxMaterial {

  PBRMaterial();
  ~PBRMaterial() final;

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);

  ////////////////////////////////////////////

  bool BeginPass(GfxTarget* targ, int iPass = 0) final;
  void EndPass(GfxTarget* targ) final;
  int BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) final;
  void EndBlock(GfxTarget* targ) final;
  void gpuInit(GfxTarget* targ, const AssetPath& assetname);
  void Init(GfxTarget* targ) final;
  void Update() final;

  ////////////////////////////////////////////

  FxShader* _shader = nullptr;
};

PBRMaterial::PBRMaterial() {
}
PBRMaterial::~PBRMaterial() {
}
void PBRMaterial::begin(const RenderContextFrameData& RCFD) {
}
void PBRMaterial::end(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

bool PBRMaterial::BeginPass(GfxTarget* targ, int iPass) {
  return false;
}
void PBRMaterial::EndPass(GfxTarget* targ) {
}
int PBRMaterial::BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) {
  return 0;
}
void PBRMaterial::EndBlock(GfxTarget* targ) {
}
void PBRMaterial::gpuInit(GfxTarget* targ, const AssetPath& assetname) {
}
void PBRMaterial::Init(GfxTarget* targ) {
}
void PBRMaterial::Update() {
}

} // namespace ork::lev2