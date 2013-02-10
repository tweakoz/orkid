////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _GFX_APP_MODELVP_H
#define _GFX_APP_MODELVP_H

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class GfxBuffer;

class CModelVP : public CUIViewport
{
	public: //
		
	CCamera*			m_pCamera;
	S64					lastClockCycle;
	static const int	iAvgFrames = 120;
	F32					fFPS[iAvgFrames];
	int					iFrame;
	CVector4			RayN, RayF;
	CVector4			RayFrusN[4];
	CVector4			RayFrusF[4];
	CUIEvent			UIEventCached;
	U32					muPickObj;
	CObject*			pCurrentOBJ;
	static vector< CObject* >	PickObjects;
	CUITabs*			mpOverlay;

	CMatrix4				mmLightPMat;
	CMatrix4				mmLightVMat;
	CMatrix4				mmLightMMat;

	/////////////////////////////////
	// secondary rendering buffers

	GfxBuffer*			mpSkyBoxBuffer;
	GfxMaterial*		mpRTEXMat;

	/////////////////////////////////
	// State Machine

	static CFSMState CModelVPMainState;
	static CFSMState CModelVPDrawPickState;
	static CFSMState CModelVPGetPickState;

	#if( _BUILD_LEVEL > _CONSOLE_BUILD_LEVEL )

	CFSM			mFSM;

	void EnterMain(void) {}
	void ProcessMain(void) {}
	void LeaveMain(void) {}
	void EnterDrawPick(void);
	void ProcessDrawPick(void) {}
	void LeaveDrawPick(void);
	void EnterGetPick(void) {}
	void LeaveGetPick(void)	{}
	void ProcessGetPick(void);
	#else
	#endif

	/////////////////////////////////

	CModelVP( string name, int x, int y, int w, int h );
	virtual EUIHandled UIEventHandler( CUIEvent *pEvent );
	virtual void draw( void );
	virtual void resize(void);

	//////////////////////////////////////////////////////
	#if( _BUILD_LEVEL > _CONSOLE_BUILD_LEVEL )
	static void PickCallback( CObject*ObjPtr )
	{
		PickObjects.push_back( ObjPtr );
	}

	U32 GetPickColor( CVector4 ScreenCoord )
	{
		CColor PickColor = GfxEnv::GetRef().GetCT()->GetRGBA( ScreenCoord );
		U32 uPickColor =	((U32)(PickColor.GetX()*255.0f));
			uPickColor |=	((U32)(PickColor.GetY()*255.0f)<<8);
			uPickColor |=	((U32)(PickColor.GetZ()*255.0f)<<16);
			uPickColor |=	((U32)(PickColor.GetW()*255.0f)<<24);

            uPickColor = 0;
		return uPickColor;
	}
	#endif
    /////////////////////////////////

	CUITabs* GetOverlay( void ) { return mpOverlay; }

};

}

#endif
