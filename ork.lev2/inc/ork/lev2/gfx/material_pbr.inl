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

class PBRMaterial;

struct PbrMatrixBlockApplicator : public MaterialInstApplicator
{
	MaterialInstItemMatrixBlock* _matrixblock = nullptr;
	const PBRMaterial* _pbrmaterial = nullptr;
	void ApplyToTarget( GfxTarget *pTARG ) final;
    static PbrMatrixBlockApplicator* getApplicator();
};

///////////////////////////////////////////////////////////////////////////////

class PBRMaterial : public GfxMaterial {

  DeclareConcreteX(PBRMaterial,GfxMaterial);
public:

  PBRMaterial();
  ~PBRMaterial() final;

 	void setTextureBaseName(std::string basename) { _textureBaseName = basename; }

	////////////////////////////////////////////

	static Texture* brdfIntegrationMap(GfxTarget* targ);

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
  void BindMaterialInstItem( MaterialInstItem* pitem ) const final;
  void UnBindMaterialInstItem( MaterialInstItem* pitem ) const final;

  ////////////////////////////////////////////

  FxShader* _shader = nullptr;
  GfxTarget* _initialTarget = nullptr;
  const FxShaderParam* _paramMVP = nullptr;
  const FxShaderParam* _paramMVPL = nullptr;
  const FxShaderParam* _paramMVPR = nullptr;
  const FxShaderParam* _paramMV = nullptr;
  const FxShaderParam* _paramMROT = nullptr;
  const FxShaderParam* _paramMapColor = nullptr;
  const FxShaderParam* _paramMapNormal = nullptr;
  const FxShaderParam* _paramMapRoughAndMetal = nullptr;
	const FxShaderParam* _parInvViewSize = nullptr;

  Texture* _texColor = nullptr;
  Texture* _texNormal = nullptr;
  Texture* _texRoughAndMetal = nullptr;
  std::string _textureBaseName;
  const FxShaderTechnique* _tekRigidGBUFFER = nullptr;
  const FxShaderTechnique* _tekRigidGBUFFER_N = nullptr;
  const FxShaderTechnique* _tekRigidGBUFFER_N_STEREO = nullptr;
	const FxShaderTechnique* _tekRigidGBUFFER_N_TEX_STEREO = nullptr;

  std::string _colorMapName;
  std::string _normalMapName;
  std::string _roughMapName;
  std::string _metalMapName;
  std::string _amboccMapName;
  std::string _emissiveMapName;

	bool _stereoVtex = false;
  bool _metalicRoughnessSingleTexture = false;
};


//////////////////////////////////////////////////////

inline PBRMaterial::PBRMaterial() {
  _rasterstate.SetShadeModel(ESHADEMODEL_SMOOTH);
  _rasterstate.SetAlphaTest(EALPHATEST_OFF);
  _rasterstate.SetBlending(EBLENDING_OFF);
  _rasterstate.SetDepthTest(EDEPTHTEST_LEQUALS);
  _rasterstate.SetZWriteMask(true);
  _rasterstate.SetCullTest(ECULLTEST_OFF);
  miNumPasses = 1;

}

////////////////////////////////////////////

inline PBRMaterial::~PBRMaterial() {
}

////////////////////////////////////////////

inline void PBRMaterial::begin(const RenderContextFrameData& RCFD) {
}

////////////////////////////////////////////

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

	auto brdfintegtex = PBRMaterial::brdfIntegrationMap(targ);

  const auto& world = mtxi->RefMMatrix();
	const auto& drect = CPD.GetDstRect();
	const auto& mrect = CPD.GetMrtRect();
	float w = mrect.miW;
	float h = mrect.miH;
	//printf( "w<%g> h<%g>\n", w, h );
	fxi->BindParamVect2( _shader, _parInvViewSize, fvec2(1.0 / w, 1.0f / h));

  if (CPD.isStereoOnePass() and CPD._stereoCameraMatrices) {
    auto stereomtx = CPD._stereoCameraMatrices;
    auto MVPL = stereomtx->MVPL(world);
    auto MVPR = stereomtx->MVPR(world);
    fxi->BindParamMatrix(_shader, _paramMVPL, MVPL);
    fxi->BindParamMatrix(_shader, _paramMVPR, MVPR);
    fxi->BindParamMatrix(_shader,_paramMROT,(world).rotMatrix33());
  } else {
    auto mcams = CPD._cameraMatrices;
    auto MVP = world
               * mcams->_vmatrix
               * mcams->_pmatrix;
    fxi->BindParamMatrix(_shader, _paramMVP, MVP);
    fxi->BindParamMatrix(_shader,_paramMROT,(world).rotMatrix33());
  }
  rsi->BindRasterState(_rasterstate);
  fxi->CommitParams();
  return true;
}

////////////////////////////////////////////

inline void PBRMaterial::EndPass(GfxTarget* targ) {
  targ->FXI()->EndPass(_shader);
}

////////////////////////////////////////////

inline int PBRMaterial::BeginBlock(GfxTarget* targ, const RenderContextInstData& RCID) {
  auto fxi       = targ->FXI();
  const RenderContextFrameData* RCFD = targ->topRenderContextFrameData();
  const auto& CPD = RCFD->topCPD();
  bool is_stereo = CPD.isStereoOnePass();

	const FxShaderTechnique* tek = _tekRigidGBUFFER;

	if( is_stereo ){
		if( _stereoVtex )
			tek = _tekRigidGBUFFER_N_TEX_STEREO;
		else
			tek = _tekRigidGBUFFER_N_STEREO;
	}
	else if( _paramMapNormal )
		tek = _tekRigidGBUFFER_N;

  fxi->BindTechnique(_shader,tek);

  int numpasses = fxi->BeginBlock(_shader,RCID);
  assert(numpasses==1);
  return numpasses;
}

////////////////////////////////////////////

inline void PBRMaterial::EndBlock(GfxTarget* targ) {
  auto fxi       = targ->FXI();
  fxi->EndBlock(_shader);
}

////////////////////////////////////////////

inline void PBRMaterial::Init(GfxTarget* targ) /*final*/ {
  assert(_initialTarget==nullptr);
  _initialTarget = targ;
  auto fxi       = targ->FXI();
  auto shass     = ork::asset::AssetManager<FxShaderAsset>::Load("orkshader://pbr");
  _shader        = shass->GetFxShader();

  _tekRigidGBUFFER = fxi->technique(_shader,"rigid_gbuffer");
  _tekRigidGBUFFER_N = fxi->technique(_shader,"rigid_gbuffer_n");
  _tekRigidGBUFFER_N_STEREO = fxi->technique(_shader,"rigid_gbuffer_n_stereo");
	_tekRigidGBUFFER_N_TEX_STEREO = fxi->technique(_shader,"rigid_gbuffer_n_tex_stereo");

  _paramMVP = fxi->parameter(_shader,"mvp");
  _paramMVPL = fxi->parameter(_shader,"mvp_l");
  _paramMVPR = fxi->parameter(_shader,"mvp_r");
  _paramMV = fxi->parameter(_shader,"mv");
  _paramMROT = fxi->parameter(_shader,"mrot");
  _paramMapColor = fxi->parameter(_shader,"ColorMap");
  _paramMapNormal = fxi->parameter(_shader,"NormalMap");
  _paramMapRoughAndMetal = fxi->parameter(_shader,"RoughAndMetalMap");
	_parInvViewSize = fxi->parameter(_shader,"InvViewportSize");

}
inline void PBRMaterial::Update() {
}

///////////////////////////////////////////////////////////////////////////////

inline void PBRMaterial::BindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////
  MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

  if (mtxblockitem) {
    //if (hBoneMatrices->GetPlatformHandle()) {
      auto applicator = PbrMatrixBlockApplicator::getApplicator();
      OrkAssert(applicator != 0);
      applicator->_pbrmaterial = this;
      applicator->_matrixblock = mtxblockitem;
      mtxblockitem->SetApplicator(applicator);
    //}
    return;
  }

  ///////////////////////////////////

  /*MaterialInstItemMatrix* mtxitem = rtti::autocast(pitem);

  if (mtxitem) {
    WiiMatrixApplicator* pyo = MtxApplicators.allocate();
    OrkAssert(pyo != 0);
    new (pyo) WiiMatrixApplicator(mtxitem, this);
    mtxitem->SetApplicator(pyo);
    return;
  }*/

  ///////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

inline void PBRMaterial::UnBindMaterialInstItem(MaterialInstItem* pitem) const {
  ///////////////////////////////////

  MaterialInstItemMatrixBlock* mtxblockitem = rtti::autocast(pitem);

  if (mtxblockitem) {
    //if (hBoneMatrices->GetPlatformHandle()) {
      auto applicator = static_cast<PbrMatrixBlockApplicator*>(mtxblockitem->mApplicator);
      if (applicator) {
        applicator->_pbrmaterial = nullptr;
        applicator->_matrixblock = nullptr;
      }
    //}
    return;
  }

  ///////////////////////////////////

  /*
  MaterialInstItemMatrix* mtxitem = rtti::autocast(pitem);

  if (mtxitem) {
    WiiMatrixApplicator* wiimtxapp = rtti::autocast(mtxitem->mApplicator);
    if (wiimtxapp) {
      MtxApplicators.deallocate(wiimtxapp);
    }
    return;
  }*/

  ///////////////////////////////////
}


///////////////////////////////////////////////////////////////////////////////

inline void PbrMatrixBlockApplicator::ApplyToTarget(GfxTarget* pTARG) // virtual
{
  auto fxi  = pTARG->FXI();
  auto mtxi = pTARG->MTXI();

  size_t inumbones      = _matrixblock->GetNumMatrices();
  const fmtx4* Matrices = _matrixblock->GetMatrices();
  FxShader* hshader     = _pbrmaterial->_shader;

  //fxi->BindParamMatrix(hshader, mMaterial->hMatMV, mtxi->RefMVMatrix());
  //fxi->BindParamMatrix(hshader, mMaterial->hWVPMatrix, mtxi->RefMVPMatrix());
  //fxi->BindParamMatrix(hshader, mMaterial->hWMatrix, mtxi->RefMMatrix());

  //fmtx4 iwmat;
  //iwmat.inverseOf(mtxi->RefMVMatrix());
  //fxi->BindParamMatrix(hshader, mMaterial->hIWMatrix, iwmat);
  //fxi->BindParamMatrixArray(hshader, mMaterial->hBoneMatrices, Matrices, (int)inumbones);
  //fxi->CommitParams();
}

} // namespace ork::lev2
