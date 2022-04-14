////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h> // for base

namespace ork { namespace rtti {
class Category;
}} // namespace ork::rtti

namespace ork { namespace reflect { namespace serdes {

struct NullSerializer final : public ISerializer {};

}}} // namespace ork::reflect::serialize
