////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _3D_MANIP_H
#define _3D_MANIP_H

#include <ork/kernel/core/singleton.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/math/TransformNode.h>
#include <ork/lev2/gfx/camera/cameraman.h>
#include <ork/rtti/RTTI.h>
#include <ork/lev2/gfx/util/grid.h>
#include <ork/object/AutoConnector.h>

namespace ork { namespace lev2
{

class CManipManager;

////////////////////////////////////////////////////////////////////////////////

struct IntersectionRecord
{
	CVector3	mIntersectionPoint;
	CVector3	mOldIntersectionPoint;
	CVector3	mBaseIntersectionPoint;
    bool		mbHasItersected;

	IntersectionRecord();

	CVector4 GetLocalSpaceDelta( const CMatrix4 &InvLocalMatrix );
};

enum EPlaneRec
{
	EPLANE_XZ = 0,
	EPLANE_YZ,
	EPLANE_XY,
	EPLANE_END,
};


class CManip : public ork::Object
{
	DECLARE_TRANSPARENT_CUSTOM_POLICY_RTTI(CManip, ork::Object, ork::rtti::AbstractPolicy)

public:

	CMatrix4				InvMatrix;
	TransformNode3D			mBaseTransform;

	IntersectionRecord		mIntersection[EPLANE_END];
	IntersectionRecord*		mActiveIntersection;

	CPlane					mPlaneXZ;
	CPlane					mPlaneYZ;
	CPlane					mPlaneXY;

	CManipManager&			mManager;

	CManip( CManipManager& mgr );

	virtual void Draw( GfxTarget *pTARG ) const = 0;
	virtual bool UIEventHandler( const ui::Event& EV ) = 0;

	CVector3 IntersectWithPlanes(const ork::CVector2& posubp);
	void SelectBestPlane(const ork::CVector2& posubp);
	void CalcPlanes();

	bool CheckIntersect( void ) const;

	CColor4 GetColor() const { return mColor; }

protected:

	CColor4		mColor;
};

class CManipTrans : public CManip
{
	RttiDeclareAbstract(CManipTrans,CManip);

public:

    CManipTrans(CManipManager& mgr);

	virtual void Draw( GfxTarget *pTARG ) const = 0;
	bool UIEventHandler( const ui::Event& EV );

protected:

	virtual void HandleMouseDown(const ork::CVector2& pos);
	virtual void HandleMouseUp(const ork::CVector2& pos);
	virtual void HandleDrag(const ork::CVector2& pos);
};

class CManipSingleTrans : public CManipTrans
{
	RttiDeclareAbstract(CManipSingleTrans, CManipTrans);

public:

	CManipSingleTrans(CManipManager& mgr);

	virtual void Draw(GfxTarget* pTARG) const;
	virtual void DrawAxis(GfxTarget* pTARG) const;

protected:

	virtual void HandleDrag(const ork::CVector2& pos);

	virtual ork::CVector3 GetNormal() const = 0;

	CMatrix4			mmRotModel;
};

class CManipDualTrans : public CManipTrans
{
	RttiDeclareAbstract(CManipDualTrans, CManipTrans);

public:

	CManipDualTrans(CManipManager& mgr);

	virtual void Draw(GfxTarget* pTARG ) const;

protected:

	virtual void GetQuad(float ext, ork::CVector4& v0, ork::CVector4& v1,
		ork::CVector4& v2, ork::CVector4& v3) const = 0;

	virtual void HandleDrag(const ork::CVector2& pos);
};

////////////////////////////////////////////////////////////////////////////////

class CManipTX : public CManipSingleTrans
{
	RttiDeclareAbstract(CManipTX, CManipSingleTrans);

public:

    CManipTX(CManipManager& mgr);

protected:

	virtual ork::CVector3 GetNormal() const { return ork::CVector3::UnitX(); };
};

////////////////////////////////////////////////////////////////////////////////

class CManipTY : public CManipSingleTrans
{
	RttiDeclareAbstract(CManipTY, CManipSingleTrans);

public:

    CManipTY(CManipManager& mgr);

protected:

	virtual ork::CVector3 GetNormal() const { return ork::CVector3::UnitY(); };
};

////////////////////////////////////////////////////////////////////////////////

class CManipTZ : public CManipSingleTrans
{
	RttiDeclareAbstract(CManipTZ, CManipSingleTrans);

public:

    CManipTZ(CManipManager& mgr);

protected:

	virtual ork::CVector3 GetNormal() const { return ork::CVector3::UnitZ(); };
};

class CManipTXY : public CManipDualTrans
{
	RttiDeclareAbstract(CManipTXY, CManipDualTrans);

public:

	CManipTXY(CManipManager& mgr);

protected:

	virtual void GetQuad(float ext, ork::CVector4& v0, ork::CVector4& v1,
		ork::CVector4& v2, ork::CVector4& v3) const;
};

class CManipTXZ : public CManipDualTrans
{
	RttiDeclareAbstract(CManipTXZ, CManipDualTrans);

public:

	CManipTXZ(CManipManager& mgr);

protected:

	virtual void GetQuad(float ext, ork::CVector4& v0, ork::CVector4& v1,
		ork::CVector4& v2, ork::CVector4& v3) const;
};

class CManipTYZ : public CManipDualTrans
{
	RttiDeclareAbstract(CManipTYZ, CManipDualTrans);

public:

	CManipTYZ(CManipManager& mgr);

protected:

	virtual void GetQuad(float ext, ork::CVector4& v0, ork::CVector4& v1,
		ork::CVector4& v2, ork::CVector4& v3) const;
};

////////////////////////////////////////////////////////////////////////////////

class CManipRot : public CManip
{
	RttiDeclareAbstract(CManipRot, CManip);

public: //

    CManipRot( CManipManager& mgr, const CVector4 &LocRotMat );

	virtual void Draw( GfxTarget *pTARG ) const;
	virtual bool UIEventHandler( const ui::Event& EV );

	virtual F32 CalcAngle( CVector4 & inv_isect, CVector4 & inv_lisect ) const = 0;

	////////////////////////////////////////

	CMatrix4			mmRotModel;
	const CVector4		mLocalRotationAxis;

};

////////////////////////////////////////////////////////////////////////////////

class CManipRX : public CManipRot
{
	RttiDeclareAbstract(CManipRX,CManipRot);

public: //

    CManipRX(CManipManager& mgr);

	virtual F32 CalcAngle( CVector4 & inv_isect, CVector4 & inv_lisect ) const;

};

////////////////////////////////////////////////////////////////////////////////

class CManipRY : public CManipRot
{
	RttiDeclareAbstract(CManipRY,CManipRot);

public: //

    CManipRY(CManipManager& mgr);

	virtual F32 CalcAngle( CVector4 & inv_isect, CVector4 & inv_lisect ) const;

};

////////////////////////////////////////////////////////////////////////////////

class CManipRZ : public CManipRot
{
	RttiDeclareAbstract(CManipRZ,CManipRot);

	public: //

    CManipRZ(CManipManager& mgr);

	virtual F32 CalcAngle( CVector4 & inv_isect, CVector4 & inv_lisect ) const;

};

////////////////////////////////////////////////////////////////////////////////

enum EManipEnable
{
	EMANIPMODE_OFF = 0,
	EMANIPMODE_ON ,
};


class GfxMaterialManip : public GfxMaterial
{
	CManipManager& mManager;

	public:

	GfxMaterialManip(GfxTarget*,CManipManager&mgr);
	virtual ~GfxMaterialManip(){};
	virtual void Init( GfxTarget *pTarg );

	virtual int  BeginBlock( GfxTarget* pTarg,const RenderContextInstData &MatCtx );
	virtual void EndBlock( GfxTarget* pTarg );
	virtual void Update( void ) {}
	virtual bool BeginPass( GfxTarget* pTarg,int iPass=0 );
	virtual void EndPass( GfxTarget* pTarg );
	virtual void UpdateMVPMatrix( GfxTarget *pTARG );

	protected:

	FxShader*		hModFX;
	const FxShaderTechnique*	hTekStd;
	const FxShaderTechnique*	hTekLuc;
	const FxShaderTechnique*	hTekPick;

	//const FxShaderTechnique*	hTekCurrent;

	const FxShaderParam*	hMVP;
	const FxShaderParam*	hTEX;
	const FxShaderParam*	hCOLOR;

	CVector4		mColor;

	bool			mbNoDepthTest;

};

////////////////////////////////////////////////////////////////////////////////

class CManipManager : public ork::AutoConnector
{
	RttiDeclareAbstract(CManipManager,ork::AutoConnector);

	////////////////////////////////////////////////////////////
	DeclarePublicAutoSlot( ObjectDeSelected );
	DeclarePublicAutoSlot( ObjectSelected );
	DeclarePublicAutoSlot( ObjectDeleted );
	DeclarePublicAutoSlot( ClearSelection );
	////////////////////////////////////////////////////////////

public:

	////////////////////////////////////////////////////////////
	void SlotObjectDeSelected( ork::Object* pOBJ );
	void SlotObjectSelected( ork::Object* pOBJ );
	void SlotObjectDeleted( ork::Object* pOBJ );
	void SlotClearSelection();
	////////////////////////////////////////////////////////////

	friend class CManipRX;
	friend class CManipRY;
	friend class CManipRZ;
	friend class CManipTX;
	friend class CManipTY;
	friend class CManipTZ;
	friend class CManipTXY;
	friend class CManipTXZ;
	friend class CManipTYZ;
	friend class CManip;
	friend class CManipRot;
	friend class CManipTrans;
	friend class CManipSingleTrans;
	friend class CManipDualTrans;

	//////////////////////////////////

	enum EManipMode
	{
		EMANIPMODE_WORLD_TRANS = 0,
		EMANIPMODE_LOCAL_ROTATE ,
	};

	enum EUIMode
	{
		EUIMODE_STD = 0,
		EUIMODE_PLACE ,
		EUIMODE_MANIP_WORLD_TRANSLATE,
		EUIMODE_MANIP_LOCAL_TRANSLATE,
		EUIMODE_MANIP_LOCAL_ROTATE,
	};

	EUIMode						meUIMode;

	//////////////////////////////////

	CManipManager();
	static void ClassInit();
	void ManipObjects( void ) { mbDoComponents = false; }
	void ManipComponents( void ) { mbDoComponents = true; }

	//////////////////////////////////

	void SetManipMode( EManipMode emode ) { meManipMode=emode ; }
	EManipMode GetManipMode( void ) { return meManipMode; }
	bool IsVisible() { return meUIMode != EUIMODE_STD; }
	EUIMode GetUIMode( void ) const { return meUIMode; }
	void SetUIMode( EUIMode emode ) { meUIMode=emode; }

	void AttachObject( ork::Object *pObject );
	void ReleaseObject( void );
	void DetachObject( void );

	void Setup( Renderer* prend );
	void Queue( Renderer* prend );

	void DrawManip(CManip* manip, GfxTarget* pTARG);
	void DrawCurrentManipSet(GfxTarget* pTARG);

	void SetHover(CManip* manip) { mpHoverManip = manip; }
	CManip* GetHover() { return mpHoverManip; }

	void SetDualAxis(bool dual) { mDualAxis = dual; }
	bool IsDualAxis() { return mDualAxis; }

	void ApplyTransform( const TransformNode3D &SetMat );
	const TransformNode3D& GetCurTransform() { return mCurTransform; }

	void EnableManip( CManip *pOBJ );
	void DisableManip( void );

	bool UIEventHandler( const ui::Event& EV );

	CCamera*			GetActiveCamera( void ) const { return mpActiveCamera; }
	void				SetActiveCamera( CCamera*pCam ) { mpActiveCamera=pCam; }

	f32					GetManipScale( void  )  const { return mfManipScale; }

	GfxMaterial *		GetMaterial( void ) { return mpManipMaterial; }

	void				CalcObjectScale( void );

	void				SetWorldTrans( bool bv ) { mbWorldTrans=bv; }
	void				SetGridSnap( bool bv ) { mbGridSnap=bv; }

	void				RebaseMatrices( void );

	Grid3d&				Grid() { return mGrid; }

	void				SetViewScale( float fvs ) { mfViewScale=fvs; }
	float				CalcViewScale( float fW, float fH, const CCameraData *camdat ) const;
		
	void				SetDrawMode(int imode) { miDrawMode=imode; }
	int					GetDrawMode() const { return miDrawMode; }
	
private:

	bool				mbWorldTrans;
	bool				mbGridSnap;

	bool				mDualAxis;

	TransformNode3D		mParentTransform;

	GfxMaterialManip*	mpManipMaterial;
	CManip*				mpTXManip;
	CManip*				mpTYManip;
	CManip*				mpTZManip;
	CManip*				mpTXYManip;
	CManip*				mpTXZManip;
	CManip*				mpTYZManip;
	CManip*				mpRXManip;
	CManip*				mpRYManip;
	CManip*				mpRZManip;
	CManip*				mpCurrentManip;
	CManip*				mpHoverManip;
	EManipMode			meManipMode;
	EManipEnable		meManipEnable;
	float				mfViewScale;
	int					miDrawMode;

	CManipHandler		mManipHandler;
	CCamera*			mpActiveCamera;

	CVector4			mPickCenter;
	CVector4			mPickAccum;

	bool				mbDoComponents;

	f32					mfManipScale;

	//CReal				mfGridSnap;
	CReal				mfBaseManipSize;

	IManipInterface*	mpCurrentInterface;
	Object*				mpCurrentObject;

	TransformNode3D		mOldTransform;
	TransformNode3D		mCurTransform;

	CReal				mObjScale;
	CReal				mObjInvScale;

	Grid3d				mGrid;
};

} }

#endif
