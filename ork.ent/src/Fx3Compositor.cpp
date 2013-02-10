////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.hpp>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/Compositor.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/enum_serializer.h>
#include <pkg/ent/PerfController.h>
///////////////////////
// (b over a)+c		BoverAplusC
// (a over b)+c		AoverBplusC
// (a+b+c)			AplusBplusC
// (a+b-c)			AplusBminusC
// (a-b-c)			AminusBplusC
// lerp(a,b,c)		AlerpBwithC
///////////////////////////////////////////////////////////////////////////////

BEGIN_ENUM_SERIALIZER(ork::ent, ECOMPOSITEBlend)
	DECLARE_ENUM(BoverAplusC)
	DECLARE_ENUM(AplusBplusC)
	DECLARE_ENUM(AlerpBwithC)
	DECLARE_ENUM(Asolo)
	DECLARE_ENUM(Bsolo)
	DECLARE_ENUM(Csolo)
END_ENUM_SERIALIZER()

///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::Fx3CompositingTechnique, "Fx3CompositingTechnique");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::Describe()
{
	ork::reflect::RegisterProperty("Mode", &Fx3CompositingTechnique::meBlendMode );

	ork::reflect::RegisterProperty("GroupA", &Fx3CompositingTechnique::mGroupA );
	ork::reflect::RegisterProperty("GroupB", &Fx3CompositingTechnique::mGroupB );
	ork::reflect::RegisterProperty("GroupC", &Fx3CompositingTechnique::mGroupC );
	ork::reflect::RegisterProperty("LevelA", &Fx3CompositingTechnique::mfLevelA );
	ork::reflect::RegisterProperty("LevelB", &Fx3CompositingTechnique::mfLevelB );
	ork::reflect::RegisterProperty("LevelC", &Fx3CompositingTechnique::mfLevelC );
	reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>( "Mode", "editor.class", "ged.factory.enum" );
	reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>( "LevelA", "editor.range.min", "-10.0" );
	reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>( "LevelA", "editor.range.max", "10.0" );
	reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>( "LevelB", "editor.range.min", "-10.0" );
	reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>( "LevelB", "editor.range.max", "10.0" );
	reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>( "LevelC", "editor.range.min", "-10.0" );
	reflect::AnnotatePropertyForEditor<Fx3CompositingTechnique>( "LevelC", "editor.range.max", "10.0" );
}
///////////////////////////////////////////////////////////////////////////////
Fx3CompositingTechnique::Fx3CompositingTechnique()
	: mpBuiltinFrameTekA(0)
	, mpBuiltinFrameTekB(0)
	, mpBuiltinFrameTekC(0)
	, mfLevelA(1.0f)
	, mfLevelB(1.0f)
	, mfLevelC(1.0f)
	, meBlendMode( AplusBplusC )
{
}
///////////////////////////////////////////////////////////////////////////////
Fx3CompositingTechnique::~Fx3CompositingTechnique()
{
	if( mpBuiltinFrameTekA )
		delete mpBuiltinFrameTekA;
	if( mpBuiltinFrameTekB )
		delete mpBuiltinFrameTekB;
	if( mpBuiltinFrameTekC )
		delete mpBuiltinFrameTekC;
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::Init(ork::lev2::GfxTarget* pTARG, int iW, int iH )
{
	if( nullptr == mpBuiltinFrameTekA )
	{
		mCompositingMaterial.Init( pTARG );

		mpBuiltinFrameTekA = new lev2::BuiltinFrameTechniques( iW,iH );
		mpBuiltinFrameTekA->Init( pTARG );
		mpBuiltinFrameTekB = new lev2::BuiltinFrameTechniques( iW,iH );
		mpBuiltinFrameTekB->Init( pTARG );
		mpBuiltinFrameTekC = new lev2::BuiltinFrameTechniques( iW,iH );
		mpBuiltinFrameTekC->Init( pTARG );
	}
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::Draw(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI)
{
	const ent::CompositingGroup* pCGA = pCCI->GetGroup(mGroupA);
	const ent::CompositingGroup* pCGB = pCCI->GetGroup(mGroupB);
	const ent::CompositingGroup* pCGC = pCCI->GetGroup(mGroupC);
	
	struct yo
	{
		static void rend_lyr_2_comp_group(	CMCIdrawdata& drawdata,
											const ent::CompositingGroup* pCG,
											lev2::BuiltinFrameTechniques* pFT,
											const char* LayerName )
		{
			lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
			lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();
			orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;
		
			ent::CompositingPassData node;
			node.mbDrawSource = (pCG != 0);		
			pFT->mfSourceAmplitude = pCG ? 1.0f : 0.0f;
			anyp PassData;
			PassData.Set<const char*>( LayerName );
			the_renderer.GetFrameData().SetUserProperty( "pass", PassData );
			node.mpGroup = pCG;
			node.mpFrameTek = pFT;
			node.mpCameraName = (pCG!=0) ? & pCG->GetCameraName() : 0;
			node.mpLayerName = (pCG!=0) ? & pCG->GetLayers() : 0;
			cgSTACK.push(node);
			pFT->Render( the_renderer );
			cgSTACK.pop();
		}
	};

	/////////////////////////////////
	if( mpBuiltinFrameTekA )// render layerA
		yo::rend_lyr_2_comp_group( drawdata, pCGA, mpBuiltinFrameTekA, "A" );
	if( mpBuiltinFrameTekB )// render layerB
		yo::rend_lyr_2_comp_group( drawdata, pCGB, mpBuiltinFrameTekB, "B" );
	if( mpBuiltinFrameTekC )// render layerC
		yo::rend_lyr_2_comp_group( drawdata, pCGC, mpBuiltinFrameTekC, "C" );
}
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::CompositeLayerToScreen(	lev2::GfxTarget* pT,
														CompositingContext& cctx,
														ECOMPOSITEBlend eblend,
														lev2::RtGroup* psrcgroupA,
														lev2::RtGroup* psrcgroupB,
														lev2::RtGroup* psrcgroupC,
														float levA, float levB, float levC )
{	

	static const float kMAXW = 1.0f;
	static const float kMAXH = 1.0f;
	auto fbi = pT->FBI();
	auto this_buf = fbi->GetThisBuffer();
	int itw = pT->GetW();
	int ith = pT->GetH();
#if 0
	auto cur_rtg = fbi->GetRtGroup();
	int iw = cur_rtg ? cur_rtg->GetW() : itw;
	int ih = cur_rtg ? cur_rtg->GetH() : ith;
	auto out_buf = cur_rtg ? cur_rtg->GetMrt(0) : this_buf;
	SRect vprect = (0,0,iw-1,ih-1);
	SRect quadrect(0,ih-1,iw-1,0);
#else
	SRect vprect(0,0,itw,ith-1);
	SRect quadrect(0,ith-1,itw-1,0);
	auto out_buf = this_buf;
#endif

	
	if( psrcgroupA )
	{
		lev2::Texture* ptexA = (psrcgroupA!=0) ? psrcgroupA->GetMrt(0)->GetTexture() : 0;
		lev2::Texture* ptexB = (psrcgroupB!=0) ? psrcgroupB->GetMrt(0)->GetTexture() : 0;
		lev2::Texture* ptexC = (psrcgroupC!=0) ? psrcgroupC->GetMrt(0)->GetTexture() : 0;
		mCompositingMaterial.SetTextureA( ptexA );
		mCompositingMaterial.SetTextureB( ptexB );
		mCompositingMaterial.SetTextureC( ptexC );

		CVector3 Levels(levA,levB,levC);
		mCompositingMaterial.SetWeights( Levels );
			
		switch( eblend )
		{
			case BoverAplusC:
				mCompositingMaterial.SetTechnique( "BoverAplusC" );
				break;
			case AplusBplusC:
				mCompositingMaterial.SetTechnique( "AplusBplusC" );
				break;
			case AlerpBwithC:
				mCompositingMaterial.SetTechnique( "AlerpBwithC" );
				break;
			case Asolo:
				mCompositingMaterial.SetTechnique( "Asolo" );
				break;
			case Bsolo:
				mCompositingMaterial.SetTechnique( "Bsolo" );
				break;
			case Csolo:
				mCompositingMaterial.SetTechnique( "Csolo" );
				break;
			default:
				mCompositingMaterial.SetTechnique( "AplusBplusC" );
				break;
		}
		

		out_buf->RenderMatOrthoQuad( vprect, quadrect, & mCompositingMaterial, 0.0f, 0.0f, kMAXW, kMAXH, 0, CVector4::White() );

	}
}	
///////////////////////////////////////////////////////////////////////////////
void Fx3CompositingTechnique::CompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI, CompositingContext& cctx )
{
	/////////////////////////////////////////////////////////////////////
	int iCSitem = 0;
	float levAA = 0.5f;
	float levBB = 0.5f;
	float levCC = 0.5f;
	float levA = 0.5f;
	float levB = 0.5f;
	float levC = 0.5f;
	float levMaster = 1.0f;
	//ECOMPOSITEBlend eblend = AplusBplusC;
	/////////////////////////////////////////////////////////////////////
	const ent::CompositingSceneItem* pCSI = 0;
	if( pCCI )
	{
		pCSI = pCCI->GetCompositingItem(0,iCSitem);
	}
	/////////////////////////////////////////////////////////////////////
	if( pCSI )
	{
		levA = mfLevelA*levMaster*levAA;
		levB = mfLevelB*levMaster*levBB;
		levC = mfLevelC*levMaster*levCC;
	}
	/////////////////////////////////////////////////////////////////////
	lev2::BuiltinFrameTechniques* pFTEKA = mpBuiltinFrameTekA;
	lev2::BuiltinFrameTechniques* pFTEKB = mpBuiltinFrameTekB;
	lev2::BuiltinFrameTechniques* pFTEKC = mpBuiltinFrameTekC;
	lev2::RtGroup* pRTA = pFTEKA ? pFTEKA->GetFinalRenderTarget() : 0;
	lev2::RtGroup* pRTB = pFTEKB ? pFTEKB->GetFinalRenderTarget() : 0;
	lev2::RtGroup* pRTC = pFTEKC ? pFTEKC->GetFinalRenderTarget() : 0;
	if( pRTA || pRTB || pRTC )
	{
		CompositeLayerToScreen(	pT, cctx, meBlendMode,
								pRTA, pRTB, pRTC,
								levA, levB, levC );
	}
	/////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
