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

class PickBufferBase : public ork::lev2::OffscreenBuffer
{
	RttiDeclareAbstract(PickBufferBase,ork::lev2::OffscreenBuffer);

	public:

	enum EPickBufferType
	{
		EPICK_FACE_VTX = 0,
		EPICK_NORMAL ,
		EPICK_WPOS ,
		EPICK_ST
	};

	PickBufferBase( OffscreenBuffer *parent,
					 int iX, int iY, int iW, int iH,
					 EPickBufferType etyp );

	void Init();

    uint64_t        AssignPickId(ork::Object*);
    ork::Object*    GetObjectFromPickId(uint64_t);

	virtual void Draw( lev2::PixelFetchContext& ctx ) = 0;

	///////////////////////

	EPickBufferType				meType;
	bool						mbInitTex;
	GfxMaterialUITextured*		mpUIMaterial;
  std::map<uint64_t,ork::Object*>	mPickIds;
	ork::lev2::RtGroup*			mpPickRtGroup;

};

///////////////////////////////////////////////////////////////////////////

template <typename VPT> class PickBuffer : public PickBufferBase
{
	public:

	PickBuffer(	lev2::OffscreenBuffer* pbuf,
					VPT* pVP,
					int iX, int iY, int iW, int iH,
					EPickBufferType etyp );


	virtual void Draw( lev2::PixelFetchContext& ctx );

	VPT* mpViewport;
};

///////////////////////////////////////////////////////////////////////////////

template <typename TLev2Viewport>
PickBuffer<TLev2Viewport>::PickBuffer(	lev2::OffscreenBuffer *Parent,
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
