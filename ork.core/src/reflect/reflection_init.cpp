////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/util/hotkey.h>
#include <ork/math/multicurve.h>
#include <ork/math/gradient.h>
#include <ork/math/transform_curve.h>
#include <ork/math/TransformNode.h>
#include <ork/asset/Asset.h>
#include <ork/asset/AssetLoader.h>
#include <ork/asset/NetAssetLoader.h>
#include <ork/dataflow/all.h>
#include <ork/object/COM.h>
#include <ork/application/application.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/util/logger.h>

namespace dflow = ork::dataflow;

namespace ork {

struct CoreAppInit {
  CoreAppInit(ork::appinitdata_ptr_t init_data) {

    COM::GetClassStatic();
    HotKeyConfiguration::GetClassStatic();
    HotKey::GetClassStatic();
    MultiCurve1D::GetClassStatic();
    math::TransformCurvePoint::GetClassStatic();
    math::TransformCurve::GetClassStatic();
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
    dflow::floatxfabsdata::GetClassStatic();
    dflow::floatxfsmoothstepdata::GetClassStatic();
    dflow::floatxfquantizedata::GetClassStatic();
    dflow::floatxfcurvedata::GetClassStatic();
    dflow::floatxfmodstepdata::GetClassStatic();

    dflow::floatxfdata::GetClassStatic();
    dflow::fvec3xfdata::GetClassStatic();

    //dflow::nullpassthrudata::GetClassStatic();
    //dflow::floatxfpassthrudata::GetClassStatic();
    //dflow::fvec3xfpassthrudata::GetClassStatic();

    logger()->defaultChannel()->log("ork.core classes registered...");

    Timer::staticInit();

    init_data->enqueuePostInitOp(AppInitOrder::REFLECTION_LINK,[init_data] { 
      logger()->defaultChannel()->log("ork.core postinit...");
      rtti::Class::InitializeClasses(); // init/link all classes
    });
  }
};

using coreappinit_ptr_t = std::shared_ptr<CoreAppInit>;
static coreappinit_ptr_t g_core_class_toucher = nullptr;
static mutex ginit_mutex("coreinit");

void initModule(ork::appinitdata_ptr_t init_data) {
  ginit_mutex.Lock();
  if(g_core_class_toucher){
    ginit_mutex.UnLock();
    return;
  }
  g_core_class_toucher = std::make_shared<ork::CoreAppInit>(init_data);
  
  // Register NetAssetLoader for catalog extension
  auto net_loader = std::make_shared<asset::NetAssetLoader>();
  asset::AssetLoader::registerLoaderForExtension("catalog", net_loader);
  opq::init();
  
  ginit_mutex.UnLock();
}

void exitModule(ork::appinitdata_ptr_t init_data){
  ginit_mutex.Lock();
  g_core_class_toucher = nullptr;
  opq::exit();
  ginit_mutex.UnLock();
}


} // namespace ork
