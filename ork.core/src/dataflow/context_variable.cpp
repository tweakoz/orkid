////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/dataflow/context_variable.h>

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////

ContextVariableRegistry& ContextVariableRegistry::instance() {
  static ContextVariableRegistry _instance;
  return _instance;
}

///////////////////////////////////////////////////////////////////////////////

void ContextVariableRegistry::register_(const std::string& dsl_name, Spec spec) {
  auto it = _entries.find(dsl_name);
  if (it != _entries.end()) {
    // Re-registering the same DSL name with identical spec is OK (matters if
    // describeX runs twice for any reason). Mismatch is a programming error.
    OrkAssert(it->second._module_class    == spec._module_class);
    OrkAssert(it->second._output_plug_name == spec._output_plug_name);
    OrkAssert(it->second._policy          == spec._policy);
    return;
  }
  _entries.emplace(dsl_name, spec);
}

///////////////////////////////////////////////////////////////////////////////

std::optional<ContextVariableRegistry::Spec>
ContextVariableRegistry::lookup(const std::string& dsl_name) const {
  auto it = _entries.find(dsl_name);
  if (it == _entries.end()) {
    return std::nullopt;
  }
  return it->second;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> ContextVariableRegistry::allNames() const {
  std::vector<std::string> names;
  names.reserve(_entries.size());
  for (auto const& entry : _entries) {
    names.push_back(entry.first);
  }
  return names;
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::dataflow
