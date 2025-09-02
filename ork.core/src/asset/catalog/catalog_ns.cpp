///////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/chunk_assembler.h>
#include <ork/file/file.h>
#include <ork/kernel/string/deco.inl>
#include <ork/util/crypt.h>
#include <ork/util/tar.h>
#include <ork/util/logger.h>
#include <boost/filesystem.hpp>
#include <regex>
#include <thread>
#include <chrono>
#include <cstdio>
#include "catalog_impl.h"

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////
// Namespace Management
////////////////////////////////////////////////////////////////

void AssetCatalog::registerNamespace(const namespaceid_t& path, assetnamespace_ptr_t ns) {
  if (!ns)
    return;

  auto impl = _impl.getShared<CatalogImpl>();

  impl->_state.atomicOp([&](CatalogImpl::CatalogState& state) {
    // Store namespace directly (no more wrapper node)
    ns->_full_path = path;

    // Add to nodes map
    state._nodes_by_namespace[path] = ns;
  });

}

///////////////////////////////////////////////////////////////////////////////

assetnamespace_ptr_t AssetCatalog::findNamespace(const namespaceid_t& fq_namespace_id) const {
  auto impl                   = _impl.getShared<CatalogImpl>();
  assetnamespace_ptr_t result = nullptr;
  // Finding namespace
  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    // Namespace count logged at higher level if needed

    auto it = state._nodes_by_namespace.find(fq_namespace_id);
    if (it != state._nodes_by_namespace.end()) {
      result = it->second;
    } else {
      for (const auto& [ns_id, node] : state._nodes_by_namespace) {
        // Namespace checking logged at higher level if needed
      }
    }
  });
  return result;
}

///////////////////////////////////////////////////////////////////////////////

assetid_list_t AssetCatalog::listNamespaces(const std::string& pattern) const {
  auto impl = _impl.getShared<CatalogImpl>();
  assetid_list_t result;
  std::set<namespaceid_t> seen; // To avoid duplicates

  impl->_state.atomicOp([&](const CatalogImpl::CatalogState& state) {
    // Add registered namespaces
    for (const auto& [ns_id, node] : state._nodes_by_namespace) {
      // Convert wildcard pattern to simple matching
      bool matches = pattern.empty();
      if (!matches && !pattern.empty()) {
        // Handle wildcard pattern (ends with *)
        if (pattern.back() == '*') {
          std::string prefix = pattern.substr(0, pattern.length() - 1);
          matches            = ns_id.find(prefix) == 0; // Starts with prefix
        } else {
          matches = ns_id.find(pattern) != std::string::npos;
        }
      }

      if (matches && seen.insert(ns_id).second) {
        result.push_back(ns_id);
      }
    }

    // Also add namespaces from manifests
    for (const auto& [ns_id, manifest_list] : state._manifests_by_namespace) {
      bool matches = pattern.empty();
      if (!matches && !pattern.empty()) {
        // Handle wildcard pattern (ends with *)
        if (pattern.back() == '*') {
          std::string prefix = pattern.substr(0, pattern.length() - 1);
          matches            = ns_id.find(prefix) == 0; // Starts with prefix
        } else {
          matches = ns_id.find(pattern) != std::string::npos;
        }
      }

      if (matches && seen.insert(ns_id).second) {
        result.push_back(ns_id);
      }
    }
  });

  return result;
}

} //namespace ork::asset::catalog {
