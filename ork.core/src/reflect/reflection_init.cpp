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

  ork::dataflow::PlugData::GetClassStatic();
  ork::dataflow::InPlugData::GetClassStatic();
  ork::dataflow::OutPlugData::GetClassStatic();

  ork::dataflow::floatxf::GetClassStatic();
  ork::dataflow::vect3xf::GetClassStatic();
  ork::dataflow::floatxfitembase::GetClassStatic();

  ork::dataflow::inplugdata<float>::GetClassStatic();
  ork::dataflow::floatinplugdata::GetClassStatic();

  ork::dataflow::inplugdata<ork::fvec3>::GetClassStatic();
  ork::dataflow::vect3inplugdata::GetClassStatic();

  ork::dataflow::outplugdata<float>::GetClassStatic();
  ork::dataflow::outplugdata<ork::fvec3>::GetClassStatic();
  ork::dataflow::floatxfinplugdata::GetClassStatic();
  ork::dataflow::vect3xfinplugdata::GetClassStatic();
}

} // namespace ork
