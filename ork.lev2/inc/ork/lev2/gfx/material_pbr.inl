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
#include <ork/lev2/lev2_asset.h>
#include <boost/filesystem.hpp>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>

namespace ork::lev2 {
using namespace boost::filesystem;
using namespace std::literals;
///////////////////////////////////////////////////////////////////////////////

class PBRMaterial : public GfxMaterial {

  DeclareConcreteX(PBRMaterial,GfxMaterial);
public:

  PBRMaterial();
  ~PBRMaterial() final;

 void setTextureBaseName(std::string basename) { _textureBaseName = basename; }

  ////////////////////////////////////////////

  void begin(const RenderContextFrameData& RCFD);
  void end(const RenderContextFrameData& RCFD);

  ////////////////////////////////////////////

  bool BeginPass(GfxTarget* targ, int iPass = 0) final;
  void EndPass(GfxTarget* targ) final;
  int BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) final;
  void EndBlock(GfxTarget* targ) final;
  void Init(GfxTarget* targ) final;
  void Update() final;

  ////////////////////////////////////////////

  FxShader* _shader = nullptr;
  GfxTarget* _initialTarget = nullptr;
  const FxShaderParam* _paramMVP = nullptr;
  const FxShaderParam* _paramMV = nullptr;
  const FxShaderParam* _paramMROT = nullptr;
  const FxShaderParam* _paramMapColor = nullptr;
  const FxShaderParam* _paramMapNormal = nullptr;
  const FxShaderParam* _paramMapRoughAndMetal = nullptr;
  Texture* _texColor = nullptr;
  Texture* _texNormal = nullptr;
  Texture* _texRoughAndMetal = nullptr;
  std::string _textureBaseName;
  const FxShaderTechnique* _tekRigidGBUFFER = nullptr;
  const FxShaderTechnique* _tekRigidGBUFFER_N = nullptr;

  std::string _colorMapName;
  std::string _normalMapName;
  std::string _roughMapName;
  std::string _metalMapName;
  std::string _amboccMapName;
  std::string _emissiveMapName;

  bool _metalicRoughnessSingleTexture = false;
};

inline PBRMaterial::PBRMaterial() {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(EBLENDING_OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECULLTEST_OFF);
  miNumPasses = 1;

}
inline PBRMaterial::~PBRMaterial() {
}
inline void PBRMaterial::begin(const RenderContextFrameData& RCFD) {
}
inline void PBRMaterial::end(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

inline bool PBRMaterial::BeginPass(GfxTarget* targ, int iPass) {
  //printf( "_name<%s>\n", mMaterialName.c_str() );
  auto fxi       = targ->FXI();
  auto rsi       = targ->RSI();
  auto mtxi      = targ->MTXI();
  auto mvpmtx = mtxi->RefMVPMatrix();
  auto rotmtx = mtxi->RefR3Matrix();
  auto mvmtx = mtxi->RefMVMatrix();
  auto vmtx = mtxi->RefVMatrix();
  //vmtx.dump("vmtx");
  const RenderContextInstData* RCID  = targ->GetRenderContextInstData();
  const RenderContextFrameData* RCFD = targ->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  fxi->BindPass(_shader, 0);
  fxi->BindParamCTex(_shader,_paramMapColor,_texColor);
  fxi->BindParamCTex(_shader,_paramMapNormal,_texNormal);
  fxi->BindParamCTex(_shader,_paramMapRoughAndMetal,_texRoughAndMetal);
  fxi->BindParamMatrix(_shader,_paramMV,mvmtx);
  const auto& world = mtxi->RefMMatrix();
  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL = stereomtx->MVPL(world);
    auto MVPR = stereomtx->MVPR(world);
    fxi->BindParamMatrix(_shader, _paramMVP, MVPL);
    fxi->BindParamMatrix(_shader, _paramMVP, MVPR);
  } else {
    auto mcams = CPD._cameraMatrices;
    auto MVP = world
               * mcams->_vmatrix
               * mcams->_pmatrix;
    fxi->BindParamMatrix(_shader, _paramMVP, MVP);
    //fxi->BindParamMatrix(_shader,_paramMROT,(world*mcams->_vmatrix).rotMatrix33());
    fxi->BindParamMatrix(_shader,_paramMROT,(world).rotMatrix33());
  }
  rsi->BindRasterState(_rasterstate);
  fxi->CommitParams();
  return true;
}
inline void PBRMaterial::EndPass(GfxTarget* targ) {
  targ->FXI()->EndPass(_shader);
}
inline int PBRMaterial::BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) {
  auto fxi       = targ->FXI();
  if( _paramMapNormal )
    fxi->BindTechnique(_shader,_tekRigidGBUFFER_N);
  else
    fxi->BindTechnique(_shader,_tekRigidGBUFFER);

  int numpasses = fxi->BeginBlock(_shader,RCID);
  assert(numpasses==1);
  return numpasses;
}
inline void PBRMaterial::EndBlock(GfxTarget* targ) {
  auto fxi       = targ->FXI();
  fxi->EndBlock(_shader);
}
inline void PBRMaterial::Init(GfxTarget* targ) /*final*/ {
  assert(_initialTarget==nullptr);
  _initialTarget = targ;
  auto fxi       = targ->FXI();
  auto shass     = ork::asset::AssetManager<FxShaderAsset>::Load("orkshader://pbr");
  _shader        = shass->GetFxShader();

  _tekRigidGBUFFER = fxi->technique(_shader,"rigid_gbuffer");
  _tekRigidGBUFFER_N = fxi->technique(_shader,"rigid_gbuffer_n");

  _paramMVP = fxi->parameter(_shader,"mvp");
  _paramMV = fxi->parameter(_shader,"mv");
  _paramMROT = fxi->parameter(_shader,"mrot");
  _paramMapColor = fxi->parameter(_shader,"ColorMap");
  _paramMapNormal = fxi->parameter(_shader,"NormalMap");
  _paramMapRoughAndMetal = fxi->parameter(_shader,"RoughAndMetalMap");

  auto basepath = "data://materials/"s+_textureBaseName+"/"s;
  auto colorpath = basepath+_textureBaseName+"_baseColor"s;
  auto normalpath = basepath+_textureBaseName+"_normal"s;
  printf( "colorpath<%s>\n", colorpath.c_str() );
  //auto ass_color = ork::asset::AssetManager<TextureAsset>::Load(colorpath.c_str());
  //auto ass_normal = ork::asset::AssetManager<TextureAsset>::Load(normalpath.c_str());
  //_texColor = ass_color->GetTexture();
  //_texNormal = ass_normal->GetTexture();
  //printf( "ass_color<%p>\n", ass_color );
  printf( "tex_color<%p>\n", _texColor );
  printf( "tex_normal<%p>\n", _texNormal );
}
inline void PBRMaterial::Update() {
}

} // namespace ork::lev2