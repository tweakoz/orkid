////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/prop.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/datablock.h>
#include <ork/kernel/fixedstring.hpp>
#include <ork/python/pycodec.inl>
#include <iostream>

namespace py = pybind11;
using namespace pybind11::literals;

namespace ork::python {
////////////////////////////////////////////////////////////////////////////////

struct PyCodecItem {
  py::object _pytype;
  ork::TypeId _orktype;
  pybind11adapter::encoderfn_t _encoder;
  pybind11adapter::decoderfn_t _decoder;
};
struct PyCodecItem64 {
  py::object _pytype;
  ork::TypeId _orktype;
  pybind11adapter::encoderfn64_t _encoder;
  pybind11adapter::decoderfn64_t _decoder;
};
struct PyCodecImpl {
  varval_t decode(const py::object& val) const;
  svar64_t decode64(const py::object& val) const;
  py::object encode(const varval_t& val) const;
  py::object encode64(const svar64_t& val) const;
  std::unordered_map<ork::TypeId::hashtype_t, PyCodecItem> _codecs_by_orktype;
  std::unordered_map<ork::TypeId::hashtype_t, PyCodecItem64> _codecs64_by_orktype;
};

///////////////////////////////////////////////////////////////////////////////

py::object PyCodecImpl::encode(const varval_t& val) const {
  py::object rval;
  if (not val.isSet()) {
    return py::none();
  }
  auto orktypeid = val.getOrkTypeId();
  auto it        = _codecs_by_orktype.find(orktypeid._hashed);
  if (it != _codecs_by_orktype.end()) {
    auto& codec = it->second;
    codec._encoder(val, rval);
  } else {
    // try primitives
    if (auto as_bool = val.tryAs<bool>()) {
      return py::bool_(as_bool.value());
    } else if (auto as_float = val.tryAs<float>()) {
      return py::float_(as_float.value());
    } else if (auto as_double = val.tryAs<double>()) {
      return py::float_(as_double.value());
    } else if (auto as_int = val.tryAs<int>()) {
      return py::int_(as_int.value());
    } else if (auto as_uint32_t = val.tryAs<uint32_t>()) {
      return py::int_(as_uint32_t.value());
    } else if (auto as_uint64_t = val.tryAs<uint64_t>()) {
      return py::int_(as_uint64_t.value());
    } else if (auto as_str = val.tryAs<std::string>()) {
      return py::str(as_str.value());
    } else if (auto as_np = val.tryAs<std::nullptr_t>()) {
      return py::none();
    } else if (auto as_reflcodec = val.tryAs<refl_codec_adapter_ptr_t>()) {
      return as_reflcodec.value()->encode();
    } else if (auto as_vmap = val.tryAs<varmap::VarMap>()) {
      return py::none();
    } else {
      printf("UNKNOWNTYPE<%s>\n", val.typeName());
      OrkAssert(false);
      throw std::runtime_error("pycodec-encode: unregistered type");
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

py::object PyCodecImpl::encode64(const svar64_t& val) const {
  py::object rval;
  if (not val.isSet()) {
    return py::none();
  }
  auto orktypeid = val.getOrkTypeId();
  auto it        = _codecs64_by_orktype.find(orktypeid._hashed);
  if (it != _codecs64_by_orktype.end()) {
    auto& codec = it->second;
    codec._encoder(val, rval);
  } else {
    // try primitives
    if (auto as_bool = val.tryAs<bool>()) {
      return py::bool_(as_bool.value());
    } else if (auto as_float = val.tryAs<float>()) {
      return py::float_(as_float.value());
    } else if (auto as_double = val.tryAs<double>()) {
      return py::float_(as_double.value());
    } else if (auto as_int = val.tryAs<int>()) {
      return py::int_(as_int.value());
    } else if (auto as_uint32_t = val.tryAs<uint32_t>()) {
      return py::int_(as_uint32_t.value());
    } else if (auto as_uint64_t = val.tryAs<uint64_t>()) {
      return py::int_(as_uint64_t.value());
    } else if (auto as_str = val.tryAs<std::string>()) {
      return py::str(as_str.value());
    } else if (auto as_np = val.tryAs<std::nullptr_t>()) {
      return py::none();
    } else if (auto as_reflcodec = val.tryAs<refl_codec_adapter_ptr_t>()) {
      return as_reflcodec.value()->encode();
    } else if (auto as_vmap = val.tryAs<varmap::VarMap>()) {
      return py::none();
    } else {
      printf("UNKNOWNTYPE<%s>\n", val.typeName());
      OrkAssert(false);
      throw std::runtime_error("pycodec-encode: unregistered type");
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

varval_t PyCodecImpl::decode(const py::object& val) const {
  varval_t rval;
  auto type = val.get_type();
  bool done = false;
  for (auto& codec_item : _codecs_by_orktype) {
    const auto& codec = codec_item.second;
    if (type.is(codec._pytype)) {
      codec._decoder(val, rval);
      return rval;
    }
  }
  std::cout << "BadValue: " << val.cast<std::string>() << std::endl;
  throw std::runtime_error("pycodec-decode: unregistered type");
  OrkAssert(false); // unknown type!
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

svar64_t PyCodecImpl::decode64(const py::object& val) const {
  svar64_t rval;
  auto type = val.get_type();
  bool done = false;
  for (auto& codec_item : _codecs64_by_orktype) {
    const auto& codec = codec_item.second;
    if (type.is(codec._pytype)) {
      codec._decoder(val, rval);
      return rval;
    }
  }
  std::cout << "decode64 :: BadValue: " << val.cast<std::string>() << std::endl;
  throw std::runtime_error("pycodec-decode: unregistered type");
  OrkAssert(false); // unknown type!
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <> py::object pb11_typecodec_t::encode(const varval_t& val) const {
  return _impl.get<PyCodecImpl>().encode(val);
}

///////////////////////////////////////////////////////////////////////////////

template <> py::object pb11_typecodec_t::encode64(const svar64_t& val) const {
  return _impl.get<PyCodecImpl>().encode64(val);
}

///////////////////////////////////////////////////////////////////////////////

template <> varval_t pb11_typecodec_t::decode(const py::object& val) const {
  return _impl.get<PyCodecImpl>().decode(val);
}

///////////////////////////////////////////////////////////////////////////////

template <> svar64_t pb11_typecodec_t::decode64(const py::object& val) const {
  return _impl.get<PyCodecImpl>().decode64(val);
}

///////////////////////////////////////////////////////////////////////////////

template <>
void pb11_typecodec_t::registerCodec(
    const pybind11::object& pytype, //
    const ork::TypeId& orktypeid,
    encoderfn_t efn,
    decoderfn_t dfn) {
  auto& item    = _impl.get<PyCodecImpl>()._codecs_by_orktype[orktypeid._hashed];
  item._pytype  = pytype;
  item._orktype = orktypeid;
  item._encoder = efn;
  item._decoder = dfn;
}

///////////////////////////////////////////////////////////////////////////////

template <>
void pb11_typecodec_t::registerCodec64(
    const pybind11::object& pytype, //
    const ork::TypeId& orktypeid,
    encoderfn64_t efn,
    decoderfn64_t dfn) {
  auto& item    = _impl.get<PyCodecImpl>()._codecs64_by_orktype[orktypeid._hashed];
  item._pytype  = pytype;
  item._orktype = orktypeid;
  item._encoder = efn;
  item._decoder = dfn;
}

///////////////////////////////////////////////////////////////////////////////

ObjectArrayCodecAdapter::ObjectArrayCodecAdapter(
    object_ptr_t obj,           //
    arrayprop_t* prop,          //
    pb11_typecodec_ptr_t codec) //
    : ReflectionCodecAdapter(obj, codec)
    , _property(prop) {
}

///////////////////////////////////////////////////////////////////////////////

pybind11::object ObjectArrayCodecAdapter::encode() {
  pybind11::list pylist;
  size_t count = _property->count(_object);
  for (size_t i = 0; i < count; i++) {
    object_ptr_t sub_object = _property->accessObject(_object, i);
    pylist.append(_codec->encode(sub_object));
  }
  return std::move(pylist);
}

///////////////////////////////////////////////////////////////////////////////

ObjectMapCodecAdapter::ObjectMapCodecAdapter(
    object_ptr_t obj,           //
    mapprop_t* prop,            //
    pb11_typecodec_ptr_t codec) //
    : ReflectionCodecAdapter(obj, codec)
    , _property(prop) {
}

///////////////////////////////////////////////////////////////////////////////

pybind11::object ObjectMapCodecAdapter::encode() {
  pybind11::list pylist;
  auto as_kvarray = _property->enumerateElements(_object);
  size_t count    = as_kvarray.size();
  for (size_t i = 0; i < count; i++) {
    const auto& kvpair = as_kvarray[i];
    const auto& key    = kvpair.first;
    const auto& val    = kvpair.second;
    varmap::var_t asv;
    OrkAssert(false); // not implemented yet..
    // object_ptr_t sub_object = _property->accessObject(_object, i);
    // pylist.append(_codec->encode(sub_object));
  }
  return std::move(pylist);
}

///////////////////////////////////////////////////////////////////////////////

SDObjectMapCodecAdapter::SDObjectMapCodecAdapter(
    object_ptr_t obj,           //
    mapprop_t* prop,            //
    pb11_typecodec_ptr_t codec) //
    : ReflectionCodecAdapter(obj, codec)
    , _property(prop) {
}

///////////////////////////////////////////////////////////////////////////////

pybind11::object SDObjectMapCodecAdapter::encode() {
  pybind11::dict pydict;
  size_t count = _property->elementCount(_object);
  for (size_t i = 0; i < count; i++) {
    std::string key;
    object_ptr_t sub_object;
    _property->GetKey(_object, i, key);
    _property->GetVal(_object, key, sub_object);
    pydict[pybind11::str(key)] = _codec->encode(sub_object);
  }
  return std::move(pydict);
}

///////////////////////////////////////////////////////////////////////////////

template <> pb11_typecodec_t::TypeCodec() {
  _impl.make<PyCodecImpl>();
  /////////////////////////////////////////////////
  // builtin decoders
  /////////////////////////////////////////////////
  auto builtins   = py::module::import("builtins");
  auto bool_type  = builtins.attr("bool");
  auto int_type   = builtins.attr("int");
  auto float_type = builtins.attr("float");
  auto str_type   = builtins.attr("str");
  ///////////////////////////////
  registerCodec(
      bool_type, //
      TypeId::of<bool>(),
      [](const varval_t& inpval, pybind11::object& outval) { // encoder
        outval = py::bool_(inpval.get<bool>());
      },
      [](const py::object& inpval, varval_t& outval) { // decoder
        outval.set<bool>(inpval.cast<bool>());
      });
  ///////////////////////////////
  registerCodec(
      int_type, //
      TypeId::of<int>(),
      [](const varval_t& inpval, pybind11::object& outval) { // encoder
        outval = py::int_(inpval.get<int>());
      },
      [](const py::object& inpval, varval_t& outval) { // decoder
        outval.set<int>(inpval.cast<int>());
      });
  ///////////////////////////////
  registerCodec(
      float_type, //
      TypeId::of<float>(),
      [](const varval_t& inpval, pybind11::object& outval) { // encoder
        outval = py::float_(inpval.get<float>());
      },
      [](const py::object& inpval, varval_t& outval) { // decoder
        outval.set<float>(inpval.cast<float>());
      });
  ///////////////////////////////
  registerCodec(
      str_type, //
      TypeId::of<std::string>(),
      [](const varval_t& inpval, pybind11::object& outval) { // encoder
        outval = py::str(inpval.get<std::string>());
      },
      [](const py::object& inpval, varval_t& outval) { // decoder
        outval.set<std::string>(inpval.cast<std::string>());
      });
  ///////////////////////////////
  // tuple type (opaque hidden type)
  ///////////////////////////////
   using tuple_ptr_t = std::shared_ptr<py::tuple>;
   registerCodec(
        builtins.attr("tuple"),    // pytype
        TypeId::of<tuple_ptr_t>(), // c++type
        [](const varval_t& inpval, pybind11::object& outval) { // encoder
          py::gil_scoped_acquire acquire;
          outval = *(inpval.get<tuple_ptr_t>());
        },
        [](const py::object& inpval, varval_t& outval) { // decoder
          auto copy_of_tuple = std::make_shared<py::tuple>(inpval.cast<py::tuple>());
          outval.set<tuple_ptr_t>(copy_of_tuple);
        });
   registerCodec64(
        builtins.attr("tuple"),    // pytype
        TypeId::of<tuple_ptr_t>(), // c++type
        [](const svar64_t& inpval, pybind11::object& outval) { // encoder
          py::gil_scoped_acquire acquire;
          outval = *(inpval.get<tuple_ptr_t>());
        },
        [](const py::object& inpval, svar64_t& outval) { // decoder
          auto copy_of_tuple = std::make_shared<py::tuple>(inpval.cast<py::tuple>());
          outval.set<tuple_ptr_t>(copy_of_tuple);
        });
  ///////////////////////////////
  // list type (opaque hidden type)
  ///////////////////////////////
   using list_ptr_t = std::shared_ptr<py::list>;
   registerCodec(
        builtins.attr("list"),    // pytype
        TypeId::of<list_ptr_t>(), // c++type
        [](const varval_t& inpval, pybind11::object& outval) { // encoder
          py::gil_scoped_acquire acquire;
          outval = *(inpval.get<list_ptr_t>());
        },
        [](const py::object& inpval, varval_t& outval) { // decoder
          auto copy_of_list = std::make_shared<py::list>(inpval.cast<py::list>());
          outval.set<list_ptr_t>(copy_of_list);
        });
   registerCodec64(
        builtins.attr("list"),    // pytype
        TypeId::of<list_ptr_t>(), // c++type
        [](const svar64_t& inpval, pybind11::object& outval) { // encoder
          py::gil_scoped_acquire acquire;
          outval = *(inpval.get<list_ptr_t>());
        },
        [](const py::object& inpval, svar64_t& outval) { // decoder
          auto copy_of_list = std::make_shared<py::list>(inpval.cast<py::list>());
          outval.set<list_ptr_t>(copy_of_list);
        });
  ///////////////////////////////
  registerCodec64(
      bool_type, //
      TypeId::of<bool>(),
      [](const svar64_t& inpval, pybind11::object& outval) { // encoder
        outval = py::bool_(inpval.get<bool>());
      },
      [](const py::object& inpval, svar64_t& outval) { // decoder
        outval.set<bool>(inpval.cast<bool>());
      });
  ///////////////////////////////
  registerCodec64(
      int_type, //
      TypeId::of<int>(),
      [](const svar64_t& inpval, pybind11::object& outval) { // encoder
        outval = py::int_(inpval.get<int>());
      },
      [](const py::object& inpval, svar64_t& outval) { // decoder
        outval.set<int>(inpval.cast<int>());
      });
  ///////////////////////////////
  registerCodec64(
      float_type, //
      TypeId::of<float>(),
      [](const svar64_t& inpval, pybind11::object& outval) { // encoder
        outval = py::float_(inpval.get<float>());
      },
      [](const py::object& inpval, svar64_t& outval) { // decoder
        outval.set<float>(inpval.cast<float>());
      });
  ///////////////////////////////
  registerCodec64(
      str_type, //
      TypeId::of<std::string>(),
      [](const svar64_t& inpval, pybind11::object& outval) { // encoder
        outval = py::str(inpval.get<std::string>());
      },
      [](const py::object& inpval, svar64_t& outval) { // decoder
        outval.set<std::string>(inpval.cast<std::string>());
      });
}

///////////////////////////////////////////////////////////////////////////////

template <> std::shared_ptr<pb11_typecodec_t> pb11_typecodec_t::instance() { // static
  struct TypeCodecFactory : public pb11_typecodec_t {
    TypeCodecFactory()
        : TypeCodec(){};
  };
  static auto _instance = std::make_shared<TypeCodecFactory>();
  return _instance;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<pybind11adapter::kw_arg_pair_t> pybind11adapter::decodeKwArgs(kwargs_t py_args) {
  std::vector<pybind11adapter::kw_arg_pair_t> kw_args;
  if (py_args) {
    for (auto item : py_args) {
      auto key = pybind11::cast<std::string>(item.first);
      auto obj = pybind11::cast<pybind11::object>(item.second);
      kw_args.push_back(std::make_pair(key, obj));
      // auto val = this->decode(obj);
      // rval.setValueForKey(key, val);
    }
  }
  return kw_args;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<pybind11::object> pybind11adapter::decodeList(list_t py_args) {
  std::vector<pybind11::object> obj_list;
  for (auto list_item : py_args) {
    auto as_obj = pybind11::cast<pybind11::object>(list_item);
    obj_list.push_back(as_obj);
  }
  return obj_list;
}
} // namespace ork::python
