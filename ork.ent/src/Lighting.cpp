////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <ork/gfx/camera.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <pkg/ent/drawable.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <pkg/ent/Lighting.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxprimitives.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightingComponentData, "LightingComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightingComponentInst, "LightingComponentInst");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightArchetype, "LightArchetype");

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightingManagerComponentData, "LightingManagerSceneComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::LightingManagerComponentInst, "LightingManagerSceneComponentInst");

template  ork::ent::LightingManagerComponentInst* ork::ent::SceneInst::FindTypedSceneComponent() const;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightingComponentData::Describe()
{
	ork::ent::RegisterFamily<LightingComponentData>(ork::AddPooledLiteral("lighting"));
	ork::reflect::RegisterProperty("LightData", &LightingComponentData::LdGetter, &LightingComponentData::LdSetter );
	ork::reflect::RegisterProperty("Dynamic", &LightingComponentData::mbDynamic );
	ork::reflect::AnnotatePropertyForEditor< LightingComponentData >("LightData", "editor.factorylistbase", "LightData" );
}

///////////////////////////////////////////////////////////////////////////////

LightingComponentData::LightingComponentData()
	: mLightData( 0 )
	, mbDynamic( false )
{
}

LightingComponentData::~LightingComponentData()
{
	if( mLightData )
		delete mLightData;
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::ComponentInst *LightingComponentData::CreateComponent(ork::ent::Entity *pent) const
{
	return OrkNew LightingComponentInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightingComponentInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

LightingComponentInst::LightingComponentInst( const LightingComponentData& data, ork::ent::Entity *pent )
	: ork::ent::ComponentInst( &data, pent )
	, mLight(0)
	, mLightData(data)
{
	ork::lev2::LightData* lightdata = data.GetLightData();

	ork::lev2::DirectionalLightData* dirlight_data = ork::rtti::autocast(lightdata);
	ork::lev2::PointLightData* pntlight_data = ork::rtti::autocast(lightdata);
	ork::lev2::SpotLightData* sptlight_data = ork::rtti::autocast(lightdata);
	ork::lev2::AmbientLightData* amblight_data = ork::rtti::autocast(lightdata);

	if( amblight_data )
	{
		const ork::CMatrix4& mtx = GetEntity()->GetEntData().GetDagNode().GetTransformNode().GetTransform()->GetMatrix();
		mLight = new ork::lev2::AmbientLight(mtx,amblight_data);
	}
	if( dirlight_data )
	{
		const ork::CMatrix4& mtx = GetEntity()->GetEntData().GetDagNode().GetTransformNode().GetTransform()->GetMatrix();
		mLight = new ork::lev2::DirectionalLight(mtx,dirlight_data);
	}
	if( pntlight_data )
	{
		const ork::CMatrix4& mtx = GetEntity()->GetEntData().GetDagNode().GetTransformNode().GetTransform()->GetMatrix();
		mLight = new ork::lev2::PointLight(mtx,pntlight_data);
	}
	if( sptlight_data )
	{
		const ork::CMatrix4& mtx = GetEntity()->GetEntData().GetDagNode().GetTransformNode().GetTransform()->GetMatrix();
		mLight = new ork::lev2::SpotLight(mtx,sptlight_data);
	}

	struct yo
	{
		//
		ork::lev2::XgmModelAsset*	mpModel;
		ork::ent::Entity*			mpEntity;
		ork::lev2::Light*			mpLight;

		static void draw_tricircle(	ork::lev2::RenderContextInstData& rcid,
									ork::lev2::GfxTarget* targ,
									const ork::lev2::CallbackRenderable* pren,
									ork::lev2::PointLight* ppntlight )
		{
# if defined(INTOOL)
			const yo* pyo = pren->GetUserData0().Get<const yo*>();

			const ork::lev2::Renderer *prenderer = rcid.GetRenderer();

			const ork::TransformNode3D* xf = & pyo->mpEntity->GetEntData().GetDagNode().GetTransformNode();
			ork::CMatrix4 mtxw = xf->GetTransform()->GetMatrix();

			if( false == targ->FBI()->IsPickState() )
			{
				ork::lev2::SphereRenderable sphrend;
				sphrend.SetColor( ork::CColor4::White() );
				sphrend.SetPosition( mtxw.GetTranslation() );
				sphrend.SetRadius( ppntlight->GetRadius() );
				prenderer->RenderSphere( sphrend );
			}

			ork::CColor4 ModColor = pren->GetModColor();
			ork::CColor4 ObjColor;
			ObjColor.SetRGBAU32( reinterpret_cast<U32>( pren->GetObject() ) );

			ork::CColor4 color = targ->FBI()->IsPickState() 
									? ObjColor
									: ModColor*ppntlight->GetColor();

			mtxw.SetScale(0.25f);
			targ->PushModColor( color );
			targ->MTXI()->PushMMatrix(mtxw);
			ork::lev2::CGfxPrimitives::GetRef().RenderDiamond( targ );
			targ->MTXI()->PopMMatrix();
			targ->PopModColor();
#endif
		}

		static void draw_dirlight(	ork::lev2::RenderContextInstData& rcid,
									ork::lev2::GfxTarget* targ,
									const ork::lev2::CallbackRenderable* pren  )
		{
# if defined(INTOOL)
			const yo* pyo = pren->GetUserData0().Get<const yo*>();
			ork::lev2::XgmModel* pmodel = (pyo->mpModel==0) ? 0 : pyo->mpModel->GetModel();
			if( pmodel )
			{
				const ork::TransformNode3D* xf = & pyo->mpEntity->GetEntData().GetDagNode().GetTransformNode();
				//const ork::lev2::RenderContextFrameData* fdata = renderer->GetTarget()->GetRenderContextFrameData();
				int inummeshes = pmodel->GetNumMeshes();
				for( int imesh=0; imesh<inummeshes; imesh++ )
				{	const ork::lev2::XgmMesh& mesh = * pmodel->GetMesh(imesh);
					int inumclusset = mesh.GetNumSubMeshes();
					for( int ics=0; ics<inumclusset; ics++ )
					{	const ork::lev2::XgmSubMesh& submesh = * mesh.GetSubMesh(ics);
						const ork::lev2::GfxMaterial* material = submesh.mpMaterial;
						int inumclus = submesh.miNumClusters;
						for( int ic=0; ic<inumclus; ic++ )
						{	bool btest = true;
							const ork::lev2::XgmCluster& cluster = submesh.RefCluster(ic);

							ork::CColor4 ModColor = pren->GetModColor();
	
							if( pyo->mpLight )
							{
								ModColor *= pyo->mpLight->GetColor();
							}
							ork::CColor4 ObjColor;
							ObjColor.SetRGBAU32( reinterpret_cast<U32>( pren->GetObject() ) );

							ork::CColor4 color = targ->FBI()->IsPickState() 
													? ObjColor
													: ModColor;

							ork::CMatrix4 mtxw = xf->GetTransform()->GetMatrix();
							ork::lev2::RenderContextInstModelData MdlCtx;

							MdlCtx.mMesh = & mesh;
							MdlCtx.mSubMesh = & submesh;
							MdlCtx.mCluster = & cluster;

							MdlCtx.SetSkinned(false);

							ork::lev2::RenderContextInstData ovrcid = rcid;
							ovrcid.SetLightingGroup( 0 );
							ovrcid.SetMaterialIndex(0);

							pmodel->RenderRigid( color, mtxw, targ, ovrcid, MdlCtx );
						}
					}
				}
			}
#endif
		}
		static void doit(	ork::lev2::RenderContextInstData& rcid,
							ork::lev2::GfxTarget* targ,
							const ork::lev2::CallbackRenderable* pren )
		{
			const yo* pyo = pren->GetUserData0().Get<const yo*>();
			
			if( pyo->mpLight )
			{
				ork::lev2::PointLight* ppntlight = rtti::autocast(pyo->mpLight);
				if( ppntlight )
				{
					draw_tricircle( rcid, targ, pren, ppntlight );
				}
				else
				{
					draw_dirlight( rcid, targ, pren );
				}
			}
			else
			{
				draw_dirlight( rcid, targ, pren );
			}
		}
		yo()
		{
			mpModel = ork::asset::AssetManager<ork::lev2::XgmModelAsset>::Load( "data://editor/dirlight" );
		}
	};

	yo* pyo = new yo;
	pyo->mpEntity = pent;
	pyo->mpLight = GetLight();

	ork::ent::CallbackDrawable* pdrw = new ork::ent::CallbackDrawable(pent);
	pent->AddDrawable( AddPooledLiteral("Default"),pdrw );
	pdrw->SetCallback( yo::doit );
	pdrw->SetOwner( & pent->GetEntData() );
	pdrw->SetData( (const yo*) pyo );

	/////////////////////////////////////////////////

	/////////////////////////////////////////////////

}

bool LightingComponentInst::DoLink(ork::ent::SceneInst *psi)
{
	if( GetLight() )
	{
		switch( GetLight()->LightType() )
		{
			case ork::lev2::ELIGHTTYPE_DIRECTIONAL:
			case ork::lev2::ELIGHTTYPE_AMBIENT:
			{
				bool bisdyn = mLightData.IsDynamic();
				GetLight()->mbIsDynamic = bisdyn;
				ork::ent::LightingManagerComponentInst* lmi = psi->FindTypedSceneComponent<ent::LightingManagerComponentInst>();

				if( lmi )
				{
					ork::lev2::LightManager& lightmanager = lmi->GetLightManager();
					
					if( bisdyn )
					{
						//orkprintf( "AddLight<%d:%08x>\n", lightmanager.mGlobalMovingLights.mPrioritizedLights.size(), GetLight() );
						lightmanager.mGlobalMovingLights.AddLight(GetLight());
					}
					else
					{
						lightmanager.mGlobalStationaryLights.AddLight(GetLight());
					}
				}
				break;
			}
		}								
	}

	return true;
}

LightingComponentInst::~LightingComponentInst()
{
	ork::ent::LightingManagerComponentInst* lmi = GetEntity()->GetSceneInst()->FindTypedSceneComponent<ent::LightingManagerComponentInst>();

	if( lmi )
	{
		ork::lev2::LightManager& lightmanager = lmi->GetLightManager();

		ork::lev2::LightContainer& global_container = lightmanager.mGlobalMovingLights;

		if( mLight )
		{
			global_container.RemoveLight( mLight );
		}

	}
	if( mLight ) delete mLight;
}

void LightingComponentInst::DoUpdate(ork::ent::SceneInst *inst)
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightingManagerComponentData::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

LightingManagerComponentData::LightingManagerComponentData()
{
}

///////////////////////////////////////////////////////////////////////////////

ork::ent::SceneComponentInst* LightingManagerComponentData::CreateComponentInst(ork::ent::SceneInst *pinst) const
{
	return new LightingManagerComponentInst( *this, pinst );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void LightingManagerComponentInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

LightingManagerComponentInst::LightingManagerComponentInst( const LightingManagerComponentData& data, ork::ent::SceneInst *pinst )
	: ork::ent::SceneComponentInst( &data, pinst )
	, mLightManager(data.Lmd())
{
}

///////////////////////////////////////////////////////////////////////////////

void LightArchetype::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

LightArchetype::LightArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void LightArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ork::ent::LightingComponentData>();
}
void LightingComponentData::DoRegisterWithScene( ork::ent::SceneComposer& sc )
{
	sc.Register<ork::ent::LightingManagerComponentData>();
}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
template const ork::ent::LightingComponentData* ork::ent::EntData::GetTypedComponent<ork::ent::LightingComponentData>() const;
template ork::ent::LightingComponentInst* ork::ent::Entity::GetTypedComponent<ork::ent::LightingComponentInst>(bool);

