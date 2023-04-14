////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/python/pycodec.h>
#include <ork/math/cvector2.h>
#include <ork/math/cvector3.h>
#include <ork/math/cvector4.h>
#include <ork/math/cmatrix3.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/quaternion.h>
#include <ork/math/frustum.h>
#include <ork/math/TransformNode.h>
#include <ork/kernel/fixedstring.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/kernel/thread.h>
#include <ork/kernel/csystem.h>
#include <ork/util/crc.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/register.h>
#include <ork/object/Object.h>
#include <ork/file/fileenv.h>
#include <ork/file/filedevcontext.h>

///////////////////////////////////////////////////////////////////////////////
namespace py = pybind11;
using namespace pybind11::literals;
///////////////////////////////////////////////////////////////////////////////
