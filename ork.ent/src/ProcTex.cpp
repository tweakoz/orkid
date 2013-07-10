////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/proctex/proctex.h>
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
#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////
#include "ProcTex.h"
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ProcTexArchetype, "ProcTexArchetype" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ProcTexControllerInst, "ProcTexControllerInst" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::ProcTexControllerData, "ProcTexControllerData" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void ProcTexArchetype::Describe()
{
}

ProcTexArchetype::ProcTexArchetype()
{
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexArchetype::DoLinkEntity( SceneInst* psi, Entity *pent ) const
{
	struct yo
	{
		const ProcTexArchetype* parch;
		Entity *pent;
		mutable float mPrevTime;
		mutable lev2::GfxMaterial3DSolid* mtl;

		yo() : parch(nullptr), pent(nullptr), mPrevTime(-1.0f), mtl(nullptr) {}

		static void doit( lev2::RenderContextInstData& rcid, lev2::GfxTarget* targ, const lev2::CallbackRenderable* pren )
		{
			bool IsPickState = rcid.GetRenderer()->GetTarget()->FBI()->IsPickState();
			if( IsPickState ) return;

			const yo* pyo = pren->GetUserData0().Get<const yo*>();

			const ProcTexArchetype* parch = pyo->parch;
			auto pent = pyo->pent;
			auto psi = pent->GetSceneInst();
			ProcTexControllerInst* ssci = pent->GetTypedComponent<ProcTexControllerInst>();
			const ProcTexControllerData&	cd = ssci->GetCD();

			float fcurtime = psi->GetGameTime();
			proctex::ProcTex& templ = cd.GetTemplate();
			
			if( std::abs(pyo->mPrevTime-fcurtime)>0.1f )
			{
				ssci->mContext.mdflowctx.Clear();
				ssci->mContext.mCurrentTime = fcurtime;
				templ.compute(ssci->mContext);
				printf( "fcurtime <%f>\n", fcurtime );
				pyo->mPrevTime = fcurtime;
			}
			ork::lev2::Texture* ptx = templ.ResultTexture();

			lev2::VtxWriter<lev2::SVtxV12C4T16> vw;

			vw.Lock( targ, & lev2::GfxEnv::GetSharedDynamicVB(), 6 );
			{
				float fZ = 0.0f;
				u32 ucolor = 0xffffffff;
				f32 x0 = -4.0f;
				f32 y0 = -4.0f;
				f32 x1 = 4.0f;
				f32 y1 = 4.0f;
				ork::CVector2 uv0( 0.0f,1.0f );
				ork::CVector2 uv1( 1.0f,1.0f );
				ork::CVector2 uv2( 1.0f,0.0f );
				ork::CVector2 uv3( 0.0f,0.0f );
				ork::CVector3 vv0( x0,y0,fZ );
				ork::CVector3 vv1( x1,y0,fZ );
				ork::CVector3 vv2( x1,y1,fZ );
				ork::CVector3 vv3( x0,y1,fZ );
				
				lev2::SVtxV12C4T16 v0( vv0, uv0, ucolor );
				lev2::SVtxV12C4T16 v1( vv1, uv1, ucolor );
				lev2::SVtxV12C4T16 v2( vv2, uv2, ucolor );
				lev2::SVtxV12C4T16 v3( vv3, uv3, ucolor );
				
				vw.AddVertex( v0 );
				vw.AddVertex( v1 );
				vw.AddVertex( v2 );

				vw.AddVertex( v2 );
				vw.AddVertex( v3 );
				vw.AddVertex( v0 );
			}
			vw.UnLock( targ );

			if( 0 == pyo->mtl )
				pyo->mtl = new lev2::GfxMaterial3DSolid(targ);

			pyo->mtl->SetTexture(ptx);
			pyo->mtl->SetColorMode(lev2::GfxMaterial3DSolid::EMODE_TEX_COLOR);

			targ->BindMaterial( pyo->mtl );

			const CMatrix4& mtx = pren->GetMatrix();

			targ->MTXI()->PushMMatrix(mtx);
			targ->GBI()->DrawPrimitive(vw,lev2::EPRIM_TRIANGLES,6);
			targ->MTXI()->PopMMatrix();
			targ->BindMaterial( nullptr );

		}
		static void BufferCB(ork::ent::DrawableBufItem&cdb)
		{

		}
	};

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

}

///////////////////////////////////////////////////////////////////////////////

void ProcTexArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ProcTexControllerData>();
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerData::Describe()
{
	ork::reflect::RegisterProperty("Template", &ProcTexControllerData::TemplateAccessor);
	ork::ent::RegisterFamily<ProcTexControllerData>(ork::AddPooledLiteral("animate"));

/*	ork::ent::RegisterFamily<SkyBoxControllerData>(ork::AddPooledLiteral("control"));

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
*/
}

///////////////////////////////////////////////////////////////////////////////

ProcTexControllerData::ProcTexControllerData()
{
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

ProcTexControllerInst::ProcTexControllerInst( const ProcTexControllerData& data, ent::Entity* pent )
	: ork::ent::ComponentInst( & data, pent )
	, mCD( data )
{
	mTimer.Start();
}

///////////////////////////////////////////////////////////////////////////////

ent::ComponentInst* ProcTexControllerData::CreateComponent(ent::Entity* pent) const
{
	return OrkNew ProcTexControllerInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////

void ProcTexControllerInst::DoUpdate(ent::SceneInst* sinst)
{

//	mPhase += mCD.GetSpinRate()*sinst->GetDeltaTime();
}


///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
}}

