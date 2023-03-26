////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include "ITypedMap.h"

#include <ork/config/config.h>

namespace ork { namespace reflect {

class BidirectionalSerializer;

template <typename KeyType, typename ValueType> class AccessorTypedMap : public ITypedMap<KeyType, ValueType> {
public:
  AccessorTypedMap(
      bool (Object::*getter)(const KeyType&, int, ValueType&) const,
      void (Object::*setter)(const KeyType&, int, const ValueType&),
      void (Object::*eraser)(const KeyType&, int));

private:
  bool ReadElement(object_constptr_t, const KeyType&, int, ValueType&) const final;
  bool EraseElement(object_ptr_t, const KeyType&, int) const final;
  bool WriteElement(object_ptr_t, const KeyType&, int, const ValueType*) const final;

  bool (Object::*mGetter)(const KeyType&, int, ValueType&) const;
  void (Object::*mSetter)(const KeyType&, int, const ValueType&);
  void (Object::*mEraser)(const KeyType&, int);
};

}} // namespace ork::reflect
