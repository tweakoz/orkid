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
#include <ork/util/function_traits.inl>
#include <iostream>
#include <type_traits>
#include <utility>

#include <ork/python/obind/nanobind.h>
#include <ork/python/obind/stl/detail/traits.h>
#include <ork/python/obind/nanobind.h>
#include <ork/python/obind/trampoline.h>
#include <ork/python/obind/operators.h>
#include <ork/python/obind/stl/optional.h>
#include <ork/python/obind/stl/string.h>
#include <ork/python/obind/stl/pair.h>
#include <ork/python/obind/stl/function.h>
#include <ork/python/obind/stl/shared_ptr.h>
#include <ork/python/obind/stl/tuple.h>
#include <ork/python/obind/ndarray.h>
#include <ork/python/obind/eval.h>

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

struct BufferDescription {
  size_t scalar_size    = 0;
  size_t num_dimensions = 0;
  std::vector<size_t> shape;
  std::vector<int64_t> strides;
};

using bufferdesc_ptr_t = std::shared_ptr<BufferDescription>;

///////////////////////////////////////////////////////////////////////////////

namespace pyb11    = pybind11;
namespace pyb11det = pybind11::detail;

struct pybind11adapter {

  using codec_t     = TypeCodec<pybind11adapter>;
  using codec_ptr_t = std::shared_ptr<codec_t>;

  using module_t        = pyb11::module_;
  using object_t        = pyb11::object;
  using handle_t        = pyb11::handle;
  using kwargs_t        = pyb11::kwargs;
  using kw_arg_pair_t   = std::pair<std::string, object_t>;
  using list_t          = pyb11::list;
  using str_t           = pyb11::str;
  using float_t         = pyb11::float_;
  using int_t           = pyb11::int_;
  using buffer_handle_t = pyb11::buffer_info;

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
    template <typename Func, typename... Extra> _clazz& prop_ro(const char* name_, Func&& f, const Extra&... extra) {
      auto& ref = Base::def_property_readonly(name_, f, extra...);
      return *this;
    }
    template <typename Getter, typename Setter, typename... Extra>
    _clazz& prop_rw(const char* name_, Getter&& g, Setter&& s, const Extra&... extra) {
      auto& ref = Base::def_property(name_, g, s, extra...);
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

    template <typename Func, typename... Extra> _clazz& as_buffer(Func&& f, const Extra&... extra) {
      Base::def_buffer(f, extra...);
      return *this;
    }
    inline pybind11::object _from_buffer(buffer_handle_t buffer) {
      return pybind11::object();
      //fn_t X = f; // this works!!!
      //Base::def(pybind11::init(f), extra...);
      return *this;
    }
    template <typename ret_t, typename... Extra> _clazz& from_buffer(std::function<ret_t(const typename ret_t::element_t* data)> setter, const Extra&... extra) {
      //using namespace ::ork::utils;
      //using traits = function_signature_t<Func>;
      //using ret_t = typename traits::return_type;
      //using args_t = typename traits::argument_types; // tuple of args
      //using fn_t = std_function_type<ret_t, args_t>;
      auto the_fn = [setter](const pybind11::buffer& data) -> ret_t {
        using elem_t = typename ret_t::element_t;
        constexpr size_t count_t = ret_t::knumelements;
        auto float_fmt = pybind11::format_descriptor<float>::format();
        auto double_fmt = pybind11::format_descriptor<double>::format();
        //int_ format
        auto int_fmt = pybind11::format_descriptor<long int>::format();
        //auto int_fmt = pybind11::format_descriptor<pybind11::int_>::format();
        pybind11::buffer_info info  = data.request();
        size_t data_len = info.size;
        auto float_ptr = static_cast<const float*>(info.ptr);
        auto double_ptr = static_cast<const double*>(info.ptr);
        auto int_ptr = static_cast<const int64_t*>(info.ptr);

        if constexpr (std::is_same<elem_t, float>::value) {
          // input is float, output is float
          if(float_fmt == info.format){
            return setter(float_ptr);
          }
          // input is double, output is float
          else if(double_fmt == info.format){
            float temp_data[count_t];
            for(size_t i = 0; i < count_t; i++){
              temp_data[i] = (float) double_ptr[i];
            }
            return setter(temp_data);
          }
          // input is int output is float
          else if(info.format[0] == 'l') { // signed long int
            float temp_data[count_t];
            for(size_t i = 0; i < count_t; i++){
              temp_data[i] = (float) int_ptr[i];
            }
            return setter(temp_data);
          }
          else{
            OrkAssert(false);
          }
        } else if constexpr (std::is_same<elem_t, double>::value) {
          // input is double, output is double
          if(double_fmt == info.format){
            return setter(double_ptr);
          }
          // input is float, output is double
          else if(float_fmt == info.format){
            double temp_data[count_t];
            for(size_t i = 0; i < count_t; i++){
              temp_data[i] = (double) float_ptr[i];
            }
            return setter(temp_data);
          }
          // input is int output is double
          else if(info.format[0] == 'l') { // signed long int
            double temp_data[count_t];
            for(size_t i = 0; i < count_t; i++){
              temp_data[i] = (double) int_ptr[i];
            }
            return setter(temp_data);
          }
          else{
            OrkAssert(false);
          }
        } else {
          OrkAssert(false);
        }
        OrkAssert(false);
        return ret_t();
      };
      Base::def(pybind11::init(the_fn), extra...);
      return *this;
    }

  };

  template <typename... Args> static auto _cast(Args&&... args) -> decltype(pybind11::cast(std::forward<Args>(args)...)) {
    return pybind11::cast(std::forward<Args>(args)...);
  }

  //////////////////////////////////

  template <typename type_, typename... options, typename... Extra>
  static auto clazz(module_t& scope, const char* name, const Extra&... extra) {
    return _clazz<type_, options...>(scope, name, extra...);
  }

  template <typename elem_t> //
  static buffer_handle_t gen_buffer(bufferdesc_ptr_t desc, elem_t* data) {
    std::vector<ssize_t> shape;
    for(auto item : desc->shape) {
      shape.push_back(item);
    }
    std::vector<ssize_t> strides;
    for(auto item : desc->strides) {
      strides.push_back(item);
    }
    return pybind11::buffer_info(
        data,             // Pointer to buffer
        desc->scalar_size, // Size of one scalar
        pybind11::format_descriptor<elem_t>::format(),
        desc->num_dimensions, // Number of dimensions
        shape,          // Buffer dimensions
        strides);       // Strides (in bytes) for each index
  }

  //////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

struct nanobindadapter {

  // namespace binder_ns = obind;

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

  using buffer_handle_t = obind::ndarray<>;

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

  template <typename type_, typename... options> class _clazz : public obind::class_<type_, options...> {

    using Base = obind::class_<type_, options...>;

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
    template <typename Func, typename... Extra> _clazz& method(const char* name_, Func&& f, const Extra&... extra) {
      auto& ref = Base::def(name_, f, extra...);
      return *this;
    }
    //
    template <typename Func, typename... Extra> _clazz& prop_ro(const char* name_, Func&& f, const Extra&... extra) {
      auto& ref = Base::def_prop_ro(name_, f, extra...);
      return *this;
    }
    template <typename Getter, typename Setter, typename... Extra>
    _clazz& prop_rw(const char* name_, Getter&& g, Setter&& s, const Extra&... extra) {
      auto& ref = Base::def_prop_rw(name_, g, s, extra...);
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
    template <typename... Args, typename... Extra> _clazz& construct(obind::init<Args...>&& init, const Extra&... extra) {
      Base::def(std::move(init), extra...);
      return *this;
    }

    template <typename Func, typename... Extra> _clazz& as_buffer(Func&& f, const Extra&... extra) {
      prop_ro("as_buffer", f, extra...);
      return *this;
    }
    template <typename ret_t, typename... Extra> _clazz& from_buffer(std::function<ret_t(const typename ret_t::element_t* data)> setter, const Extra&... extra) {
      OrkAssert(false);
      //Base::def_buffer(f, extra...);
      return *this;
    }

  };

  //////////////////////////////////

  template <typename type_, typename... options, typename... Extra>
  static auto clazz(module_t& scope, const char* name, const Extra&... extra) {
    return _clazz<type_, options...>(scope, name, extra...);
  }
  template <typename... Args> static auto _borrow(Args&&... args) -> decltype(obind::borrow(std::forward<Args>(args)...)) {
    return obind::borrow(std::forward<Args>(args)...);
  }

  template <typename elem_t> //
  static buffer_handle_t _create_ndarray(bufferdesc_ptr_t desc, elem_t* data) {
      // Assuming DescType is a structure that contains the scalar size, number of dimensions, shape, and strides
      obind::dlpack::dtype data_type;
      data_type.code = (uint8_t) obind::dlpack::dtype_code::Float;
      data_type.bits = sizeof(elem_t) * 8;
      data_type.lanes = 1;
      return buffer_handle_t(
          data, 
          desc->num_dimensions,        // Number of dimensions
          desc->shape.data(),          // Shape array
          handle_t(),
          desc->strides.data(),        // Strides array
          data_type                   // data type
      );
  }

  template <typename elem_t> //
  static buffer_handle_t gen_buffer(bufferdesc_ptr_t desc, elem_t* data) {
    return _create_ndarray<elem_t>(desc,data);
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename ADAPTER> struct ORK_API TypeCodec {

  // namespace binder_ns = ADAPTER::binder_ns;

  //////////////////////////////////

  using decoderfn_t   = std::function<void(const typename ADAPTER::object_t& inpval, varval_t& outval)>;
  using encoderfn_t   = std::function<void(const varval_t& inpval, typename ADAPTER::object_t& outval)>;
  using decoderfn64_t = std::function<void(const typename ADAPTER::object_t& inpval, svar64_t& outval)>;
  using encoderfn64_t = std::function<void(const svar64_t& inpval, typename ADAPTER::object_t& outval)>;
  using object_t = typename ADAPTER::object_t;

  //////////////////////////////////

  static std::shared_ptr<TypeCodec> instance();

  //////////////////////////////////
  object_t encode(const varval_t& val) const;
  object_t encode64(const svar64_t& val) const;
  varval_t decode(const object_t& val) const;
  svar64_t decode64(const object_t& val) const;

  //////////////////////////////////

  varmap::VarMap decode_kwargs(typename ADAPTER::kwargs_t kwargs);
  std::vector<varval_t> decodeList(typename ADAPTER::list_t py_args);

  //////////////////////////////////
  // register std codec (will reduce boilerplate for a lot of cases)
  //////////////////////////////////

  void registerCodec(
      const object_t& pytype, //
      const ork::TypeId& orktypeid,
      encoderfn_t efn,
      decoderfn_t dfn);

  void registerCodec64(
      const object_t& pytype, //
      const ork::TypeId& orktypeid,
      encoderfn64_t efn,
      decoderfn64_t dfn);

  template <typename ORKTYPE> //
  void registerStdCodec(const object_t& pytype);

  template <typename ORKTYPE> //
  void registerStdCodecBIG(const object_t& pytype);

  template <typename PYREPR, typename ORKTYPE> //
  void registerRawPtrCodec(const object_t& pytype);

protected:
  TypeCodec();
  svar128_t _impl;
};

///////////////////////////////////////////////////////////////////////////////

using pb11_typecodec_t     = TypeCodec<pybind11adapter>;
using pb11_typecodec_ptr_t = std::shared_ptr<pb11_typecodec_t>;

using obind_typecodec_t     = TypeCodec<nanobindadapter>;
using obind_typecodec_ptr_t = std::shared_ptr<obind_typecodec_t>;

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::python
