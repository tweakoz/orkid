////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <orktool/manip/manip.h>
#include <ork/lev2/input/input.h>
#include <ork/math/audiomath.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/gfx/lev2renderer.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/gfx/pickbuffer.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManip,"CManip");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTrans,"CManipTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipSingleTrans, "CManipSingleTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipDualTrans, "CManipDualTrans");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipRot,"CManipRot");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTX,"CManipTX");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTY,"CManipTY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTZ,"CManipTZ");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTXY,"CManipTXY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTXZ,"CManipTXZ");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipTYZ,"CManipTYZ");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipRX,"CManipRX");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipRY,"CManipRY");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipRZ,"CManipRZ");

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::CManipManager,"CManipManager");

////////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
////////////////////////////////////////////////////////////////////////////////

void CManip::Describe(){}

void CManipTrans::Describe(){}
void CManipSingleTrans::Describe(){}
void CManipDualTrans::Describe(){}
void CManipRot::Describe(){}

void CManipTX::Describe(){}
void CManipTY::Describe(){}
void CManipTZ::Describe(){}
void CManipTXY::Describe(){}
void CManipTXZ::Describe(){}
void CManipTYZ::Describe(){}

void CManipRX::Describe(){}
void CManipRY::Describe(){}
void CManipRZ::Describe(){}

////////////////////////////////////////////////////////////////////////////////

void CManipManager::Describe()
{
	RegisterAutoSlot( CManipManager, ObjectDeleted );
	RegisterAutoSlot( CManipManager, ObjectSelected );
	RegisterAutoSlot( CManipManager, ObjectDeSelected );
	RegisterAutoSlot( CManipManager, ClearSelection );
}

CManipManager::CManipManager()
	: mpTXManip(0)
	, mpTYManip(0)
	, mpTZManip(0)
	, mpTXYManip(0)
	, mpTXZManip(0)
	, mpTYZManip(0)
	, mpRXManip(0)
	, mpRYManip(0)
	, mpRZManip(0)
	, mpCurrentManip(0)
	, mpHoverManip(0)
	, meManipMode( EMANIPMODE_WORLD_TRANS )
	, meManipEnable( EMANIPMODE_OFF )
	, mbDoComponents(false)
	, mfManipScale( 1.0f )
	, mfBaseManipSize( 100.0f )
	, mpCurrentInterface( 0 )
	, mpCurrentObject( 0 )
	, mbWorldTrans( false )
	, mbGridSnap( false )
	, mpManipMaterial( 0 )
	, meUIMode( EUIMODE_STD )
	, mDualAxis(false)
	, mfViewScale(1.0f)
	, ConstructAutoSlot(ObjectDeSelected)
	, ConstructAutoSlot(ObjectSelected)
	, ConstructAutoSlot(ObjectDeleted)
	, ConstructAutoSlot(ClearSelection)
{
	SetupSignalsAndSlots();
}

////////////////////////////////////////////////////////////////////////////////

void CManipManager::SlotObjectDeleted( ork::Object* pOBJ )
{
	if( mpCurrentObject == pOBJ )
	{
		DetachObject();
	}
}
void CManipManager::SlotObjectSelected( ork::Object* pOBJ )
{
}
void CManipManager::SlotObjectDeSelected( ork::Object* pOBJ )
{
}
void CManipManager::SlotClearSelection()
{
}

////////////////////////////////////////////////////////////////////////////////

bool CManipManager::UIEventHandler( const ui::Event& EV )
{
	bool rval = false;

	switch(EV.miEventCode)
	{
		case ui::UIEV_KEY:
		case ui::UIEV_KEYUP:
		{
			if(EV.mbSHIFT)
				mDualAxis = true;
			else
				mDualAxis = false;
		}
		break;
	}

	//printf( "CManipManager::UIEventHandler mpCurrentManip<%p>\n", mpCurrentManip );

	if(mpCurrentManip)
		rval = mpCurrentManip->UIEventHandler( EV );

	return rval;
}

////////////////////////////////////////////////////////////////////////////////

IntersectionRecord::IntersectionRecord()
	: mIntersectionPoint(0.0f,0.0f,0.0f)
	, mOldIntersectionPoint(0.0f,0.0f,0.0f)
	, mBaseIntersectionPoint(0.0f,0.0f,0.0f)
	, mbHasItersected(false)
{

}

CVector4 IntersectionRecord::GetLocalSpaceDelta( const CMatrix4 &InvLocalMatrix )
{
	return mIntersectionPoint.Transform(InvLocalMatrix)-mOldIntersectionPoint.Transform(InvLocalMatrix);
}


////////////////////////////////////////////////////////////////////////////////

CManip::CManip( CManipManager& mgr )
	: mManager( mgr )
	, mActiveIntersection( 0 )
	, mColor()
{
}

////////////////////////////////////////////////////////////////////////////////

bool CManip::CheckIntersect( void ) const
{	
	bool bisect = (mActiveIntersection==nullptr)?false:mActiveIntersection->mbHasItersected;
	//printf( "manip<%p> ai<%p> CheckIntersect<%d>\n", this, mActiveIntersection, int(bisect) );

	return bisect;
}

////////////////////////////////////////////////////////////////////////////////

void CManip::CalcPlanes()
{
	CMatrix4 CurMatrix;
	mBaseTransform.GetMatrix(CurMatrix);
	CVector4 origin = CurMatrix.GetTranslation();
	CVector4 normalX = CurMatrix.GetXNormal();
	CVector4 normalY = CurMatrix.GetYNormal();
	CVector4 normalZ = CurMatrix.GetZNormal();
	/////////////////////////////
	if(		mManager.mbWorldTrans
		&&	(CManipManager::EMANIPMODE_WORLD_TRANS==mManager.GetManipMode()) )
	{
		normalX = CVector4( 1.0f, 0.0f, 0.0f );
		normalY = CVector4( 0.0f, 1.0f, 0.0f );
		normalZ = CVector4( 0.0f, 0.0f, 1.0f );
	}
	/////////////////////////////
	mPlaneXZ.CalcFromNormalAndOrigin( normalY, origin );
	mPlaneYZ.CalcFromNormalAndOrigin( normalX, origin );
	mPlaneXY.CalcFromNormalAndOrigin( normalZ, origin );

}

////////////////////////////////////////////////////////////////////////////////

CVector3 CManip::IntersectWithPlanes(const ork::CVector2& posubp)
{
	CVector3 rval;
	
	const CCamera* cam = mManager.GetActiveCamera();
	CMatrix4 CurMatrix;
	mBaseTransform.GetMatrix(CurMatrix);
	/////////////////////////////
	CVector3 rayN, rayF;
	cam->GenerateDepthRay( posubp, rayN, rayF, InvMatrix );
	CVector4 rayDir = (rayF-rayN).Normal();
	Ray3 ray;
	ray.mOrigin = rayN;
	ray.mDirection = rayDir;
	/////////////////////////////
	CReal dist;
	mIntersection[EPLANE_XZ].mbHasItersected = mPlaneXZ.Intersect( ray, dist );
	CVector4 rayOutXZ = (rayDir*dist);
	mIntersection[EPLANE_YZ].mbHasItersected = mPlaneYZ.Intersect( ray, dist );
	CVector4 rayOutYZ = (rayDir*dist);
	mIntersection[EPLANE_XY].mbHasItersected = mPlaneXY.Intersect( ray, dist );
	CVector4 rayOutXY = (rayDir*dist);
	/////////////////////////////
	mIntersection[EPLANE_XZ].mIntersectionPoint = rayN+rayOutXZ;
	mIntersection[EPLANE_YZ].mIntersectionPoint = rayN+rayOutYZ;
	mIntersection[EPLANE_XY].mIntersectionPoint = rayN+rayOutXY;
	/////////////////////////////
	rval = rayDir.GetXYZ();
	return rval;
}

////////////////////////////////////////////////////////////////////////////////

void CManip::SelectBestPlane(const ork::CVector2& posubp)
{
	bool brotmode = (mManager.GetManipMode() == CManipManager::EMANIPMODE_LOCAL_ROTATE);

	CalcPlanes();
	CVector3 RayDir = IntersectWithPlanes(posubp);
	/////////////////////////////
	mIntersection[EPLANE_XZ].mOldIntersectionPoint = mIntersection[EPLANE_XZ].mIntersectionPoint;
	mIntersection[EPLANE_YZ].mOldIntersectionPoint = mIntersection[EPLANE_YZ].mIntersectionPoint;
	mIntersection[EPLANE_XY].mOldIntersectionPoint = mIntersection[EPLANE_XY].mIntersectionPoint;
	mIntersection[EPLANE_XZ].mBaseIntersectionPoint = mIntersection[EPLANE_XZ].mIntersectionPoint;
	mIntersection[EPLANE_YZ].mBaseIntersectionPoint = mIntersection[EPLANE_YZ].mIntersectionPoint;
	mIntersection[EPLANE_XY].mBaseIntersectionPoint = mIntersection[EPLANE_XY].mIntersectionPoint;
	/////////////////////////////
	// rot manips use explicit planes
	/////////////////////////////
	if( brotmode )
	{
		if( mManager.mpCurrentManip->GetClass() == CManipRX::GetClassStatic() )
		{
			mActiveIntersection = & mIntersection[EPLANE_YZ];
		}
		else if( mManager.mpCurrentManip->GetClass() == CManipRY::GetClassStatic() )
		{
			mActiveIntersection = & mIntersection[EPLANE_XZ];
		}
		else if( mManager.mpCurrentManip->GetClass() == CManipRZ::GetClassStatic() )
		{
			mActiveIntersection = & mIntersection[EPLANE_XY];
		}
	}
	else
	{
		CReal dotxz = RayDir.Dot(mPlaneXZ.GetNormal());
		CReal dotxy = RayDir.Dot(mPlaneXY.GetNormal());
		CReal dotyz = RayDir.Dot(mPlaneYZ.GetNormal());
		CReal adotxz = CFloat::Abs(dotxz);
		CReal adotxy = CFloat::Abs(dotxy);
		CReal adotyz = CFloat::Abs(dotyz);

		//printf( "mManager.mpCurrentManip<%p>\n", mManager.mpCurrentManip );

		if( mManager.mpCurrentManip->GetClass() == CManipTX::GetClassStatic() )
		{
			mActiveIntersection = (adotxy>adotxz) 
								? & mIntersection[EPLANE_XZ]
								: & mIntersection[EPLANE_XY];
		}
		else if( mManager.mpCurrentManip->GetClass() == CManipTY::GetClassStatic() )
		{
			mActiveIntersection = (adotxy>adotyz) 
								? & mIntersection[EPLANE_XY]
								: & mIntersection[EPLANE_YZ];
		}
		else if( mManager.mpCurrentManip->GetClass() == CManipTZ::GetClassStatic() )
		{
			mActiveIntersection = (adotxz>adotyz) 
								? & mIntersection[EPLANE_XZ]
								: & mIntersection[EPLANE_YZ];
		}
	}
	/////////////////////////////
	if( mActiveIntersection==&mIntersection[EPLANE_XZ] )
	{
		printf( "Manip<%s> using XZ plane\n", GetClass()->Name().c_str() );
	}
	else if( mActiveIntersection==&mIntersection[EPLANE_XY] )
	{
		printf( "Manip<%s> using XY plane\n", GetClass()->Name().c_str() );
	}
	else if( mActiveIntersection==&mIntersection[EPLANE_YZ] )
	{
		printf( "Manip<%s> using YZ plane\n", GetClass()->Name().c_str() );
	}
	/////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::RebaseMatrices( void )
{
	if( mpCurrentInterface && mpCurrentObject )
	{
		TransformNode3D Mat = mpCurrentInterface->GetTransform(mpCurrentObject);
		TransformNode3D MatT = mpCurrentInterface->GetTransform(mpCurrentObject);
		mCurTransform	= Mat;
		mOldTransform	= Mat;
		CalcObjectScale();
	}
}

///////////////////////////////////////////////////////////////////////////////

bool bPushIgnore = false;

void CManipManager::AttachObject( ork::Object *pobj )
{
	object::ObjectClass* pclass = rtti::safe_downcast<object::ObjectClass*>(pobj->GetClass());
	//CClass *pClass = pOBJ->GetClass();

	if( mpCurrentObject && pobj != mpCurrentObject )
	{
		delete mpCurrentInterface;
		mpCurrentInterface = 0;
	}

	any16 anno = pclass->Description().GetClassAnnotation( "editor.3dxfinterface" );

	if( anno.IsSet() )
	{
		ConstString classname = anno.Get<ConstString>();
		const rtti::Class *pifclass = rtti::autocast(rtti::Class::FindClass(classname));

		if( pifclass )
		{
			mpCurrentInterface = rtti::autocast(pifclass->CreateObject());
			mpCurrentObject = pobj;
			RebaseMatrices();
		}
		else
		{
			mpCurrentInterface = 0;
			mpCurrentObject = 0;
		}
	}
}

void CManipManager::DetachObject()
{
	mpCurrentInterface = 0;
	mpCurrentObject = 0;
	meManipEnable = EMANIPMODE_OFF;
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::CalcObjectScale( void )
{
	CVector3 pos;
	CQuaternion rot;
	CReal scale;
	CMatrix4 ScaleMat;
	mCurTransform.GetMatrix(ScaleMat);
	ScaleMat.DecomposeMatrix( pos, rot, scale );

	mObjScale = scale;
	mObjInvScale = CReal(1.0f)/mObjScale;
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::ReleaseObject( void )
{
	if( bPushIgnore )
	{
		bPushIgnore = false;
	}
	if( mpCurrentInterface && mpCurrentObject && (EMANIPMODE_ON==meManipEnable) )
	{
		mpCurrentInterface->Detach(mpCurrentObject);
	}
	meManipEnable = EMANIPMODE_OFF;
}

///////////////////////////////////////////////////////////////////////////////

float CManipManager::CalcViewScale( float fW, float fH, const CCameraData *camdat ) const
{
	CMatrix4 MatW;
	mCurTransform.GetMatrix(MatW);

	//////////////////////////////////////////////////////////////
	// Calc World Scale of manip (maintain constant size)

	CVector2 VP( fW, fH );

	CVector3 Pos = MatW.GetTranslation();
	CVector3 UpVector;
	CVector3 RightVector;
	camdat->GetPixelLengthVectors( Pos, VP, UpVector, RightVector );

	float rscale = RightVector.Mag();

	//printf( "manip rscale<%f>\n", rscale );

	//////////////////////////////////////////////////////////////

	return rscale;
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::Setup( ork::lev2::Renderer* prend )
{
	GfxTarget* pTARG = prend->GetTarget();

	if( mpCurrentInterface )
	{
		bool isshift = false; //CSystem::IsKeyDepressed(VK_SHIFT);
		if( isshift )
		{
			mCurTransform = mpCurrentInterface->GetTransform(mpCurrentObject);
		}
	}

	CMatrix4 MatW;
	mCurTransform.GetMatrix(MatW);

	const CVector4 & ScreenXNorm = pTARG->MTXI()->GetScreenRightNormal();

	const CVector4 V0 = MatW.GetTranslation();
	const CVector4 V1 = V0 + ScreenXNorm*CReal(30.0f);

	mfManipScale = CReal(mfBaseManipSize)*mfViewScale;

	//////////////////////////////////////////////////////////////

	if( 0 == mpManipMaterial )
	{
		mpManipMaterial = new GfxMaterialManip(pTARG,*this);
		mpManipMaterial->mRasterState.SetDepthTest( EDEPTHTEST_OFF );

		if( mpTXManip == 0 ) mpTXManip = new CManipTX(*this);
		if( mpTYManip == 0 ) mpTYManip = new CManipTY(*this);
		if( mpTZManip == 0 ) mpTZManip = new CManipTZ(*this);
		if( mpTXYManip == 0) mpTXYManip = new CManipTXY(*this);
		if( mpTXZManip == 0) mpTXZManip = new CManipTXZ(*this);
		if( mpTYZManip == 0) mpTYZManip = new CManipTYZ(*this);
		if( mpRXManip == 0 ) mpRXManip = new CManipRX(*this);
		if( mpRYManip == 0 ) mpRYManip = new CManipRY(*this);
		if( mpRZManip == 0 ) mpRZManip = new CManipRZ(*this);
	}

}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::DrawManip(CManip* pmanip, GfxTarget* pTARG )
{	
	if(!pmanip)
		return;
    
    lev2::PickBufferBase* pickBuf = pTARG->FBI()->GetCurrentPickBuffer();
	U32 pickID = pickBuf ? pickBuf->AssignPickId((ork::Object*)pmanip) : 0;
	CColor4 col = pmanip->GetColor();

	pTARG->SetCurrentObject(pmanip);
	//orkprintf( "MANIP<%p>\n", pmanip );

	if(pTARG->FBI()->IsPickState())
	{
		pTARG->PushModColor(ork::CColor4(pickID));
		pmanip->Draw( pTARG );
		pTARG->PopModColor();
	}
	else
	{
		CColor4 outcolor = (GetHover() == pmanip) ? CColor4::Yellow() : col;
		outcolor.SetW( 0.6f );

		pTARG->PushModColor(outcolor);
		pmanip->Draw( pTARG );
		pTARG->PopModColor();
	}
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::DrawCurrentManipSet(GfxTarget* pTARG)
{
	switch(meManipMode)
	{
	case EMANIPMODE_WORLD_TRANS:
		{
			if(mDualAxis)
			{
				// sm - dual axis disabled atm
				DrawManip(mpTXYManip, pTARG);
				DrawManip(mpTXZManip, pTARG);
				DrawManip(mpTYZManip, pTARG);
			}
			else
			{
				DrawManip(mpTXManip, pTARG);
				DrawManip(mpTYManip, pTARG);
				DrawManip(mpTZManip, pTARG);
			}
		}
		break;

	case EMANIPMODE_LOCAL_ROTATE:
		{
			DrawManip(mpRXManip, pTARG);
			DrawManip(mpRYManip, pTARG);
			DrawManip(mpRZManip, pTARG);
		}
		break;
	}
}


////////////////////////////////////////////////////////////////////////////////

static void ManipRenderCallback( ork::lev2::RenderContextInstData& rcid, ork::lev2::GfxTarget* targ, const ork::lev2::CallbackRenderable* pren )
{
	CManipManager* pmanipman = pren->GetUserData0().Get<CManipManager*>();
	pmanipman->SetDrawMode(0);
	pmanipman->DrawCurrentManipSet( targ );
}

void CManipManager::Queue(ork::lev2::Renderer* prend)
{
	if( mpCurrentInterface && mpCurrentObject )
	{
		anyp ap; ap.Set<CManipManager*>( this );

		CallbackRenderable& rable = prend->QueueCallback();
		rable.SetUserData0( ap );
		rable.SetSortKey(0x7fffffff);
		rable.SetCallback( ManipRenderCallback );
	}
	
}
///////////////////////////////////////////////////////////////////////////////

void CManipManager::ApplyTransform( const TransformNode3D &SetMat )
{
	mCurTransform = SetMat;

	if( (0!=mpCurrentInterface) && (0!=mpCurrentObject) )
	{
		mpCurrentInterface->SetTransform( mpCurrentObject, SetMat );
	}
}

///////////////////////////////////////////////////////////////////////////////

void CManipManager::DisableManip( void )
{
	if(EMANIPMODE_ON == meManipEnable)
		ReleaseObject();

	orkprintf( "Disable Manip\n" );
	mpCurrentManip = 0;
	meManipEnable = EMANIPMODE_OFF;
}

void CManipManager::EnableManip( CManip *pObj )
{
	orkprintf( "Enable Manip\n" );
	mpCurrentManip = pObj;
	meManipEnable = EMANIPMODE_ON;

	RebaseMatrices();
}



}

} // end namespace ork
