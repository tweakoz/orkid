////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>

#include <ork/reflect/properties/DirectTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/kernel/orklut.hpp>

#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

///////////////////////////////////////////////////////////////////////////////

namespace dflow = ::ork::dataflow;

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::particle {
///////////////////////////////////////////////////////////////////////////////

particlebufferdata_ptr_t ParticleModuleData::_no_connection = nullptr;

///////////////////////////////////////////////////////////////////////////////

void ModuleData::describeX(class_t* clazz) {
  // clazz->annotate<Module>("dflowicon", &GetPtclModuleIcon);
  // clazz->annotate<bool>("dflowshouldblend", true);
}

///////////////////////////////////////////////////////////////////////////////

ModuleData::ModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

void ParticleModuleData::describeX(class_t* clazz) {
}

ParticleModuleData::ParticleModuleData() {
}

void ParticleModuleData:: _initShared(dflow::dgmoduledata_ptr_t data){
  createInputPlug<ParticleBufferPlugTraits>(data, dflow::EPR_UNIFORM, "pool");
  createOutputPlug<ParticleBufferPlugTraits>(data, dflow::EPR_UNIFORM, "pool");
}

ParticleModuleInst::ParticleModuleInst(const ParticleModuleData* data, dflow::GraphInst* ginst)
  : DgModuleInst(data,ginst){

}

void ParticleModuleInst::_onLink(dflow::GraphInst* inst){
  auto ptcl_context = inst->_impl.getShared<Context>();
  _input_buffer = typedInputNamed<ParticleBufferPlugTraits>("pool");
  _output_buffer = typedOutputNamed<ParticleBufferPlugTraits>("pool");
  if (_input_buffer->_connectedOutput) {
    _pool                              = _input_buffer->value()._pool;
    _output_buffer->value_ptr()->_pool = _pool;
  } else {
    OrkAssert(false);
  }

}

///////////////////////////////////////////////////////////////////////////////

#if 0
bool psys_graph::CanConnect(const dflow::inplugbase* pin, const dflow::outplugbase* pout) const {
  bool brval = false;
  brval |= (&pin->GetDataTypeId() == &typeid(ParticleBuffer)) && (&pout->GetDataTypeId() == &typeid(ParticleBuffer));
  brval |= (&pin->GetDataTypeId() == &typeid(float)) && (&pout->GetDataTypeId() == &typeid(float));
  brval |= (&pin->GetDataTypeId() == &typeid(fvec3)) && (&pout->GetDataTypeId() == &typeid(fvec3));
  return brval;
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph::describeX(class_t* clazz) {
  // ork::reflect::annotatePropertyForEditor< psys_graph >("Modules", "editor.factorylistbase", "dflow/dgmodule" );
}

///////////////////////////////////////////////////////////////////////////////

psys_graph::psys_graph()
    : mdflowregisters("ptcl_buf", 16)
    , mbEmitEnable(true)
    //, mfDuration(0.0f)
    , mfElapsed(0.0f) {
  // orkprintf( "constructing psys_graph<%08x>\n", this );
  mdflowctx.SetRegisters<ParticleBuffer>(&mdflowregisters);
}

///////////////////////////////////////////////////////////////////////////////

/*bool psys_graph::Query(event::Event* event) const {
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
}*/

///////////////////////////////////////////////////////////////////////////////

psys_graph::~psys_graph() {
  // orkprintf( "deleting psys_graph<%08x>\n", this );
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph::operator=(const psys_graph& oth) {
  OrkAssert(false);
}

//////////////////////////////////////////////////////////////////////////////

void psys_graph::Update(ptcl::Context* pctx, float fdt) {
  // orkprintf( "updating psys_graph<%08x> fDT<%f>\n", this, fdt );
  // return;

  const orklut<int, dflow::dgmodule*>& Topos = LockTopoSortedChildrenForWrite(104);
  for (orklut<int, dflow::dgmodule*>::const_iterator it = Topos.begin(); it != Topos.end(); it++) {
    dflow::dgmodule* pmodule = it->second;
    Module* ppsysmodule              = rtti::autocast(pmodule);
    ppsysmodule->Compute(fdt);
  }
  UnLockTopoSortedChildren();

  mfElapsed += fdt;
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph::Reset(ptcl::Context* pctx) {
  const orklut<int, dflow::dgmodule*>& Topos = LockTopoSortedChildrenForWrite(105);
  for (orklut<int, dflow::dgmodule*>::const_iterator it = Topos.begin(); it != Topos.end(); it++) {
    dflow::dgmodule* pmodule = it->second;
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

  dflow::dyn_external* pdyn = GetExternal();

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

  _topology = _graphdata->generateTopology();

  Reset(0);
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph_pool::describeX(class_t* clazz) {
  //  ork::reflect::RegisterProperty("Template", &psys_graph_pool::TemplateAccessor);

  // ork::reflect::RegisterProperty("MaxInstances", &psys_graph_pool::miPoolSize);
  // ork::reflect::annotatePropertyForEditor<psys_graph_pool>("MaxInstances", "editor.range.min", "1");
  // ork::reflect::annotatePropertyForEditor<psys_graph_pool>("MaxInstances", "editor.range.max", "128");
}

///////////////////////////////////////////////////////////////////////////////

psys_graph_pool::psys_graph_pool() {
}

///////////////////////////////////////////////////////////////////////////////

void psys_graph_pool::BindTemplate(const psys_graph& InTemplate) {
  printf("psys_graph_pool<%p>::BindTemplate(%p) mGraphPool<%p>\n", (void*)this, (void*)&InTemplate, (void*)mGraphPool);
  mGraphPool = new ork::pool<psys_graph>(miPoolSize);
  printf(" new mGraphPool<%p>\n", (void*)mGraphPool);
  ////////////////////////////////////////////
  /*
  ork::ResizableString str;
  ork::stream::ResizableStringOutputStream ostream(str);
  ork::reflect::serdes::BinarySerializer binoser(ostream);
  InTemplate.GetClass()->Description().SerializeProperties(binoser, &InTemplate);
  ////////////////////////////////////////////
  for (int i = 0; i < miPoolSize; i++) {
    psys_graph& clone = mGraphPool->direct_access(i);
    new (&clone) psys_graph();
    ork::stream::StringInputStream istream(str);
    ork::reflect::serdes::BinaryDeserializer biniser(istream);
    InTemplate.GetClass()->Description().DeserializeProperties(biniser, &clone);
  }
  mNewTemplate = &InTemplate;*/
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

#endif
///////////////////////////////////////////////////////////////////////////////

} // namespace ptcl

namespace ptcl = ork::lev2::particle;

///////////////////////////////////////////////////////////////////////////////

//ImplementReflectionX(ptcl::psys_graph, "psys::Graph");
ImplementReflectionX(ptcl::ModuleData, "psys::ModuleData");
ImplementReflectionX(ptcl::ParticleModuleData, "psys::ParticleModuleData");
//ImplementReflectionX(ptcl::psys_graph_pool, "psys::GraphPool");

