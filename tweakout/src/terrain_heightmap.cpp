////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/math/plane.h>
#include <ork/math/misc_math.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/kernel/prop.h>

#include "terrain_synth.h"

#if 0
///////////////////////////////////////////////////////////////////////////////
//INSTANTIATE_TRANSPARENT_RTTI( ork::dataflow::plug<ork::MeshUtil::HeightMap>,"plug<HeightMap>" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( ork::dataflow::inplug<ork::ent::HeightMap>,"inplug<HeightMap>" );
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI( ork::dataflow::outplug<ork::ent::HeightMap>,"outplug<HeightMap>" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
template<>
ork::EPropType inplug<ork::terrain::HeightMap>::GetDataType() const
{
	return ork::EPROPTYPE_OBJECTREFERENCE;
}
template<>
ork::EPropType outplug<ork::terrain::HeightMap>::GetDataType() const
{
	return ork::EPROPTYPE_OBJECTREFERENCE;
}

template<>
ork::EPropType inplug<ork::terrain::cv2_map2d>::GetDataType() const
{
	return ork::EPROPTYPE_END;
}
template<>
ork::EPropType outplug<ork::terrain::cv2_map2d>::GetDataType() const
{
	return ork::EPROPTYPE_END;
}
template<>
ork::EPropType inplug<ork::terrain::cv3_map2d>::GetDataType() const
{
	return ork::EPROPTYPE_END;
}
template<>
ork::EPropType outplug<ork::terrain::cv3_map2d>::GetDataType() const
{
	return ork::EPROPTYPE_END;
}
template<>
ork::EPropType inplug<ork::terrain::cv4_map2d>::GetDataType() const
{
	return ork::EPROPTYPE_END;
}
template<>
ork::EPropType outplug<ork::terrain::cv4_map2d>::GetDataType() const
{
	return ork::EPROPTYPE_END;
}
//#endif
template<> void inplug<ork::ent::HeightMap>::Describe() {}
template<> void outplug<ork::ent::HeightMap>::Describe() {}
}} // namespace ork::dataflow

///////////////////////////////////////////////////////////////////////////////
//void ork::dataflow::plug<ork::MeshUtil::HeightMap>::Describe() {}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace terrain {
///////////////////////////////////////////////////////////////////////////////

}
}
#endif