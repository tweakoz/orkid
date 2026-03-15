////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/any.h>
#include <ork/pch.h>
#include <ork/reflect/properties/register.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/enum_serializer.inl>

#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>
#include <ork/ecs/datatable.h>

#include "ProbeComponent_impl.h"

ImplementReflectionX(ork::ecs::ProbeComponentData, "ProbeComponentData");
ImplementReflectionX(ork::ecs::ProbeComponent, "ProbeComponent");
ImplementReflectionX(ork::ecs::ProbeSystemData, "ProbeSystemData");

///////////////////////////////////////////////////////////////////////////////

ImplementEnumSerializer(ork::lev2::ProbeActivationMode);

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
using lev2::ProbeActivationMode;

BeginEnumRegistration(ProbeActivationMode);
RegisterEnum(ProbeActivationMode, ALWAYS);
RegisterEnum(ProbeActivationMode, BAKE_ONLY);
EndEnumRegistration();
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::object;
using namespace ork::reflect;

///////////////////////////////////////////////////////////////////////////////
// ProbeComponentData
///////////////////////////////////////////////////////////////////////////////

void ProbeComponentData::describeX(ComponentDataClass* clazz) {
  InvokeEnumRegistration(ProbeActivationMode);
  clazz->intProperty("ImageDimension", int_range{64, 4096}, &ProbeComponentData::_imageDim);
  clazz->directProperty("OutputFolder", &ProbeComponentData::_outputFolder);
  clazz->directProperty("OutputPrefix", &ProbeComponentData::_outputPrefix);
  clazz->directProperty("RenderLayer", &ProbeComponentData::_renderLayer);
  clazz->directEnumProperty("ActivationMode", &ProbeComponentData::_activationMode);
}

ProbeComponentData::ProbeComponentData() {
}

Component* ProbeComponentData::createComponent(ecs::Entity* pent) const {
  return new ProbeComponent(*this, pent);
}

object::ObjectClass* ProbeComponentData::componentClass() {
  return ProbeComponent::GetClassStatic();
}

void ProbeComponentData::DoRegisterWithScene(ork::ecs::SceneComposer& sc) const {
  sc.Register<ork::ecs::ProbeSystemData>();
}

///////////////////////////////////////////////////////////////////////////////
// ProbeComponent
///////////////////////////////////////////////////////////////////////////////

void ProbeComponent::describeX(ObjectClass* clazz) {
}

ProbeComponent::ProbeComponent(const ProbeComponentData& data, ecs::Entity* pent)
    : ork::ecs::Component(&data, pent)
    , _CD(data) {
}

void ProbeComponent::_onUninitialize(Simulation* psi) {
}

bool ProbeComponent::_onLink(Simulation* psi) {
  _system = psi->findSystem<ProbeSystem>();
  return true;
}

void ProbeComponent::_onUnlink(Simulation* psi) {
}

bool ProbeComponent::_onStage(Simulation* psi) {
  _system->_onStageComponent(this);
  return true;
}

void ProbeComponent::_onUnstage(Simulation* psi) {
  _system->_onUnstageComponent(this);
}

bool ProbeComponent::_onActivate(Simulation* psi) {
  _system->_onActivateComponent(this);
  return true;
}

void ProbeComponent::_onDeactivate(Simulation* psi) {
  _system->_onDeactivateComponent(this);
}

void ProbeComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
}

///////////////////////////////////////////////////////////////////////////////
// ProbeSystemData
///////////////////////////////////////////////////////////////////////////////

void ProbeSystemData::describeX(SystemDataClass* clazz) {
}

ProbeSystemData::ProbeSystemData() {
}

System* ProbeSystemData::createSystem(ork::ecs::Simulation* pinst) const {
  return new ProbeSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////
// ProbeSystem
///////////////////////////////////////////////////////////////////////////////

ProbeSystem::ProbeSystem(const ProbeSystemData& data, ork::ecs::Simulation* pinst)
    : ork::ecs::System(&data, pinst) {
}

void ProbeSystem::_onStageComponent(ProbeComponent* component) {
  _components.insert(component);
  auto ent = component->GetEntity();
  auto& CD = component->_CD;

  // Create LightProbe
  auto probe = std::make_shared<lev2::LightProbe>();
  probe->_type = lev2::LightProbeType::REFLECTION;
  probe->_activationMode = CD._activationMode;
  probe->_active = (CD._activationMode == lev2::ProbeActivationMode::ALWAYS);
  probe->resize(CD._imageDim);
  probe->_name = ent->data()->GetName().c_str();
  probe->_renderLayer = CD._renderLayer;

  // Set world matrix from entity transform
  auto xf = ent->transform();
  probe->_worldMatrix.compose(xf->_translation, xf->_rotation, xf->_uniformScale);

  component->_probe = probe;

  // Create ProbeNode on the default layer
  if (_sgSystem && _sgSystem->_default_layer) {
    auto layer = _sgSystem->_default_layer;
    auto name = std::string("probe_") + probe->_name;
    auto pnode = layer->createProbeNode(name, probe);
    component->_probeNode = pnode;
  }
}

void ProbeSystem::_onUnstageComponent(ProbeComponent* component) {
  if (component->_probeNode && _sgSystem && _sgSystem->_default_layer) {
    _sgSystem->_default_layer->removeProbeNode(component->_probeNode);
    component->_probeNode = nullptr;
  }
  component->_probe = nullptr;
  _components.erase(component);
}

void ProbeSystem::_onActivateComponent(ProbeComponent* component) {
}

void ProbeSystem::_onDeactivateComponent(ProbeComponent* component) {
}

bool ProbeSystem::_onLink(Simulation* psi) {
  _sgSystem = psi->findSystem<SceneGraphSystem>();
  return true;
}

void ProbeSystem::_onUnLink(Simulation* psi) {
}

void ProbeSystem::_onUpdate(Simulation* inst) {
}

bool ProbeSystem::_onStage(Simulation* psi) {
  return true;
}

void ProbeSystem::_onUnstage(Simulation* inst) {
}

bool ProbeSystem::_onActivate(Simulation* psi) {
  return true;
}

void ProbeSystem::_onDeactivate(Simulation* inst) {
}

///////////////////////////////////////////////////////////////////////////////

static std::string expandEnvVars(const std::string& input) {
  std::string result = input;
  size_t pos = 0;
  while ((pos = result.find("${", pos)) != std::string::npos) {
    size_t end = result.find("}", pos);
    if (end == std::string::npos) break;
    std::string var = result.substr(pos + 2, end - pos - 2);
    const char* val = getenv(var.c_str());
    result.replace(pos, end - pos + 1, val ? val : "");
  }
  return result;
}

void ProbeSystem::markAllDirty() {
  for (auto* comp : _components) {
    auto probe = comp->_probe;
    if (!probe) continue;
    auto xf = comp->GetEntity()->transform();
    probe->_worldMatrix.compose(xf->_translation, xf->_rotation, xf->_uniformScale);
    probe->_dirty = true;
  }
}

bool ProbeSystem::areAllClean() const {
  for (auto* comp : _components) {
    if (comp->_probe && comp->_probe->_dirty) return false;
  }
  return true;
}

void ProbeSystem::activateBakeOnly() {
  for (auto* comp : _components) {
    if (comp->_probe && comp->_probe->_activationMode == lev2::ProbeActivationMode::BAKE_ONLY) {
      comp->_probe->_active = true;
      comp->_probe->_dirty = true;
    }
  }
}

void ProbeSystem::deactivateBakeOnly() {
  for (auto* comp : _components) {
    if (comp->_probe && comp->_probe->_activationMode == lev2::ProbeActivationMode::BAKE_ONLY) {
      comp->_probe->_active = false;
    }
  }
}

int ProbeSystem::bakeAll(lev2::Context* ctx, const std::string& output_base) {
  // Identity rotation — shader matches engine's envtools.i2 convention
  fquat rot;

  // Temporarily activate BAKE_ONLY probes for baking
  std::vector<lev2::lightprobe_ptr_t> activated_for_bake;
  for (auto* comp : _components) {
    auto probe = comp->_probe;
    if (probe && probe->_activationMode == lev2::ProbeActivationMode::BAKE_ONLY && !probe->_active) {
      probe->_active = true;
      probe->_dirty = true;
      activated_for_bake.push_back(probe);
    }
  }

  int index = 0;
  for (auto* comp : _components) {
    auto probe = comp->_probe;
    if (!probe) continue;

    // Update transform from entity
    auto xf = comp->GetEntity()->transform();
    probe->_worldMatrix.compose(xf->_translation, xf->_rotation, xf->_uniformScale);

    std::string folder = expandEnvVars(comp->_CD._outputFolder);
    if (folder.empty()) folder = output_base;

    std::string filename = FormatString(
        "%s/%s_%d.png",
        folder.c_str(), comp->_CD._outputPrefix.c_str(), index);

    printf("Probe %d: cubeTexture=%p cubeRTG=%p dirty=%d dim=%d renderLayer=%s\n",
           index, probe->_cubeTexture.get(), probe->_cubeRenderRTG.get(),
           probe->_dirty, probe->_dim, probe->_renderLayer.c_str());
    if (probe->_cubeRenderRTG) {
      printf("  cubeRTG: %dx%d cubeMap=%d sizeDirty=%d\n",
             probe->_cubeRenderRTG->width(), probe->_cubeRenderRTG->height(),
             probe->_cubeRenderRTG->_cubeMap, probe->_cubeRenderRTG->mbSizeDirty);
    }
    if (probe->_cubeTexture) {
      probe->exportEquirectangular(ctx, rot, file::Path(filename));
      printf("Exported probe %d: %s\n", index, filename.c_str());
    } else {
      printf("Probe %d: SKIPPED — no cubemap texture\n", index);
    }
    index++;
  }

  // Deactivate BAKE_ONLY probes that were temporarily activated
  for (auto& probe : activated_for_bake) {
    probe->_active = false;
  }

  return index;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
