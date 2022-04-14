////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once  

#include <ork/file/path.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>

#include <ork/ecs/scene.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

struct EcsEditor {

	EcsEditor( ork::ecs::scenedata_ptr_t scene) : _scene(scene) {}

	ork::ecs::scenedata_ptr_t _scene;
	ork::object_ptr_t _currentobject;

};

} //namespace ork::ecs {
