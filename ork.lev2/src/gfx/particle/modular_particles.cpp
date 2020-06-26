////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/renderer/renderer.h>
#include <ork/lev2/gfx/camera/cameradata.h>
#include <ork/lev2/gfx/gfxprimitives.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/reflect/enum_serializer.inl>

#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>

#include <ork/lev2/gfx/particle/modular_particles.h>
//#include <ork/kernel/fixedlut.hpp>

///////////////////////////////////////////////////////////////////////////////

#include <ork/stream/FileInputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/reflect/serialize/BinaryDeserializer.h>
#include <ork/reflect/serialize/BinarySerializer.h>
#include <ork/stream/ResizableStringOutputStream.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/ShallowDeserializer.h>
#include <ork/lev2/lev2_asset.h>

///////////////////////////////////////////////////////////////////////////////

// template class ork::fixedlut<ork::Char4,ork::lev2::particle::EventQueue*,8>;

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::psys_graph, "psys::Graph");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::Module, "psys::Module");
INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::ParticleModule, "psys::ParticleModule");

INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::lev2::particle::PtclBufOutPlug, "psys::pbufoutplug");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(ork::lev2::particle::PtclBufInpPlug, "psys::pbufinpplug");

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::lev2::particle::psys_graph_pool, "psys::GraphPool");

template <> void ork::dataflow::outplug<ork::lev2::particle::psys_ptclbuf>::Describe() {
}
template <> void ork::dataflow::inplug<ork::lev2::particle::psys_ptclbuf>::Describe() {
}

namespace ork { namespace dataflow {
template <> int MaxFanout<ork::lev2::particle::psys_ptclbuf>() {
  return 1;
}
extern bool gbGRAPHLIVE;
template <> const ork::lev2::particle::psys_ptclbuf& outplug<ork::lev2::particle::psys_ptclbuf>::GetInternalData() const {
  OrkAssert(mOutputData != 0);
  return *mOutputData;
}
template <> const ork::lev2::particle::psys_ptclbuf& outplug<ork::lev2::particle::psys_ptclbuf>::GetValue() const {
  return GetInternalData();
}

}} // namespace ork::dataflow

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 { namespace particle {
///////////////////////////////////////////////////////////////////////////////

psys_ptclbuf ParticleModule::gNoCon;

static lev2::Texture* GetPtclModuleIcon(ork::dataflow::dgmodule* pmod) {
  static const char* assetname = "lev2://textures/dfnodesel";
  static auto texasset         = asset::AssetManager<TextureAsset>::Load(assetname);
  return texasset->GetTexture();
}

///////////////////////////////////////////////////////////////////////////////

void Module::Describe() {
  reflect::annotateClassForEditor<Module>("dflowicon", &GetPtclModuleIcon);
  reflect::annotateClassForEditor<Module>("dflowshouldblend", true);
}

///////////////////////////////////////////////////////////////////////////////

Module::Module()
    : mpParticleContext(0)
    , mpTemplateModule(0) {
}

///////////////////////////////////////////////////////////////////////////////

void Module::Link(ork::lev2::particle::Context* pctx) {
  mpParticleContext = pctx;
  DoLink();
}

void ParticleModule::Describe() {
}

///////////////////////////////////////////////////////////////////////////////

bool psys_graph::CanConnect(const ork::dataflow::inplugbase* pin, const ork::dataflow::outplugbase* pout) const {
  bool brval = false;
  brval |= (&pin->GetDataTypeId() == &typeid(psys_ptclbuf)) && (&pout->GetDataTypeId() == &typeid(psys_ptclbuf));
  brval |= (&pin->GetDataTypeId() == &typeid(float)) && (&pout->GetDataTypeId() == &typeid(float));
  brval |= (&pin->GetDataTypeId() == &typeid(fvec3)) && (&pout->GetDataTypeId() == &typeid(fvec3));
  return brval;
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph::Describe() {
  // ork::reflect::annotatePropertyForEditor< psys_graph >("Modules", "editor.factorylistbase", "dflow/dgmodule" );
}

///////////////////////////////////////////////////////////////////////////////

psys_graph::psys_graph()
    : mdflowregisters("ptcl_buf", 16)
    , mbEmitEnable(true)
    //, mfDuration(0.0f)
    , mfElapsed(0.0f) {
  // orkprintf( "constructing psys_graph<%08x>\n", this );
  mdflowctx.SetRegisters<psys_ptclbuf>(&mdflowregisters);
}

///////////////////////////////////////////////////////////////////////////////

bool psys_graph::Query(event::Event* event) const {
  auto pfilter = dynamic_cast<ork::ObjectFactoryFilter*>(event);
  if (pfilter) {
    const object::ObjectClass* pclass = pfilter->mpClass;
    pfilter->mbFactoryOK              = false;
    if (pclass) {
      if (pclass->IsSubclassOf(Module::GetClassStatic()))
        pfilter->mbFactoryOK = true;
    }
    return true;
  }

  return dataflow::graph_inst::Query(event);
}

///////////////////////////////////////////////////////////////////////////////

psys_graph::~psys_graph() {
  // orkprintf( "deleting psys_graph<%08x>\n", this );
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph::operator=(const psys_graph& oth) {
  OrkAssert(false);
}

//////////////////////////////////////////////////////////////////////////////

void psys_graph::Update(ork::lev2::particle::Context* pctx, float fdt) {
  // orkprintf( "updating psys_graph<%08x> fDT<%f>\n", this, fdt );
  // return;

  const orklut<int, ork::dataflow::dgmodule*>& Topos = LockTopoSortedChildrenForWrite(104);
  for (orklut<int, ork::dataflow::dgmodule*>::const_iterator it = Topos.begin(); it != Topos.end(); it++) {
    ork::dataflow::dgmodule* pmodule = it->second;
    Module* ppsysmodule              = rtti::autocast(pmodule);
    ppsysmodule->Compute(fdt);
  }
  UnLockTopoSortedChildren();

  mfElapsed += fdt;
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph::Reset(ork::lev2::particle::Context* pctx) {
  const orklut<int, ork::dataflow::dgmodule*>& Topos = LockTopoSortedChildrenForWrite(105);
  for (orklut<int, ork::dataflow::dgmodule*>::const_iterator it = Topos.begin(); it != Topos.end(); it++) {
    ork::dataflow::dgmodule* pmodule = it->second;
    Module* ppsysmodule              = rtti::autocast(pmodule);
    ppsysmodule->Reset();
  }
  mfElapsed    = 0.0f;
  mbEmitEnable = true;
  UnLockTopoSortedChildren();
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph::PrepForStart() {
  ///////////////////////////////////////////////////

  ork::dataflow::dyn_external* pdyn = GetExternal();

  size_t inummods = GetNumChildren();
  for (size_t i = 0; i < inummods; i++) {
    if (GetChild(i)) {
      GetChild(i)->OnTopologyUpdate();
      GetChild(i)->OnStart();

      ExtConnector* pext = rtti::autocast(GetChild(i));
      if (pext) {
        pext->BindConnector(pdyn);
      }
    }
  }

  ///////////////////////////////////////////////////

  mdflowctx.Clear();
  RefreshTopology(mdflowctx);
  Reset(0);
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph_pool::Describe() {
  //	ork::reflect::RegisterProperty("Template", &psys_graph_pool::TemplateAccessor);

  ork::reflect::RegisterProperty("MaxInstances", &psys_graph_pool::miPoolSize);
  ork::reflect::annotatePropertyForEditor<psys_graph_pool>("MaxInstances", "editor.range.min", "1");
  ork::reflect::annotatePropertyForEditor<psys_graph_pool>("MaxInstances", "editor.range.max", "128");
}

///////////////////////////////////////////////////////////////////////////////

psys_graph_pool::psys_graph_pool()
    : mGraphPool(0)
    , miPoolSize(1)
    , mNewTemplate(0) {
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph_pool::BindTemplate(const psys_graph& InTemplate) {
  printf("psys_graph_pool<%p>::BindTemplate(%p) mGraphPool<%p>\n", this, &InTemplate, mGraphPool);
  mGraphPool = new ork::pool<psys_graph>(miPoolSize);
  printf(" new mGraphPool<%p>\n", mGraphPool);
  ////////////////////////////////////////////
  ork::ResizableString str;
  ork::stream::ResizableStringOutputStream ostream(str);
  ork::reflect::serialize::BinarySerializer binoser(ostream);
  InTemplate.GetClass()->Description().SerializeProperties(binoser, &InTemplate);
  ////////////////////////////////////////////
  for (int i = 0; i < miPoolSize; i++) {
    psys_graph& clone = mGraphPool->direct_access(i);
    new (&clone) psys_graph();
    ork::stream::StringInputStream istream(str);
    ork::reflect::serialize::BinaryDeserializer biniser(istream);
    InTemplate.GetClass()->Description().DeserializeProperties(biniser, &clone);
  }
  mNewTemplate = &InTemplate;
}

///////////////////////////////////////////////////////////////////////////////

psys_graph* psys_graph_pool::Allocate() {
  psys_graph* pinstance = nullptr;
  if (mGraphPool) {
    pinstance = mGraphPool->allocate();
    if (pinstance) {
      pinstance->BindExternal(0);
    }
  }
  return pinstance;
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph_pool::Free(psys_graph* pgraph) {
  if (pgraph) {
    OrkAssert(pgraph != mNewTemplate);

    if (mGraphPool)
      mGraphPool->deallocate(pgraph);
    pgraph->BindExternal(0);
  }
}

///////////////////////////////////////////////////////////////////////////////

}}} // namespace ork::lev2::particle
