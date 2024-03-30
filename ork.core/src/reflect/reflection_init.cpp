////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/hotkey.h>
#include <ork/math/multicurve.h>
#include <ork/math/gradient.h>
#include <ork/math/TransformNode.h>
#include <ork/asset/Asset.h>
#include <ork/dataflow/all.h>
#include <ork/object/COM.h>

namespace dflow = ork::dataflow;

namespace ork {

struct ClassToucher {
  ClassToucher() {
    COM::GetClassStatic();
    HotKeyConfiguration::GetClassStatic();
    HotKey::GetClassStatic();
    MultiCurve1D::GetClassStatic();
    GradientBase::GetClassStatic();
    gradient_fvec4_t::GetClassStatic();
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

    dflow::inplugdata<dflow::FloatPlugTraits>::GetClassStatic();
    dflow::outplugdata<dflow::FloatPlugTraits>::GetClassStatic();
    dflow::inplugdata<dflow::FloatXfPlugTraits>::GetClassStatic();

    dflow::inplugdata<dflow::Vec3fPlugTraits>::GetClassStatic();
    dflow::outplugdata<dflow::Vec3fPlugTraits>::GetClassStatic();
    dflow::inplugdata<dflow::Vec3XfPlugTraits>::GetClassStatic();

    dflow::floatinplugdata::GetClassStatic();
    dflow::vect3inplugdata::GetClassStatic();

    dflow::modscabiasdata::GetClassStatic();
    dflow::floatxfitembasedata::GetClassStatic();
    dflow::floatxfmoddata::GetClassStatic();
    dflow::floatxfscaledata::GetClassStatic();
    dflow::floatxfbiasdata::GetClassStatic();
    dflow::floatxfpowdata::GetClassStatic();
    dflow::floatxfsinedata::GetClassStatic();
    dflow::floatxfcurvedata::GetClassStatic();
    dflow::floatxfmodstepdata::GetClassStatic();

    dflow::floatxfdata::GetClassStatic();
    dflow::fvec3xfdata::GetClassStatic();

    //dflow::nullpassthrudata::GetClassStatic();
    //dflow::floatxfpassthrudata::GetClassStatic();
    //dflow::fvec3xfpassthrudata::GetClassStatic();
  }
};

void TouchCoreClasses() {
  static ork::ClassToucher toucher;
}

} // namespace ork
