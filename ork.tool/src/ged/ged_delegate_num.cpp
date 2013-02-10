////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IObjectPropertyType.h>
#include "ged_delegate.hpp"
///////////////////////////////////////////////////////////////////////////////
template class ork::tool::ged::GedBoolNode<ork::tool::ged::PropSetterObj>;
template class ork::tool::ged::GedIntNode< ork::tool::ged::GedIoDriver<int> >;
template class ork::tool::ged::GedFloatNode< ork::tool::ged::GedIoDriver<float> >;
///////////////////////////////////////////////////////////////////////////////
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<ork::CMatrix4> , ork::CMatrix4 >;
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<ork::CVector4> , ork::CVector4 >;
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<ork::CVector3> , ork::CVector3 >;
template class ork::tool::ged::GedSimpleNode< ork::tool::ged::GedIoDriver<ork::CVector2> , ork::CVector2 >;
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
