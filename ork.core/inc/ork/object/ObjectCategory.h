////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/rtti/Category.h>

namespace ork { namespace rtti {
struct RTTIData;
}} // namespace ork::rtti
namespace ork { namespace reflect {
class ISerializer;
class IDeserializer;
}} // namespace ork::reflect

namespace ork { namespace object {

using ObjectCategory = rtti::Category;

}} // namespace ork::object
