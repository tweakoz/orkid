////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/swift/orkid_handle.h>

namespace ork::swift {

// ================================================================
// TypeRegistry Static Members
// ================================================================

std::unordered_map<std::type_index, const char*> TypeRegistry::_type_to_name;
std::unordered_map<std::type_index, uint64_t> TypeRegistry::_type_to_crc;

} // namespace ork::swift
