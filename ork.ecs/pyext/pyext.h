////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/python/pycodec.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/register.h>
#include <ork/object/Object.h>
#include <ork/file/fileenv.h>
#include <ork/file/filedevcontext.h>
#include <ork/lev2/init.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/lev2/gfx/material_freestyle.h>
#include <ork/lev2/gfx/rtgroup.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/primitives.inl>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/lev2/gfx/scenegraph/scenegraph.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/ezapp.h>
#include <ork/lev2/ui/event.h>
#include <ork/lev2/lev2_types.h>

#include <ork/ecs/scene.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/component.h>
#include <ork/ecs/system.h>
#include <ork/ecs/simulation.h>
#include <ork/ecs/SceneGraphComponent.h>

///////////////////////////////////////////////////////////////////////////////
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////

using namespace ork;
using namespace ork::ecs;

namespace ork::ecs {

using pyentity_ptr_t     = ork::python::unmanaged_ptr<Entity>;
using pysystem_ptr_t     = ork::python::unmanaged_ptr<System>;
using pysgsystem_ptr_t   = ork::python::unmanaged_ptr<SceneGraphSystem>;

} // namespace ork::lev2
