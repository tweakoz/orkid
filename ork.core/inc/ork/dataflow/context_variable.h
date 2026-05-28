////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

// ContextVariableRegistry — runtime mapping from a DSL-author-visible name
// (e.g. "time", "dt", "ptc.unit_age") to the C++ module class + output plug
// that materializes the value, plus a policy for whether the source module
// is lazy-added (Singleton) or required to already exist (RequireExisting).
//
// HyperSyn's Expr factory walks this registry at import time to populate its
// namespace tree (Expr.time, Expr.dt, Expr.ptc.unit_age, ...). The lowerer
// uses it at generatedflow() time to resolve each ContextRef into a real
// graph connection.
//
// Registrations live in each module's describeX(), next to the plug
// declarations themselves — single source of truth.

#pragma once

#include <map>
#include <optional>
#include <string>
#include <vector>

namespace ork::rtti {
struct Class;
}

namespace ork::dataflow {

class ContextVariableRegistry {
public:
  enum Policy {
    // Lazy-add an instance of the module class on first reference (e.g.
    // Globals: only one per graph; missing means "we'll create it"). Reserved
    // name `_DSL_<ModuleClass>` avoids clashing with user-named modules.
    SINGLETON,
    // Require a module of this class to already exist in the graph (e.g.
    // ParticlePoolData: the user MUST explicitly declare a Pool — the
    // lowerer never invents one). Missing raises a clear error.
    REQUIRE_EXISTING,
  };

  struct Spec {
    rtti::Class* _module_class    = nullptr;
    std::string  _output_plug_name;
    Policy       _policy          = SINGLETON;
  };

  static ContextVariableRegistry& instance();

  // Register a DSL name. Idempotent — re-registering the same name with the
  // same spec is a no-op; re-registering with a DIFFERENT spec asserts.
  void register_(const std::string& dsl_name, Spec spec);

  // Lookup; returns std::nullopt for unregistered names.
  std::optional<Spec> lookup(const std::string& dsl_name) const;

  // All registered DSL names (used by the Python Expr factory to enumerate
  // the namespace tree at import time, and by introspection helpers).
  std::vector<std::string> allNames() const;

private:
  ContextVariableRegistry() = default;
  std::map<std::string, Spec> _entries;
};

} // namespace ork::dataflow
