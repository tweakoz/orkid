///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {

class CharacterLocoData : public ComponentData
{
	RttiDeclareConcrete(CharacterLocoData, ComponentData)

	ComponentInst *createComponent(Entity *pent) const final;

public:

    float mWalkSpeed = 30.0f;
    float mRunSpeed = 50.0f;
    float mSpeedLerpRate = 1.0f;
};

} //namespace ork::ent {
///////////////////////////////////////////////////////////////////////////////
