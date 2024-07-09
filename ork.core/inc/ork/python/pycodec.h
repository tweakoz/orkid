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

#include <ork/python/obind/nanobind.h>
#include <ork/python/obind/stl/detail/traits.h>

#define OrkPyAssert(x)                                                                                                             \
  {                                                                                                                                \
    if ((x) == 0) {                                                                                                                \
      char buffer[1024];                                                                                                           \
      snprintf(buffer, sizeof(buffer), "Assert At: [File %s] [Line %d] [Reason: Assertion %s failed]", __FILE__, __LINE__, #x);    \
      try {                                                                                                                        \
        pyb11::object traceback       = pyb11::module::import("traceback");                                                        \
        pyb11::object formatted_stack = traceback.attr("format_stack")();                                                          \
        for (const auto& frame : formatted_stack) {                                                                                \
          std::cout << pyb11::str(frame);                                                                                          \
        }                                                                                                                          \
      } catch (const pyb11::error_already_set& e) {                                                                                \
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

namespace pyb11    = pybind11;
namespace pyb11det = pybind11::detail;

struct pybind11adapter {

  using codec_t     = TypeCodec<pybind11adapter>;
  using codec_ptr_t = std::shared_ptr<codec_t>;

  using module_t      = pyb11::module_;
  using object_t      = pyb11::object;
  using handle_t      = pyb11::handle;
  using kwargs_t      = pyb11::kwargs;
  using kw_arg_pair_t = std::pair<std::string, object_t>;
  using list_t        = pyb11::list;
  using str_t         = pyb11::str;
  using float_t       = pyb11::float_;
  using int_t         = pyb11::int_;

  using varval_t      = varmap::var_t;
  using decoderfn_t   = std::function<void(const object_t& inpval, varval_t& outval)>;
  using encoderfn_t   = std::function<void(const varval_t& inpval, object_t& outval)>;
  using decoderfn64_t = std::function<void(const object_t& inpval, svar64_t& outval)>;
  using encoderfn64_t = std::function<void(const svar64_t& inpval, object_t& outval)>;

  template <typename T> static T _cast2ork(const object_t& obj);

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

  template <typename... Args> static auto init(Args&&... args);

  //////////////////////////////////

  template <typename type_, typename... options>           //
  class _clazz : public pyb11::class_<type_, options...> { //

    using Base = pyb11::class_<type_, options...>;

  public:
    //
    template <typename... Extra>
    _clazz(handle_t scope, const char* name, const Extra&... extra)
        : Base(scope, name, extra...) {
    }
    //
    template <typename Func> _clazz& method(Func&& f) {
      Base::def(f);
      return *this;
    }
    //
    template <typename Func> _clazz& method(const char* name_, Func&& f) {
      auto& ref = Base::def(name_, f);
      return *this;
    }
    //
    template <typename Func, typename... Extra> _clazz& method(const char* name_, Func&& f, const Extra&... extra) {
      auto& ref = Base::def(name_, f, extra...);
      return *this;
    }
    //
    template <typename Func,typename... Extra> _clazz& prop_ro(const char* name_, Func&& f, const Extra&... extra) {
      auto& ref = Base::def_property_readonly(name_, f, extra...);
      return *this;
    }
    //
    /*
      template <typename Func> _clazz& constructor(Func&& f) {
        auto& ref =  Base::def(f);
        return *this;
      }*/
    template <typename... Extra> _clazz& construct(const pyb11det::initimpl::constructor<>& init, const Extra&... extra) {
      std::move(init).execute(*this, extra...);
      return *this;
    }
    template <typename... Args, typename... Extra>
    _clazz& construct(pyb11det::initimpl::factory<Args...>&& init, const Extra&... extra) {
      // auto& ref = Base::def(pyb11::init(std::move(init)), extra...);
      std::move(init).execute(*this, extra...);
      return *this;
    }
    /*
      template <typename... Args, typename... Extra>
      _clazz &constructor(const pyb11det::initimpl::alias_constructor<Args...> &init, const Extra &...extra) {
        return *this;
      }*/
  };

  //////////////////////////////////

  template <typename type_, typename... options, typename... Extra>
  static auto clazz(module_t& scope, const char* name, const Extra&... extra) {
    return _clazz<type_, options...>(scope, name, extra...);
  }
};

///////////////////////////////////////////////////////////////////////////////

struct nanobindadapter {

  using codec_t     = TypeCodec<nanobindadapter>;
  using codec_ptr_t = std::shared_ptr<codec_t>;

  using module_t      = obind::module_;
  using object_t      = obind::object;
  using handle_t      = obind::handle;
  using kwargs_t      = obind::kwargs;
  using kw_arg_pair_t = std::pair<std::string, object_t>;
  using list_t        = obind::list;
  using str_t         = obind::str;
  using float_t       = obind::float_;
  using int_t         = obind::int_;

  using varval_t      = varmap::var_t;
  using decoderfn_t   = std::function<void(const object_t& inpval, varval_t& outval)>;
  using encoderfn_t   = std::function<void(const varval_t& inpval, object_t& outval)>;
  using decoderfn64_t = std::function<void(const object_t& inpval, svar64_t& outval)>;
  using encoderfn64_t = std::function<void(const svar64_t& inpval, object_t& outval)>;

  template <typename T> static T _cast2ork(const object_t& obj);

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

  template <typename... Args> static auto init(Args&&... args);

  //////////////////////////////////

  template <typename type_>           //
  class _clazz : public obind::class_<type_> { //

    using Base = obind::class_<type_>;

  public:
    //
    template <typename... Extra>
    _clazz(handle_t scope, const char* name, const Extra&... extra)
        : Base(scope, name, extra...) {
    }
    //
    template <typename Func> _clazz& method(Func&& f) {
      Base::def(f);
      return *this;
    }
    //
    template <typename Func> _clazz& method(const char* name_, Func&& f) {
      auto& ref = Base::def(name_, f);
      return *this;
    }
    //
    template <typename Func, typename... Extra> _clazz& method(const char* name_, Func&& f, const Extra&... extra) {
      auto& ref = Base::def(name_, f, extra...);
      return *this;
    }
    //
    template <typename Func,typename... Extra> _clazz& prop_ro(const char* name_, Func&& f, const Extra&... extra) {
      auto& ref = Base::def_prop_ro(name_, f, extra...);
      return *this;
    }
    //
    /*
      template <typename Func> _clazz& constructor(Func&& f) {
        auto& ref =  Base::def(f);
        return *this;
      }*/
    template <typename... Extra> _clazz& construct(const obind::init<>& init, const Extra&... extra) {
      Base::def(std::move(init), extra...);
      return *this;
    }
    template <typename... Args, typename... Extra>
    _clazz& construct(obind::init<Args...>&& init, const Extra&... extra) {
      Base::def(std::move(init), extra...);
      return *this;
    }
    /*
      template <typename... Args, typename... Extra>
      _clazz &constructor(const obind::detail::initimpl::alias_constructor<Args...> &init, const Extra &...extra) {
        return *this;
      }*/
  };

  //////////////////////////////////

  template <typename type_, typename... options, typename... Extra>
  static auto clazz(module_t& scope, const char* name, const Extra&... extra) {
    return _clazz<type_>(scope, name, extra...);
  }
};

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
