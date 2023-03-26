////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/ISerializer.h> // for base

namespace ork { namespace rtti {
class Category;
}} // namespace ork::rtti

namespace ork { namespace reflect { namespace serdes {

struct NullSerializer final : public ISerializer {};

}}} // namespace ork::reflect::serialize
