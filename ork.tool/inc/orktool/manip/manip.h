////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

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
	fvec3	mIntersectionPoint;
	fvec3	mOldIntersectionPoint;
	fvec3	mBaseIntersectionPoint;
    bool		mbHasItersected;

	IntersectionRecord();

	fvec4 GetLocalSpaceDelta( const fmtx4 &InvLocalMatrix );
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

	fmtx4				InvMatrix;
	TransformNode			mBaseTransform;

	IntersectionRecord		mIntersection[EPLANE_END];
	IntersectionRecord*		mActiveIntersection;

	fplane3					mPlaneXZ;
	fplane3					mPlaneYZ;
	fplane3					mPlaneXY;

	CManipManager&			mManager;

	CManip( CManipManager& mgr );

	virtual void Draw( GfxTarget *pTARG ) const = 0;
	virtual bool UIEventHandler( const ui::Event& EV ) = 0;

	fvec3 IntersectWithPlanes(const ork::fvec2& posubp);
	void SelectBestPlane(const ork::fvec2& posubp);
	void CalcPlanes();

	bool CheckIntersect( void ) const;

	fcolor4 GetColor() const { return mColor; }

protected:

	fcolor4		mColor;
};

class CManipTrans : public CManip
{
	RttiDeclareAbstract(CManipTrans,CManip);

public:

    CManipTrans(CManipManager& mgr);

	bool UIEventHandler( const ui::Event& EV ) final;

protected:

	virtual void HandleMouseDown(const ork::fvec2& pos);
	virtual void HandleMouseUp(const ork::fvec2& pos);
	virtual void HandleDrag(const ork::fvec2& pos);
};

class CManipSingleTrans : public CManipTrans
{
	RttiDeclareAbstract(CManipSingleTrans, CManipTrans);

public:

	CManipSingleTrans(CManipManager& mgr);

	virtual void DrawAxis(GfxTarget* pTARG) const;

protected:

    void Draw(GfxTarget* pTARG) const final;
	void HandleDrag(const ork::fvec2& pos) final;

	virtual ork::fvec3 GetNormal() const = 0;

	fmtx4			mmRotModel;
};

class CManipDualTrans : public CManipTrans
{
	RttiDeclareAbstract(CManipDualTrans, CManipTrans);

public:

	CManipDualTrans(CManipManager& mgr);

	void Draw(GfxTarget* pTARG ) const final;

protected:

	virtual void GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1,
		ork::fvec4& v2, ork::fvec4& v3) const = 0;

	void HandleDrag(const ork::fvec2& pos) final;
};

////////////////////////////////////////////////////////////////////////////////

class CManipTX : public CManipSingleTrans
{
	RttiDeclareAbstract(CManipTX, CManipSingleTrans);

public:

    CManipTX(CManipManager& mgr);

protected:

	virtual ork::fvec3 GetNormal() const final { return ork::fvec3::UnitX(); };
};

////////////////////////////////////////////////////////////////////////////////

class CManipTY : public CManipSingleTrans
{
	RttiDeclareAbstract(CManipTY, CManipSingleTrans);

public:

    CManipTY(CManipManager& mgr);

protected:

	virtual ork::fvec3 GetNormal() const final { return ork::fvec3::UnitY(); };
};

////////////////////////////////////////////////////////////////////////////////

class CManipTZ : public CManipSingleTrans
{
	RttiDeclareAbstract(CManipTZ, CManipSingleTrans);

public:

    CManipTZ(CManipManager& mgr);

protected:

	virtual ork::fvec3 GetNormal() const final { return ork::fvec3::UnitZ(); };
};

class CManipTXY : public CManipDualTrans
{
	RttiDeclareAbstract(CManipTXY, CManipDualTrans);

public:

	CManipTXY(CManipManager& mgr);

protected:

	void GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1,
		ork::fvec4& v2, ork::fvec4& v3) const final;
};

class CManipTXZ : public CManipDualTrans
{
	RttiDeclareAbstract(CManipTXZ, CManipDualTrans);

public:

	CManipTXZ(CManipManager& mgr);

protected:

	void GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1,
		ork::fvec4& v2, ork::fvec4& v3) const final;
};

class CManipTYZ : public CManipDualTrans
{
	RttiDeclareAbstract(CManipTYZ, CManipDualTrans);

public:

	CManipTYZ(CManipManager& mgr);

protected:

	void GetQuad(float ext, ork::fvec4& v0, ork::fvec4& v1,
		ork::fvec4& v2, ork::fvec4& v3) const final;
};

////////////////////////////////////////////////////////////////////////////////

class CManipRot : public CManip
{
	RttiDeclareAbstract(CManipRot, CManip);

public: //

    CManipRot( CManipManager& mgr, const fvec4 &LocRotMat );

	void Draw( GfxTarget *pTARG ) const final;
	bool UIEventHandler( const ui::Event& EV ) final;

	virtual F32 CalcAngle( fvec4 & inv_isect, fvec4 & inv_lisect ) const = 0;

	////////////////////////////////////////

	fmtx4			mmRotModel;
	const fvec4		mLocalRotationAxis;

};

////////////////////////////////////////////////////////////////////////////////

class CManipRX : public CManipRot
{
	RttiDeclareAbstract(CManipRX,CManipRot);

public: //

    CManipRX(CManipManager& mgr);

	F32 CalcAngle( fvec4 & inv_isect, fvec4 & inv_lisect ) const final;

};

////////////////////////////////////////////////////////////////////////////////

class CManipRY : public CManipRot
{
	RttiDeclareAbstract(CManipRY,CManipRot);

public: //

    CManipRY(CManipManager& mgr);

	F32 CalcAngle( fvec4 & inv_isect, fvec4 & inv_lisect ) const final;

};

////////////////////////////////////////////////////////////////////////////////

class CManipRZ : public CManipRot
{
	RttiDeclareAbstract(CManipRZ,CManipRot);

	public: //

    CManipRZ(CManipManager& mgr);

	F32 CalcAngle( fvec4 & inv_isect, fvec4 & inv_lisect ) const final;

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
	~GfxMaterialManip() final {};
	void Init( GfxTarget *pTarg ) final;

	int  BeginBlock( GfxTarget* pTarg,const RenderContextInstData &MatCtx ) final;
	void EndBlock( GfxTarget* pTarg ) final;
	void Update( void ) final {}
	bool BeginPass( GfxTarget* pTarg,int iPass=0 ) final;
	void EndPass( GfxTarget* pTarg ) final;
	void UpdateMVPMatrix( GfxTarget *pTARG ) final;

	protected:

	FxShader*		hModFX;
	const FxShaderTechnique*	hTekStd;
	const FxShaderTechnique*	hTekLuc;
	const FxShaderTechnique*	hTekPick;

	//const FxShaderTechnique*	hTekCurrent;

	const FxShaderParam*	hMVP;
	const FxShaderParam*	hTEX;
	const FxShaderParam*	hCOLOR;

	fvec4		mColor;

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

	void ApplyTransform( const TransformNode &SetMat );
	const TransformNode& GetCurTransform() { return mCurTransform; }

	void EnableManip( CManip *pOBJ );
	void DisableManip( void );

	bool UIEventHandler( const ui::Event& EV );

	CCamera*			getActiveCamera( void ) const { return mpActiveCamera; }
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

	TransformNode		mParentTransform;

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

	fvec4			mPickCenter;
	fvec4			mPickAccum;

	bool				mbDoComponents;

	f32					mfManipScale;

	//float				mfGridSnap;
	float				mfBaseManipSize;

	IManipInterface*	mpCurrentInterface;
	Object*				mpCurrentObject;

	TransformNode		mOldTransform;
	TransformNode		mCurTransform;

	float				mObjScale;
	float				mObjInvScale;

	Grid3d				mGrid;
};

} }

