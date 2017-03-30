#pragma once

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/timer.h>

#include <ork/lev2/gfx/gfxenv_enum.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/kernel/mutex.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/event/Event.h>
#include <ork/object/AutoConnector.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

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

	void Init();
	
    uint32_t        AssignPickId(ork::Object*);
    ork::Object*    GetObjectFromPickId(uint32_t);

	virtual void Draw( lev2::GetPixelContext& ctx ) = 0;

	///////////////////////

	EPickBufferType				meType;
	bool						mbInitTex;
	GfxMaterialUITextured*		mpUIMaterial;
    std::map<uint32_t,ork::Object*>	mPickIds;
	ork::lev2::RtGroup*			mpPickRtGroup;

};

///////////////////////////////////////////////////////////////////////////

template <typename VPT> class CPickBuffer : public PickBufferBase
{
	public:

	CPickBuffer(	lev2::GfxBuffer* pbuf,
					VPT* pVP,
					int iX, int iY, int iW, int iH,
					EPickBufferType etyp );
	

	virtual void Draw( lev2::GetPixelContext& ctx );
	
	VPT* mpViewport;
};

///////////////////////////////////////////////////////////////////////////////

template <typename TLev2Viewport>
CPickBuffer<TLev2Viewport>::CPickBuffer(	lev2::GfxBuffer *Parent,
											TLev2Viewport *pVP,
											int iX, int iY, int iW, int iH,
											EPickBufferType etyp )

	: PickBufferBase( Parent, iX, iY, iW, iH, etyp )
	, mpViewport( pVP )
{
	Init();
}

///////////////////////////////////////////////////////////////////////////////
}} //namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

