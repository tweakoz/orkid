////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
