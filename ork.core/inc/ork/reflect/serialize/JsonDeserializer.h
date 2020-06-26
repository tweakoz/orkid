////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/reflect/IDeserializer.h>
#include <ork/stream/InputStreamBuffer.h>

#include <ork/orkstl.h>
#include <rapidjson/reader.h>
#include <rapidjson/document.h>

namespace ork { namespace reflect { namespace serialize {

class JsonDeserializer : public IDeserializer {
public:
  JsonDeserializer(stream::IInputStream& stream);

  void deserialize(bool&) override;
  void deserialize(char&) override;
  void deserialize(short&) override;
  void deserialize(int&) override;
  void deserialize(long&) override;
  void deserialize(float&) override;
  void deserialize(double&) override;

  void deserialize(MutableString&) override;
  void deserialize(ResizableString&) override;
  void deserializeData(unsigned char*, size_t) override;

  void deserializeSharedObject(object_ptr_t&) override;
  void deserializeObjectProperty(const ObjectProperty*, object_ptr_t) override;

  void beginCommand(Command&) override;
  void endCommand(const Command&) override;

private:
  stream::InputStreamBuffer<1024 * 4> mStream;

  ////////////////////////////////////////////

  rapidjson::Document _document;
};

}}} // namespace ork::reflect::serialize
