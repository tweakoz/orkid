////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/lev2/gfx/gfxmaterial_ui.h>
#include <ork/lev2/gfx/rtgroup.h>

#if defined( ORK_CONFIG_QT )
#include <ork/lev2/qtui/qtui.h>
#endif
#include <ork/rtti/downcast.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>

INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::PickBufferBase, "ork::lev2::PickBufferBase" );

namespace ork { namespace lev2 {

void PickBufferBase::Describe()
{
}

PickBufferBase::PickBufferBase( lev2::GfxBuffer *Parent, int iX, int iY, int iW, int iH, EPickBufferType etyp )
	: ork::lev2::GfxBuffer( Parent, iX, iY, iW, iH, lev2::EBUFFMT_RGBA32,lev2::ETGTTYPE_EXTBUFFER )
	, meType( etyp )
	, mbInitTex( true )
	, mpPickRtGroup( new lev2::RtGroup( GetContext(), iW, iH ) )
{
	mpUIMaterial = new ork::lev2::GfxMaterialUITextured( GetContext() );
}

uint32_t PickBufferBase::AssignPickId(ork::Object*pobj)
{
	uint32_t lo = rand()&0xffff;
	uint32_t hi = rand()&0xffff;

	uint32_t pid = lo | (hi<<16);
	mPickIds[pid] = pobj;
	return pid;
}
ork::Object* PickBufferBase::GetObjectFromPickId(uint32_t pid)
{
	auto it = mPickIds.find(pid);
	ork::Object* pobj = (it==mPickIds.end()) ? nullptr : it->second;
	return pobj;
}

void PickBufferBase::Init()
{
	mpPickRtGroup->SetMrt( 0, new ork::lev2::RtBuffer(	mpPickRtGroup,
													lev2::ETGTTYPE_MRT0,
													lev2::EBUFFMT_RGBA64,
													miWidth, miHeight ) );

	mpPickRtGroup->SetMrt( 1, new ork::lev2::RtBuffer(	mpPickRtGroup,
													lev2::ETGTTYPE_MRT1,
													lev2::EBUFFMT_RGBA64,
													miWidth, miHeight ) );

	//mpPickRtGroup->GetMrt(0)->RefClearColor() = mParent->GetClearColor();

	////////////////////////////////////////////////////////////////
	// Umm, Mrt's cant have seperate clear colors, that suxorz...
	////////////////////////////////////////////////////////////////
	//mpPickRtGroup->GetMrt(0)->RefClearColor() = CVector4(0.0f,0.0f,0.0f,0.0f);
	//mpPickRtGroup->GetMrt(1)->RefClearColor() = CVector4(0.0f,0.0f,0.0f,0.0f);

	//mpPickRtGroup->GetMrt(0)->SetContext(mParent->GetContext());
	//mpPickRtGroup->GetMrt(1)->SetContext(mParent->GetContext());
}

}} // namespace ork { namespace lev2 {


