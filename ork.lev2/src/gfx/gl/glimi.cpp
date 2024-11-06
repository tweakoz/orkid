////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include "gl.h"

#include <ork/lev2/ui/ui.h>

namespace ork { namespace lev2 {

GlImiInterface::GlImiInterface( ContextGL& target )
	: ImmInterface( target )
{
}

///////////////////////////////////////////////////////////////////////////////

void GlImiInterface::DrawPrim( const fvec4 *Points, int inumpoints, PrimitiveType eType )
{

}

void GlImiInterface::DrawLine( const fvec4 &From, const fvec4 &To )
{
}

void GlImiInterface::DrawPoint( F32 fx, F32 fy, F32 fz )
{

}

} }

