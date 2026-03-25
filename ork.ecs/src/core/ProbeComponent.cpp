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
#include "message_private.h"
#include <ork/file/path.h>

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
  clazz->directProperty("OutputFolder", &ProbeComponentData::_outputFolder)
      ->annotate("editor.filebase", "<assetcache>")
      ->annotate("editor.browsetype", "folder");
  clazz->directProperty("OutputPrefix", &ProbeComponentData::_outputPrefix);
  clazz->directProperty("RenderLayer", &ProbeComponentData::_renderLayer);
  clazz->directEnumProperty("ActivationMode", &ProbeComponentData::_activationMode);
  clazz->intProperty("Supersample", int_range{0, 6}, &ProbeComponentData::_supersample);
  clazz->intProperty("TemporalFrames", int_range{0, 64}, &ProbeComponentData::_temporalFrames);
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
    if (_system==nullptr) return false;
  _system->_onStageComponent(this);
  return true;
}

void ProbeComponent::_onUnstage(Simulation* psi) {
    if (_system==nullptr) return;
  _system->_onUnstageComponent(this);
}

bool ProbeComponent::_onActivate(Simulation* psi) {
    if (_system==nullptr) return false;
  _system->_onActivateComponent(this);
  return true;
}

void ProbeComponent::_onDeactivate(Simulation* psi) {
    if (_system==nullptr) return;
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
  probe->_pDim = &CD._imageDim;
  probe->_pSupersample = &CD._supersample;
  probe->_pTemporalFrames = &CD._temporalFrames;
  probe->_pRenderLayer = &CD._renderLayer;
  probe->_dim = CD._imageDim;
  probe->_name = ent->data()->GetName().c_str();

  // Read from spawner's transform (authoritative in edit mode)
  auto xf = ent->data()->transform();
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

// removed: expandEnvVars — use file::Path::expandPathString() instead

void ProbeSystem::_onRequest(impl::sys_response_ptr_t response, token_t reqID, evdata_t data) {
  switch (reqID.hashed()) {
    case Bake._hashed: {
      auto output_base = data.get<std::string>();
      _pendingBakes.push_back({output_base, nullptr, response, false});
      break;
    }
    case BakeSelected._hashed: {
      auto spawner = data.get<spawndata_ptr_t>();
      _pendingBakes.push_back({"", spawner, response, false});
      break;
    }
    default:
      System::_onRequest(response, reqID, data);
      break;
  }
}

void ProbeSystem::_onGpuUpdate(Simulation* psi, lev2::Context* ctx) {
  for (auto& pending : _pendingBakes) {
    if (!pending._activated) {
      // First frame: activate probes and mark dirty
      _activateBakeOnly();
      _markAllDirty();
      pending._activated = true;
    } else if (_areAllClean()) {
      // Probes rendered — bake and finish
      int n;
      if (!pending._selectedSpawner) {
        n = _bakeAll(ctx, pending._outputBase);
      } else {
        n = _bakeSelected(ctx, pending._selectedSpawner);
      }
      pending._response->_responseData.set<int>(n);
      pending._response->_ready.store(true);
      _deactivateBakeOnly();
    }
  }
  // Remove completed bakes
  _pendingBakes.erase(
    std::remove_if(_pendingBakes.begin(), _pendingBakes.end(),
      [](const PendingBake& pb) { return pb._response->_ready.load(); }),
    _pendingBakes.end());
}

void ProbeSystem::_markAllDirty() {
  for (auto* comp : _components) {
    auto probe = comp->_probe;
    if (!probe) continue;
    // Read from spawner's transform (authoritative in edit mode)
    auto xf = comp->GetEntity()->data()->transform();
    probe->_worldMatrix.compose(xf->_translation, xf->_rotation, xf->_uniformScale);
    probe->_dirty = true;
    probe->_accumFrameCount = 0;
    probe->_accumWriteIdx = 0;
  }
}

bool ProbeSystem::_areAllClean() const {
  for (auto* comp : _components) {
    if (comp->_probe && comp->_probe->_dirty) return false;
  }
  return true;
}

void ProbeSystem::_activateBakeOnly() {
  for (auto* comp : _components) {
    if (comp->_probe && comp->_probe->_activationMode == lev2::ProbeActivationMode::BAKE_ONLY) {
      comp->_probe->_active = true;
      comp->_probe->_dirty = true;
    }
  }
}

void ProbeSystem::_deactivateBakeOnly() {
  for (auto* comp : _components) {
    if (comp->_probe && comp->_probe->_activationMode == lev2::ProbeActivationMode::BAKE_ONLY) {
      comp->_probe->_active = false;
    }
  }
}

int ProbeSystem::_bakeAll(lev2::Context* ctx, const std::string& output_base) {
  // 180-degree Y rotation to compensate for negated X/Z in cubemap capture
  fquat rot;
  rot.fromAxisAngle(fvec4(0, 1, 0, PI));

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

    // Read from spawner's transform (authoritative in edit mode)
    auto xf = comp->GetEntity()->data()->transform();
    probe->_worldMatrix.compose(xf->_translation, xf->_rotation, xf->_uniformScale);

    std::string folder = file::Path::expandPathString(comp->_CD._outputFolder);
    if (folder.empty()) folder = output_base;

    std::string filename = FormatString(
        "%s/%s_%d.png",
        folder.c_str(), comp->_CD._outputPrefix.c_str(), index);

    printf("Probe %d: cubeTexture=%p cubeRTG=%p dirty=%d dim=%d renderLayer=%s\n",
           index, probe->_cubeTexture.get(), probe->_cubeRenderRTG.get(),
           probe->_dirty, probe->dim(), probe->renderLayer().c_str());
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

int ProbeSystem::_bakeSelected(lev2::Context* ctx, spawndata_ptr_t spawner) {
  // 180-degree Y rotation to compensate for negated X/Z in cubemap capture
  fquat rot;
  rot.fromAxisAngle(fvec4(0, 1, 0, PI));
  auto spawner_name = std::string(spawner->GetName().c_str());
  auto spawner_xf = spawner->transform();

  int index = 0;
  int baked = 0;
  for (auto* comp : _components) {
    auto probe = comp->_probe;
    if (!probe) { index++; continue; }

    // Check if this component's entity came from the selected spawner
    auto ent = comp->GetEntity();
    if (ent->data().get() != spawner.get()) { index++; continue; }

    // Use the spawner's transform directly
    probe->_worldMatrix.compose(spawner_xf->_translation, spawner_xf->_rotation, spawner_xf->_uniformScale);

    std::string folder = file::Path::expandPathString(comp->_CD._outputFolder);
    if (folder.empty()) folder = "/tmp/ecs_probes";

    std::string filename = FormatString(
        "%s/%s_%d.png",
        folder.c_str(), comp->_CD._outputPrefix.c_str(), index);

    if (probe->_cubeTexture) {
      probe->exportEquirectangular(ctx, rot, file::Path(filename));
      printf("Exported probe %d (%s): %s\n", index, spawner_name.c_str(), filename.c_str());
      baked++;
    } else {
      printf("Probe %d (%s): SKIPPED — no cubemap texture\n", index, spawner_name.c_str());
    }
    index++;
  }
  return baked;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs
