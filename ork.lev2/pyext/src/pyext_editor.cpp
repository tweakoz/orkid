////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include "pyext.h"
#include <ork/lev2/input/inputdevice.h>
#include <ork/lev2/editor/editor.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork::lev2 {

void pyinit_editor(py::module& module_lev2) {
  using namespace editor;
  auto type_codec                  = python::TypeCodec::instance();
  /////////////////////////////////////////////////////////////////////////////////
}

} //namespace ork::lev2::editor {
