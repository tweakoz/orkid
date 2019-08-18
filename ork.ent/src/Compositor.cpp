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
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingGroup, "CompositingGroup");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingScene, "CompositingScene");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingSceneItem, "CompositingSceneItem");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingGroupEffect, "CompositingGroupEffect");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingTechnique, "CompositingTechnique");

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingManagerComponentData, "CompositingManager");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositingManagerComponentInst, "CompositingManagerInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::CompositorArchetype, "CompositorArchetype");

template  ork::ent::CompositingManagerComponentInst* ork::ent::SceneInst::FindSystem() const;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
void CompositingTechnique::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
CompositingContext::CompositingContext()
	: miWidth(0)
	, miHeight(0)
	, mCTEK(nullptr)
{
}

///////////////////////////////////////////////////////////////////////////////

CompositingContext::~CompositingContext()
{
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::Init( lev2::GfxTarget* pTARG )
{
	if( (miWidth!=pTARG->GetW()) || (miHeight!=pTARG->GetH()) )
	{	miWidth = pTARG->GetW();
		miHeight = pTARG->GetH();
	}
	mUtilMaterial.Init( pTARG );
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::Resize( int iW, int iH )
{
	miWidth = iW;
	miHeight = iH;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::Draw( lev2::GfxTarget* pTARG, CMCIdrawdata& drawdata, CompositingComponentInst* pCCI )
{
	Init(pTARG); // fixme lazy init
	if( mCTEK )
	{	mCTEK->Init(pTARG,miWidth,miHeight);
		mCTEK->Draw(drawdata,pCCI);
	}
}

///////////////////////////////////////////////////////////////////////////////

void CompositingContext::CompositeToScreen( ork::lev2::GfxTarget* pT, CompositingComponentInst* pCCI )
{
	Init(pT);
	if( mCTEK )
		mCTEK->CompositeToScreen( pT, pCCI, *this );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingScene::Describe()
{
	ork::reflect::RegisterMapProperty("Items", &CompositingScene::mItems);
	ork::reflect::AnnotatePropertyForEditor< CompositingScene >("Items", "editor.factorylistbase", "CompositingSceneItem" );
}

///////////////////////////////////////////////////////////////////////////////

CompositingScene::CompositingScene()
{
}

///////////////////////////////////////////////////////////////////////////////

void CompositingSceneItem::Describe()
{


	ork::reflect::RegisterProperty( "Technique",
									& CompositingSceneItem::GetTech,
									& CompositingSceneItem::SetTech );
	ork::reflect::AnnotatePropertyForEditor<CompositingSceneItem>("Technique", "editor.factorylistbase", "CompositingTechnique" );

}

///////////////////////////////////////////////////////////////////////////////

CompositingSceneItem::CompositingSceneItem()
	: mpTechnique(nullptr)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::Describe()
{
	ork::reflect::RegisterProperty("Type", &CompositingGroupEffect::meFrameEffect );
	ork::reflect::RegisterProperty("Amount", &CompositingGroupEffect::mfEffectAmount );
	ork::reflect::RegisterProperty("FinalRezScale", &CompositingGroupEffect::mfFinalResMult );
	ork::reflect::RegisterProperty("FxRezScale", &CompositingGroupEffect::mfFxResMult );
	ork::reflect::RegisterProperty("FeedbackAmount", &CompositingGroupEffect::mfFeedbackAmount );
	ork::reflect::RegisterProperty("FbUvTexture", &CompositingGroupEffect::GetTextureAccessor,  &CompositingGroupEffect::SetTextureAccessor );
    ork::reflect::RegisterProperty( "PostFxFeedback", & CompositingGroupEffect::mbPostFxFeedback );

	ork::reflect::AnnotatePropertyForEditor<CompositingGroupEffect>("Type", "editor.class", "ged.factory.enum" );
    ork::reflect::AnnotatePropertyForEditor<CompositingGroupEffect>("FbUvTexture", "editor.class", "ged.factory.assetlist" );
    ork::reflect::AnnotatePropertyForEditor<CompositingGroupEffect>("FbUvTexture", "editor.assettype", "lev2tex" );
    ork::reflect::AnnotatePropertyForEditor<CompositingGroupEffect>("FbUvTexture", "editor.assetclass", "lev2tex");

	reflect::AnnotatePropertyForEditor< CompositingGroupEffect >("FxRezScale", "editor.range.min", "0.025" );
	reflect::AnnotatePropertyForEditor< CompositingGroupEffect >("FxRezScale", "editor.range.max", "1.0" );
	reflect::AnnotatePropertyForEditor< CompositingGroupEffect >("FinalRezScale", "editor.range.min", "0.025" );
	reflect::AnnotatePropertyForEditor< CompositingGroupEffect >("FinalRezScale", "editor.range.max", "1.0" );
}

///////////////////////////////////////////////////////////////////////////////

CompositingGroupEffect::CompositingGroupEffect()
	: meFrameEffect(lev2::EFRAMEFX_NONE)
	, mfEffectAmount(0.0f)
	, mfFeedbackAmount(0.0f)
	, mfFinalResMult(0.5f)
	, mfFxResMult(0.5f)
    , mTexture(0)
	, mbPostFxFeedback(false)
{
}

///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::SetTextureAccessor( ork::rtti::ICastable* const & tex)
{	mTexture = tex ? ork::rtti::autocast( tex ) : 0;
}

///////////////////////////////////////////////////////////////////////////////

void CompositingGroupEffect::GetTextureAccessor( ork::rtti::ICastable* & tex) const
{	tex = mTexture;
}

ork::lev2::Texture*	CompositingGroupEffect::GetFbUvMap() const
{
    return (mTexture==0) ? 0 : mTexture->GetTexture();
}

///////////////////////////////////////////////////////////////////////////////

const char* CompositingGroupEffect::GetEffectName() const
{
	static const char* None = "none";
	static const char* Std = "standard";
	static const char* Comic = "comic";
	static const char* Glow = "glow";
	static const char* Ghostly = "ghostly";
	static const char* AfterLife = "afterlife";

	const char* EffectName = None;
	switch( meFrameEffect )
	{
		case lev2::EFRAMEFX_NONE:
			EffectName = None;
			break;
		case lev2::EFRAMEFX_STANDARD:
			EffectName = Std;
			break;
		case lev2::EFRAMEFX_COMIC:
			EffectName = Comic;
			break;
		case lev2::EFRAMEFX_GLOW:
			EffectName = Glow;
			break;
		case lev2::EFRAMEFX_GHOSTLY:
			EffectName = Ghostly;
			break;
		case lev2::EFRAMEFX_AFTERLIFE:
			EffectName = AfterLife;
			break;
		default:
			break;
	}
	return EffectName;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void CompositingGroup::Describe()
{
	ork::reflect::RegisterProperty("Camera", &CompositingGroup::mCameraName);
	ork::reflect::RegisterProperty("Layers", &CompositingGroup::mLayers);
	ork::reflect::RegisterProperty("Effect", &CompositingGroup::EffectAccessor );
}

///////////////////////////////////////////////////////////////////////////////

CompositingGroup::CompositingGroup()
{
}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::WriteMorphTarget( dataflow::MorphKey name, float flerpval )
{
}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::RecallMorphTarget( dataflow::MorphKey name )
{
}

///////////////////////////////////////////////////////////////////////////////

void CompositingMorphable::Morph1D( const dataflow::morph_event* pme )
{
}

///////////////////////////////////////////////////////////////////////////////
void CompositingSceneItem::GetTech(ork::rtti::ICastable*& val) const
{
	CompositingTechnique* nonconst = const_cast< CompositingTechnique* >( mpTechnique );
	val = nonconst;
}
///////////////////////////////////////////////////////////////////////////////
void CompositingSceneItem::SetTech( ork::rtti::ICastable* const & val)
{
	ork::rtti::ICastable* ptr = val;
	mpTechnique = ( (ptr==0) ? 0 : rtti::safe_downcast<CompositingTechnique*>(ptr) );
}
}}
//template const ork::ent::CompositingComponentData* ork::ent::EntData::GetTypedComponent<ork::ent::CompositingComponentData>() const;
//template ork::ent::CompositingComponentInst* ork::ent::Entity::GetTypedComponent<ork::ent::CompositingComponentInst>(bool);
