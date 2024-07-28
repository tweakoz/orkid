////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/lev2/gfx/particle/modular_forces.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

using namespace ork::dataflow;

namespace ork::lev2::particle {

struct DragModuleInst : public ParticleModuleInst {

  DragModuleInst(const DragModuleData* gmd, dataflow::GraphInst* ginst)
      : ParticleModuleInst(gmd, ginst) {
  }

  ////////////////////////////////////////////////////

  void onLink(GraphInst* inst) final {

    _onLink(inst);
    _input_drag  = typedInputNamed<FloatXfPlugTraits>("drag");

  }

  ////////////////////////////////////////////////////

  void compute(GraphInst* inst, ui::updatedata_ptr_t updata) final {
    auto drag  = _input_drag->value();
    float dt              = updata->_dt;
    for (int i = 0; i < _pool->GetNumAlive(); i++) {
      BasicParticle* particle = _pool->GetActiveParticle(i);
      particle->mVelocity *= drag;
    }
  }

  ////////////////////////////////////////////////////
  floatxf_inp_pluginst_ptr_t _input_drag;

};

//////////////////////////////////////////////////////////////////////////

DragModuleData::DragModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

static void _reshapeDragIOs( dataflow::moduledata_ptr_t mdata ){
  auto typed = std::dynamic_pointer_cast<DragModuleData>(mdata);
  ModuleData::createInputPlug<FloatXfPlugTraits>(mdata, EPR_UNIFORM, "drag");
}

//////////////////////////////////////////////////////////////////////////

std::shared_ptr<DragModuleData> DragModuleData::createShared() {
  auto data = std::make_shared<DragModuleData>();
  _initPoolIOs(data);
  _reshapeDragIOs(data);
  return data;
}

//////////////////////////////////////////////////////////////////////////

void DragModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory( [] -> rtti::castable_ptr_t {
    return DragModuleData::createShared();
  });
  clazz->annotateTyped<moduleIOreshape_fn_t>("reshapeIOs",[](dataflow::moduledata_ptr_t mdata){
    _reshapeDragIOs(mdata);
  });
}

//////////////////////////////////////////////////////////////////////////

dgmoduleinst_ptr_t DragModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<DragModuleInst>(this,ginst);
}

} // namespace ork::lev2::particle

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::DragModuleData, "psys::DragModuleData");
