////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/util/crc.h>
#include <ork/kernel/svariant.h>

namespace ork::ecs {
///////////////////////////////////////////////////////////////////////////////

struct DataKey {
  bool valid() const;
  bool operator == (const DataKey& rhs) const;
  svar64_t _encoded;
};

struct DataVar {
  bool valid() const;
  bool operator == (const DataVar& rhs) const;
  svar64_t _encoded;
};

struct DataKvPair {
  DataKey _key;
  DataVar _val;
};

struct DataTable {

  DataVar find(const DataKey& key) const;
  svar64_t& operator[] (const DataKey& key);
  svar64_t& operator[] (const CrcString& key);
  const svar64_t& operator[] (const DataKey& key) const;
  const svar64_t& operator[] (const CrcString& key) const;

  std::vector<DataKvPair> _items;
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::ecs