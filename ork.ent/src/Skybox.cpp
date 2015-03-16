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
#include <pkg/ent/event/MeshEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/gfx/camera.h>
///////////////////////////////////////////////////////////////////////////////
#include "Skybox.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SkyBoxArchetype, "SkyBoxArchetype" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SkyBoxControllerInst, "SkyBoxControllerInst" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SkyBoxControllerData, "SkyBoxControllerData" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void SkyBoxArchetype::Describe()
{
}

SkyBoxArchetype::SkyBoxArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

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
			bool IsPickState = rcid.GetRenderer()->GetTarget()->FBI()->IsPickState();
			float fphase = ssci->GetPhase();
			
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
				// picking support
				///////////////////////////////////////////////////////////

				CColor4 ObjColor;
				//toz64 ObjColor.SetRGBAU32( reinterpret_cast<U32>( (u32)((size_t)pren->GetObject() )) );

				CColor4 color = targ->FBI()->IsPickState() ? ObjColor : pren->GetModColor();

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
				CMatrix4 mtxSPIN;
				mtxSPIN.RotateY(fphase);
				CMatrix4 mtxSKY;
				mtxSKY.SetScale( fscale );
				mtxSKY.SetTranslation( pos );
				
				mtxSKY = mtxSPIN*mtxSKY;
				
				MatCtx.ForceNoZWrite( true );

			//	printf( "DrawSkyBox pos<%f %f %f>\n", pos.GetX(), pos.GetY(), pos.GetX() );

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

							//printf( "DrawSkyBox clus<%d>\n", ic );

							cd.GetModel()->RenderRigid(	color, 
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

	#if 0 //DRAWTHREADS
	CallbackDrawable* pdrw = new CallbackDrawable(pent);
	pent->AddDrawable( AddPooledLiteral("Default"), pdrw );
	pdrw->SetCallback( yo::doit );
	pdrw->SetBufferCallback( yo::BufferCB );
	pdrw->SetOwner(  & pent->GetEntData() );
	pdrw->SetSortKey(0);

	yo* pyo = new yo;
	pyo->parch = this;
	pyo->pent = pent;

	anyp ap;
	ap.Set<const yo*>( pyo );
	pdrw->SetData( ap );
#endif
	
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

///////////////////////////////////////////////////////////////////////////////

SkyBoxControllerData::SkyBoxControllerData()
	: mfSpinRate( 0.0f )
	, mModelAsset(0)
	, mfScale(1.0f)
{
}

///////////////////////////////////////////////////////////////////////////////

lev2::XgmModel* SkyBoxControllerData::GetModel() const
{
	lev2::XgmModel* pmodel = (mModelAsset!=0) ? mModelAsset->GetModel() : 0;
	return pmodel;
}

///////////////////////////////////////////////////////////////////////////////

void SkyBoxControllerInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

SkyBoxControllerInst::SkyBoxControllerInst( const SkyBoxControllerData& data, ent::Entity* pent )
	: ork::ent::ComponentInst( & data, pent )
	, mCD( data )
	, mPhase( 0.0f )
{
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* SkyBoxControllerData::CreateComponent(ent::Entity* pent) const
{
	return OrkNew SkyBoxControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void SkyBoxControllerInst::DoUpdate(ent::SceneInst* sinst)
{
	mPhase += mCD.GetSpinRate()*sinst->GetDeltaTime();
}


///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}}
