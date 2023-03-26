////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "IMap.h"
#include <ork/reflect/BidirectionalSerializer.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

template <typename KeyType, typename ValueType> //
class ITypedMap : public IMap {

public:
protected:
  virtual bool GetKey(object_constptr_t, int idx, KeyType&) const              = 0;
  virtual bool GetVal(object_constptr_t, const KeyType& k, ValueType& v) const = 0;
  virtual bool ReadElement(
      object_constptr_t, //
      const KeyType&,
      int,
      ValueType&) const = 0;
  virtual bool EraseElement(
      object_ptr_t, //
      const KeyType&,
      int) const = 0;
  virtual bool WriteElement(
      object_ptr_t, //
      const KeyType&,
      int,
      const ValueType*) const = 0;

  ITypedMap()
      : IMap() {
  }

private:
  // from ObjectProperty
  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t sernode) const override;
};

}} // namespace ork::reflect
