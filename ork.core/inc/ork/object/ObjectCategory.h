////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/Category.h>

namespace ork { namespace rtti {
class RTTIData;
}} // namespace ork::rtti
namespace ork { namespace reflect {
class ISerializer;
class IDeserializer;
}} // namespace ork::reflect

namespace ork { namespace object {

using ObjectCategory = rtti::Category;

}} // namespace ork::object
