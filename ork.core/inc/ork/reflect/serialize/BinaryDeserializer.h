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

  void deserialize(bool&) override;
  void deserialize(char&) override;
  void deserialize(short&) override;
  void deserialize(int&) override;
  void deserialize(long&) override;
  void deserialize(float&) override;
  void deserialize(double&) override;

  void deserializeSharedObject(object_ptr_t&) override;
  void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;

  void deserialize(MutableString&) override;
  void deserialize(ResizableString&) override;
  void deserializeData(unsigned char*, size_t) override;

  // bool ReferenceObject(rtti::ICastable*) override;
  void beginCommand(Command&) override;
  void endCommand(const Command&) override;

private:
  // int FindObject(rtti::ICastable* object);

  template <typename T> bool Read(T&);

  char Peek();
  bool Match(char c);

  stream::InputStreamBuffer<1> mStream;
  // using trackervect_t = std::unordered_map<std::string, rtti::castable_rawptr_t>;
  // trackervect_t _reftracker;
  // const Command* mCurrentCommand;
  StringPool mStringPool;
};

}}} // namespace ork::reflect::serialize
