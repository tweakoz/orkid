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

namespace py = nanobind;
using namespace nanobind::literals;
using codec_t = ork::python::nanobindadapter::codec_t;

namespace ork::python {
////////////////////////////////////////////////////////////////////////////////

struct NanoCodecItem {
  py::object _pytype;
  ork::TypeId _orktype;
  nanobindadapter::encoderfn_t _encoder;
  nanobindadapter::decoderfn_t _decoder;
};
struct NanoCodecItem64 {
  py::object _pytype;
  ork::TypeId _orktype;
  nanobindadapter::encoderfn64_t _encoder;
  nanobindadapter::decoderfn64_t _decoder;
};
struct NanoCodecImpl {
  varval_t decode(const py::object& val) const;
  svar64_t decode64(const py::object& val) const;
  py::object encode(const varval_t& val) const;
  py::object encode64(const svar64_t& val) const;
  std::unordered_map<ork::TypeId::hashtype_t, NanoCodecItem> _codecs_by_orktype;
  std::unordered_map<ork::TypeId::hashtype_t, NanoCodecItem64> _codecs64_by_orktype;
};

///////////////////////////////////////////////////////////////////////////////

py::object NanoCodecImpl::encode(const varval_t& val) const {
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
    //} else if (auto as_str = val.tryAs<std::string>()) {
    //  return py::str_(as_str.value());
    } else if (auto as_np = val.tryAs<std::nullptr_t>()) {
      return py::none();
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

py::object NanoCodecImpl::encode64(const svar64_t& val) const {
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
    //} else if (auto as_str = val.tryAs<std::string>()) {
    //  return py::string(as_str.value());
    } else if (auto as_np = val.tryAs<std::nullptr_t>()) {
      return py::none();
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

varval_t NanoCodecImpl::decode(const py::object& val) const {
  varval_t rval;
  auto type = val.type();
  bool done = false;
  for (auto& codec_item : _codecs_by_orktype) {
    const auto& codec = codec_item.second;
    if (type.is(codec._pytype)) {
      codec._decoder(val, rval);
      return rval;
    }
  }
  std::cout << "BadValue: " << py::cast<std::string>(val) << std::endl;
  throw std::runtime_error("pycodec-decode: unregistered type");
  OrkAssert(false); // unknown type!
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

svar64_t NanoCodecImpl::decode64(const py::object& val) const {
  svar64_t rval;
  auto type = val.type();
  bool done = false;
  for (auto& codec_item : _codecs64_by_orktype) {
    const auto& codec = codec_item.second;
    if (type.is(codec._pytype)) {
      codec._decoder(val, rval);
      return rval;
    }
  }
  std::cout << "decode64 :: BadValue: " << py::cast<std::string>(val) << std::endl;
  throw std::runtime_error("pycodec-decode: unregistered type");
  OrkAssert(false); // unknown type!
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

template <> py::object codec_t::encode(const varval_t& val) const {
  return _impl.get<NanoCodecImpl>().encode(val);
}

///////////////////////////////////////////////////////////////////////////////

template <> py::object codec_t::encode64(const svar64_t& val) const {
  return _impl.get<NanoCodecImpl>().encode64(val);
}

///////////////////////////////////////////////////////////////////////////////

template <> varval_t codec_t::decode(const py::object& val) const {
  return _impl.get<NanoCodecImpl>().decode(val);
}

///////////////////////////////////////////////////////////////////////////////

template <> svar64_t codec_t::decode64(const py::object& val) const {
  return _impl.get<NanoCodecImpl>().decode64(val);
}

///////////////////////////////////////////////////////////////////////////////

template <>
void codec_t::registerCodec(
    const py::object& pytype, //
    const ork::TypeId& orktypeid,
    encoderfn_t efn,
    decoderfn_t dfn) {
  auto& item    = _impl.get<NanoCodecImpl>()._codecs_by_orktype[orktypeid._hashed];
  item._pytype  = pytype;
  item._orktype = orktypeid;
  item._encoder = efn;
  item._decoder = dfn;
}

///////////////////////////////////////////////////////////////////////////////

template <>
void codec_t::registerCodec64(
    const py::object& pytype, //
    const ork::TypeId& orktypeid,
    encoderfn64_t efn,
    decoderfn64_t dfn) {
  auto& item    = _impl.get<NanoCodecImpl>()._codecs64_by_orktype[orktypeid._hashed];
  item._pytype  = pytype;
  item._orktype = orktypeid;
  item._encoder = efn;
  item._decoder = dfn;
}

///////////////////////////////////////////////////////////////////////////////

template <> codec_t::TypeCodec() {
  _impl.make<NanoCodecImpl>();
  /////////////////////////////////////////////////
  // builtin decoders
  /////////////////////////////////////////////////
  auto builtins   = py::module_::import_("builtins");
  auto bool_type  = builtins.attr("bool");
  auto int_type   = builtins.attr("int");
  auto float_type = builtins.attr("float");
  auto str_type   = builtins.attr("str");
  ///////////////////////////////
  registerCodec(
      bool_type, //
      TypeId::of<bool>(),
      [](const varval_t& inpval, py::object& outval) { // encoder
        outval = py::bool_(inpval.get<bool>());
      },
      [](const py::object& inpval, varval_t& outval) { // decoder
        outval.set<bool>(py::cast<bool>(inpval));
      });
  ///////////////////////////////
  registerCodec(
      int_type, //
      TypeId::of<int>(),
      [](const varval_t& inpval, py::object& outval) { // encoder
        outval = py::int_(inpval.get<int>());
      },
      [](const py::object& inpval, varval_t& outval) { // decoder
        outval.set<int>(py::cast<int>(inpval));
      });
  ///////////////////////////////
  registerCodec(
      float_type, //
      TypeId::of<float>(),
      [](const varval_t& inpval, py::object& outval) { // encoder
        outval = py::float_(inpval.get<float>());
      },
      [](const py::object& inpval, varval_t& outval) { // decoder
        outval.set<float>(py::cast<float>(inpval));
      });
  ///////////////////////////////
  registerCodec(
      str_type, //
      TypeId::of<std::string>(),
      [](const varval_t& inpval, py::object& outval) { // encoder
        outval = py::cast(inpval.get<std::string>());
      },
      [](const py::object& inpval, varval_t& outval) { // decoder
        outval.set<std::string>(py::cast<std::string>(inpval));
      });
  ///////////////////////////////
  registerCodec64(
      bool_type, //
      TypeId::of<bool>(),
      [](const svar64_t& inpval, py::object& outval) { // encoder
        outval = py::bool_(inpval.get<bool>());
      },
      [](const py::object& inpval, svar64_t& outval) { // decoder
        outval.set<bool>(py::cast<bool>(inpval));
      });
  ///////////////////////////////
  registerCodec64(
      int_type, //
      TypeId::of<int>(),
      [](const svar64_t& inpval, py::object& outval) { // encoder
        outval = py::int_(inpval.get<int>());
      },
      [](const py::object& inpval, svar64_t& outval) { // decoder
        outval.set<int>(py::cast<int>(inpval));
      });
  ///////////////////////////////
  registerCodec64(
      float_type, //
      TypeId::of<float>(),
      [](const svar64_t& inpval, py::object& outval) { // encoder
        outval = py::float_(inpval.get<float>());
      },
      [](const py::object& inpval, svar64_t& outval) { // decoder
        outval.set<float>(py::cast<float>(inpval));
      });
  ///////////////////////////////
  registerCodec64(
      str_type, //
      TypeId::of<std::string>(),
      [](const svar64_t& inpval, py::object& outval) { // encoder
        outval = py::cast(inpval.get<std::string>());
      },
      [](const py::object& inpval, svar64_t& outval) { // decoder
        outval.set<std::string>(py::cast<std::string>(inpval));
      });
}

///////////////////////////////////////////////////////////////////////////////

template <> std::shared_ptr<codec_t> codec_t::instance() { // static
  struct TypeCodecFactory : public codec_t {
    TypeCodecFactory()
        : TypeCodec(){};
  };
  static auto _instance = std::make_shared<TypeCodecFactory>();
  return _instance;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<nanobindadapter::kw_arg_pair_t> nanobindadapter::decodeKwArgs(kwargs_t py_args) {
  std::vector<nanobindadapter::kw_arg_pair_t> kw_args;
  if (py_args) {
    for (auto item : py_args) {
      auto key = py::cast<std::string>(item.first);
      auto obj = py::cast<py::object>(item.second);
      kw_args.push_back(std::make_pair(key, obj));
      // auto val = this->decode(obj);
      // rval.setValueForKey(key, val);
    }
  }
  return kw_args;
}

///////////////////////////////////////////////////////////////////////////////

std::vector<py::object> nanobindadapter::decodeList(list_t py_args) {
  std::vector<py::object> obj_list;
  for (auto list_item : py_args) {
    auto as_obj = py::cast<py::object>(list_item);
    obj_list.push_back(as_obj);
  }
  return obj_list;
}
} // namespace ork::python
