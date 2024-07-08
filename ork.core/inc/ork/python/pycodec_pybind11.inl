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

template <typename T> 
T pybind11adapter::cast2ork(const object_t& obj) {
    return obj.cast<T>();
}
    //////////////////////////////////
  template <typename T> pybind11::object pybind11adapter::handle2object(const T& obj) {
    return pybind11::cast<object_t>(obj);
  }
  template <typename T> pybind11::object pybind11adapter::cast_to_pyobject(const T& obj) {
    return pybind11::cast<object_t>(obj);
  }
  template <typename T> pybind11::object pybind11adapter::cast_to_pyhandle(const T& obj) {
    return pybind11::cast<handle_t>(obj);
  }
  template <typename T> void pybind11adapter::cast_to_var(const pybind11::object& inpval, varval_t& outval) {
    auto ork_val = cast2ork<T>(inpval);
    outval.set<T>(ork_val);
  }
  template <typename T> void  pybind11adapter::cast_to_v64(const pybind11::object& inpval, svar64_t& outval) {
    auto ork_val = cast2ork<T>(inpval);
    outval.set<T>(ork_val);
  }

  template <typename T> pybind11::object pybind11adapter::cast_var_to_py(const varval_t& var) {
    return pybind11::cast(var.get<T>());
  }
  template <typename T> pybind11::object pybind11adapter::cast_v64_to_py(const svar64_t& v64) {
    return pybind11::cast(v64.get<T>());
  }
////////////////////////////////////////////////////////////////

struct ReflectionCodecAdapter {
  inline ReflectionCodecAdapter(object_ptr_t obj, //
                         pb11_typecodec_ptr_t codec)
      : _object(obj)
      , _codec(codec) {
  }
  inline virtual ~ReflectionCodecAdapter() {
  }
  virtual pybind11::object encode() = 0;
  object_ptr_t _object;
  pb11_typecodec_ptr_t _codec;
};

////////////////////////////////////////////////////////////////

template <typename T, typename PT> struct TypedArrayCodecAdapter : public ReflectionCodecAdapter {
  using typedprop_t = reflect::ITyped<T>;
  using pythontype_t = PT;
  using arrayprop_t = reflect::ITypedArray<T>;
  TypedArrayCodecAdapter(object_ptr_t obj, //
                         arrayprop_t* prop, //
                         pb11_typecodec_ptr_t codec) //
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
                          pb11_typecodec_ptr_t codec);
  pybind11::object encode() final;

  arrayprop_t* _property = nullptr;
};

////////////////////////////////////////////////////////////////

struct ObjectMapCodecAdapter : public ReflectionCodecAdapter {
  using mapprop_t = reflect::IObjectMap;
  ObjectMapCodecAdapter(object_ptr_t obj, //
                       mapprop_t* prop, //
                       pb11_typecodec_ptr_t codec);
  pybind11::object encode() final ;
  mapprop_t* _property = nullptr;
};

////////////////////////////////////////////////////////////////

struct SDObjectMapCodecAdapter : public ReflectionCodecAdapter {
  using mapprop_t = reflect::ITypedMap<std::string,object_ptr_t>;
  SDObjectMapCodecAdapter(object_ptr_t obj, //
                          mapprop_t* prop, //
                          pb11_typecodec_ptr_t codec);
  pybind11::object encode() final;
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
