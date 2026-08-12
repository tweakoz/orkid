////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/reflect/properties/registerX.inl>

#include <ork/ecs/ecs.h>
#include <ork/ecs/system.h>
#include <ork/ecs/HypermeshComponent.h>
#include <ork/ecs/AssetSystem.h>
#include <ork/ecs/SceneGraphComponent.h>
#include <ork/ecs/entity.inl>
#include <ork/ecs/scene.inl>
#include <ork/ecs/simulation.inl>

// Full lev2 hypermesh types — only included in the .cpp (DSO-isolation; see the header).
#include <ork/lev2/gfx/hypermesh/hm_drawable.h>

#include <ork/ecs/datatable.h>       // SET_PARAM payload + the 2.20 widened decode
#include <ork/dataflow/module.inl>   // typedInputNamed (sink "value" DATA-plug poke)
#include <ork/dataflow/plug_data.inl> // inplugdata<T>::setValue instantiation
#include <ork/util/logger.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_hmcomp =
    logger()->configureChannel("ecs.hmcomp", fvec3(0.9, 0.7, 0.3));
///////////////////////////////////////////////////////////////////////////////
using namespace ork;
using namespace ork::lev2;
///////////////////////////////////////////////////////////////////////////////

void HypermeshComponentData::describeX(ComponentDataClass* clazz) {
  clazz->directProperty("LayerName", &HypermeshComponentData::_layername);
  clazz->directProperty("NodeName", &HypermeshComponentData::_nodename);
  clazz->directProperty("SkipAutoDpp", &HypermeshComponentData::_skipAutoDpp);
  clazz->directProperty("VisGroup", &HypermeshComponentData::_visgroup);
  clazz->directProperty("Visible", &HypermeshComponentData::_visible);
  // the WHOLE hypermesh description (graphdata + material ref + flags) round-trips —
  // hypermesh is model B from day one (no Python at load, no wire-step swap).
  clazz->directObjectProperty("DrawableData", &HypermeshComponentData::_drawabledata)
      ->annotate<ConstString>("editor.factorylistbase", "DrawableData");
}

HypermeshComponentData::HypermeshComponentData() {
}

Component* HypermeshComponentData::createComponent(ecs::Entity* pent) const {
  return new HypermeshComponent(*this, pent);
}

void HypermeshComponentData::DoRegisterWithScene(ecs::SceneComposer& sc) const {
  sc.Register<HypermeshSystemData>();
}

object::ObjectClass* HypermeshComponentData::componentClass() {
  return HypermeshComponent::GetClassStatic();
}

///////////////////////////////////////////////////////////////////////////////

void HypermeshComponent::describeX(object::ObjectClass* clazz) {
}

HypermeshComponent::HypermeshComponent(const HypermeshComponentData& cd, ecs::Entity* pent)
    : Component(&cd, pent)
    , _HCD(cd) {
}

HypermeshComponent::~HypermeshComponent() {
}

bool HypermeshComponent::_onLink(Simulation* psi) {
  _sgsys  = psi->findSystem<SceneGraphSystem>();
  _system = psi->findSystem<HypermeshSystem>();
  return _sgsys != nullptr && _system != nullptr;
}

void HypermeshComponent::_onUnlink(Simulation* psi) {
}

bool HypermeshComponent::_onStage(Simulation* psi) {
  if (!_system) {
    // scenes must declare the hosting system (self.system_data("HypermeshSystem"));
    // fail LOUDLY instead of crashing through the null.
    logchan_hmcomp->log(
        "HypermeshComponent::_onStage: NO HypermeshSystem in this scene — declare it "
        "(self.system_data(\"HypermeshSystem\")); component inert");
    return true;
  }
  _system->_onStageComponent(this);
  return true;
}

void HypermeshComponent::_onUnstage(Simulation* psi) {
  if (_system)
    _system->_onUnstageComponent(this);
}

bool HypermeshComponent::_onActivate(Simulation* psi) {
  return true;
}

void HypermeshComponent::_onDeactivate(Simulation* psi) {
}

void HypermeshComponent::_onNotify(Simulation* psi, token_t evID, evdata_t data) {
  if (!_system)
    return;
  _system->_applyNotifyToComponent(this, evID);
}

///////////////////////////////////////////////////////////////////////////////

void HypermeshSystemData::describeX(SystemDataClass* clazz) {
}

HypermeshSystemData::HypermeshSystemData() {
}

System* HypermeshSystemData::createSystem(ecs::Simulation* pinst) const {
  return new HypermeshSystem(*this, pinst);
}

///////////////////////////////////////////////////////////////////////////////

void HypermeshSystem::describeX(object::ObjectClass* clazz) {
}

HypermeshSystem::HypermeshSystem(const HypermeshSystemData& data, Simulation* pinst)
    : System(&data, pinst)
    , _HSD(data) {
}

HypermeshSystem::~HypermeshSystem() {
}

bool HypermeshSystem::_onLink(Simulation* psi) {
  _sgsys  = psi->findSystem<SceneGraphSystem>();
  _assets = psi->findSystem<AssetSystem>(); // may be null (no asset registry in this scene)
  return _sgsys != nullptr;
}

void HypermeshSystem::_onUnLink(Simulation* psi) {}
bool HypermeshSystem::_onStage(Simulation* psi) { return true; }
void HypermeshSystem::_onUnstage(Simulation* inst) {}
bool HypermeshSystem::_onActivate(Simulation* psi) { return true; }
void HypermeshSystem::_onDeactivate(Simulation* inst) {}

// No update-thread work: the hypermesh graph computes IN-FRAME on the render thread
// (ComputeDrawable::onGpuUpdate -> the live hook's dispatch phases). The host's clock
// duty reduces to the pause contract (see _applyNotifyToComponent).
void HypermeshSystem::_onUpdate(Simulation* inst) {}

///////////////////////////////////////////////////////////////////////////////
// Stage / unstage — create the drawable (its GPU side materializes lazily on the first
// onGpuUpdate), wire the material resolver against the AssetSystem registry, attach the
// scenegraph node.
///////////////////////////////////////////////////////////////////////////////

void HypermeshSystem::_onStageComponent(HypermeshComponent* component) {
  _components.atomicOp([component](component_set_t& unlocked) {
    unlocked.insert(component);
  });

  auto& HCD = component->_HCD;
  auto hmdd = HCD._drawabledata;
  if (!hmdd) {
    logchan_hmcomp->log("HypermeshComponent staged with NULL drawabledata");
    return;
  }

  // material resolution is BY NAME through the AssetSystem artifact registry. The
  // registry populates at the AssetSystem's gpu-init, which may land AFTER stage time —
  // so the drawable polls this resolver each frame until it yields (then builds).
  if (!hmdd->_resolved_material && !hmdd->_material_asset_name.empty() && _assets) {
    AssetSystem* asys = _assets;
    std::string mtlname = hmdd->_material_asset_name;
    hmdd->_material_resolver = [asys, mtlname]() -> lev2::pbrmaterial_ptr_t {
      auto attempt = asys->artifacts().typedValueForKey<lev2::pbrmaterial_ptr_t>(mtlname);
      return attempt ? attempt.value() : nullptr;
    };
  }
  // E.3 — the generic by-name resolver for per-gid materials (same retry
  // contract: the drawable polls until every bound gid's material yields).
  if (_assets and not hmdd->_material_resolver_named) {
    AssetSystem* asys = _assets;
    hmdd->_material_resolver_named = [asys](const std::string& name) -> lev2::pbrmaterial_ptr_t {
      auto attempt = asys->artifacts().typedValueForKey<lev2::pbrmaterial_ptr_t>(name);
      return attempt ? attempt.value() : nullptr;
    };
  }

  // Told BEFORE the drawable exists, because createDrawable() decides there and then
  // whether to register the stored-mode section bake as pending async work. A member
  // that launches hidden has no bake in flight — an offscreen host waiting on the async
  // marker would otherwise wait forever on a bake that cannot start until the set is
  // switched to. The drawable re-registers the marker itself at the moment it does build.
  hmdd->_launch_hidden = not HCD._visible;

  component->_drawable = hmdd->createDrawable();

  // layer default: the forward compositor renders a FIXED role set (std_forward,
  // std_transparent, ... — fwdnode_impl_top k_roles); the SG system's _default_layer
  // ("sg_default") is NOT in it, so a drawable attached there silently never renders.
  // Default to std_forward when the scene has it.
  // createLayer is idempotent (returns the existing layer on a name hit), so this is
  // a safe find-or-create; on a non-forward preset the layer simply goes unrendered.
  auto layer = _sgsys->_scene->createLayer(
      HCD._layername.empty() ? "std_forward" : HCD._layername);

  auto ent = component->GetEntity();
  std::string node_name = HCD._nodename.empty()
      ? FormatString("hypermesh_%llu", (unsigned long long)ent->_entref)
      : HCD._nodename;

  // E.4 — early-z: hypermesh participates in the depth prepass (the generated
  // ptex3d materials carry FWD_SSBO_CUSTOM[_INSTANCED]_DEPTHPREPASS; a material
  // without the variant yields a null dpp pipeline and the draw self-skips).
  // ...unless the author declared this hypermesh a NON-CASTER (see
  // HypermeshComponentData::_skipAutoDpp): the depth_prepass layer is also the
  // sun-cascade caster set, so joining it costs a depth pass per band.
  auto dpp_layer = HCD._skipAutoDpp
                 ? lev2::scenegraph::layer_ptr_t(nullptr)
                 : _sgsys->_scene->createLayer("depth_prepass");
  bool launch_visible = HCD._visible;
  auto attach_op = [component, layer, dpp_layer, node_name, launch_visible]() {
    // Hypermesh graphs produce geometry in their own object/world space; keep the node at
    // identity (same rationale as particles — the graph owns placement; per-instance
    // matrices place copies). Host-xf-driven placement arrives with the E.2 instance edge.
    component->_sgnode = layer->createDrawableNode(node_name, component->_drawable);
    // the launch half of the visgroup contract (see the header): disabled here means the
    // node never reaches gpuUpdate/preRender, so this member's mesh, cull, atlases and
    // section bake are all deferred to the frame it is first switched on.
    component->_sgnode->_enabled = launch_visible;
    if (dpp_layer)
      dpp_layer->addDrawableNode(component->_sgnode);
  };
  _sgsys->_renderops.push(attach_op);
}

void HypermeshSystem::_onUnstageComponent(HypermeshComponent* component) {
  _components.atomicOp([component](component_set_t& unlocked) {
    unlocked.erase(component);
  });

  if (component->_sgnode) {
    auto sgnode = component->_sgnode;
    auto detach_op = [sgnode]() {
      for (auto l : sgnode->_layers) {
        l->removeDrawableNode(sgnode);
      }
      sgnode->_layers.clear();
    };
    _sgsys->_renderops.push(detach_op);
  }
  component->_sgnode   = nullptr;
  component->_drawable = nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// Events — PAUSE/RESUME ride the LiveHypermesh pause contract (B.4): the live hook stops
// accumulating the clock (dt=0, abstime holds); resume continues from the same instant.
// The live handle exists only after the drawable's lazy bootstrap — a PAUSE arriving
// before first render simply finds _live null and is a no-op (nothing was animating yet).
///////////////////////////////////////////////////////////////////////////////

void HypermeshSystem::_applyNotifyToComponent(HypermeshComponent* c, token_t evID) {
  auto hmdd = c->_HCD._drawabledata;
  if (!hmdd || !hmdd->_live)
    return;
  if (evID == PAUSE._hashed) {
    hmdd->_live->_paused = true;
  } else if (evID == RESUME._hashed) {
    hmdd->_live->_paused = false;
  }
}

void HypermeshSystem::_onNotify(token_t evID, evdata_t data) {
  // SET_VISGROUP: flip a whole presentation set on or off (HypermeshComponentData::
  // _visgroup). The members are collected here, on the update thread, but the enable
  // is written from a RENDER OP — the render thread reads node->_enabled inside
  // Scene::gpuUpdate / Scene::preRender, and the render-op queue is the seam where
  // this system is already allowed to touch the scenegraph (the stage-time attach
  // goes through it too, which is also what guarantees _sgnode is populated by now).
  if (evID == SET_VISGROUP._hashed) {
    auto table_ptr = data.getShared<DataTable>();
    if (!table_ptr) return;
    auto const& table = *table_ptr;
    auto group  = table["group"_tok].get<std::string>();
    bool enable = false;
    if (auto as_int = table["enable"_tok].tryAs<int>())
      enable = (as_int.value() != 0);
    else if (auto as_bool = table["enable"_tok].tryAs<bool>())
      enable = as_bool.value();
    if (group.empty()) { // an empty group is EVERY ungrouped hypermesh — refuse, loudly
      logchan_hmcomp->log("SET_VISGROUP: empty group name — ignored");
      return;
    }
    std::vector<HypermeshComponent*> members;
    _components.atomicOp([&](component_set_t& unlocked) {
      for (auto* c : unlocked)
        if (c->_HCD._visgroup == group)
          members.push_back(c);
    });
    if (members.empty()) {
      logchan_hmcomp->log("SET_VISGROUP<%s>: no hypermesh declares that group", group.c_str());
      return;
    }
    if (_sgsys) {
      auto vis_op = [members, enable]() {
        for (auto* c : members)
          if (c->_sgnode)
            c->_sgnode->_enabled = enable;
      };
      _sgsys->_renderops.push(vis_op);
    }
    logchan_hmcomp->log("SET_VISGROUP<%s> -> %s (%zu members)",
                        group.c_str(), enable ? "on" : "off", members.size());
    return;
  }
  // SET_PARAM (E.6/2.12 ECS leg): poke the matching MaterialParamSinks' "value"
  // DATA plugs (the documented pokeable contract — the drawable's per-frame drain
  // reads the DATA plug, so a STATIC graph picks it up with zero recompute).
  if (evID == SET_PARAM._hashed) {
    auto table_ptr = data.getShared<DataTable>();
    if (!table_ptr) return;
    auto const& table = *table_ptr;
    auto name = table["name"_tok].get<std::string>();
    float value;
    if (not decodeSetParamValue(table["value"_tok], value)) { // 2.20 widened decode
      logchan_hmcomp->log("SET_PARAM<%s>: value is not numeric — ignored", name.c_str());
      return;
    }
    _components.atomicOp([&](component_set_t& unlocked) {
      for (auto* c : unlocked) {
        auto hmdd = c->_HCD._drawabledata;
        if (!hmdd || !hmdd->_graphdata)
          continue;
        for (size_t im = 0; im < hmdd->_graphdata->numModules(); im++) {
          auto sink = std::dynamic_pointer_cast<lev2::hypermesh::MaterialParamSinkData>(hmdd->_graphdata->module(im));
          if (sink && sink->_param_name == name)
            if (auto plug = sink->typedInputNamed<dataflow::FloatPlugTraits>("value"))
              plug->setValue(value);
        }
      }
    });
    return;
  }
  _components.atomicOp([this, evID](component_set_t& unlocked) {
    for (auto* c : unlocked) {
      _applyNotifyToComponent(c, evID);
    }
  });
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs

ImplementReflectionX(ork::ecs::HypermeshComponentData, "HypermeshComponentData");
ImplementReflectionX(ork::ecs::HypermeshComponent,     "HypermeshComponent");
ImplementReflectionX(ork::ecs::HypermeshSystemData,    "HypermeshSystemData");
ImplementReflectionX(ork::ecs::HypermeshSystem,        "HypermeshSystem");
