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
#include <ork/grammar/lruleset.h>

namespace dflow = ork::dataflow;

namespace ork {

struct CoreAppInit {
  CoreAppInit(ork::appinitdata_ptr_t init_data) {

    Timer::staticInit();

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

    // GR1.a — the LRuleSet grammar-as-data schema. SIX independent touches (T1): each serializes
    // as a sub-object inside the consuming module's grammar property; an untouched class strips to
    // "class": "" in the JSON + FindClass-null-deserializes SILENTLY. All six, always. They live
    // here (not in a family's init) because the schema is family-neutral — every consumer gets them.
    grammar::LExpr::GetClassStatic();
    grammar::LSymbolDef::GetClassStatic();
    grammar::LTurtleOp::GetClassStatic();
    grammar::LParamBinding::GetClassStatic();
    grammar::LRuleDef::GetClassStatic();
    grammar::LRuleSet::GetClassStatic();

    dflow::GraphData::GetClassStatic();

    dflow::ModuleData::GetClassStatic();
    dflow::DgModuleData::GetClassStatic();
    dflow::LambdaModuleData::GetClassStatic();
    // composite (subgraph / loop) modules + their reflected boundary tables. Every
    // embedded class MUST be touched or the nested GraphData-in-module round-trip
    // deserializes it as an empty "class": "" (the dflow serialization gotcha).
    dflow::SubGraphPromotion::GetClassStatic();
    dflow::LoopCarry::GetClassStatic();
    dflow::LoopIterFeed::GetClassStatic();
    dflow::SubGraphModuleData::GetClassStatic();
    dflow::LoopModuleData::GetClassStatic();
    dflow::MinModuleData::GetClassStatic();
    dflow::MaxModuleData::GetClassStatic();
    dflow::LerpModuleData::GetClassStatic();
    dflow::PowModuleData::GetClassStatic();
    dflow::Vec4CombineModuleData::GetClassStatic();

    dflow::PlugData::GetClassStatic();
    dflow::InPlugData::GetClassStatic();
    dflow::OutPlugData::GetClassStatic();

    dflow::floatinplugdata::GetClassStatic();
    dflow::vect3inplugdata::GetClassStatic();

    dflow::inplugdata<dflow::FloatPlugTraits>::GetClassStatic();
    dflow::outplugdata<dflow::FloatPlugTraits>::GetClassStatic();
    dflow::inplugdata<dflow::FloatXfPlugTraits>::GetClassStatic();

    dflow::inplugdata<dflow::Vec2fPlugTraits>::GetClassStatic();
    dflow::outplugdata<dflow::Vec2fPlugTraits>::GetClassStatic();
    dflow::inplugdata<dflow::Vec3fPlugTraits>::GetClassStatic();
    dflow::outplugdata<dflow::Vec3fPlugTraits>::GetClassStatic();
    dflow::inplugdata<dflow::Vec3XfPlugTraits>::GetClassStatic();
    dflow::inplugdata<dflow::Vec4fPlugTraits>::GetClassStatic();
    dflow::outplugdata<dflow::Vec4fPlugTraits>::GetClassStatic();
    dflow::inplugdata<dflow::Vec4XfPlugTraits>::GetClassStatic();
    dflow::fvec4xfdata::GetClassStatic();

    dflow::inplugdata<dflow::IntPlugTraits>::GetClassStatic();
    dflow::outplugdata<dflow::IntPlugTraits>::GetClassStatic();

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
    // D.2 (particles model B): the per-plug TRANSFORMER component classes — these serialize on
    // particle floatxf plugs and MUST be touched or the linker strips their registration and
    // JsonDeserializer's FindClass asserts on the first particle-graph load.
    dflow::biasxfdata::GetClassStatic();
    gradient_fvec4_t::GetClassStatic(); // "GradientV4" — serializes inside psys::GradientMaterial (streak gradients)
    dflow::scalexfdata::GetClassStatic();
    dflow::modxfdata::GetClassStatic();
    dflow::fquatxfdata::GetClassStatic();
    dflow::vect4inplugdata::GetClassStatic();
    dflow::quatinplugdata::GetClassStatic();
    dflow::inplugdata<dflow::QuatXfPlugTraits>::GetClassStatic(); // "dflow::inplugdata<quatxf>" — SPHR/elliptical Orientation plug

    //dflow::nullpassthrudata::GetClassStatic();
    //dflow::floatxfpassthrudata::GetClassStatic();
    //dflow::fvec3xfpassthrudata::GetClassStatic();

    logger()->defaultChannel()->log("ork.core classes registered...");

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
