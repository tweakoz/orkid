////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/stream/InputStreamBuffer.h>
#include <ork/kernel/string/StringPool.h>

namespace ork { namespace reflect { namespace serialize {

class BinaryDeserializer final : public IDeserializer {
public:
  BinaryDeserializer(stream::IInputStream& stream);
  ~BinaryDeserializer();

  void deserializeSharedObject(object_ptr_t&) override;
  void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;

  void beginCommand(Command&) override;
  void endCommand(const Command&) override;
  void deserializeItem() override;

private:
  template <typename T> //
  void Read(T&);

  char Peek();
  bool Match(char c);

  stream::InputStreamBuffer<1> mStream;
  StringPool mStringPool;
};

}}} // namespace ork::reflect::serialize
