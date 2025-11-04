////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <ork/asset/catalog/types.h>
#include <ork/asset/catalog/manifest.h>
#if !defined(ORK_IOS)
#include <ork/util/crypt.h>
#endif
#include <memory>
#include <map>
#include <vector>
#include <string>

namespace ork::asset::catalog {

// Type aliases moved to types.h

////////////////////////////////////////////////////////////////////////////////
// Asset namespace - represents a logical grouping of assets with tree structure
// Examples: "game|textures", "game|models|characters", "editor|icons"
//
// This structure combines both namespace data and hierarchical tree organization:
// - Namespace properties: ID, display name, codec, manifest, statistics
// - Tree structure: parent/child relationships for inheritance
// - Intermediate nodes: Some nodes may be containers without actual asset data
////////////////////////////////////////////////////////////////////////////////

struct AssetNamespace {

  explicit AssetNamespace(const namespaceid_t& id);
  ~AssetNamespace();
  
  ////////////////////////////////////////////////////////////////////////////////
  // Identity and Tree Structure
  ////////////////////////////////////////////////////////////////////////////////

  namespaceid_t _id;                    // Unique identifier (e.g., "game::textures")
  namespaceid_t _full_path;             // Full hierarchical path (e.g., "game|textures")
  
  // Tree structure
  std::weak_ptr<AssetNamespace> _parent;
  std::map<std::string, std::shared_ptr<AssetNamespace>> _children;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Container vs Data Node
  ////////////////////////////////////////////////////////////////////////////////
  bool isContainerOnly() const;          // True if this is just a structural node
  void setContainerOnly(bool container); // Mark as container-only (no assets)
      
  ////////////////////////////////////////////////////////////////////////////////
  // Manifest and Codec association
  ////////////////////////////////////////////////////////////////////////////////

  bool hasCodec() const;
    
  ////////////////////////////////////////////////////////////////////////////////
  // Setters for configuration
  ////////////////////////////////////////////////////////////////////////////////

  void setCodec(encryptioncodec_ptr_t codec);
  void setPriority(int priority);
  
  ////////////////////////////////////////////////////////////////////////////////
  // Getters for additional properties
  ////////////////////////////////////////////////////////////////////////////////

  encryptioncodec_ptr_t getCodec() const;
  int getPriority() const;
  
  ////////////////////////////////////////////////////////////////////////////////
  // Tree Navigation Methods (moved from AssetNamespaceNode)
  ////////////////////////////////////////////////////////////////////////////////
  
  // Build full path by traversing up the tree
  std::string buildFullPath() const;
  
  // Find a child node by path (e.g., "textures|characters")
  std::shared_ptr<AssetNamespace> findChild(const std::string& path) const;
  
  // Create or get child node
  static std::shared_ptr<AssetNamespace> getOrCreateChild(std::shared_ptr<AssetNamespace> parent, const std::string& id);
    
  // Debug helpers
  void printTree(int indent = 0) const;
  size_t countNodes() const;
  size_t maxDepth() const;
  
private:
  // Implementation
  svar64_t _impl;
};


////////////////////////////////////////////////////////////////////////////////
// Helper functions
////////////////////////////////////////////////////////////////////////////////

// Split namespace path into components (e.g., "game|textures" -> ["game", "textures"])
namespace_component_list_t splitNamespacePath(const namespaceid_t& path);

// Join namespace components into path
namespaceid_t joinNamespacePath(const namespace_component_list_t& components);

// Check if namespace path is valid
bool isValidNamespacePath(const namespaceid_t& path);

// Get parent namespace path (e.g., "game|textures|characters" -> "game|textures")
namespaceid_t getParentNamespacePath(const namespaceid_t& path);

// Get leaf component of namespace path (e.g., "game|textures|characters" -> "characters")
std::string getNamespaceLeaf(const namespaceid_t& path);

} // namespace ork::asset::catalog