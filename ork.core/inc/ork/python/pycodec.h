////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/pch.h>
#include <ork/python/context.h>
#include <ork/python/wraprawpointer.inl>
#include <ork/kernel/varmap.inl>
#include <iostream>

namespace ork::python {

# define OrkPyAssert( x ) { if( (x) == 0 ) { \
   char buffer[1024]; \
   snprintf( buffer, sizeof(buffer), "Assert At: [File %s] [Line %d] [Reason: Assertion %s failed]", __FILE__, __LINE__, #x ); \
   try { \
        py::object traceback = py::module::import("traceback");\
        py::object formatted_stack = traceback.attr("format_stack")();\
        for (const auto& frame : formatted_stack) {\
            std::cout << py::str(frame);\
        }\
    } catch (const py::error_already_set& e) {\
        std::cerr << "Failed to print Python call stack: " << e.what() << std::endl;\
    }\
   OrkAssertFunction(&buffer[0]);  \
   } \
  }

struct pybind11adapter{
  using object_t = pybind11::object;
  using handle_t = pybind11::handle;
  using kwargs_t = pybind11::kwargs;
  using list_t = pybind11::list;

  using varval_t    = varmap::var_t;
  using decoderfn_t = std::function<void(const object_t& inpval, varval_t& outval)>;
  using encoderfn_t = std::function<void(const varval_t& inpval, object_t& outval)>;
  using decoderfn64_t = std::function<void(const object_t& inpval, svar64_t& outval)>;
  using encoderfn64_t = std::function<void(const svar64_t& inpval, object_t& outval)>;

  template <typename T> static T cast2ork(const object_t& obj) {
    return obj.cast<T>();
  }

/*        [](const varval_t& inpval, ADAPTER::object_t& outval) { // encoder
          auto rawval = inpval.get<ORKTYPE>();
          outval      = ADAPTER::cast(PYREPR(rawval));
        },*/

  //////////////////////////////////
  using kw_arg_pair_t = std::pair<std::string, object_t>;
  static std::vector<kw_arg_pair_t> decodeKwArgs(kwargs_t py_args){
    std::vector<kw_arg_pair_t> kw_args;
    if (py_args) {
      for (auto item : py_args) {
        auto key = pybind11::cast<std::string>(item.first);
        auto obj = pybind11::cast<pybind11::object>(item.second);
        kw_args.push_back(std::make_pair(key,obj));
        //auto val = this->decode(obj);
        //rval.setValueForKey(key, val);
      }
    }
    return kw_args;
  }
  //////////////////////////////////
  static std::vector<object_t> decodeList(list_t py_args){
    std::vector<object_t> obj_list;
    for (auto list_item : py_args) {
      auto as_obj = pybind11::cast<object_t>(list_item);
      obj_list.push_back(as_obj);
    }
    return obj_list;
  }
  //////////////////////////////////

  template <typename T> static object_t handle2object(const T& obj) {
    return pybind11::cast<object_t>(obj);
  }
  template <typename T> static object_t cast_to_pyobject(const T& obj) {
    return pybind11::cast<object_t>(obj);
  }
  template <typename T> static object_t cast_to_pyhandle(const T& obj) {
    return pybind11::cast<handle_t>(obj);
  }
  template <typename T> static void cast_to_var(const object_t& inpval, varval_t& outval) {
    auto ork_val = cast2ork<T>(inpval);
    outval.set<T>(ork_val);
  }
  template <typename T> static void  cast_to_v64(const object_t& inpval, svar64_t& outval) {
    auto ork_val = cast2ork<T>(inpval);
    outval.set<T>(ork_val);
  }

  template <typename T> static object_t cast_var_to_py(const varval_t& var) {
    return pybind11::cast(var.get<T>());
  }
  template <typename T> static object_t cast_v64_to_py(const svar64_t& v64) {
    return pybind11::cast(v64.get<T>());
  }
};
struct nanobindadapter{
};

using varval_t    = varmap::var_t;


template <typename ADAPTER>
struct ORK_API TypeCodec {
  //////////////////////////////////
  using decoderfn_t = std::function<void(const typename ADAPTER::object_t& inpval, varval_t& outval)>;
  using encoderfn_t = std::function<void(const varval_t& inpval, typename ADAPTER::object_t& outval)>;
  using decoderfn64_t = std::function<void(const typename ADAPTER::object_t& inpval, svar64_t& outval)>;
  using encoderfn64_t = std::function<void(const svar64_t& inpval, typename ADAPTER::object_t& outval)>;
  
  //////////////////////////////////
  static std::shared_ptr<TypeCodec> instance();
  ADAPTER::object_t encode(const varval_t& val) const;
  ADAPTER::object_t encode64(const svar64_t& val) const;
  varval_t decode(const ADAPTER::object_t& val) const;
  svar64_t decode64(const ADAPTER::object_t& val) const;
  void registerCodec(
      const ADAPTER::object_t& pytype, //
      const ork::TypeId& orktypeid,
      encoderfn_t efn,
      decoderfn_t dfn);
  void registerCodec64(
      const ADAPTER::object_t& pytype, //
      const ork::TypeId& orktypeid,
      encoderfn64_t efn,
      decoderfn64_t dfn);
  //////////////////////////////////
  // register std codec (will reduce boilerplate for a lot of cases)
  //////////////////////////////////
  template <typename ORKTYPE> void registerStdCodec(const ADAPTER::object_t& pytype) {
    this->registerCodec(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const varval_t& inpval, ADAPTER::object_t& outval) { // encoder
          outval = ADAPTER::template cast_var_to_py<ORKTYPE>(inpval);
        },
        [](const ADAPTER::object_t& inpval, varval_t& outval) { // decoder
          ADAPTER::template cast_to_var<ORKTYPE>(inpval,outval);
        });
    this->registerCodec64(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const svar64_t& inpval, ADAPTER::object_t& outval) { // encoder
          outval = ADAPTER::template cast_v64_to_py<ORKTYPE>(inpval);
        },
        [](const ADAPTER::object_t& inpval, svar64_t& outval) { // decoder
          ADAPTER::template cast_to_v64<ORKTYPE>(inpval,outval);
        });
  }
  template <typename ORKTYPE> void registerStdCodecBIG(const ADAPTER::object_t& pytype) {
    this->registerCodec(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const varval_t& inpval, ADAPTER::object_t& outval) { // encoder
          outval = ADAPTER::template cast_var_to_py<ORKTYPE>(inpval);
        },
        [](const ADAPTER::object_t& inpval, varval_t& outval) { // decoder
          ADAPTER::template cast_to_var<ORKTYPE>(inpval,outval);
        });
  }
  //////////////////////////////////
  // register std codec (will reduce boilerplate for a lot of cases)
  //////////////////////////////////
  template <typename PYREPR, typename ORKTYPE> void registerRawPtrCodec(const ADAPTER::object_t& pytype) {
    this->registerCodec(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const varval_t& inpval, ADAPTER::object_t& outval) { // encoder
          auto rawval = inpval.get<ORKTYPE>();
          auto pyrepr = PYREPR(rawval);
          //auto h = ADAPTER::template cast_to_pyobject(pyrepr);
          auto h = pybind11::cast(pyrepr);
          outval      = h; //ADAPTER::cast_to_pyobject(h);
        },
        [](const ADAPTER::object_t& inpval, varval_t& outval) { // decoder
          auto intermediate_val = inpval.template cast<PYREPR>();
          auto ptr_val          = intermediate_val.get();
          outval.set<ORKTYPE>(ptr_val);
        });
    this->registerCodec64(
        pytype, //
        TypeId::of<ORKTYPE>(),
        [](const svar64_t& inpval, ADAPTER::object_t& outval) { // encoder
          auto rawval = inpval.get<ORKTYPE>();
          auto pyrepr = PYREPR(rawval);
          //auto h = ADAPTER::template cast_to_pyobject<PYREPR>(pyrepr);
          auto h = pybind11::cast(pyrepr);
          outval      = h;//ADAPTER::cast_to_pyobject(h);
        },
        [](const ADAPTER::object_t& inpval, svar64_t& outval) { // decoder
          auto intermediate_val = inpval.template cast<PYREPR>();
          auto ptr_val          = intermediate_val.get();
          outval.set<ORKTYPE>(ptr_val);
        });
  }
  //////////////////////////////////
  inline varmap::VarMap decode_kwargs(ADAPTER::kwargs_t kwargs) { //
    auto from_py = ADAPTER::decodeKwArgs(kwargs);
    varmap::VarMap rval;
    for (auto item : from_py) {
      auto key = item.first;
      auto obj = item.second;
      auto val = this->decode(obj);
      rval.setValueForKey(key, val);
    }
    return rval;
  }
  //////////////////////////////////
  std::vector<varval_t> decodeList(ADAPTER::list_t py_args){
    auto as_objlist = ADAPTER::decodeList(py_args);
    std::vector<varval_t> decoded_list;
    for (auto list_item : as_objlist) {
      varval_t val  = this->decode(list_item);
      decoded_list.push_back(val);
    }
    return decoded_list;
  }
  //////////////////////////////////
protected:
  TypeCodec();
  svar128_t _impl;
};

using typecodec_t = TypeCodec<pybind11adapter>;
using typecodec_ptr_t = std::shared_ptr<typecodec_t>;

} // namespace ork::python
