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
#include <iostream>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::NodeCompositingTechnique, "NodeCompositingTechnique");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingNode, "CompositingNode");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::PassThroughCompositingNode, "PassThroughCompositingNode");
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::Describe()
{
	ork::reflect::RegisterProperty( "RootNode",
									& NodeCompositingTechnique::GetRoot,
									& NodeCompositingTechnique::SetRoot );
	ork::reflect::AnnotatePropertyForEditor<NodeCompositingTechnique>("RootNode", "editor.factorylistbase", "CompositingNode" );
}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::NodeCompositingTechnique()
	: mpRootNode(nullptr)
{
}
///////////////////////////////////////////////////////////////////////////////
NodeCompositingTechnique::~NodeCompositingTechnique()
{
	if( mpRootNode )
		delete mpRootNode;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::GetRoot(ork::rtti::ICastable*& val) const
{
	CompositingNode* nonconst = const_cast< CompositingNode* >( mpRootNode );
	val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::SetRoot( ork::rtti::ICastable* const & val)
{
	ork::rtti::ICastable* ptr = val;
	mpRootNode = ( (ptr==0) ? 0 : rtti::safe_downcast<CompositingNode*>(ptr) );
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::Init( lev2::GfxTarget* pTARG, int w, int h )
{
	if( mpRootNode )
	{
		mpRootNode->Init(pTARG,w,h);
		
	}
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::Draw(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI)
{
	if( mpRootNode )
	{
		mpRootNode->Draw(drawdata,pCCI);
	}
}
///////////////////////////////////////////////////////////////////////////////
void NodeCompositingTechnique::CompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI, CompositingContext& cctx )
{
	if( mpRootNode )
	{
		mpRootNode->CompositeToScreen(pT,pCCI,cctx);
	}
}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::CompositingBuffer()
{
}
///////////////////////////////////////////////////////////////////////////////
CompositingBuffer::~CompositingBuffer()
{
}
///////////////////////////////////////////////////////////////////////////////
void CompositingNode::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
CompositingNode::CompositingNode()
{
}
///////////////////////////////////////////////////////////////////////////////
CompositingNode::~CompositingNode()
{
}
void CompositingNode::Init( lev2::GfxTarget* pTARG, int w, int h )
{
	DoInit(pTARG,w,h);
}
void CompositingNode::Draw(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI)
{
	DoDraw(drawdata,pCCI);
}
void CompositingNode::CompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI, CompositingContext& cctx )
{
	DoCompositeToScreen(pT,pCCI,cctx);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::Describe()
{
	ork::reflect::RegisterProperty( "Group", & PassThroughCompositingNode::mGroup );
}
///////////////////////////////////////////////////////////////////////////////
PassThroughCompositingNode::PassThroughCompositingNode()
	: mFTEK(nullptr)
{
}
///////////////////////////////////////////////////////////////////////////////
PassThroughCompositingNode::~PassThroughCompositingNode()
{
	if( mFTEK ) delete mFTEK;
}
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::DoInit( lev2::GfxTarget* pTARG, int iW, int iH ) // virtual
{
	if( nullptr == mFTEK )
	{
		mCompositingMaterial.Init( pTARG );

		mFTEK = new lev2::BuiltinFrameTechniques( iW,iH );
		mFTEK->Init( pTARG );
	}
}
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::DoDraw(CMCIdrawdata& drawdata, CompositingComponentInst* pCCI) // virtual
{
	const ent::CompositingGroup* pCG = pCCI->GetGroup(mGroup);
	lev2::FrameRenderer& the_renderer = drawdata.mFrameRenderer;
	lev2::RenderContextFrameData& framedata = the_renderer.GetFrameData();
	orkstack<CompositingPassData>& cgSTACK = drawdata.mCompositingGroupStack;
	
	ent::CompositingPassData node;
	node.mbDrawSource = (pCG != 0);		
	mFTEK->mfSourceAmplitude = pCG ? 1.0f : 0.0f;
	anyp PassData;
	PassData.Set<const char*>( "All" );
	the_renderer.GetFrameData().SetUserProperty( "pass", PassData );
	node.mpGroup = pCG;
	node.mpFrameTek = mFTEK;
	node.mpCameraName = (pCG!=0) ? & pCG->GetCameraName() : 0;
	node.mpLayerName = (pCG!=0) ? & pCG->GetLayers() : 0;
	cgSTACK.push(node);
	mFTEK->Render( the_renderer );
	cgSTACK.pop();
}
///////////////////////////////////////////////////////////////////////////////
void PassThroughCompositingNode::DoCompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI, CompositingContext& cctx ) // virtual
{
	static const float kMAXW = 1.0f;
	static const float kMAXH = 1.0f;
	SRect vprect(0,0,pT->GetW()-1,pT->GetH()-1);
	SRect quadrect(0,pT->GetH()-1,pT->GetW()-1,0);
	lev2::RtGroup* pRT = mFTEK ? mFTEK->GetFinalRenderTarget() : nullptr;
	if( pRT )
	{	lev2::Texture* ptexA = (pRT!=0) ? pRT->GetMrt(0)->GetTexture() : 0;
		mCompositingMaterial.SetTextureA( ptexA );
		CVector3 Levels(1.0f,0.0f,0.0f);
		mCompositingMaterial.SetWeights( Levels );
		mCompositingMaterial.SetTechnique( "Asolo" );
		//pT->FBI()->SetRtGroup(0);
		pT->FBI()->GetThisBuffer()->RenderMatOrthoQuad( vprect, quadrect, & mCompositingMaterial, 0.0f, 0.0f, kMAXW, kMAXH, 0, CVector4::White() );
	}
}
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
