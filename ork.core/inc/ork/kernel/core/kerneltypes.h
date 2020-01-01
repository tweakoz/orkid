////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

