////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/bullet.h>
#include <btBulletDynamicsCommon.h>

#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>

#include <pkg/ent/drawable.h>
#include <pkg/ent/scene.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <BulletCollision/CollisionShapes/btConvexHullShape.h>
#include <ork/gfx/camera.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/kernel/opq.h>

using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

PhysicsDebugger::PhysicsDebugger() 
	: mClearOnBeginInternalTick(true)
	, mbDEBUG(false)
	, mMutex("bulletdebuggerlines")
{

}

void PhysicsDebugger::Lock()
{
	mMutex.Lock();
}

void PhysicsDebugger::UnLock()
{
	mMutex.UnLock();
}

BulletDebugDrawDBData::BulletDebugDrawDBData(BulletSystem* psi, ork::ent::Entity* pent )
	: mpEntity( pent )
	, mpBWCI( psi )
	, mpDebugger(0)
{
	for( int i=0; i<ork::ent::DrawableBuffer::kmaxbuffers; i++ )
	{
		mDBRecs[i].mpEntity = pent;
		mDBRecs[i].mpBWCI = psi;
	}
}

///////////////////////////////////////////////////////////////////////////////

void BulletDebugQueueToLayerCallback(ork::ent::DrawableBufItem&cdb)
{
	AssertOnOpQ2( UpdateSerialOpQ() );

	BulletDebugDrawDBData* pdata = cdb.mUserData0.Get<BulletDebugDrawDBData*>();

	if( pdata->mpDebugger->IsDebugEnabled() )
	{
		BulletSystem* pinst = pdata->mpBWCI;

		BulletDebugDrawDBRec* prec = pdata->mDBRecs+cdb.miBufferIndex;
		cdb.mUserData1.Set( prec );

		if( pinst )
		{
			pinst->BulletWorld()->debugDrawWorld();
			pdata->mpDebugger->Lock();
			prec->mLines1 = pinst->Debugger().GetLines1();
			prec->mLines2 = pinst->Debugger().GetLines2();
			pdata->mpDebugger->UnLock();
			pinst->Debugger().RenderClear();

		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void BulletDebugRenderCallback(	ork::lev2::RenderContextInstData& rcid,
							ork::lev2::GfxTarget* targ,
							const ork::lev2::CallbackRenderable* pren )
{
	AssertOnOpQ2( MainThreadOpQ() );

	//////////////////////////////////////////
	BulletDebugDrawDBData* pyo = pren->GetDrawableDataA().Get<BulletDebugDrawDBData*>();

	if( pyo->mpDebugger->IsDebugEnabled() && pren->GetUserData1().IsA<BulletDebugDrawDBRec*>() )
	{
		BulletDebugDrawDBRec* srec = pren->GetUserData1().Get<BulletDebugDrawDBRec*>();
		//////////////////////////////////////////
		if( false == targ->FBI()->IsPickState() )
		{
			//////////////////////////////////////////
			const ork::ent::Entity* pent = pyo->mpEntity;
			const BulletSystem* pbwci = pyo->mpBWCI;
			if( 0 == pbwci ) return;
			//////////////////////////////////////////
			const ork::lev2::RenderContextFrameData* framedata = targ->GetRenderContextFrameData();
			const ork::CCameraData* cdata = framedata->GetCameraData();

			pyo->mpDebugger->Lock();
			pyo->mpDebugger->Render(rcid, targ, srec->mLines1);	
			pyo->mpDebugger->Render(rcid, targ, srec->mLines2);
			pyo->mpDebugger->UnLock();
			//	RenderClear();

		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void PhysicsDebugger::Render( ork::lev2::RenderContextInstData& rcid, ork::lev2::GfxTarget* ptarg )
{
//	Render(rcid, ptarg, mClearOnBeginInternalTickLines);	
//	Render(rcid, ptarg, mClearOnRenderLines);
//	RenderClear();
}

void PhysicsDebugger::Render(ork::lev2::RenderContextInstData &rcid, ork::lev2::GfxTarget *ptarg,
							 const orkvector<PhysicsDebuggerLine> &lines)
{
	int inumlines = lines.size();

	const ork::lev2::Renderer* prenderer = rcid.GetRenderer();

	const ork::CCameraData* pcamdata = ptarg->GetRenderContextFrameData()->GetCameraData();

	ork::CVector3 szn = 0;

	DynamicVertexBuffer<SVtxV12C4T16>& vb = GfxEnv::GetSharedDynamicVB();

	//int ibase = vb.GetNum();
	int icount = inumlines*2;

	if( icount )
	{
		VtxWriter<SVtxV12C4T16> vwriter;
		vwriter.Lock( ptarg, &vb, icount );
		//ptarg->GBI()->LockVB( vwriter, icount );
		for( int il=0; il<inumlines; il++ )
		{
			const PhysicsDebuggerLine& line = lines[il];

			U32 ucolor = line.mColor.GetBGRAU32(); // ptarg->CColor4ToU32(line.mColor);

			CVector3 vf = line.mFrom+szn;
			CVector3 vt = line.mTo+szn;

			vwriter.AddVertex( ork::lev2::SVtxV12C4T16( vf.GetX(), vf.GetY(), vf.GetZ(), 0.0f, 0.0f, ucolor ) );
			vwriter.AddVertex( ork::lev2::SVtxV12C4T16( vt.GetX(), vt.GetY(), vt.GetZ(), 0.0f, 0.0f, ucolor ) );
		}
		vwriter.UnLock(ptarg);


		auto cam_z = pcamdata->GetZNormal();

		static GfxMaterial3DSolid material( ptarg );
		material.mRasterState.SetZWriteMask( true );
		material.SetColorMode( ork::lev2::GfxMaterial3DSolid::EMODE_VERTEX_COLOR );
		ptarg->BindMaterial( & material );
		ptarg->PushModColor( CVector4::White() );
		ptarg->FXI()->InvalidateStateBlock();
		ork::CMatrix4 mtx_dbg;
		mtx_dbg.SetTranslation( cam_z*-1.3f );

		ptarg->MTXI()->PushMMatrix(mtx_dbg);
		ptarg->GBI()->DrawPrimitive( vwriter, ork::lev2::EPRIM_LINES );
		ptarg->MTXI()->PopMMatrix();
		ptarg->PopModColor();
		ptarg->BindMaterial( 0 );
	}
}

void PhysicsDebugger::AddLine(const ork::CVector3 &from, const ork::CVector3 &to, const ork::CVector3 &color)
{
	if(mClearOnBeginInternalTick)
		mClearOnBeginInternalTickLines.push_back(PhysicsDebuggerLine(from, to, color));
	else
		mClearOnRenderLines.push_back(PhysicsDebuggerLine(from, to, color));
}

void PhysicsDebugger::drawLine(const btVector3& from,const btVector3& to,const btVector3& color)
{
	CVector3 vfrom = !from;
	CVector3 vto = !to;
	CVector3 vclr = !color * (1.0f/256.0f);

	AddLine(vfrom, vto, vclr);
}

void PhysicsDebugger::drawContactPoint(const btVector3& PointOnB,const btVector3& normalOnB,btScalar distance,int lifeTime,const btVector3& color)
{
	bool clearOnBeginInternalTick = mClearOnBeginInternalTick;
	mClearOnBeginInternalTick = false;

	CVector3 vfrom = !PointOnB;
	CVector3 vdir = !normalOnB;
	CVector3 vto = vfrom + vdir * 4.0F;//distance;
	CVector3 vclr = !color;

	AddLine(vfrom, vto, vclr);

	mClearOnBeginInternalTick = clearOnBeginInternalTick;
}

void PhysicsDebugger::reportErrorWarning(const char* warningString)
{
}

void PhysicsDebugger::draw3dText(const btVector3& location,const char* textString)
{
}

void PhysicsDebugger::setDebugMode(int debugMode)
{
}

int  PhysicsDebugger::getDebugMode() const
{
	return (false==mbDEBUG)
		? 0
		: btIDebugDraw::DBG_DrawContactPoints
		| btIDebugDraw::DBG_DrawWireframe
		| btIDebugDraw::DBG_DrawAabb;
		//| btIDebugDraw::DBG_DrawAabb;
	//return btIDebugDraw::DBG_DrawContactPoints|btIDebugDraw::DBG_DrawWireframe|btIDebugDraw::DBG_DrawAabb;
}

} }
