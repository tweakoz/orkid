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
#include <ork/dataflow/all.h>

namespace dflow = ork::dataflow;

namespace ork {

void TouchCoreClasses() {
  HotKeyConfiguration::GetClassStatic();
  HotKey::GetClassStatic();
  MultiCurve1D::GetClassStatic();
  asset::Asset::GetClassStatic();
  TransformNode::GetClassStatic();
  DecompTransform::GetClassStatic();

  dflow::GraphData::GetClassStatic();

  dflow::ModuleData::GetClassStatic();
  dflow::DgModuleData::GetClassStatic();
  dflow::LambdaModuleData::GetClassStatic();

  dflow::PlugData::GetClassStatic();
  dflow::InPlugData::GetClassStatic();
  dflow::OutPlugData::GetClassStatic();


  dflow::floatinplugdata::GetClassStatic();
  dflow::vect3inplugdata::GetClassStatic();
  dflow::floatxfinplugdata::GetClassStatic();
  dflow::fvec3xfinplugdata::GetClassStatic();

  dflow::floatxfdata::GetClassStatic();
  dflow::fvec3xfdata::GetClassStatic();
  dflow::floatxfitembasedata::GetClassStatic();

  dflow::inplugdata<dflow::FloatPlugTraits>::GetClassStatic();
  dflow::inplugdata<dflow::FloatXfPlugTraits>::GetClassStatic();
  dflow::outplugdata<dflow::FloatPlugTraits>::GetClassStatic();

  dflow::floatinplugdata::GetClassStatic();

  dflow::inplugdata<dflow::Vec3fPlugTraits>::GetClassStatic();
  dflow::inplugdata<dflow::Vec3XfPlugTraits>::GetClassStatic();
  dflow::outplugdata<dflow::Vec3fPlugTraits>::GetClassStatic();
  dflow::vect3inplugdata::GetClassStatic();

  //dflow::vect3inplugxfdata<dflow::fvec3xfdata>::GetClassStatic();
}

} // namespace ork
