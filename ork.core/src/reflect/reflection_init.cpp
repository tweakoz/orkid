////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/util/hotkey.h>
#include <ork/math/multicurve.h>
#include <ork/math/TransformNode.h>
#include <ork/asset/Asset.h>
#include <ork/dataflow/dataflow.h>

namespace ork {

void TouchCoreClasses() {
  HotKeyConfiguration::GetClassStatic();
  HotKey::GetClassStatic();
  MultiCurve1D::GetClassStatic();
  asset::Asset::GetClassStatic();
  TransformNode::GetClassStatic();
  DecompTransform::GetClassStatic();
}

} // namespace ork
