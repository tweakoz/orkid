////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2014, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include "enemy_fighter.h"
#include "enemy_spawner.h"
#include "world.h"

namespace ork { namespace tweakout {

void Init()
{
	ork::wiidom::WorldArchetype::GetClassStatic();
	ork::wiidom::WorldControllerData::GetClassStatic();
}

}}