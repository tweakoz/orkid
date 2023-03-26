////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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

  static const int kDeserializeInsertElement = -1;

  AccessorVariantMap(
      bool (Object::*)(IDeserializer&, int, ISerializer&),
      bool (Object::*)(serdes::node_ptr_t) const,
      bool (Object::*)(AccessorVariantMapContext&) const);

private:
  bool (Object::*mReadElement)(IDeserializer& key, int, ISerializer& value);
  bool (Object::*_writeelement)(serdes::node_ptr_t) const;
  bool (Object::*mMapSerialization)(AccessorVariantMapContext&) const;

  void deserialize(serdes::node_ptr_t) const override;
  void serialize(serdes::node_ptr_t) const override;
};

}} // namespace ork::reflect
