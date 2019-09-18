////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/renderer/compositor.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/renderer/builtin_frameeffects.h>
#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
///////////////////////
// (b over a)+c		BoverAplusC
// (a over b)+c		AoverBplusC
// (a+b+c)			AplusBplusC
// (a+b-c)			AplusBminusC
// (a-b-c)			AminusBplusC
// lerp(a,b,c)		AlerpBwithC
///////////////////////////////////////////////////////////////////////////////

BEGIN_ENUM_SERIALIZER(ork::lev2, ECOMPOSITEBlend)
DECLARE_ENUM(BoverAplusC)
DECLARE_ENUM(AplusBplusC)
DECLARE_ENUM(AlerpBwithC)
DECLARE_ENUM(Asolo)
DECLARE_ENUM(Bsolo)
DECLARE_ENUM(Csolo)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::lev2::Fx3CompositingTechnique, "Fx3CompositingTechnique");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::describeX(class_t* c) {
  ork::reflect::RegisterProperty("Mode", &Fx3CompositingTechnique::meBlendMode);

  ork::reflect::RegisterProperty("GroupA", &Fx3CompositingTechnique::mGroupA);
  ork::reflect::RegisterProperty("GroupB", &Fx3CompositingTechnique::mGroupB);
  ork::reflect::RegisterProperty("GroupC", &Fx3CompositingTechnique::mGroupC);
  ork::reflect::RegisterProperty("LevelA", &Fx3CompositingTechnique::mfLevelA);
  ork::reflect::RegisterProperty("LevelB", &Fx3CompositingTechnique::mfLevelB);
  ork::reflect::RegisterProperty("LevelC", &Fx3CompositingTechnique::mfLevelC);
  reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>("Mode", "editor.class", "ged.factory.enum");
  reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>("LevelA", "editor.range.min", "-10.0");
  reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>("LevelA", "editor.range.max", "10.0");
  reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>("LevelB", "editor.range.min", "-10.0");
  reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>("LevelB", "editor.range.max", "10.0");
  reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>("LevelC", "editor.range.min", "-10.0");
  reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>("LevelC", "editor.range.max", "10.0");
}
///////////////////////////////////////////////////////////////////////////////
Fx3CompositingTechnique::Fx3CompositingTechnique()
    : mpBuiltinFrameTekA(0)
    , mpBuiltinFrameTekB(0)
    , mpBuiltinFrameTekC(0)
    , mfLevelA(1.0f)
    , mfLevelB(1.0f)
    , mfLevelC(1.0f)
    , meBlendMode(AplusBplusC) {}
///////////////////////////////////////////////////////////////////////////////
Fx3CompositingTechnique::~Fx3CompositingTechnique() {
  if (mpBuiltinFrameTekA)
    delete mpBuiltinFrameTekA;
  if (mpBuiltinFrameTekB)
    delete mpBuiltinFrameTekB;
  if (mpBuiltinFrameTekC)
    delete mpBuiltinFrameTekC;
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::Init(ork::lev2::GfxTarget* pTARG, int iW, int iH) {
  if (nullptr == mpBuiltinFrameTekA) {
    mCompositingMaterial.Init(pTARG);

    mpBuiltinFrameTekA = new lev2::BuiltinFrameTechniques(iW, iH);
    mpBuiltinFrameTekA->Init(pTARG);
    mpBuiltinFrameTekB = new lev2::BuiltinFrameTechniques(iW, iH);
    mpBuiltinFrameTekB->Init(pTARG);
    mpBuiltinFrameTekC = new lev2::BuiltinFrameTechniques(iW, iH);
    mpBuiltinFrameTekC->Init(pTARG);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::Draw(CompositorDrawData& drawdata, CompositingImpl* pCCI) {
  const lev2::CompositingGroup* pCGA = pCCI->compositingGroup(mGroupA);
  const lev2::CompositingGroup* pCGB = pCCI->compositingGroup(mGroupB);
  const lev2::CompositingGroup* pCGC = pCCI->compositingGroup(mGroupC);

  struct yo {
    static void rend_lyr_2_comp_group(CompositorDrawData& drawdata,
                                      const lev2::CompositingGroup* pCG,
                                      lev2::BuiltinFrameTechniques* pFT,
                                      const char* layername) {
      lev2::FrameRenderer& the_renderer       = drawdata.mFrameRenderer;
      lev2::RenderContextFrameData& framedata = the_renderer.framedata();
      orkstack<CompositingPassData>& cgSTACK  = drawdata.mCompositingGroupStack;
      auto node = pFT->createPassData(pCG);
      the_renderer.framedata().setLayerName(layername);
      cgSTACK.push(node);
      pFT->Render(the_renderer);
      cgSTACK.pop();
    }
  };

  /////////////////////////////////
  if (mpBuiltinFrameTekA) // render layerA
    yo::rend_lyr_2_comp_group(drawdata, pCGA, mpBuiltinFrameTekA, "A");
  if (mpBuiltinFrameTekB) // render layerB
    yo::rend_lyr_2_comp_group(drawdata, pCGB, mpBuiltinFrameTekB, "B");
  if (mpBuiltinFrameTekC) // render layerC
    yo::rend_lyr_2_comp_group(drawdata, pCGC, mpBuiltinFrameTekC, "C");
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::CompositeLayerToScreen(lev2::GfxTarget* pT,
                                                     CompositingContext& cctx,
                                                     ECOMPOSITEBlend eblend,
                                                     lev2::RtGroup* psrcgroupA,
                                                     lev2::RtGroup* psrcgroupB,
                                                     lev2::RtGroup* psrcgroupC,
                                                     float levA,
                                                     float levB,
                                                     float levC) {

  static const float kMAXW = 1.0f;
  static const float kMAXH = 1.0f;
  auto fbi                 = pT->FBI();
  auto this_buf            = fbi->GetThisBuffer();
  int itw                  = pT->GetW();
  int ith                  = pT->GetH();
#if 0
	auto cur_rtg = fbi->GetRtGroup();
	int iw = cur_rtg ? cur_rtg->GetW() : itw;
	int ih = cur_rtg ? cur_rtg->GetH() : ith;
	auto out_buf = cur_rtg ? cur_rtg->GetMrt(0) : this_buf;
	SRect vprect = (0,0,iw-1,ih-1);
	SRect quadrect(0,ih-1,iw-1,0);
#else
  SRect vprect(0, 0, itw, ith - 1);
  SRect quadrect(0, ith - 1, itw - 1, 0);
  auto out_buf = this_buf;
#endif

  if (psrcgroupA) {
    lev2::Texture* ptexA = (psrcgroupA != 0) ? psrcgroupA->GetMrt(0)->GetTexture() : 0;
    lev2::Texture* ptexB = (psrcgroupB != 0) ? psrcgroupB->GetMrt(0)->GetTexture() : 0;
    lev2::Texture* ptexC = (psrcgroupC != 0) ? psrcgroupC->GetMrt(0)->GetTexture() : 0;
    mCompositingMaterial.SetTextureA(ptexA);
    mCompositingMaterial.SetTextureB(ptexB);
    mCompositingMaterial.SetTextureC(ptexC);

    mCompositingMaterial.SetLevelA(fvec4(levA, levA, levA, levA));
    mCompositingMaterial.SetLevelB(fvec4(levB, levB, levB, levB));
    mCompositingMaterial.SetLevelC(fvec4(levC, levC, levC, levC));

    switch (eblend) {
      case BoverAplusC:
        mCompositingMaterial.SetTechnique("BoverAplusC");
        break;
      case AplusBplusC:
        mCompositingMaterial.SetTechnique("AplusBplusC");
        break;
      case AlerpBwithC:
        mCompositingMaterial.SetTechnique("AlerpBwithC");
        break;
      case Asolo:
        mCompositingMaterial.SetTechnique("Asolo");
        break;
      case Bsolo:
        mCompositingMaterial.SetTechnique("Bsolo");
        break;
      case Csolo:
        mCompositingMaterial.SetTechnique("Csolo");
        break;
      default:
        mCompositingMaterial.SetTechnique("AplusBplusC");
        break;
    }

    out_buf->RenderMatOrthoQuad(vprect, quadrect, &mCompositingMaterial, 0.0f, 0.0f, kMAXW, kMAXH, 0, fvec4::White());
  }
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::CompositeToScreen(ork::lev2::GfxTarget* pT, CompositingImpl* pCCI, CompositingContext& cctx) {
  /////////////////////////////////////////////////////////////////////
  int iCSitem     = 0;
  float levAA     = 0.5f;
  float levBB     = 0.5f;
  float levCC     = 0.5f;
  float levA      = 0.5f;
  float levB      = 0.5f;
  float levC      = 0.5f;
  float levMaster = 1.0f;
  // ECOMPOSITEBlend eblend = AplusBplusC;
  /////////////////////////////////////////////////////////////////////
  const lev2::CompositingSceneItem* pCSI = 0;
  if (pCCI) {
    pCSI = pCCI->compositingItem(0, iCSitem);
  }
  /////////////////////////////////////////////////////////////////////
  if (pCSI) {
    levA = mfLevelA * levMaster * levAA;
    levB = mfLevelB * levMaster * levBB;
    levC = mfLevelC * levMaster * levCC;
  }
  /////////////////////////////////////////////////////////////////////
  lev2::BuiltinFrameTechniques* pFTEKA = mpBuiltinFrameTekA;
  lev2::BuiltinFrameTechniques* pFTEKB = mpBuiltinFrameTekB;
  lev2::BuiltinFrameTechniques* pFTEKC = mpBuiltinFrameTekC;
  lev2::RtGroup* pRTA                  = pFTEKA ? pFTEKA->GetFinalRenderTarget() : 0;
  lev2::RtGroup* pRTB                  = pFTEKB ? pFTEKB->GetFinalRenderTarget() : 0;
  lev2::RtGroup* pRTC                  = pFTEKC ? pFTEKC->GetFinalRenderTarget() : 0;
  if (pRTA || pRTB || pRTC) {
    CompositeLayerToScreen(pT, cctx, meBlendMode, pRTA, pRTB, pRTC, levA, levB, levC);
  }
  /////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::lev2
///////////////////////////////////////////////////////////////////////////////
