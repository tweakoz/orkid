////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/file/fileenv.h>
#include <ork/rtti/RTTIX.inl>

#include <ork/ecs/component.h>
#include <ork/ecs/system.h>
#include <ork/ecs/pysys/PythonComponent.h>
#include <ork/util/logger.h>
#include <ork/util/fast_set.inl>

#include <Python.h>
#include <ork/python/pycodec.h>
#include <ork/python/context.h>

namespace ork::ecs::pysys {

typedef ork::FixedString<256> script_funcname_t;
struct PythonContext;
struct ScriptObject;
}

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecssim {
  extern ::ork::python::pb11_typecodec_ptr_t simonly_codec_instance();
}
///////////////////////////////////////////////////////////////////////////////

namespace ork::ecs {

namespace pysys {

using GSTATE = ::ork::python::GlobalState;
using gstate_ptr_t = std::shared_ptr<GSTATE>;

struct PythonContext;
using pythoncontext_ptr_t = std::shared_ptr<PythonContext>;

}



} // namespace ork::ecs {

namespace ork::ecs::pysys {


} // namespace ork::ecs::pysys {
