////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/asset/catalog/namespace.h>
#include <ork/kernel/string/deco.inl>
#include <ork/kernel/mutex.h>
#include <ork/util/logger.h>
#include <queue>

namespace ork::asset::catalog {

static logchannel_ptr_t logchan_catalog = logger()->getChannel("CATALOG");

////////////////////////////////////////////////////////////////
// AssetNamespaceImpl - Pimpl implementation
////////////////////////////////////////////////////////////////

struct AssetNamespaceImpl {
  assetmanifest_ptr_t _manifest;        // Associated manifest
  encryptioncodec_ptr_t _codec;         // Encryption codec (optional)
  int _priority = 100;                  // Download priority
  bool _is_container_only = false;      // True if this is just a structural node
    
  AssetNamespaceImpl() {}
};

////////////////////////////////////////////////////////////////
// AssetNamespace
////////////////////////////////////////////////////////////////

AssetNamespace::AssetNamespace(const namespaceid_t& id) : _id(id) {
  _impl.makeShared<AssetNamespaceImpl>();
}

////////////////////////////////////////////////////////////////

AssetNamespace::~AssetNamespace() {
}

////////////////////////////////////////////////////////////////

void AssetNamespace::setCodec(encryptioncodec_ptr_t codec) {
  auto impl = _impl.getShared<AssetNamespaceImpl>();
  impl->_codec = codec;
}

////////////////////////////////////////////////////////////////

void AssetNamespace::setPriority(int priority) {
  auto impl = _impl.getShared<AssetNamespaceImpl>();
  impl->_priority = priority;
}

////////////////////////////////////////////////////////////////

encryptioncodec_ptr_t AssetNamespace::getCodec() const {
  auto impl = _impl.getShared<AssetNamespaceImpl>();
  return impl->_codec;
}

////////////////////////////////////////////////////////////////

int AssetNamespace::getPriority() const {
  auto impl = _impl.getShared<AssetNamespaceImpl>();
  return impl->_priority;
}

////////////////////////////////////////////////////////////////
// AssetNamespace implementations moved from header
////////////////////////////////////////////////////////////////

bool AssetNamespace::hasCodec() const {
  auto impl = _impl.getShared<AssetNamespaceImpl>();
  return impl->_codec != nullptr;
}

////////////////////////////////////////////////////////////////
// Container/Tree methods for AssetNamespace
////////////////////////////////////////////////////////////////

bool AssetNamespace::isContainerOnly() const {
  auto impl = _impl.getShared<AssetNamespaceImpl>();
  return impl->_is_container_only;
}

////////////////////////////////////////////////////////////////

void AssetNamespace::setContainerOnly(bool container) {
  auto impl = _impl.getShared<AssetNamespaceImpl>();
  impl->_is_container_only = container;
}

////////////////////////////////////////////////////////////////

std::string AssetNamespace::buildFullPath() const {
  // _full_path is already maintained, just return it
  return _full_path;
}

////////////////////////////////////////////////////////////////

std::shared_ptr<AssetNamespace> AssetNamespace::findChild(const std::string& path) const {
  auto it = _children.find(path);
  return (it != _children.end()) ? it->second : nullptr;
}

////////////////////////////////////////////////////////////////

std::shared_ptr<AssetNamespace> AssetNamespace::getOrCreateChild(std::shared_ptr<AssetNamespace> parent, const std::string& id) {
  auto it = parent->_children.find(id);
  if (it != parent->_children.end()) {
    return it->second;
  }
  
  // Create new child
  auto child = std::make_shared<AssetNamespace>(id);
  child->_parent = parent;
  child->_full_path = parent->_full_path.empty() ? id : parent->_full_path + "|" + id;
  parent->_children[id] = child;
  return child;
}

////////////////////////////////////////////////////////////////

void AssetNamespace::printTree(int indent) const {
  std::string spaces(indent * 2, ' ');
  auto impl = _impl.getShared<AssetNamespaceImpl>();
  printf("%s%s%s\n", spaces.c_str(), _id.c_str(), 
         isContainerOnly() ? " [container]" : " [namespace]");
  
  for (const auto& [name, child] : _children) {
    child->printTree(indent + 1);
  }
}

////////////////////////////////////////////////////////////////

size_t AssetNamespace::countNodes() const {
  size_t count = 1;
  for (const auto& [name, child] : _children) {
    count += child->countNodes();
  }
  return count;
}

////////////////////////////////////////////////////////////////

size_t AssetNamespace::maxDepth() const {
  size_t max_depth = 0;
  for (const auto& [name, child] : _children) {
    max_depth = std::max(max_depth, child->maxDepth());
  }
  return max_depth + 1;
}

////////////////////////////////////////////////////////////////
// Utility functions
////////////////////////////////////////////////////////////////

namespace_component_list_t splitNamespacePath(const namespaceid_t& path) {
  namespace_component_list_t components;
  
  // TODO: Implement "|" delimiter parsing
  if (!path.empty()) {
    components.push_back(path);
  }
  
  return components;
}

namespaceid_t joinNamespacePath(const namespace_component_list_t& components) {
  std::string result;
  
  for (size_t i = 0; i < components.size(); ++i) {
    if (i > 0) {
      result += "|";
    }
    result += components[i];
  }
  
  return result;
}

bool isValidNamespacePath(const namespaceid_t& path) {
  if (path.empty()) return false;
  
  // TODO: Implement validation rules
  // - No leading/trailing "|"
  // - No empty components
  // - Valid characters only
  
  return true;
}

namespaceid_t getParentNamespacePath(const namespaceid_t& path) {
  size_t pos = path.rfind('|');
  if (pos != std::string::npos) {
    return path.substr(0, pos);
  }
  return "";
}

std::string getNamespaceLeaf(const namespaceid_t& path) {
  size_t pos = path.rfind('|');
  if (pos != std::string::npos) {
    return path.substr(pos + 1);
  }
  return path;
}

} // namespace ork::asset::catalog