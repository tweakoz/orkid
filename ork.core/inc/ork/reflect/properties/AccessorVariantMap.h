////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include "IMap.h"
#include <ork/reflect/Command.h>

#include <ork/config/config.h>

namespace ork { namespace reflect {

class BidirectionalSerializer;
class Command;

class AccessorVariantMapContext {
public:
  AccessorVariantMapContext(BidirectionalSerializer& bidi);
  BidirectionalSerializer& Bidi();
  void BeginItem();
  void BeginValue();
  void EndItem();

private:
  BidirectionalSerializer& mBidi;
  Command mItemCommand;
  Command mAttributeCommand;
};

class AccessorVariantMap : public IMap {
  static void GetClassStatic(); // Kill inherited GetClassStatic()
public:
  typedef void (*SerializerCallbackItem)(AccessorVariantMapContext& ctx, BidirectionalSerializer&);

  static const int kDeserializeInsertItem = -1;

  AccessorVariantMap(
      bool (Object::*)(IDeserializer&, int, ISerializer&),
      bool (Object::*)(IDeserializer&, int, IDeserializer*) const,
      bool (Object::*)(AccessorVariantMapContext&) const);

private:
  bool (Object::*mReadItem)(IDeserializer& key, int, ISerializer& value);
  bool (Object::*mWriteItem)(IDeserializer& key, int, IDeserializer* value) const;
  bool (Object::*mMapSerialization)(AccessorVariantMapContext&) const;

  void deserialize(IDeserializer& serializer, object_ptr_t) const override;
  void serialize(ISerializer& serializer, object_constptr_t) const override;
  void deserializeItem(IDeserializer* value, IDeserializer& key, int, object_ptr_t) const override;
  void serializeItem(ISerializer& value, IDeserializer& key, int, object_constptr_t) const override;
};

}} // namespace ork::reflect
