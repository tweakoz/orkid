////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/python/pycodec.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>
#include <ork/object/Object.h>
#include <ork/reflect/properties/ITyped.h>
#include <ork/reflect/properties/ITypedArray.h>
#include <ork/reflect/properties/ITypedMap.h>
#include <ork/reflect/properties/IObjectArray.h>
#include <ork/reflect/properties/IObject.h>
#include <ork/reflect/properties/IObjectMap.h>

namespace ork::python {

////////////////////////////////////////////////////////////////

struct ReflectionCodecAdapter {
  inline ReflectionCodecAdapter(object_ptr_t obj, //
                         typecodec_ptr_t codec)
      : _object(obj)
      , _codec(codec) {
  }
  inline virtual ~ReflectionCodecAdapter() {
  }
  virtual pybind11::object encode() = 0;
  object_ptr_t _object;
  typecodec_ptr_t _codec;
};

////////////////////////////////////////////////////////////////

template <typename T, typename PT> struct TypedArrayCodecAdapter : public ReflectionCodecAdapter {
  using typedprop_t = reflect::ITyped<T>;
  using pythontype_t = PT;
  using arrayprop_t = reflect::ITypedArray<T>;
  TypedArrayCodecAdapter(object_ptr_t obj, //
                         arrayprop_t* prop, //
                         typecodec_ptr_t codec) //
      : ReflectionCodecAdapter(obj, codec)
      , _property(prop) {
  }
  pybind11::object encode() final {
    pybind11::list pylist;
    size_t count = _property->count(_object);
    for (size_t i = 0; i < count; i++) {
      T value;
      _property->get(value, _object, i);
      pylist.append(pythontype_t(value));
    }
    return std::move(pylist);
  }
  arrayprop_t* _property = nullptr;
};

////////////////////////////////////////////////////////////////

struct ObjectArrayCodecAdapter : public ReflectionCodecAdapter {
  using arrayprop_t = reflect::IObjectArray;
  ObjectArrayCodecAdapter(object_ptr_t obj, //
                          arrayprop_t* prop, //
                          typecodec_ptr_t codec) //
      : ReflectionCodecAdapter(obj, codec)
      , _property(prop) {
  }
  pybind11::object encode() final {
    pybind11::list pylist;
    size_t count = _property->count(_object);
    for (size_t i = 0; i < count; i++) {
      object_ptr_t sub_object = _property->accessObject(_object, i);
      pylist.append(_codec->encode(sub_object));
    }
    return std::move(pylist);
  }
  arrayprop_t* _property = nullptr;
};

////////////////////////////////////////////////////////////////

struct ObjecMapCodecAdapter : public ReflectionCodecAdapter {
  using mapprop_t = reflect::IObjectMap;
  ObjecMapCodecAdapter(object_ptr_t obj, //
                       mapprop_t* prop, //
                       typecodec_ptr_t codec) //
      : ReflectionCodecAdapter(obj, codec)
      , _property(prop) {
  }
  pybind11::object encode() final {
    pybind11::list pylist;
    auto as_kvarray = _property->enumerateElements(_object);
    size_t count = as_kvarray.size();
    for (size_t i = 0; i < count; i++) {
      const auto& kvpair = as_kvarray[i];
      const auto& key = kvpair.first;
      const auto& val = kvpair.second;
      varmap::var_t asv;
      //object_ptr_t sub_object = _property->accessObject(_object, i);
      //pylist.append(_codec->encode(sub_object));
    }
    return std::move(pylist);
  }
  mapprop_t* _property = nullptr;
};

////////////////////////////////////////////////////////////////
using refl_codec_adapter_ptr_t = std::shared_ptr<ReflectionCodecAdapter>;
////////////////////////////////////////////////////////////////
using IntArrayPropertyAdapter= TypedArrayCodecAdapter<int, pybind11::int_>;
using FloatArrayPropertyAdapter = TypedArrayCodecAdapter<float, pybind11::float_>;
////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////
} //namespace ork::python {
