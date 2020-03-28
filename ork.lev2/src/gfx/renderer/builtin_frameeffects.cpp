////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/prop.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <ork/lev2/gfx/renderer/NodeCompositor/NodeCompositorFx3.h>

BEGIN_ENUM_SERIALIZER(ork::lev2, EFrameEffect)
DECLARE_ENUM(EFRAMEFX_NONE)
DECLARE_ENUM(EFRAMEFX_STANDARD)
DECLARE_ENUM(EFRAMEFX_COMIC)
DECLARE_ENUM(EFRAMEFX_GLOW)
DECLARE_ENUM(EFRAMEFX_GHOSTLY)
DECLARE_ENUM(EFRAMEFX_AFTERLIFE)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

TexBuffer::TexBuffer(OffscreenBuffer* parent, EBufferFormat efmt, int iW, int iH)
    : OffscreenBuffer(parent, 0, 0, iW, iH, efmt) {
  lev2::ContextCreationParams params = lev2::GfxEnv::GetRef().GetCreationParams();
  params.miNumSharedVerts            = 4 << 10;
  lev2::GfxEnv::GetRef().PushCreationParams(params);
  this->initContext();
  lev2::GfxEnv::GetRef().PopCreationParams();
}

///////////////////////////////////////////////////////////////////////////////

CompositingPassData BuiltinFrameTechniques::createPassData(const lev2::CompositingGroup* group) {
  lev2::CompositingPassData node;
  node.mbDrawSource       = (group != nullptr);
  this->mfSourceAmplitude = group ? 1.0f : 0.0f; // yuk....
  node.mpGroup            = group;
  node.mpFrameTek         = this;
  node.mpCameraName       = (group != nullptr) ? &group->_cameraName : nullptr;
  node.mpLayerName        = (group != nullptr) ? &group->_layers : nullptr;
  return node;
}

///////////////////////////////////////////////////////////////////////////

static const int kGLOWBUFSIZE = 256;

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameTechniques::SetEffect(const char* FxName, float famount, float fbamt) {
  if (famount < 0.0f)
    famount = 0.0f;
  if (famount > 1.0f)
    famount = 1.0f;
  if (fbamt < 0.0f)
    fbamt = 0.0f;
  if (fbamt > 1.0f)
    fbamt = 1.0f;

  mEffectName      = FxName;
  mfAmount         = famount;
  mfFeedbackAmount = fbamt;
}

///////////////////////////////////////////////////////////////////////////////
static const int kFXW      = 512;
static const int kFXH      = 512;
static const int kFINALHDW = 1024;
static const int kFINALHDH = 1024;

void WriteRtgTex(const char* name, ork::lev2::Context* pT, RtGroup* pgrp, int imrt) {
  Texture* ptex = pgrp->GetMrt(imrt)->texture();
  pT->TXI()->SaveTexture(name, ptex);
}

RtGroup* BuiltinFrameTechniques::GetNextWriteRtGroup() const {
  miRtGroupIndex++;
  int idxm      = (miRtGroupIndex % knumpingpongbufs);
  RtGroup* rval = mpHDRRtGroup[idxm];
  return rval;
}

void BuiltinFrameTechniques::ResizeFinalBuffer(int iw, int ih) {
  if ((miFinalW != iw) || (miFinalH != ih)) {
    miFinalW = iw;
    miFinalH = ih;

    mpMrtFinalHD->Resize(miFinalW, miFinalH);
    mpMrtFinal->Resize(miFinalW, miFinalH);
  }
}
void BuiltinFrameTechniques::ResizeFxBuffer(int iw, int ih) {
  if ((miFxW != iw) || (miFxH != ih)) {
    miFxW = iw;
    miFxH = ih;

    for (int i = 0; i < knumpingpongbufs; i++) {
      mpHDRRtGroup[i]->Resize(iw, ih);
    }
  }
}
void BuiltinFrameTechniques::DoInit(Context* pTARG) {
  // ork::lev2::Context* pTARG = Parent->context();
  auto fbi                = pTARG->FBI();
  OffscreenBuffer* Parent = fbi->GetThisBuffer();
  pTARG                   = Parent ? Parent->context() : pTARG;
  auto clear_color        = Parent ? Parent->GetClearColor() : fbi->GetClearColor();

  static const int kmultisamplesH = 2;
  static const int kmultisamplesL = 1;

  miFxW    = kFXW;
  miFxH    = kFXH;
  miW      = kFXW;
  miH      = kFXH;
  miFinalW = kFINALHDW;
  miFinalH = kFINALHDH;

  for (int i = 0; i < knumpingpongbufs; i++) {
    auto grp = new RtGroup(pTARG, miW, miH, kmultisamplesH);
    auto buf = new RtBuffer(lev2::ERTGSLOT0, lev2::EBufferFormat::RGBA16F, miW, miH);

    buf->_debugName = FormatString("BFTEK::PingPongBuf%d", i);
    mpHDRRtGroup[i] = grp;

    mpHDRRtGroup[i]->SetMrt(0, buf);

    // mpHDRRtGroup[i]->GetMrt(0)->RefClearColor() = clear_color;
    // mpHDRRtGroup[i]->GetMrt(0)->SetContext(pTARG);
  }

  mpMrtAux0 = new RtGroup(pTARG, kGLOWBUFSIZE, kGLOWBUFSIZE, 1);
  mpMrtAux1 = new RtGroup(pTARG, kGLOWBUFSIZE, kGLOWBUFSIZE, 1);

  auto bufa = new RtBuffer(lev2::ERTGSLOT0, lev2::EBufferFormat::RGBA16F, kGLOWBUFSIZE, kGLOWBUFSIZE);

  auto bufb = new RtBuffer(lev2::ERTGSLOT0, lev2::EBufferFormat::RGBA16F, kGLOWBUFSIZE, kGLOWBUFSIZE);

  bufa->_debugName = FormatString("BFTEK::Aux0");
  bufb->_debugName = FormatString("BFTEK::Aux1");
  mpMrtAux0->SetMrt(0, bufa);
  mpMrtAux1->SetMrt(0, bufb);

  // mpMrtAux0->GetMrt(0)->RefClearColor() = clear_color;
  // mpMrtAux1->GetMrt(0)->RefClearColor() = clear_color;

  ////////////////////////////////////////////////////////////////

  // mpMrtAux0->GetMrt(0)->SetContext(pTARG);
  // mpMrtAux1->GetMrt(0)->SetContext(pTARG);
  mpAuxBuffer0 = new TexBuffer(Parent, lev2::EBufferFormat::RGBA16F, kGLOWBUFSIZE, kGLOWBUFSIZE);
  mpAuxBuffer1 = new TexBuffer(Parent, lev2::EBufferFormat::RGBA16F, kGLOWBUFSIZE, kGLOWBUFSIZE);

  ////////////////////////////////////////////////////////////////

  mpMrtFinalHD = new RtGroup(pTARG, kFINALHDW, kFINALHDH, kmultisamplesH);

  auto buff = new RtBuffer(lev2::ERTGSLOT0, lev2::EBufferFormat::RGBA16F, kFINALHDW, kFINALHDH);

  mpMrtFinalHD->SetMrt(0, buff);

  buff->_debugName = FormatString("BFTEK::Final");

  mOutputRt = mpMrtFinalHD;

  ////////////////////////////////////////////////////////////////
  // Aux Buffers (External Buffer)

  // mpAuxBuffer0->mClearColor = fvec4::Blue();

  mFrameEffectBlurX.PostInit(pTARG, "orkshader://framefx", "frameeffect_glow_blurx");
  mFrameEffectBlurY.PostInit(pTARG, "orkshader://framefx", "frameeffect_glow_blury");
  mFrameEffectStandard.PostInit(pTARG, "orkshader://framefx", "frameeffect_standard");
  mFBoutMaterial.PostInit(pTARG, "orkshader://framefx", "frameeffect_fbout");
  mFrameEffectRadialBlur.PostInit(pTARG, "orkshader://framefx", "frameeffect_radialblur");
  mFrameEffectComic.PostInit(pTARG, "orkshader://framefx", "frameeffect_comic");
  mFrameEffectGlowJoin.PostInit(pTARG, "orkshader://framefx", "frameeffect_glow_join");
  mFrameEffectGhostJoin.PostInit(pTARG, "orkshader://framefx", "frameeffect_ghost_join");
  mFrameEffectDofJoin.PostInit(pTARG, "orkshader://framefx", "frameeffect_dof_join");
  mFrameEffectAfterLifeJoin.PostInit(pTARG, "orkshader://framefx", "frameeffect_afterlife_join");

  mFrameEffectDbgDepth.PostInit(pTARG, "orkshader://framefx", "frameeffect_debugdepth");
  mFrameEffectDbgNormals.PostInit(pTARG, "orkshader://framefx", "frameeffect_debugnormals");

  mUtilMaterial.SetUserFx("orkshader://solid", "feedbackatten");
  mUtilMaterial.Init(pTARG);

  mFBinMaterial.SetUserFx("orkshader://solid", "distortedfeedback");
  mFBinMaterial.Init(pTARG);

  mpRadialMap = ork::asset::AssetManager<ork::lev2::TextureAsset>::Load("data://effect_textures/radialgrad")->GetTexture();
}

///////////////////////////////////////////////////////////////////////////////

BuiltinFrameTechniques::~BuiltinFrameTechniques() {
  for (int i = 0; i < knumpingpongbufs; i++) {
    if (mpHDRRtGroup[i])
      delete mpHDRRtGroup[i];
  }

  if (mpMrtFinalHD)
    delete mpMrtFinalHD;
  if (mpMrtAux0)
    delete mpMrtAux0;
  if (mpMrtAux1)
    delete mpMrtAux1;
}

///////////////////////////////////////////////////////////////////////////////

BuiltinFrameTechniques::BuiltinFrameTechniques(int iW, int iH)
    : FrameTechniqueBase(iW, iH)
    , mpMrtFinalHD(0)
    , mpMrtAux1(0)
    , mpMrtAux0(0)
    , mpReadRtGroup(0)
    , mfFeedbackAmount(0.0f)
    , mfAmount(0.0f)
    , mpRadialMap(0)
    , mOutputRt(0)
    , mfSourceAmplitude(1.0f)
    , mpFbUvMap(0)
    , miRtGroupIndex(0)
    , mbPostFxFb(false)
    , miFxW(0)
    , miFxH(0)
    , miFinalW(0)
    , miFinalH(0) {

  mEffectName = "none";
  miWidth     = iW;
  miHeight    = iH;

  for (int i = 0; i < knumpingpongbufs; i++) {
    mpHDRRtGroup[i] = 0;
  }
}

///////////////////////////////////////////////////////////////////////////////

BuiltinFrameEffectMaterial::BuiltinFrameEffectMaterial()
    : mFxFile("")
    , mTekName("")
    , mpNoiseMap(0)
    , mpAuxMap0(0)
    , mpAuxMap1(0)
    , hFX(0)
    , hMVP(0)
    , hModColor(0)
    , hMrtMap0(0)
    , hMrtMap1(0)
    , hMrtMap2(0)
    , hMrtMap3(0)
    , hAuxMap0(0)
    , hAuxMap1(0)
    , hNoiseMap(0)
    , hTime(0)
    , hBlurFactor(0)
    , hBlurFactorI(0)
    , hViewportDim(0)
    , hEffectAmount(0)
    , mfEffectAmount(0.0f)
    , mpCurrentRtGroup(0)
    , mpPreviousRtGroup(0) {
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameEffectMaterial::Init(Context* pTarg) {
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameEffectMaterial::PostInit(Context* pTarg, const char* FxFile, const char* TekName) {
  mFxFile  = FxFile;
  mTekName = TekName;
  // _rtgroup = grp;

  if (FxShaderAsset* asset = asset::AssetManager<FxShaderAsset>::Load(mFxFile.c_str())) {
    // orkprintf( "Asset<%s> %p mTekName<%s>\n", FxFile, asset, mTekName.c_str() );

    hFX = asset->GetFxShader();
    OrkAssert(hFX != 0);

    hTek = pTarg->FXI()->technique(hFX, mTekName.c_str());

    OrkAssert(hTek != 0);
    hMVP          = pTarg->FXI()->parameter(hFX, "mvp");
    hModColor     = pTarg->FXI()->parameter(hFX, "modcolor");
    hMrtMap0      = pTarg->FXI()->parameter(hFX, "MrtMap0");
    hMrtMap1      = pTarg->FXI()->parameter(hFX, "MrtMap1");
    hMrtMap2      = pTarg->FXI()->parameter(hFX, "MrtMap2");
    hMrtMap3      = pTarg->FXI()->parameter(hFX, "MrtMap3");
    hAuxMap0      = pTarg->FXI()->parameter(hFX, "AuxMap0");
    hAuxMap1      = pTarg->FXI()->parameter(hFX, "AuxMap1");
    hNoiseMap     = pTarg->FXI()->parameter(hFX, "NoiseMap");
    hTime         = pTarg->FXI()->parameter(hFX, "time");
    hBlurFactor   = pTarg->FXI()->parameter(hFX, "BlurFactor");
    hBlurFactorI  = pTarg->FXI()->parameter(hFX, "BlurFactorI");
    hViewportDim  = pTarg->FXI()->parameter(hFX, "viewportdim");
    hEffectAmount = pTarg->FXI()->parameter(hFX, "EffectAmount");

    mpNoiseMap = ork::asset::AssetManager<ork::lev2::TextureAsset>::Load("data://effect_textures/colornoise")->GetTexture();
  }
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameEffectMaterial::SetAuxMaps(Texture* AuxMap0, Texture* AuxMap1) {
  mpAuxMap0 = AuxMap0;
  mpAuxMap1 = AuxMap1;
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameEffectMaterial::Update(void) {
}

///////////////////////////////////////////////////////////////////////////////

bool BuiltinFrameEffectMaterial::BeginPass(Context* pTarg, int iPass) {
  auto rsi = pTarg->RSI();
  auto fxi = pTarg->FXI();
  auto fbi = pTarg->FBI();
  auto mxi = pTarg->MTXI();

  _rasterstate.SetCullTest(ECULLTEST_OFF);
  ///////////////////////////////
  rsi->BindRasterState(_rasterstate);
  ///////////////////////////////

  F32 fVPW = (F32)fbi->GetVPW();
  F32 fVPH = (F32)fbi->GetVPH();

  fmtx4 MatP = mxi->Ortho(0.0f, fVPW, 0.0f, fVPH, 0.0f, 1.0f);

  ///////////////////////////////

  fvec2 Dims;
  Dims.SetX(float(pTarg->mainSurfaceWidth()));
  Dims.SetY(float(pTarg->mainSurfaceHeight()));

  ///////////////////////////////

  fxi->BindPass(hFX, iPass);
  fxi->BindParamMatrix(hFX, hMVP, MatP);
  fxi->BindParamVect4(hFX, hModColor, pTarg->RefModColor());
  fxi->BindParamFloat(hFX, hTime, (float)OldSchool::GetRef().GetLoResRelTime());
  fxi->BindParamVect2(hFX, hViewportDim, fvec2(Dims.GetX(), Dims.GetY()));

  Texture* ptex0 = 0;
  Texture* ptex1 = 0;
  Texture* ptex3 = 0;

  if (mpCurrentRtGroup) {
    int inumtargs = mpCurrentRtGroup->GetNumTargets();
    ptex0         = (inumtargs > 0) ? mpCurrentRtGroup->GetMrt(0)->texture() : 0;
    ptex1         = (inumtargs > 1) ? mpCurrentRtGroup->GetMrt(1)->texture() : 0;
    ptex3         = (inumtargs > 3) ? mpCurrentRtGroup->GetMrt(2)->texture() : 0;
  }
  fxi->BindParamCTex(hFX, hMrtMap0, ptex0);
  fxi->BindParamCTex(hFX, hMrtMap1, ptex1);
  fxi->BindParamCTex(hFX, hMrtMap3, ptex3);

  if (mpPreviousRtGroup)
    fxi->BindParamCTex(hFX, hMrtMap2, mpPreviousRtGroup->GetMrt(0)->texture());
  else
    fxi->BindParamCTex(hFX, hMrtMap2, 0);

  fxi->BindParamCTex(hFX, hNoiseMap, mpNoiseMap);
  fxi->BindParamCTex(hFX, hAuxMap0, mpAuxMap0);
  fxi->BindParamCTex(hFX, hAuxMap1, mpAuxMap1);

  float BlurFactor(16.0f);
  fxi->BindParamFloat(hFX, hBlurFactor, BlurFactor);
  fxi->BindParamInt(hFX, hBlurFactorI, int(BlurFactor));

  fxi->BindParamFloat(hFX, hEffectAmount, mfEffectAmount);

  fxi->CommitParams();

  return true;
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameEffectMaterial::EndPass(Context* pTarg) {
  pTarg->FXI()->EndPass(hFX);
}

///////////////////////////////////////////////////////////////////////////////

int BuiltinFrameEffectMaterial::BeginBlock(Context* pTarg, const RenderContextInstData& MatCtx) {
  pTarg->FXI()->BindTechnique(hFX, hTek);
  int inumpasses = pTarg->FXI()->BeginBlock(hFX, MatCtx);
  return inumpasses;
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameEffectMaterial::EndBlock(Context* pTarg) {
  pTarg->FXI()->EndBlock(hFX);
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameTechniques::Render(FrameRenderer& frenderer) {
  const ork::lev2::ContextCreationParams& CreationParams = ork::lev2::GfxEnv::GetRef().GetCreationParams();

  RenderContextFrameData& RCFD = frenderer.framedata();
  auto CIMPL                   = RCFD._cimpl;
  assert(CIMPL != nullptr);
  const auto& topCPD = CIMPL->topCPD();

  Context* pTARG = RCFD.GetTarget();

  if (false == pTARG->IsDeviceAvailable())
    return;

  /////////////////////////////////////////////////

  bool bfeedback = true; //(CreationParams.miQuality>1);

  /////////////////////////////////////////////////
  auto dst_rect = topCPD.GetDstRect();
  auto mrt_rect = topCPD.GetMrtRect();
  /////////////////////////////////////////////////

  std::string dumpfname;
  static int gdumpidx = 0;

  gdumpidx++;

  // TODO cleanup with CIMPL
  if (mEffectName == "none") {
    /*CompositingPassData CPD;
    pTARG->FBI()->SetAutoClear(true);
    pTARG->SetRenderContextFrameData( & RCFD );
    CPD.SetDstRect( dst_rect );
    RtGroupRenderTarget rt(mpMrtFinalHD);
    CPD._irendertarget = &rt;
    CIMPL->pushCPD(CPD);
    pTARG->FBI()->PushRtGroup( mpMrtFinalHD );
    pTARG->BeginFrame();
        frenderer.renderMisc();
    pTARG->EndFrame();
    pTARG->FBI()->PopRtGroup();
    CIMPL->popCPD(CPD);
    pTARG->SetRenderContextFrameData( 0 );

    mOutputRt = mpMrtFinalHD;*/
  } else {
    /////////////////////////////////////////////////
    // Render Scene into Mrt
    /////////////////////////////////////////////////

    if (1) {
      /*
      RtGroup* rtgroupFBW = GetNextWriteRtGroup();
      pTARG->FBI()->SetAutoClear(true);
      FrameData.SetDstRect( mrt_rect );
      pTARG->FBI()->PushRtGroup( rtgroupFBW );
      pTARG->SetRenderContextFrameData( & RCFD );
      RtGroupRenderTarget rt(rtgroupFBW);
      FrameData.PushRenderTarget(&rt);
      pTARG->BeginFrame();
      {
          //////////////////////////////////////////
          // render scene into Mrt0
          //////////////////////////////////////////

          frenderer.renderMisc();

          //////////////////////////////////////////
          // render feedback->Mrt0
          //////////////////////////////////////////

          if( bfeedback )
          {
              float fatten = mfFeedbackAmount*mfSourceAmplitude;
              float ffback = mfFeedbackAmount;
              int iW = rtgroupFBW->GetW();
              int iH = rtgroupFBW->GetH();

              SRect vprect(0,0,iW,iH);
              SRect quadrect(0,iH,iW,0);

              ////////////////////////////////////////////////////
              // attenuate direct source
              ////////////////////////////////////////////////////

              fvec4 clr1(0.0f,0.0f,0.0f,fatten);
              mUtilMaterial.SetTexture( mpRadialMap );
              mUtilMaterial.SetColorMode( GfxMaterial3DSolid::EMODE_USER );
              mUtilMaterial._rasterstate.SetBlending( EBLENDING_ALPHA );
              pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad( vprect, quadrect, & mUtilMaterial, 0.0f, 0.0f, 1.0f, 1.0f, 0, clr1
      );

              ////////////////////////////////////////////////////
              // add feedback
              ////////////////////////////////////////////////////
              RtGroup* rtgroupFBR = GetReadRtGroup();
              if( rtgroupFBR )
              {
                  Texture* ptex = rtgroupFBR->GetMrt(0)->texture();
                  if( ptex )
                  {
                      static const float kMAXW = 1;// - 1.0f/rtgroupFBW->GetW();
                      static const float kMAXH = 1; // - 1.0f/10.0f;a
                      fmtx4 mtx;
                      //mtx.SetScale(2.0f,2.0f,2.0f);
                      fvec4 fboutclr(ffback,ffback,ffback,1.0f);
                      pTARG->PushModColor( fboutclr );
                      mFBinMaterial.SetAuxMatrix( mtx );
                      mFBinMaterial.SetTexture( ptex );
                      //mFBinMaterial.SetTexture2( mpRadialMap );
                      mFBinMaterial.SetTexture2( mpFbUvMap );
                      mFBinMaterial.SetColorMode( GfxMaterial3DSolid::EMODE_USER );
                      mFBinMaterial._rasterstate.SetBlending( EBLENDING_ADDITIVE );
                      auto thisbuf = pTARG->FBI()->GetThisBuffer();
                      thisbuf->RenderMatOrthoQuad( vprect,
                                                   quadrect,
                                                   & mFBinMaterial,
                                                   0.0f, 0.0f,
                                                   kMAXW, kMAXH,
                                                   0, fboutclr );
                      pTARG->PopModColor();
                  }
              }
          }

          //////////////////////////////////////////

      }
      pTARG->EndFrame();

      pTARG->FBI()->PopRtGroup();
      SetReadRtGroup( rtgroupFBW );
      //dumpfname = CreateFormattedString( "dump_fr_%d_A.tga", gdumpidx );
      //WriteRtgTex( dumpfname.c_str(), pTARG, group, 0 );

      FrameData.SetDstRect( dst_rect );
      /////////////////////////////////////////////////
      PreProcess( FrameData );	// Blured Mrt0 -> AuxBuffer1
      FrameData.PopRenderTarget();
      */
    }
    if (1) { /*
                /////////////////////////////////////////////////
                // postprocess into the Final Rt Group
                /////////////////////////////////////////////////

                //dumpfname = CreateFormattedString( "dump_fr_%d_PRE.tga", gdumpidx );
                //WriteRtgTex( dumpfname.c_str(), pTARG, GetCurrentRtGroup(), 0 );

                PostProcess( FrameData );	// Join Buffers -> mpMrtFinal

                if( mbPostFxFb ) // post fx feedback ?
                    SetReadRtGroup( mpMrtFinal );

                //auto dumpfname = CreateFormattedString( "dump_fr_%d_POST.tga", gdumpidx );
                //WriteRtgTex( dumpfname.c_str(), pTARG, GetReadRtGroup(), 0 );

                /////////////////////////////////////////////////
                // Feedback (Current::MRT0->Previous::Mrt0)
                /////////////////////////////////////////////////
                if( bfeedback )
                {
                    RtGroup* rtgroupFBOUT = GetNextWriteRtGroup();
                    RtGroup* rtgroupFBINP = GetReadRtGroup();

                    int igW = rtgroupFBOUT->GetW();
                    int igH = rtgroupFBOUT->GetH();
                    //int igW = rtgroupFBOUT->GetW()-1;
                    //int igH = rtgroupFBOUT->GetH()-1;
                    static const float kMAXW = 1; //igW;// - 1.0f/float(igW);
                    static const float kMAXH = 1; //igH;// - 1.0f/float(igH);


                    mFBoutMaterial.BindRtGroups( rtgroupFBINP, 0 );
                    mFBoutMaterial._rasterstate.SetBlending( EBLENDING_OFF );
                    pTARG->FBI()->PushRtGroup( rtgroupFBOUT );
                    pTARG->GBI()->BeginFrame( );
                    {
                        SRect vprect(0,0,igW,igH);
                        SRect quadrect(0,igH,igW,0);
                        fvec4 clr( 1.0f, 1.0f, 1.0f, 1.0f );
                        auto thisbuf = pTARG->FBI()->GetThisBuffer();
                        thisbuf->RenderMatOrthoQuad( vprect,
                                                     quadrect,
                                                     & mFBoutMaterial,
                                                     0.0f, 0.0f,
                                                     kMAXW, kMAXH,
                                                     0,
                                                     clr );
                    }

                    pTARG->GBI()->EndFrame( );

                    //dumpfname = CreateFormattedString( "dump_fr_%d_POSTFB.tga", gdumpidx );
                    //WriteRtgTex( dumpfname.c_str(), pTARG, GetPreviousRtGroup(), 0 );

                    pTARG->FBI()->PopRtGroup();

                    SetReadRtGroup(rtgroupFBOUT);
                }
                */
    }

    /////////////////////////////////////////////////
  }
}

///////////////////////////////////////////////////////////////////////////////

void GlowRenderMatOrthoQuad(
    Context* pTARG,
    MatrixStackInterface* MTXIO,
    const SRect& vprect,
    const SRect& QuadRect,
    GfxMaterial* pmat,
    float fu0          = 0.0f,
    float fv0          = 0.0f,
    float fu1          = 1.0f,
    float fv1          = 1.0f,
    float* uv2         = 0,
    const fcolor4& clr = fcolor4::White()) {
  static SRasterState DefaultRasterState;
  ViewportRect vprectNew(vprect.miX, vprect.miY, vprect.miX2 - vprect.miX, vprect.miY2 - vprect.miY);

  // align source pixels to target pixels if sizes match
  float fx0 = float(QuadRect.miX);
  float fy0 = float(QuadRect.miY);
  float fx1 = float(QuadRect.miX2);
  float fy1 = float(QuadRect.miY2);

  float zeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  if (NULL == uv2)
    uv2 = zeros;

  MTXIO    = pTARG->MTXI();
  auto fbi = pTARG->FBI();
  auto fxi = pTARG->FXI();
  auto gbi = pTARG->GBI();

  MTXIO->PushPMatrix(MTXIO->Ortho(fx0, fx1, fy0, fy1, 0.0f, 1.0f));
  MTXIO->PushVMatrix(fmtx4::Identity);
  MTXIO->PushMMatrix(fmtx4::Identity);
  pTARG->RSI()->BindRasterState(DefaultRasterState, true);
  fbi->pushViewport(vprectNew);
  fbi->pushScissor(vprectNew);
  { // Draw Full Screen Quad with specified material
    pTARG->BindMaterial(pmat);
    fxi->InvalidateStateBlock();
    pTARG->PushModColor(clr);
    {
      ork::lev2::DynamicVertexBuffer<ork::lev2::SVtxV12C4T16>& vb = lev2::GfxEnv::GetSharedDynamicVB();

      U32 uc = 0xffffffff;
      ork::lev2::VtxWriter<ork::lev2::SVtxV12C4T16> vw;
      vw.Lock(pTARG, &vb, 6);
      vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f, fu0, fv0, uv2[0], uv2[1], uc));
      vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy1, 0.0f, fu1, fv1, uv2[4], uv2[5], uc));
      vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy0, 0.0f, fu1, fv0, uv2[2], uv2[3], uc));

      vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy0, 0.0f, fu0, fv0, uv2[0], uv2[1], uc));
      vw.AddVertex(ork::lev2::SVtxV12C4T16(fx0, fy1, 0.0f, fu0, fv1, uv2[6], uv2[7], uc));
      vw.AddVertex(ork::lev2::SVtxV12C4T16(fx1, fy1, 0.0f, fu1, fv1, uv2[4], uv2[5], uc));
      vw.UnLock(pTARG);

      gbi->DrawPrimitive(vw, ork::lev2::EPRIM_TRIANGLES);
    }
    pTARG->PopModColor();
  }
  fbi->popScissor();
  fbi->popViewport();
  MTXIO->PopPMatrix();
  MTXIO->PopVMatrix();
  MTXIO->PopMMatrix();
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameTechniques::PreProcess(RenderContextFrameData& FrameData) {
  Context* pTARG = FrameData.GetTarget();

  if ((mEffectName == "glow") || (mEffectName == "ghostly") || (mEffectName == "dof") || (mEffectName == "afterlife")) {
    auto MTXI0 = mpAuxBuffer0->context()->MTXI();
    auto MTXI1 = mpAuxBuffer1->context()->MTXI();

    ////////////////////////////////////////
    // Blur in X Direction
    // From: Mrt0
    // To: Aux0
    ////////////////////////////////////////

    mFrameEffectBlurX.BindRtGroups(GetReadRtGroup(), 0);
    mFrameEffectBlurX.SetAuxMaps(0, 0);
    //
    pTARG->FBI()->PushRtGroup(mpMrtAux0);
    pTARG->GBI()->BeginFrame();
    pTARG->FXI()->BeginFrame();
    GlowRenderMatOrthoQuad(
        pTARG, MTXI0, SRect(0, 0, kGLOWBUFSIZE, kGLOWBUFSIZE), SRect(kGLOWBUFSIZE, 0, 0, kGLOWBUFSIZE), &mFrameEffectBlurX);
    pTARG->GBI()->EndFrame();
    pTARG->FBI()->PopRtGroup();
    //
    lev2::Texture* pAux0Tex = mpMrtAux0->GetMrt(0)->texture();

    ////////////////////////////////////////
    // Blur in Y Direction
    // From: Aux0
    // To: Aux1
    ////////////////////////////////////////

    mFrameEffectBlurY.BindRtGroups(0, 0);
    mFrameEffectBlurY.SetAuxMaps(pAux0Tex, 0);
    //
    pTARG->FBI()->PushRtGroup(mpMrtAux1);
    pTARG->GBI()->BeginFrame();
    pTARG->FXI()->BeginFrame();
    GlowRenderMatOrthoQuad(
        pTARG, MTXI1, SRect(0, 0, kGLOWBUFSIZE, kGLOWBUFSIZE), SRect(kGLOWBUFSIZE, 0, 0, kGLOWBUFSIZE), &mFrameEffectBlurY);
    pTARG->GBI()->EndFrame();
    pTARG->FBI()->PopRtGroup();
  }
}

///////////////////////////////////////////////////////////////////////////////
// PostProcessing into Final output buffer
///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameTechniques::PostProcess(RenderContextFrameData& FrameData) {
  Context* pTARG = FrameData.GetTarget();

  pTARG->FBI()->PushRtGroup(mpMrtFinal);
  pTARG->GBI()->BeginFrame();
  pTARG->FXI()->BeginFrame();
  {
    int itx0 = 0;
    int itx1 = mpMrtFinal->GetW();
    int ity0 = 0;
    int ity1 = mpMrtFinal->GetH();
    ;

    SRect rect_vp(itx0, ity0, itx1, ity1);
    SRect rect_quad(itx0, ity1, itx1, ity0);
    RtGroup* cur = GetReadRtGroup();
    ////////////////////////////////////////
    // Post Processing
    ////////////////////////////////////////
    if (mEffectName == "standard") {
      mFrameEffectStandard.BindRtGroups(cur, 0);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectStandard);
    } else if (mEffectName == "comic") {
      mFrameEffectComic.BindRtGroups(cur, 0);
      mFrameEffectComic.SetEffectAmount(mfAmount);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectComic);
    } else if (mEffectName == "radialblur") {
      mFrameEffectRadialBlur.BindRtGroups(cur, 0);
      mFrameEffectRadialBlur.SetEffectAmount(mfAmount);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectRadialBlur);
    } else if (mEffectName == "glow") {
      mFrameEffectGlowJoin.BindRtGroups(cur, 0);
      mFrameEffectGlowJoin.SetAuxMaps(0, 0);
      auto tex_aux0 = mpMrtAux0->GetMrt(0)->texture();
      auto tex_aux1 = mpMrtAux1->GetMrt(0)->texture();

      mFrameEffectGlowJoin.SetAuxMaps(tex_aux0, tex_aux1);
      mFrameEffectGlowJoin.SetEffectAmount(mfAmount);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectGlowJoin);
    } else if (mEffectName == "ghostly") {
      mFrameEffectGhostJoin.BindRtGroups(cur, 0);
      auto tex_aux0 = mpMrtAux0->GetMrt(0)->texture();
      auto tex_aux1 = mpMrtAux1->GetMrt(0)->texture();
      mFrameEffectGhostJoin.SetAuxMaps(tex_aux0, tex_aux1);
      mFrameEffectGhostJoin.SetEffectAmount(mfAmount);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectGhostJoin);
    } else if (mEffectName == "dof") {
      mFrameEffectDofJoin.BindRtGroups(cur, 0);
      lev2::Texture* pAux0Tex = mpMrtAux0->GetMrt(0)->texture();
      lev2::Texture* pAux1Tex = mpMrtAux1->GetMrt(0)->texture();
      mFrameEffectDofJoin.SetAuxMaps(pAux0Tex, pAux1Tex);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectDofJoin);
    } else if (mEffectName == "afterlife") {
      mFrameEffectAfterLifeJoin.BindRtGroups(cur, 0);
      lev2::Texture* pAux0Tex = mpMrtAux0->GetMrt(0)->texture();
      lev2::Texture* pAux1Tex = mpMrtAux1->GetMrt(0)->texture();
      mFrameEffectAfterLifeJoin.SetAuxMaps(pAux0Tex, pAux1Tex);
      mFrameEffectAfterLifeJoin.SetEffectAmount(mfAmount);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectAfterLifeJoin);
    } else if (mEffectName == "painterly") {
      mFrameEffectPainterly.BindRtGroups(cur, 0);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectPainterly);
    } else if (mEffectName == "debugdepth") {
      mFrameEffectDbgDepth.BindRtGroups(cur, 0);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectDbgDepth);
    } else if (mEffectName == "debugnormals") {
      mFrameEffectDbgNormals.BindRtGroups(cur, 0);
      pTARG->FBI()->GetThisBuffer()->RenderMatOrthoQuad(rect_vp, rect_quad, &mFrameEffectDbgNormals);
    }
    // else if( mEffectName == "debugdeflit" )
    //{
    //	static MatFrameEffect FrameEffectStandard( mpHDRRtGroup, "fxshader://framefx", "frameeffect_debugdeflit" );
    //	pTARG->GetThisBuffer()->RenderMatOrthoQuad( SRect(itx0,ity0,itx1,ity1), SRect(itx1, ity0, itx0, ity1), & FrameEffectStandard
    //);
    //}
  }
  pTARG->GBI()->EndFrame();
  pTARG->FBI()->PopRtGroup();

  mOutputRt = mpMrtFinal;
}

///////////////////////////////////////////////////////////////////////////////

void BuiltinFrameTechniques::update(const CompositingPassData& CPD, int itargw, int itargh) {
  const char* EffectName  = "none";
  float fFxAmt            = 0.0f;
  float fFbAmt            = 0.0f;
  float fFinResMult       = 0.5f;
  float fFxResMult        = 0.5f;
  lev2::Texture* pFbUvMap = nullptr;
  bool bpostfxfb          = false;

  if (CPD.mpGroup) {
    const lev2::CompositingGroupEffect& effect = CPD.mpGroup->_effect;
    EffectName                                 = effect.GetEffectName();
    fFxAmt                                     = effect.GetEffectAmount();
    fFbAmt                                     = effect.GetFeedbackAmount();
    pFbUvMap                                   = effect.GetFbUvMap();
    bpostfxfb                                  = effect.IsPostFxFeedback();
    fFinResMult                                = effect.GetFinalRezScale();
    fFxResMult                                 = effect.GetFxRezScale();
  }

  ////////////////////////////////////////

  this->SetEffect(EffectName, fFxAmt, fFbAmt);
  this->SetFbUvMap(pFbUvMap);
  this->SetPostFxFb(bpostfxfb);

  ////////////////////////////////////////
  // set buffer sizes
  ////////////////////////////////////////
  float fW    = itargw;
  float fH    = itargh;
  int ifxW    = int(fFxResMult * fW);
  int ifxH    = int(fFxResMult * fH);
  int ifinalW = int(fFinResMult * fW);
  int ifinalH = int(fFinResMult * fH);
  this->ResizeFinalBuffer(ifinalW, ifinalH);
  this->ResizeFxBuffer(ifxW, ifxH);
}

///////////////////////////////////////////////////////////////////////////////

BasicFrameTechnique::BasicFrameTechnique()
    : FrameTechniqueBase(0, 0)
    , _shouldBeginAndEndFrame(true) {
}

///////////////////////////////////////////////////////////////////////////////

void BasicFrameTechnique::Render(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.framedata();
  Context* pTARG                    = FrameData.GetTarget();
  auto tgt_rect                     = pTARG->mainSurfaceRectAtOrigin();
  // FrameData.SetDstRect( tgt_rect );
  /*
  IRenderTarget* pTopRenderTarget = FrameData.GetRenderTarget();
  if( _shouldBeginAndEndFrame )
      pTopRenderTarget->BeginFrame( frenderer );
  {
      frenderer.renderMisc();
  }
  if( _shouldBeginAndEndFrame )
      pTopRenderTarget->EndFrame( frenderer );
      */
}

///////////////////////////////////////////////////////////////////////////////

PickFrameTechnique::PickFrameTechnique()
    : FrameTechniqueBase(0, 0) {
}

///////////////////////////////////////////////////////////////////////////////

void PickFrameTechnique::Render(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.framedata();
  Context* pTARG                    = FrameData.GetTarget();
  auto tgt_rect                     = pTARG->mainSurfaceRectAtOrigin();
  // FrameData.SetDstRect( tgt_rect );
  {
    // frenderer.renderMisc();
  }
}

///////////////////////////////////////////////////////////////////////////////

ShadowFrameTechnique::ShadowFrameTechnique(Window* Parent, ui::Viewport* pvp, int iW, int iH)
    : FrameTechniqueBase(iW, iH)
    , _pShadowBuffer(0) {
  _pShadowBuffer                  = new TexBuffer(Parent, EBufferFormat::Z32, 1024, 1024);
  _pShadowBuffer->RefClearColor() = fvec4::Black();
}

///////////////////////////////////////////////////////////////////////////////

void ShadowFrameTechnique::Render(FrameRenderer& frenderer) {
  RenderContextFrameData& FrameData = frenderer.framedata();
  Context* pTARG                    = FrameData.GetTarget();
  /////////////////////////////////////////////////
  int itx0 = 0;
  int itx1 = itx0 + pTARG->mainSurfaceWidth();
  int ity0 = 0;
  int ity1 = ity0 + pTARG->mainSurfaceHeight();
  /////////////////////////////////////////////////

  /*
  fmtx4 lvmat = GetPropertyValueDynamic<fmtx4>( ContextData.GetScene(), "LightLookAt" );
  fmtx4 lpmat = GetPropertyValueDynamic<fmtx4>( ContextData.GetScene(), "LightProjection" );

  fvec3 lpos = GetPropertyValueDynamic<fvec3>( ContextData.GetScene(), "LightPos" );
  fvec3 cpos = ContextData.GetCamera()->CamFocus;

  mpShadowBuffer->context()->beginFrame();
  mpShadowBuffer->context()->PushCamera( ContextData.GetCamera() );
  mpShadowBuffer->context()->MTXI()->PushPMatrix( lpmat );
  mpShadowBuffer->context()->MTXI()->PushVMatrix( lvmat );
  mpShadowBuffer->context()->MTXI()->PushMMatrix( fmtx4::Identity );
  {
      ContextData.SetRenderingMode( RenderContextFrameData::ERENDMODE_SHADOWMAP );
      ContextData.GetRenderer().setContext(mpShadowBuffer->context());
      ContextData.GetRenderer().SetCamera( & ContextData.GetCamera()->mCameraData );
      ContextData.GetScene()->SendToRenderer( & ContextData.GetRenderer(), 1 );
      ContextData.GetRenderer().drawEnqueuedRenderables();
  }
  mpShadowBuffer->context()->PopCamera();
  mpShadowBuffer->context()->MTXI()->PopPMatrix();
  mpShadowBuffer->context()->MTXI()->PopVMatrix();
  mpShadowBuffer->context()->MTXI()->PopMMatrix();
  mpShadowBuffer->context()->endFrame();
*/

  // FrameData.GetRenderer()->setContext(pTARG);
  _pShadowBuffer->SetDirty(false);

  /////////////////////////////////////////////////

  // pTARG->GetThisBuffer()->RenderMatOrthoQuad(	SRect(itx0,ity0,itx1,ity1),
  //											SRect(itx1, ity1, itx0, ity0),
  //											mpShadowBuffer->mpMaterial );

  /////////////////////////////////////////////////

  /*
  pTARG->SetShadowVMatrix( lvmat );
  pTARG->SetShadowPMatrix( lpmat );
  ContextData.SetRenderingMode( RenderContextFrameData::ERENDMODE_SHADOWED );
  pTARG->SetShadowBuffer(mpShadowBuffer);
  mpSceneEditorVP->RenderScene( ContextData );
  pTARG->SetShadowBuffer(0);
*/
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::lev2
