////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/DirectTypedMap.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/qtui/qtui.hpp>
#include "ged_delegate.hpp"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

template class GedSimpleNode< GedIoDriver<PoolString> , PoolString >;

///////////////////////////////////////////////////////////////////////////////
}}}
///////////////////////////////////////////////////////////////////////////////
