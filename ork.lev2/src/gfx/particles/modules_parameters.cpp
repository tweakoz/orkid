////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// ParametersModule — runtime-mutable scalar parameter source for the
// HyperSyn DSL's self.expose() feature. Each exposed parameter becomes a
// named float output plug on this module; downstream chains read it via a
// dataflow connection. The per-instance current value is mutated at runtime
// by the ECS layer (SET_PARAM notify → ParametersModuleInst::setParam).
//
// onReset restores defaults — so when an ECS slot recycles, params snap
// back to their declared defaults rather than carrying over from the prior
// firing.

#include <ork/pch.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/lev2/gfx/particle/modular_particles2.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

namespace dflow = ::ork::dataflow;

namespace ork::lev2::particle {

///////////////////////////////////////////////////////////////////////////////

struct ParametersModuleInst : dflow::DgModuleInst {

  ParametersModuleInst(const ParametersModuleData* data, dflow::GraphInst* ginst)
      : dflow::DgModuleInst(data, ginst)
      , _pmd(data) {
    // Seed runtime values from the data's defaults.
    for (auto const& kv : _pmd->_defaults) {
      _values[kv.first] = kv.second;
    }
  }

  void onLink(dflow::GraphInst* inst) final {
    // Bind one float output plug per exposed param.
    for (auto const& kv : _pmd->_defaults) {
      auto plug = typedOutputNamed<dflow::FloatPlugTraits>(kv.first.c_str());
      OrkAssert(plug);
      _output_plugs[kv.first] = plug;
    }
  }

  void compute(dflow::GraphInst* inst, ui::updatedata_ptr_t updata) final {
    for (auto const& kv : _values) {
      auto it = _output_plugs.find(kv.first);
      if (it != _output_plugs.end()) {
        it->second->setValue(kv.second);
      }
    }
  }

  void onReset(dflow::GraphInst* inst) final {
    // Recycling a slot restores defaults — gameplay-tuned values from the
    // prior firing don't bleed into the next.
    for (auto const& kv : _pmd->_defaults) {
      _values[kv.first] = kv.second;
    }
  }

  void setParam(const std::string& name, float value) {
    auto it = _values.find(name);
    if (it != _values.end()) {
      it->second = value;
    }
    // Silently ignore unknown params — gameplay code can fire-and-forget
    // a SET_PARAM without coupling to which graphs expose what.
  }

  const ParametersModuleData* _pmd;
  std::unordered_map<std::string, float> _values;
  std::unordered_map<std::string, dflow::float_out_pluginst_ptr_t> _output_plugs;
};

///////////////////////////////////////////////////////////////////////////////

ParametersModuleData::ParametersModuleData() {
}

///////////////////////////////////////////////////////////////////////////////

void ParametersModuleData::addFloatParam(const std::string& name, float default_value) {
  _defaults[name] = default_value;
  // Idempotent — only create the plug if not already present. Re-exposing
  // updates the default but leaves the existing plug alone.
  for (auto const& existing : _outputs) {
    if (existing->_name == name) return;
  }
  // Wrap the raw `this` in a shared_ptr alias so createOutputPlug's API
  // signature is happy. The aliasing constructor doesn't take ownership;
  // the actual lifetime is managed by the original shared_ptr held
  // elsewhere (e.g. by the graphdata).
  auto self_alias = std::shared_ptr<ParametersModuleData>(
      std::shared_ptr<ParametersModuleData>{}, this);
  ModuleData::createOutputPlug<dflow::FloatPlugTraits>(self_alias, dflow::EPR_UNIFORM, name.c_str());
}

float ParametersModuleData::defaultFor(const std::string& name) const {
  auto it = _defaults.find(name);
  return it != _defaults.end() ? it->second : 0.0f;
}

bool ParametersModuleData::hasParam(const std::string& name) const {
  return _defaults.find(name) != _defaults.end();
}

std::vector<std::string> ParametersModuleData::paramNames() const {
  std::vector<std::string> names;
  names.reserve(_defaults.size());
  for (auto const& kv : _defaults) {
    names.push_back(kv.first);
  }
  return names;
}

///////////////////////////////////////////////////////////////////////////////

std::shared_ptr<ParametersModuleData> ParametersModuleData::createShared() {
  // Note: no _reshapeIOs analog needed — output plugs are added dynamically
  // by addFloatParam as the DSL author calls self.expose(). Empty graph
  // until first expose().
  return std::make_shared<ParametersModuleData>();
}

///////////////////////////////////////////////////////////////////////////////

dflow::dgmoduleinst_ptr_t ParametersModuleData::createInstance(dataflow::GraphInst* ginst) const {
  return std::make_shared<ParametersModuleInst>(this, ginst);
}

///////////////////////////////////////////////////////////////////////////////

void ParametersModuleData::describeX(class_t* clazz) {
  clazz->setSharedFactory([]() -> rtti::castable_ptr_t {
    return ParametersModuleData::createShared();
  });
}

///////////////////////////////////////////////////////////////////////////////
// Free helpers (defined here for use from outside lev2 — e.g. ECS layer):
//   set_param_on_graphinst — find the ParametersModule in this graphinst and
//                            call setParam on its inst. Returns true if found.
///////////////////////////////////////////////////////////////////////////////

bool set_param_on_graphinst(dflow::graphinst_ptr_t ginst, const std::string& name, float value) {
  if (!ginst) return false;
  // firstModuleInst returns the base shared_ptr — re-cast to access our
  // typed setParam.
  auto base = ginst->firstModuleInst<ParametersModuleInst>();
  auto pmi  = std::dynamic_pointer_cast<ParametersModuleInst>(base);
  if (!pmi) return false;
  pmi->setParam(name, value);
  return true;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::particle
///////////////////////////////////////////////////////////////////////////////

namespace ptcl = ork::lev2::particle;

ImplementReflectionX(ptcl::ParametersModuleData, "psys::ParametersModuleData");
