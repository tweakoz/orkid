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

#define OrkPyAssert(x)                                                                                                             \
  {                                                                                                                                \
    if ((x) == 0) {                                                                                                                \
      char buffer[1024];                                                                                                           \
      snprintf(buffer, sizeof(buffer), "Assert At: [File %s] [Line %d] [Reason: Assertion %s failed]", __FILE__, __LINE__, #x);    \
      try {                                                                                                                        \
        py::object traceback       = py::module::import("traceback");                                                              \
        py::object formatted_stack = traceback.attr("format_stack")();                                                             \
        for (const auto& frame : formatted_stack) {                                                                                \
          std::cout << py::str(frame);                                                                                             \
        }                                                                                                                          \
      } catch (const py::error_already_set& e) {                                                                                   \
        std::cerr << "Failed to print Python call stack: " << e.what() << std::endl;                                               \
      }                                                                                                                            \
      OrkAssertFunction(&buffer[0]);                                                                                               \
    }                                                                                                                              \
  }

///////////////////////////////////////////////////////////////////////////////
namespace ork::python {
///////////////////////////////////////////////////////////////////////////////

using varval_t = varmap::var_t;

template <typename ADAPTER> struct ORK_API TypeCodec;

///////////////////////////////////////////////////////////////////////////////

struct pybind11adapter {

  using codec_t       = TypeCodec<pybind11adapter>;
  using codec_ptr_t   = std::shared_ptr<codec_t>;

  using module_t      = pybind11::module_;
  using object_t      = pybind11::object;
  using handle_t      = pybind11::handle;
  using kwargs_t      = pybind11::kwargs;
  using kw_arg_pair_t = std::pair<std::string, object_t>;
  using list_t        = pybind11::list;
  using str_t         = pybind11::str;
  using float_t         = pybind11::float_;
  using int_t         = pybind11::int_;

  using varval_t      = varmap::var_t;
  using decoderfn_t   = std::function<void(const object_t& inpval, varval_t& outval)>;
  using encoderfn_t   = std::function<void(const varval_t& inpval, object_t& outval)>;
  using decoderfn64_t = std::function<void(const object_t& inpval, svar64_t& outval)>;
  using encoderfn64_t = std::function<void(const svar64_t& inpval, object_t& outval)>;

  template <typename T> static T cast2ork(const object_t& obj);

  //////////////////////////////////
  static std::vector<kw_arg_pair_t> decodeKwArgs(kwargs_t py_args);
  static std::vector<object_t> decodeList(list_t py_args);
  //////////////////////////////////

  template <typename T> static object_t handle2object(const T& obj);
  template <typename T> static object_t cast_to_pyobject(const T& obj);
  template <typename T> static object_t cast_to_pyhandle(const T& obj);
  template <typename T> static void cast_to_var(const object_t& inpval, varval_t& outval);
  template <typename T> static void cast_to_v64(const object_t& inpval, svar64_t& outval);
  template <typename T> static object_t cast_var_to_py(const varval_t& var);
  template <typename T> static object_t cast_v64_to_py(const svar64_t& v64);
  template <typename T> static bool isinstance(const object_t& inpval);

  template <typename... Args>
  static auto init(Args&&... args);
};

///////////////////////////////////////////////////////////////////////////////

struct nanobindadapter {};

///////////////////////////////////////////////////////////////////////////////

template <typename ADAPTER> struct ORK_API TypeCodec {

  //////////////////////////////////

  using decoderfn_t   = std::function<void(const typename ADAPTER::object_t& inpval, varval_t& outval)>;
  using encoderfn_t   = std::function<void(const varval_t& inpval, typename ADAPTER::object_t& outval)>;
  using decoderfn64_t = std::function<void(const typename ADAPTER::object_t& inpval, svar64_t& outval)>;
  using encoderfn64_t = std::function<void(const svar64_t& inpval, typename ADAPTER::object_t& outval)>;

  //////////////////////////////////

  static std::shared_ptr<TypeCodec> instance();

  //////////////////////////////////
  ADAPTER::object_t encode(const varval_t& val) const;
  ADAPTER::object_t encode64(const svar64_t& val) const;
  varval_t decode(const ADAPTER::object_t& val) const;
  svar64_t decode64(const ADAPTER::object_t& val) const;

  //////////////////////////////////

  varmap::VarMap decode_kwargs(ADAPTER::kwargs_t kwargs);
  std::vector<varval_t> decodeList(ADAPTER::list_t py_args);

  //////////////////////////////////
  // register std codec (will reduce boilerplate for a lot of cases)
  //////////////////////////////////

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

  template <typename ORKTYPE> //
  void registerStdCodec(const ADAPTER::object_t& pytype);

  template <typename ORKTYPE> //
  void registerStdCodecBIG(const ADAPTER::object_t& pytype);

  template <typename PYREPR, typename ORKTYPE> //
  void registerRawPtrCodec(const ADAPTER::object_t& pytype);

protected:
  TypeCodec();
  svar128_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

using pb11_typecodec_t     = TypeCodec<pybind11adapter>;
using pb11_typecodec_ptr_t = std::shared_ptr<pb11_typecodec_t>;

} // namespace ork::python
