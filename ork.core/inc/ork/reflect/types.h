////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <memory>
#include <ork/kernel/svariant.h>

namespace ork::reflect {

struct ObjectProperty;
class ISerializer;
class IDeserializer;
class IObjectFunctor;

}


namespace ork::reflect::serdes {

struct ISerializer;
struct IDeserializer;
struct Node;

class BidirectionalSerializer;

using node_ptr_t = std::shared_ptr<Node>;
using var_t      = svar1024_t;

}
