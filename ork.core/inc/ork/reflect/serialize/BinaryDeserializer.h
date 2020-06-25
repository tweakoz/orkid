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

  bool Deserialize(bool&) override;
  bool Deserialize(char&) override;
  bool Deserialize(short&) override;
  bool Deserialize(int&) override;
  bool Deserialize(long&) override;
  bool Deserialize(float&) override;
  bool Deserialize(double&) override;

  bool Deserialize(const AbstractProperty*) override;
  bool deserializeObject(rtti::ICastable*&) override;
  bool deserializeSharedObject(rtti::castable_ptr_t&) override;
  bool deserializeObjectProperty(const ObjectProperty*, Object*) override;

  bool Deserialize(MutableString&) override;
  bool Deserialize(ResizableString&) override;
  bool DeserializeData(unsigned char*, size_t) override;

  bool ReferenceObject(rtti::ICastable*) override;
  bool BeginCommand(Command&) override;
  bool EndCommand(const Command&) override;

private:
  int FindObject(rtti::ICastable* object);

  template <typename T> bool Read(T&);

  char Peek();
  bool Match(char c);

  stream::InputStreamBuffer<1> mStream;
  using trackervect_t = std::unordered_map<std::string, rtti::castable_rawptr_t>;
  trackervect_t _reftracker;
  const Command* mCurrentCommand;
  StringPool mStringPool;
};

}}} // namespace ork::reflect::serialize
