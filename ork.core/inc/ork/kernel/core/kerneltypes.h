////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/orktypes.h>
#include <ork/orkstl.h>

namespace ork {

class PoolString;
class Object;

typedef orklist<std::string>        tokenlist;
typedef orkmap<PoolString,Object*>	ObjectMap;

}

