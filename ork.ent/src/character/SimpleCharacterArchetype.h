///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>
#include <pkg/ent/AudioComponent.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/math/quaternion.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/bullet.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/ModelArchetype.h>
#include <pkg/ent/SimpleAnimatable.h>
#include <pkg/ent/ScriptComponent.h>
#include <pkg/ent/input.h>
#include <pkg/ent/ModelComponent.h>
#include <pkg/ent/event/MeshEvent.h>
#include <pkg/ent/event/AnimFinishEvent.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <pkg/ent/LuaBindings.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ent {

class SimpleCharacterArchetype : public Archetype
{
	RttiDeclareConcrete( SimpleCharacterArchetype, Archetype );

	void DoStartEntity(Simulation* psi, const fmtx4 &world, Entity *pent ) const final {}
	void DoCompose(ork::ent::ArchComposer& composer) final;

public:

	SimpleCharacterArchetype();

};

class SimpleCharControllerData : public ComponentData
{
	RttiDeclareConcrete(SimpleCharControllerData, ComponentData)

	ComponentInst *createComponent(Entity *pent) const final;

public:

    float mWalkSpeed = 30.0f;
    float mRunSpeed = 50.0f;
    float mSpeedLerpRate = 1.0f;
};
} //namespace ork::ent {
