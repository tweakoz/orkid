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
///////////////////////////////////////////////////////////////////////////////

#include <ork/reflect/properties/ObjectProperty.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/properties/ITyped.h>
#include "ged_delegate.hpp"
///////////////////////////////////////////////////////////////////////////////
template class ork::tool::ged::GedBoolNode<ork::tool::ged::PropSetterObj>;
template class ork::tool::ged::GedIntNode< ork::tool::ged::GedIoDriver<int> >;
template class ork::tool::ged::GedFloatNode< ork::tool::ged::GedIoDriver<float> >;
///////////////////////////////////////////////////////////////////////////////
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<ork::fmtx4> , ork::fmtx4 >;
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<ork::fvec4> , ork::fvec4 >;
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<ork::fvec3> , ork::fvec3 >;
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<ork::fvec2> , ork::fvec2 >;
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<int> , int >;
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
SliderBase::SliderBase()
	: mlogmode(false)
	, mfTextPos( 0.0f )
	, mfIndicPos( 0.0f )
	, miLabelH(0)
{
}
///////////////////////////////////////////////////////////////////////////////
} } }
///////////////////////////////////////////////////////////////////////////////
