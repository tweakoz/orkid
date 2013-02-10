////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/kernel/string/string.h>

#include <orktool/filter/gfx/collada/collada.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////

	
	
	
///////////////////////////////////////////////////////////////////////////////

bool SaveGGM( const AssetPath& Filename, const lev2::XgmModel *mdl )	
{
	return ork::lev2::SaveXGM(Filename,mdl);
}

///////////////////////////////////////////////////////////////////////////////

} }
