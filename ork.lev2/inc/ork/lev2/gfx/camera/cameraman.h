////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/kernel/core/singleton.h>
#include <ork/gfx/camera.h>
#include <ork/math/units.h>
#include <ork/lev2/ui/ui.h>
#include <ork/util/hotkey.h>

namespace ork { namespace lev2 {

#define DEF_EYEZ	-500.0f
#define DEF_DEPTH	1000.0f

///////////////////////////////////////////////////////////////////////////////

class CManipHandler
{
	public: //

	CMatrix4		IMVPMat;	
	CQuaternion		Quat;
	CVector3		Origin;
	CVector3		RayNear;
	CVector3		RayFar;
	CVector3		RayNormal;

	CVector3		XNormal;
	CVector3		YNormal;
	CVector3		ZNormal;
	
	CPlane			XYPlane;
	CPlane			XZPlane;
	CPlane			YZPlane;

	CVector3		XYIntersect;
	CVector3		XZIntersect;
	CVector3		YZIntersect;
	CVector3		XYIntersectBase;
	CVector3		XZIntersectBase;
	CVector3		YZIntersectBase;

	CReal			XYDistance;
	CReal			XZDistance;
	CReal			YZDistance;
	
	bool			DoesIntersectXY;
	bool			DoesIntersectXZ;
	bool			DoesIntersectYZ;

	CReal			XZAngle;
	CReal			YZAngle;
	CReal			XYAngle;
	
	CReal			XZAngleBase;
	CReal			YZAngleBase;
	CReal			XYAngleBase;

	///////////////////////////////////////////////////////////////////////////////
	// 

	CVector3		CamXNormal;
	CVector3		CamYNormal;
	CVector3		CamZNormal;
	Frustum			mFrustum;
    CVector3		camrayN, camrayF;

	///////////////////////////////////////////////////////////////////////////////

	CManipHandler();

	void Init( const ork::CVector2& posubp, const CMatrix4 & RCurIMVPMat, const CQuaternion & RCurQuat );
	bool IntersectXZ( const ork::CVector2& posubp, CVector3 &Intersection, CReal &Angle );
	bool IntersectYZ( const ork::CVector2& posubp, CVector3 &Intersection, CReal &Angle );
	bool IntersectXY( const ork::CVector2& posubp, CVector3 &Intersection, CReal &Angle );
	void Intersect( const ork::CVector2& posubp );
	void GenerateIntersectionRays( const ork::CVector2& posubp, CVector3& RayZNormal, CVector3& RayNear );

	///////////////////////////////////////////////////////////////////////////////
};

class CCamera : public ork::Object
{
	RttiDeclareAbstract( CCamera, ork::Object );

protected:

	std::string type_name;
	std::string instance_name;
	std::string other_info;

public:

/////////////////////////////////////////////////////////////////////
	CCamera();
/////////////////////////////////////////////////////////////////////

	CCameraData	mCameraData;
	
	CReal	mfWorldSizeAtLocator;

	CMatrix4 mCamRot;

	CQuaternion QuatC, QuatL, QuatCPushed;

	CMatrix4 lookatmatrix;
	CMatrix4 eyematrixROT;
	CMatrix4 clipmatrix;
	
    CVector3 camrayN, camrayF;
	CVector3 CamFocus, CamFocusZNormal;
	CVector4 CamFocusYNormal;
	CVector4 CamSet;
	CVector3 CamLoc;
	CVector3 PrevCamLoc;
	CVector4 MeasuredCameraVelocity;
	CVector4 LastMeasuredCameraVelocity;

    CVector4 vec_billboardUp;
    CVector4 vec_billboardRight;

	float		mfLoc;

	CVector3	mvCenter;

	CReal locscale;

	ui::Viewport *mpViewport;
	
	CManipHandler ManipHandler;

	bool		mbInMotion;

	//////////////////////////////////////////////////////////////////////////////

	bool IsXVertical() const;
	bool IsYVertical() const;
	bool IsZVertical() const;

	CQuaternion VerticalRot( CReal amt ) const;
	CQuaternion HorizontalRot( CReal amt ) const;

	//////////////////////////////////////////////////////////////////////////////

	CCameraData& GetCameraData() { return mCameraData; }
	const CCameraData& GetCameraData() const { return mCameraData; }

	//////////////////////////////////////////////////////////////////////////////

	virtual bool UIEventHandler( const ui::Event& EV ) = 0;
	virtual void draw(GfxTarget *pT) = 0;
	
	virtual void RenderUpdate( void ) = 0;

	virtual void SetFromWorldSpaceMatrix(const CMatrix4 &) = 0;
	
    virtual CReal ViewLengthToWorldLength( const CVector4 &pos, CReal ViewLength ) = 0;
	virtual void GenerateDepthRay( const CVector2& pos2D, CVector3 &rayN, CVector3 &rayF,  const CMatrix4 &IMat  ) const = 0;

	std::string get_full_name();

	void CommonPostSetup( void );

	void AttachViewport( ui::Viewport* pVP ) { mpViewport = pVP; }
	ui::Viewport* GetViewport( void ) const { return mpViewport; }

	void SetName( const std::string& Name ) { instance_name=Name; }
	const std::string& GetName() const { return instance_name; }

	bool CheckMotion();
};

///////////////////////////////////////////////////////////////////////////////

struct CamEvTrackData
{
    int ipushX;
    int ipushY;
    int icurX;
    int icurY;
    
    CVector3 vPushCenter;

};

///////////////////////////////////////////////////////////////////////////////

class CCamera_persp : public CCamera
{
	RttiDeclareConcrete( CCamera_persp, CCamera );

	public: //

	enum erotmode
	{
		EROT_SCREENXY = 0,
		EROT_SCREENZ,
	};

	erotmode meRotMode;

    CamEvTrackData mEvTrackData;

	CReal aper;
	CReal tx, ty, tz;
	CReal player_rx, player_ry, player_rz;
	CReal move_vel; 
	float far_max;
	float near_min;

	CReal curquat[4];
	CReal lastquat[4];

	CMatrix4 mRot, mTrans;
	
	CVector4 mMoveVelocity;

	CVector4 CamBaseLoc;

	HotKey	mHK_In, mHK_Out;
	HotKey	mHK_ReAlign, mHK_Origin;
	HotKey	mHK_Pik2Foc, mHK_Foc2Pik;

	HotKey	mHK_RotL, mHK_RotR, mHK_RotU, mHK_RotD, mHK_RotZ;
	HotKey	mHK_MovL, mHK_MovR, mHK_MovU, mHK_MovD, mHK_MovF, mHK_MovB;
	HotKey	mHK_AperMinus, mHK_AperPlus;

	HotKey	mHK_MouseRot, mHK_MouseDolly;

	int beginx, beginy;
	int leftbutton, middlebutton, rightbutton;

	bool mDoRotate;
	bool mDoDolly;
	bool mDoPan;

	bool UIEventHandler( const ui::Event& EV ) override;
	void draw( GfxTarget *pT ) override;

	void RenderUpdate( void ); // virtual
	void SetFromWorldSpaceMatrix(const CMatrix4 &matrix); // virtual
	
    CReal ViewLengthToWorldLength( const CVector4 &pos, CReal ViewLength ); // virtual
	void GenerateDepthRay( const CVector2& pos2D, CVector3 &rayN, CVector3 &rayF,  const CMatrix4 &IMat ) const; // virtual

    void DrawBillBoardQuad( CVector4 &inpt, CReal size );
	
	void UpdateMatrices( void );

    void PanBegin( const CamEvTrackData& ed );
    void PanUpdate( const CamEvTrackData& ed );
    void PanEnd();

    void RotBegin( const CamEvTrackData& ed );
    void RotUpdate( const CamEvTrackData& ed );
    void RotEnd();

    void DollyBegin( const CamEvTrackData& ed );
    void DollyUpdate( const CamEvTrackData& ed );
    void DollyEnd();

    CCamera_persp();

};

///////////////////////////////////////////////////////////////////////////////

} }

///////////////////////////////////////////////////////////////////////////////
