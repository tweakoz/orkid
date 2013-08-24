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
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/drawable.h>
#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/event/MeshEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
///////////////////////////////////////////////////////////////////////////////
//#include "ModelArchetype.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ModelComponentData, "ModelComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ModelComponentInst, "ModelComponentInst");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

/*void ModelArchetype::Describe()
{
}
ModelArchetype::ModelArchetype()
{
}

void ModelArchetype::DoCompose(ork::ent::ArchComposer& composer) 
{
	composer.Register<EditorPropMapData>();
	composer.Register<ork::ent::ModelComponentData>();
	//pedpropmapdata->SetProperty( "visual.lighting.reciever.scope", "static" );
}*/

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ModelComponentData::Describe()
{
	ork::ent::RegisterFamily<ModelComponentData>(ork::AddPooledLiteral("control"));

	ork::reflect::RegisterProperty("Offset", &ModelComponentData::mOffset);
	ork::reflect::RegisterProperty("Rotate", &ModelComponentData::mRotate);

	reflect::RegisterProperty("Model", &ModelComponentData::mModel);
	reflect::RegisterProperty("ShowBoundingSphere", &ModelComponentData::mbShowBoundingSphere);
	reflect::RegisterProperty("EditorDagDebug", &ModelComponentData::mbCopyDag);

	ork::reflect::AnnotatePropertyForEditor<ModelComponentData>("Model", "editor.class", "ged.factory.assetlist");
	ork::reflect::AnnotatePropertyForEditor<ModelComponentData>("Model", "editor.assettype", "xgmodel");
	ork::reflect::AnnotatePropertyForEditor<ModelComponentData>("Model", "editor.assetclass", "xgmodel");

	ork::reflect::RegisterMapProperty("LayerFxMap", &ModelComponentData::mLayerFx);
	ork::reflect::AnnotatePropertyForEditor<ModelComponentData>("LayerFxMap", "editor.assettype", "FxShader");
	ork::reflect::AnnotatePropertyForEditor<ModelComponentData>("LayerFxMap", "editor.assetclass", "FxShader");

	ork::reflect::RegisterProperty("AlwaysVisible", &ModelComponentData::mAlwaysVisible);
	ork::reflect::RegisterProperty("Scale", &ModelComponentData::mfScale);

	reflect::AnnotatePropertyForEditor<ModelComponentData>( "Scale", "editor.range.min", "-1000.0" );
	reflect::AnnotatePropertyForEditor<ModelComponentData>( "Scale", "editor.range.max", "1000.0" );
	//reflect::AnnotatePropertyForEditor<ModelComponentData>( "Scale", "editor.range.log", "true" );
}

///////////////////////////////////////////////////////////////////////////////

lev2::XgmModel* ModelComponentData::GetModel() const
{
	return (mModel==0) ? 0 : mModel->GetModel();
}

///////////////////////////////////////////////////////////////////////////////

ModelComponentData::ModelComponentData()
	: mModel(0)
	, mAlwaysVisible(false)
	, mfScale( 1.0f )
	, mRotate(0.0f,0.0f,0.0f)
	, mOffset(0.0f,0.0f,0.0f)
	, mbShowBoundingSphere(false)
	, mbCopyDag(false)
{
}

///////////////////////////////////////////////////////////////////////////////

ComponentInst *ModelComponentData::CreateComponent(Entity *pent) const
{
	ComponentInst* pinst = OrkNew ModelComponentInst( *this, pent );
	return pinst;
}

///////////////////////////////////////////////////////////////////////////////

void ModelComponentData::SetModelAccessor(ork::rtti::ICastable *const &model)
{
	mModel = model ? ork::rtti::safe_downcast<ork::lev2::XgmModelAsset *>(model) : 0;
}
void ModelComponentData::GetModelAccessor(ork::rtti::ICastable *&model) const
{
	model = mModel;
}

///////////////////////////////////////////////////////////////////////////////

void ModelComponentInst::Describe()
{

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ModelComponentInst::~ModelComponentInst()
{
	if( mXgmModelInst ) delete mXgmModelInst;
}
ModelComponentInst::ModelComponentInst(const ModelComponentData &data, Entity *pent)
	: ComponentInst( & data, pent )
	, mData( data )
	, mXgmModelInst( 0 )
{
	mModelDrawable = new ModelDrawable(pent); // deleted when entity deleted
	lev2::XgmModel* model = data.GetModel();

	if( model )
	{
		mXgmModelInst = new ork::lev2::XgmModelInst(model);

		mModelDrawable->SetModelInst( mXgmModelInst );
		mModelDrawable->SetScale( mData.GetScale() );

	//	ork::ent::ModelDrawable *drawable = new ork::ent::ModelDrawable(pent);
	//	drawable->SetModelInst(minst);
		pent->AddDrawable(AddPooledLiteral("Default"),mModelDrawable);
		mModelDrawable->SetOwner(pent);

		mXgmModelInst->RefLocalPose().BindPose();
		mXgmModelInst->RefLocalPose().BuildPose();
	}

	const orklut<PoolString,lev2::FxShaderAsset*>& lfxmap = mData.GetLayerFXMap();
	 			
	for( orklut<PoolString,lev2::FxShaderAsset*>::const_iterator it=lfxmap.begin(); it!=lfxmap.end(); it++ )
	{
		lev2::FxShaderAsset* passet = it->second;

		if( passet && passet->IsLoaded() )
		{
			lev2::FxShader* pfxshader = passet->GetFxShader();

			if( pfxshader )
			{
				lev2::GfxMaterialFx* pfxmaterial = new lev2::GfxMaterialFx();
				pfxmaterial->SetEffect( pfxshader );
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void ModelComponentInst::DoUpdate( ork::ent::SceneInst* psi )
{
	if( mData.IsCopyDag() )
	{
		//mEntity->GetEntData().GetDagNode()
		GetEntity()->GetDagNode() = GetEntity()->GetEntData().GetDagNode();
	}

	mModelDrawable->SetScale( mData.GetScale() );
	mModelDrawable->SetRotate( mData.GetRotate() );
	mModelDrawable->SetOffset( mData.GetOffset() );
	mModelDrawable->ShowBoundingSphere( mData.ShowBoundingSphere() );
}

///////////////////////////////////////////////////////////////////////////////

bool ModelComponentInst::DoNotify(const ork::event::Event *event)
{
	if(const event::MeshEnableEvent *meshenaev = ork::rtti::autocast(event))
	{
		if(GetModelDrawable().GetModelInst())
		{
			if(meshenaev->IsEnable())
				GetModelDrawable().GetModelInst()->EnableMesh(meshenaev->GetName());
			else
				GetModelDrawable().GetModelInst()->DisableMesh(meshenaev->GetName());
			return true;
		}
	}
	else if(const event::MeshLayerFxEvent *lfxev = ork::rtti::autocast(event))
	{	if(GetModelDrawable().GetModelInst())
		{	lev2::GfxMaterialFx* pmaterial = 0;
			if( lfxev->IsEnable() )
			{	orklut<PoolString,lev2::GfxMaterialFx*>::const_iterator it=mFxMaterials.find(lfxev->GetName());
				if( it != mFxMaterials.end() )
				{	lev2::GfxMaterialFx* pmaterial = it->second;
				}
			}
			GetModelDrawable().GetModelInst()->SetLayerFxMaterial(pmaterial);
			return true;
		}
	}
	return false;
}

///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}}
