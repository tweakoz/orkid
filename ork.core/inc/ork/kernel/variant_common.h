#pragma once 

#include <cxxabi.h>
#include <ork/util/crc64.h>

#if defined(HAS_CONCEPTS)
#include <concepts>
#endif

#define VAR_DEBUG

namespace ork {
#if defined(VAR_DEBUG)
template <typename T> inline std::string demangled_typename() {
  auto typestr          = typeid(T).name();
  int status            = 0;
  const char* demangled = abi::__cxa_demangle(typestr, 0, 0, &status);
  auto rval             = std::string(demangled);
  free((void*)demangled);
  return rval;
}
#endif

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct attempt_cast {
  attempt_cast(T* d)
      : _data(d) {
  }
  operator bool() const {
    return (nullptr != _data);
  }
  T& value() const {
    OrkAssert(_data != nullptr);
    return *_data;
  }
  T* _data;
};

template <typename T> struct attempt_cast_const {
  attempt_cast_const(const T* d)
      : _data(d) {
  }
  operator bool() const {
    return (nullptr != _data);
  }
  const T& value() const {
    OrkAssert(_data != nullptr);
    return *_data;
  }
  const T* _data;
};

///////////////////////////////////////////////////////////////////////////////
// TypeId
// todo: try to see if we can get the hash at compile time!
///////////////////////////////////////////////////////////////////////////////

struct TypeId {
  using hashtype_t   = uint64_t;
  hashtype_t _hashed = 0;
  std::string _typename;
  template <typename T> static TypeId of() {
    TypeId rval;
    rval._typename = typeid(T).name();
    boost::Crc64 crcgen;
    crcgen.init();
    crcgen.accumulate((const void*)rval._typename.c_str(), rval._typename.length());
    crcgen.finish();
    rval._hashed = crcgen.result();
    return rval;
  }

  static TypeId fromStdTypeInfo(const std::type_info* tinfo) {
    TypeId rval;
    rval._typename = tinfo->name();
    boost::Crc64 crcgen;
    crcgen.init();
    crcgen.accumulate((const void*)rval._typename.c_str(), rval._typename.length());
    crcgen.finish();
    rval._hashed = crcgen.result();
    return rval;
  }
};

namespace __vartraits {

#if defined(HAS_CONCEPTS)

template <typename T>
concept equality_comparable = requires(T obj) {
  { obj == obj } -> std::same_as<bool>;
  { obj != obj } -> std::same_as<bool>;
};

template <typename T, typename T2> //
struct __equal_to {
  static bool compare(const T& lhs, const T2& rhs){requires equality_comparable<T>{return bool(lhs == rhs);
} static bool compare(const T& lhs, const T2& rhs)
  requires(not equality_comparable<T>)
{
  OrkAssert(false);
  return false;
}
};

//////////////////////////////////////////////////
#else // non concept methods
//////////////////////////////////////////////////

template <typename T> class DvariantIsEqualityComparable {
private:
  static void* conv(bool); // to check convertibility to bool
  template <typename U>
  static std::true_type test(
      decltype(conv(std::declval<U const&>() == std::declval<U const&>())),
      decltype(conv(!(std::declval<U const&>() == std::declval<U const&>()))));
  // fallback:
  template <typename U> static std::false_type test(...);

public:
  static constexpr bool value = decltype(test<T>(nullptr, nullptr))::value;
};

struct __equal_to {
  template <typename T, typename T2, typename std::enable_if<not std::is_same<T, T2>::value, void>::type* = nullptr> //
  static bool compare(const T& lhs, const T2& rhs) {
    return false;
  }
  template <typename T, typename std::enable_if<DvariantIsEqualityComparable<T>::value, void>::type* = nullptr> //
  static bool compare(const T& lhs, const T& rhs) {
    return bool(lhs == rhs);
  }
  template <typename T, typename std::enable_if<not DvariantIsEqualityComparable<T>::value, void>::type* = nullptr> //
  static bool compare(const T& lhs, const T& rhs) {
    return bool(false);
  }
};

//////////////////////////////////////////////////
#endif
//////////////////////////////////////////////////

} // namespace __vartraits
} // namespace ork {
