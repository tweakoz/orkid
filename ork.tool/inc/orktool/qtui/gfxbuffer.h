////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORKTOOL_GFXBUFFER_H
#define _ORKTOOL_GFXBUFFER_H

///////////////////////////////////////////////////////////////////////////
#include <orktool/orktool_pch.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/lev2renderer.h>
#include <ork/lev2/gfx/renderable.h>
#include <ork/lev2/ui/ui.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/lev2/qtui/qtui.h>
///////////////////////////////////////////////////////////////////////////
#include <ork/dataflow/dataflow.h>
#include <ork/lev2/gfx/builtin_frameeffects.h>

namespace ork {
namespace tool {
/*
///////////////////////////////////////////////////////////////////////////

class PickBufferBase : public ork::lev2::GfxBuffer
{
	RttiDeclareAbstract(PickBufferBase,ork::lev2::GfxBuffer);

	public:

	enum EPickBufferType
	{
		EPICK_FACE_VTX = 0,
		EPICK_NORMAL ,
		EPICK_WPOS ,
		EPICK_ST
	};

	PickBufferBase( GfxBuffer *parent,
					 int iX, int iY, int iW, int iH,
					 EPickBufferType etyp );

	virtual void Draw( void ) = 0;

	ork::lev2::GfxMaterialUITextured *mpUIMaterial;


	EPickBufferType	meType;
	bool			mbInitTex;
    uint32_t        AssignPickId(ork::Object*);
    ork::Object*    GetObjectFromPickId(uint32_t);

    std::vector<ork::Object*> mPickIds;

};

///////////////////////////////////////////////////////////////////////////

template <typename TLev2Viewport> class CPickBuffer : public PickBufferBase
{
	public:

	CPickBuffer(	lev2::GfxBuffer *Parent,
					TLev2Viewport *pVP,
					int iX, int iY, int iW, int iH,
					EPickBufferType etyp );
	
	TLev2Viewport *mpViewport;

	virtual void Draw( void );

	ork::lev2::RtGroup*	mpPickRtGroup;
};

///////////////////////////////////////////////////////////////////////////////

template <typename TLev2Viewport>
CPickBuffer<TLev2Viewport>::CPickBuffer(	lev2::GfxBuffer *Parent,
											TLev2Viewport *pVP,
											int iX, int iY, int iW, int iH,
											EPickBufferType etyp )

	: PickBufferBase( Parent, iX, iY, iW, iH, etyp )
	, mpViewport( pVP )
	, mpPickRtGroup( new lev2::RtGroup( Parent, iW, iH ) )
{

	mpPickRtGroup->SetMrt( 0, new ork::lev2::CMrtBuffer(	Parent,
													lev2::ETGTTYPE_MRT0,
													lev2::EBUFFMT_RGBA64,
													0, 0, iW, iH ) );

	mpPickRtGroup->SetMrt( 1, new ork::lev2::CMrtBuffer(	Parent,
													lev2::ETGTTYPE_MRT1,
													lev2::EBUFFMT_RGBA64,
													0, 0, iW, iH ) );

	mpPickRtGroup->GetMrt(0)->mClearColor = Parent->mClearColor;

	////////////////////////////////////////////////////////////////
	// Umm, Mrt's cant have seperate clear colors, that suxorz...
	////////////////////////////////////////////////////////////////
	mpPickRtGroup->GetMrt(0)->mClearColor = CVector4(0.0f,0.0f,0.0f,0.0f);
	mpPickRtGroup->GetMrt(1)->mClearColor = CVector4(0.0f,0.0f,0.0f,0.0f);

	mpPickRtGroup->GetMrt(0)->mpContext = Parent->mpContext;
	mpPickRtGroup->GetMrt(1)->mpContext = Parent->mpContext;

}*/

///////////////////////////////////////////////////////////////////////////

class MiniorkMainWindow;


///////////////////////////////////////////////////////////////////////////////

} // namespace tool
} // namespace ork

#endif
