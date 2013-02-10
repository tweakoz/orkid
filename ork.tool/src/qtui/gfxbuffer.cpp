////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/gfxbuffer.h>
///////////////////////////////////////////////////////////////////////////////
/*
INSTANTIATE_TRANSPARENT_RTTI( ork::lev2::PickBufferBase, "ork::lev2::PickBufferBase" );

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

void PickBufferBase::Describe()
{
}

PickBufferBase::PickBufferBase( lev2::GfxBuffer *Parent, int iX, int iY, int iW, int iH, EPickBufferType etyp )
	: ork::lev2::GfxBuffer( Parent, iX, iY, iW, iH, lev2::EBUFFMT_RGBA32,lev2::ETGTTYPE_EXTBUFFER )
	, meType( etyp )
	, mbInitTex( true )
{
	mpUIMaterial = new ork::lev2::GfxMaterialUITextured( GetContext() );

}

uint32_t PickBufferBase::AssignPickId(ork::Object*pobj)
{
   uint32_t pid = uint32_t(mPickIds.size());
   mPickIds.push_back(pobj);
   return pid;
}
ork::Object* PickBufferBase::GetObjectFromPickId(uint32_t pid)
{
    return mPickIds[pid];
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
*/
