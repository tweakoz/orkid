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
#include <pkg/ent/ModelArchetype.h>
#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/event/MeshEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/gfx/camera.h>
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ModelArchetype, "ModelArchetype" );
//INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SkyBoxArchetype, "SkyBoxArchetype" );
//INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SkyBoxControllerInst, "SkyBoxControllerInst" );
//INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SkyBoxControllerData, "SkyBoxControllerData" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void ModelArchetype::Describe()
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
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/*void ModelComponentData::Describe()
{
	ork::ent::RegisterFamily<ModelComponentData>(ork::AddPooledLiteral("control"));

	ork::reflect::RegisterProperty("Offset", &ModelComponentData::mOffset);
	ork::reflect::RegisterProperty("Rotate", &ModelComponentData::mRotate);

	reflect::RegisterProperty("Model", &ModelComponentData::mModel);

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

ModelComponentData::ModelComponentData()
	: mModel(0)
	, mAlwaysVisible(false)
	, mfScale( 1.0f )
	, mRotate(0.0f,0.0f,0.0f)
	, mOffset(0.0f,0.0f,0.0f)
{
}

ComponentInst *ModelComponentData::CreateComponent(Entity *pent) const
{
	ComponentInst* pinst = OrkNew ModelComponentInst( *this, pent );
	return pinst;
}

void ModelComponentData::SetModelAccessor(ork::rtti::ICastable *const &model)
{
	mModel = model ? ork::rtti::safe_downcast<ork::lev2::XgmModelAsset *>(model) : 0;
}
void ModelComponentData::GetModelAccessor(ork::rtti::ICastable *&model) const
{
	model = mModel;
}

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
		pent->AddDrawable(mModelDrawable);
		mModelDrawable->SetOwner(pent);

		mXgmModelInst->RefLocalPose().BindPose();
		mXgmModelInst->RefLocalPose().BuildPose();
	}

	const orklut<PoolString,lev2::FxShaderAsset*>& lfxmap = mData.GetLayerFXMap();
	 			
	for( orklut<PoolString,lev2::FxShaderAsset*>::const_iterator it=lfxmap.begin(); it!=lfxmap.end(); it++ )
	{
		lev2::FxShaderAsset* passet = it->second;
		lev2::FxShader* pfxshader = passet->GetFxShader();

		if( pfxshader )
		{
			lev2::GfxMaterialFx* pfxmaterial = new lev2::GfxMaterialFx();
			pfxmaterial->SetEffect( pfxshader );
		}
	}
}

void ModelComponentInst::DoUpdate( ork::ent::SceneInst* psi )
{
	mModelDrawable->SetScale( mData.GetScale() );
	mModelDrawable->SetRotate( mData.GetRotate() );
	mModelDrawable->SetOffset( mData.GetOffset() );
}

bool ModelComponentInst::Notify(const ork::event::Event *event)
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
*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

/*void SkyBoxArchetype::Describe()
{
	//reflect::RegisterProperty( "SpinRate", & SkyBoxArchetype::mfSpinRate );
	//reflect::AnnotatePropertyForEditor<SkyBoxArchetype>( "SpinRate", "editor.range.min", "-6.28" );
	//reflect::AnnotatePropertyForEditor<SkyBoxArchetype>( "SpinRate", "editor.range.max", "6.28" );
}

SkyBoxArchetype::SkyBoxArchetype()
//	: mfSpinRate(0.0f)
{
}

void SkyBoxArchetype::DoLinkEntity( SceneInst* psi, Entity *pent ) const
{
	struct yo
	{
		const SkyBoxArchetype* parch;
		Entity *pent;

		static void doit( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren )
		{
			const yo* pyo = pren->GetUserData0().Get<const yo*>();

			const SkyBoxArchetype* parch = pyo->parch;
			const Entity* pent = pyo->pent;
			const SkyBoxControllerInst* ssci = pent->GetTypedComponent<SkyBoxControllerInst>();
			const SkyBoxControllerData&	cd = ssci->GetCD();
			
			if( cd.GetModel() )
			{
				ork::lev2::XgmModelInst minst( cd.GetModel() );
				ork::lev2::RenderContextInstData MatCtx;
				ork::lev2::RenderContextInstModelData MdlCtx;
				ork::lev2::XgmMaterialStateInst MatInst(minst);
				MatCtx.SetMaterialInst(&MatInst);
				MatCtx.SetRenderer(rcid.GetRenderer());
				MdlCtx.SetSkinned(false);

				///////////////////////////////////////////////////////////
				// setup headlight (default lighting)
				///////////////////////////////////////////////////////////
				ork::CMatrix4				HeadLightMatrix;
				ork::lev2::LightingGroup	HeadLightGroup;
				ork::lev2::AmbientLightData	HeadLightData;
				ork::lev2::AmbientLight		HeadLight(HeadLightMatrix,&HeadLightData);
				ork::lev2::LightManagerData	HeadLightManagerData;
				ork::lev2::LightManager HeadLightManager(HeadLightManagerData);
				HeadLightData.SetColor(ork::CVector3(1.3f, 1.3f, 1.5f));
				HeadLightData.SetAmbientShade( 0.75f );
				HeadLight.miInFrustumID = 1;
				HeadLightGroup.mLightMask.AddLight( & HeadLight );
				HeadLightGroup.mLightManager = & HeadLightManager;
				const lev2::RenderContextFrameData& FrameData = *targ->GetRenderContextFrameData();
				HeadLightMatrix = FrameData.GetCameraData()->GetIVMatrix();
				HeadLightManager.mGlobalMovingLights.AddLight( & HeadLight );
				HeadLightManager.mLightsInFrustum.push_back(& HeadLight);
				MatCtx.SetLightingGroup( & HeadLightGroup );
				///////////////////////////////////////////////////////////
				// setup headlight (default lighting)
				///////////////////////////////////////////////////////////
				
				float fscale = cd.GetScale();
				CVector3 pos = FrameData.GetCameraData()->GetEye();
				CMatrix4 mtxSKY;
				mtxSKY.SetScale( fscale );
				mtxSKY.SetTranslation( pos );
				MatCtx.ForceNoZWrite( true );

				int inummeshes = cd.GetModel()->GetNumMeshes();
				for( int imesh=0; imesh<inummeshes; imesh++ )
				{
					const lev2::XgmMesh& mesh = * cd.GetModel()->GetMesh(imesh);

					int inumclusset = mesh.GetNumSubMeshes();

					for( int ics=0; ics<inumclusset; ics++ )
					{
						const lev2::XgmSubMesh& submesh = * mesh.GetSubMesh(ics);
						const lev2::GfxMaterial* material = submesh.mpMaterial;

						int inumclus = submesh.miNumClusters;

						MatCtx.SetMaterialIndex(ics);

						for( int ic=0; ic<inumclus; ic++ )
						{

							MdlCtx.mMesh = & mesh;
							MdlCtx.mSubMesh = & submesh;
							MdlCtx.mCluster = & submesh.RefCluster(ic);

							cd.GetModel()->RenderRigid(	ork::CColor4::White(), 
																mtxSKY,
																targ,
																MatCtx,
																MdlCtx );
						}
					}
				}
			}
		}
		static void BufferCB(ork::ent::DrawableBufItem&cdb)
		{

		}
	};

	CallbackDrawable* pdrw = new CallbackDrawable(pent);
	pent->AddDrawable( pdrw );
	pdrw->SetCallback( yo::doit );
	pdrw->SetBufferCallback( yo::BufferCB );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetSortKey(0);

	yo* pyo = new yo;
	pyo->parch = this;
	pyo->pent = pent;

	anyp a4;
	a4.Set<const yo*>( pyo );
	pdrw->SetData( a4 );

}
///////////////////////////////////////////////////////////////////////////////
void SkyBoxArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<SkyBoxControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void SkyBoxControllerData::Describe()
{
	ork::ent::RegisterFamily<SkyBoxControllerData>(ork::AddPooledLiteral("control"));

	reflect::RegisterProperty( "SpinRate", & SkyBoxControllerData::mfSpinRate );

	reflect::AnnotatePropertyForEditor<SkyBoxControllerData>( "SpinRate", "editor.range.min", "-6.28" );
	reflect::AnnotatePropertyForEditor<SkyBoxControllerData>( "SpinRate", "editor.range.max", "6.28" );

	reflect::RegisterProperty("Model", &SkyBoxControllerData::mModelAsset);

	ork::reflect::AnnotatePropertyForEditor<SkyBoxControllerData>("Model", "editor.class", "ged.factory.assetlist");
	ork::reflect::AnnotatePropertyForEditor<SkyBoxControllerData>("Model", "editor.assettype", "xgmodel");
	ork::reflect::AnnotatePropertyForEditor<SkyBoxControllerData>("Model", "editor.assetclass", "xgmodel");

	ork::reflect::RegisterProperty("Scale", &SkyBoxControllerData::mfScale);

	reflect::AnnotatePropertyForEditor<SkyBoxControllerData>( "Scale", "editor.range.min", "-1000.0" );
	reflect::AnnotatePropertyForEditor<SkyBoxControllerData>( "Scale", "editor.range.max", "1000.0" );
}

SkyBoxControllerData::SkyBoxControllerData()
	: mfSpinRate( 0.0f )
	, mModelAsset(0)
	, mfScale(1.0f)
{
}

lev2::XgmModel* SkyBoxControllerData::GetModel() const
{
	lev2::XgmModel* pmodel = (mModelAsset!=0) ? mModelAsset->GetModel() : 0;
	return pmodel;
}

void SkyBoxControllerInst::Describe()
{
}
SkyBoxControllerInst::SkyBoxControllerInst( const SkyBoxControllerData& data, ent::Entity* pent )
	: ork::ent::ComponentInst( & data, pent )
	, mCD( data )
	, mPhase( 0.0f )
{
}
ent::ComponentInst* SkyBoxControllerData::CreateComponent(ent::Entity* pent) const
{
	return OrkNew SkyBoxControllerInst( *this, pent );
}
void SkyBoxControllerInst::DoUpdate(ent::SceneInst* sinst)
{
	mPhase += mCD.GetSpinRate()*sinst->GetDeltaTime();
}

*/
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}}
