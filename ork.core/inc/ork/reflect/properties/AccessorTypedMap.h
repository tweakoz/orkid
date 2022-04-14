////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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
