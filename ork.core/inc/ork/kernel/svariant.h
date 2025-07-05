////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
//	static_variants allow you to stick any RAII compliant object
//	   that will fit into into its space (the size is templatized).
//	svar's are handy for passing around objects in an opaque or abstract,
//	   but typesafe way..
//  Basically what differentiates it from other variants
//     is there are no implicit ducktypish autocast operators.
//  ie. If you put an "int" you had better ask for an "int" back.
//     If you attempt to retrieve typed data that does not match
//     what is currenty contained, you will get an assert.
//  You can safely query if what you think is contained actually is.
//	Upon destruction, or resetting of the variant,
//	   the contained object's destructor will be called to reclaim
//	   any resources which might have been allocated by that object.
//
//  They are not internally thread safe, fyi..
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <assert.h>
#include <memory>
#include <new>
#include <string.h>
#include <type_traits>
#include <typeinfo>
#include <functional>
#include <atomic>

#include <ork/kernel/atomic.h>
#include <ork/orkstd.h>
#include <ork/util/crc64.h>
#include <cxxabi.h>
// #if !defined(__APPLE__)
#define SVAR_DEBUG
// #endif

// #define HAS_CONCEPTS

#if defined(HAS_CONCEPTS)
#include <concepts>
#endif

namespace ork {

struct DemangleCache {

  DemangleCache() {
    _sentinel = 0;
  }
  ~DemangleCache() {
    printf("destroying DemangleCache<%p>\n", this);
    _sentinel = 1;
  }
  std::unordered_map<std::string, std::string> _impl;
  int _sentinel = -1;

  std::string lookup(const std::string& typestr) {
    OrkAssert(_sentinel == 0);
    std::string rval;
    auto it = _impl.find(typestr);
    if (it != _impl.end()) {
      rval = it->second;
    } else {
#if defined(SVAR_DEBUG)
      int status            = 0;
      const char* demangled = abi::__cxa_demangle(typestr.c_str(), 0, 0, &status);
      rval                  = (status == 0) ? std::string(demangled) : typestr;
      free((void*)demangled);
#else
      rval = "unknown";
#endif
    }
    return rval;
  }
};

using demangle_cache_ptr_t = DemangleCache*;

template <typename T> inline std::string demangled_typename() {
  // This is thread-local and static, hence it's initialized only once per thread
  thread_local static demangle_cache_ptr_t _cache = new DemangleCache; // leak until we figure out post main deinit issue.
  auto typestr                                    = typeid(T).name();
  return _cache->lookup(typestr);
}
///////////////////////////////////////////////////////////////////////////////

struct static_variant_base;
template <int tsize> struct static_variant;

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

namespace __svartraits {

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

template <typename T> class IsEqualityComparable {
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
  template <typename T, typename std::enable_if<IsEqualityComparable<T>::value, void>::type* = nullptr> //
  static bool compare(const T& lhs, const T& rhs) {
    return bool(lhs == rhs);
  }
  template <typename T, typename std::enable_if<not IsEqualityComparable<T>::value, void>::type* = nullptr> //
  static bool compare(const T& lhs, const T& rhs) {
    return bool(false);
  }
};

//////////////////////////////////////////////////
#endif
//////////////////////////////////////////////////

} // namespace __svartraits

///////////////////////////////////////////////////////////////////////////////

struct SvarDescriptor {

  using destroyer_t = void (*)(static_variant_base& var);
  using copier_t    = void (*)(static_variant_base& lhs, const static_variant_base& rhs);
  using equals_t    = bool (*)(const static_variant_base& lhs, const static_variant_base& rhs);
  using mtinfo_t    = const std::type_info* (*)();
  using length_t    = size_t (*)();
  using typstr_t    = std::string (*)();

  template <typename T> static void destroy_impl(static_variant_base& var);
  // from rhs's descriptor, copy the value into lhs
  template <typename T> static void copy_impl(static_variant_base& lhs, const static_variant_base& rhs);
  // from lhs's descriptor, compare the value with rhs
  template <typename T> static bool equals_impl(const static_variant_base& lhs, const static_variant_base& rhs);
  template <typename T> static const std::type_info* mtinfo_impl();
  template <typename T> static std::string typstr_impl();
  template <typename T> static size_t getlength_impl();
  template <typename T> void assign();

  /////////////////////////////////////////

  destroyer_t _destroyer = nullptr;
  copier_t _copier       = nullptr;
  equals_t _equals       = nullptr;
  mtinfo_t _mtinfo       = nullptr;
  typstr_t _typstr       = nullptr;
  length_t _getlength    = nullptr;
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> struct SvarDescriptorFactory {
  static constexpr SvarDescriptor create() {
    SvarDescriptor rval;
    rval.assign<T>();
    return rval;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct static_variant_base {

  using descriptor_factory_t = SvarDescriptor (*)();
  friend struct SvarDescriptor;
  //////////////////////////////////////////////////////////////
  bool canConvertFrom(const static_variant_base& oth) const {
    return capacity() >= oth.size();
  }
  //////////////////////////////////////////////////////////////
  descriptor_factory_t descriptorFactory() const {
    return _descriptorFactory.load();
  }
  //////////////////////////////////////////////////////////////
  const std::type_info* _mtinfo() const {
    auto desc_factory = descriptorFactory();
    return (desc_factory != nullptr) ? (desc_factory()._mtinfo()) : nullptr;
  }
  //////////////////////////////////////////////////////////////
  size_t size() const {
    auto descriptor = (_descriptorFactory.load())();
    return descriptor._getlength();
  }
  //////////////////////////////////////////////////////////////
  void clear() {
    _destroy();
  }
  //////////////////////////////////////////////////////////////
  // call the destroyer on contained object
  //////////////////////////////////////////////////////////////
  void _destroy() {
    auto descriptor_factory = _descriptorFactory.exchange(nullptr);
    if (descriptor_factory) {
      auto descriptor = descriptor_factory();
      OrkAssert(descriptor._destroyer);
      descriptor._destroyer(*this);
    }
  }
  //////////////////////////////////////////////////////////////
  //	assign descriptor
  //////////////////////////////////////////////////////////////
  template <typename T> void assignDescriptor() {
    _descriptorFactory.store(&SvarDescriptorFactory<T>::create);
  }
  //////////////////////////////////////////////////////////////
  const std::type_info* typeInfo() const {
    return _mtinfo();
  }
  //////////////////////////////////////////////////////////////
  const char* typeName() const {
    auto mtinfo = _mtinfo();
    return mtinfo ? mtinfo->name() : "";
  }
  //////////////////////////////////////////////////////////////
  // return true if the variant has been set to something
  //////////////////////////////////////////////////////////////
  bool isSet() const {
    return (_mtinfo() != 0);
  }
  //////////////////////////////////////////////////////////////
  std::string typestr() const {
    auto descriptor_factory = descriptorFactory();
    if (descriptor_factory) {
      OrkAssert(_mtinfo() != nullptr);
      auto descriptor = descriptor_factory();
      return descriptor._typstr();
    }
    return std::string();
  }
  //////////////////////////////////////////////////////////////
  uint64_t hash() const {
    auto desc = descriptorFactory()();
    boost::Crc64 crcgen;
    crcgen.init();
    crcgen.accumulate(data(), desc._getlength());
    crcgen.finish();
    return crcgen.result();
  }
  //////////////////////////////////////////////////////////////
  TypeId getOrkTypeId() const {
    return TypeId::fromStdTypeInfo(_mtinfo());
  }
  //////////////////////////////////////////////////////////////
  bool operator==(const static_variant_base& oth) const {
    auto descriptor = (_descriptorFactory.load())();
    auto e          = descriptor._equals;
    return e(*this, oth);
  }
  //////////////////////////////////////////////////////////////
  void convertFromOtherSize(const static_variant_base& oth) {
    size_t oth_size = oth.size();
    OrkAssert(capacity() >= oth_size);
    auto descriptor_factory = oth.descriptorFactory();
    if( descriptor_factory == nullptr) {
      _destroy();
      return;
    }
    auto descriptor = descriptor_factory();
    auto c         = descriptor._copier;
    OrkAssert(c != nullptr);
    c(*this, oth);
  }
  //////////////////////////////////////////////////////////////
  size_t capacity() const {
    return _capacity();
  }
  //////////////////////////////////////////////////////////////
  const void* data() const {
    return _data();
  }
  //////////////////////////////////////////////////////////////
  virtual size_t _capacity() const  = 0;
  virtual const void* _data() const = 0;
  //////////////////////////////////////////////////////////////
  std::atomic<descriptor_factory_t> _descriptorFactory;
  bool _assert_on_destroy = false;
  //////////////////////////////////////////////////////////////
protected:
  static_variant_base()
      : _descriptorFactory(nullptr) {
  }
  virtual ~static_variant_base() {
  }

};

///////////////////////////////////////////////////////////////////////////////

template <int tsize> struct static_variant : public static_variant_base {
public:
  friend struct SvarDescriptor;

  static constexpr size_t ksize = tsize;

  //////////////////////////////////////////////////////////////
  // default constuctor
  //////////////////////////////////////////////////////////////
  static_variant() {
  }
  ~static_variant() {
    _destroy();
  }
  //////////////////////////////////////////////////////////////
  // copy constuctor
  //////////////////////////////////////////////////////////////
  static_variant(const static_variant& oth)
      : static_variant_base() {
    _descriptorFactory      = nullptr;
    auto descriptor_factory = oth._descriptorFactory.load();
    if (descriptor_factory) {
      auto descriptor = descriptor_factory();
      auto c          = descriptor._copier;
      if (c)
        c(*this, oth);
    }
  }
  //////////////////////////////////////////////////////////////
  static_variant& operator=(const static_variant& oth) {
    auto descriptor_factory = oth._descriptorFactory.load();
    if (descriptor_factory) {
      auto descriptor = descriptor_factory();
      auto c          = descriptor._copier;
      if (c)
        c(*this, oth);
    }
    return *this;
  }
  //////////////////////////////////////////////////////////////
  // typed constructor
  //////////////////////////////////////////////////////////////
  template <typename T> static_variant(const T& value) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    memset(_buffer, 0, ksize);
    T* pval = (T*)&_buffer[0];
    new (pval) T(value);
    assignDescriptor<T>();
  }
  //////////////////////////////////////////////////////////////
  // return true if the contained object is a T
  //////////////////////////////////////////////////////////////
  template <typename T> bool isA() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    auto mtinfo = _mtinfo();
    return (mtinfo != 0) ? (*mtinfo) == typeid(T) : false;
  }
  //////////////////////////////////////////////////////////////
  // return true if the contained object is a T
  //////////////////////////////////////////////////////////////
  template <typename T> bool isShared() const {
    static_assert(sizeof(std::shared_ptr<T>) <= ksize, "static_variant size violation");
    auto mtinfo = _mtinfo();
    return (mtinfo != 0) ? (*mtinfo) == typeid(std::shared_ptr<T>) : false;
  }
  //////////////////////////////////////////////////////////////
  // assign an object to the variant, assert if it does not fit
  //////////////////////////////////////////////////////////////
  template <typename T> void set(const T& value) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    _destroy();
    T* pval = (T*)&_buffer[0];
    new (pval) T(value);
    assignDescriptor<T>();
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> T& get() {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    OrkAssert(isA<T>());
    T* pval = (T*)&_buffer[0];
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> const T& get() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    auto descriptor_factory = _descriptorFactory.load();
    OrkAssert(descriptor_factory != nullptr);
    auto descriptor = descriptor_factory();
    auto mtinfo     = descriptor._mtinfo();
    auto& tinfo     = typeid(T);
    // auto typestr = demangled_typename<T>();
    // printf("T<%s>\n", typestr.c_str());
    // printf("tinfo: %p:%s\n", (void*)&tinfo, tinfo.name());
    // printf("_mtinfo: %p:%s\n", (void*)mtinfo, mtinfo->name());
    // fflush(stdout);
    OrkAssert(tinfo == *mtinfo);
    const T* pval = (const T*)&_buffer[0];
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> std::shared_ptr<T>& getShared() const {
    typedef std::shared_ptr<T> sharedptr_t;
    static_assert(sizeof(sharedptr_t) <= ksize, "static_variant size violation");
    assert(typeid(sharedptr_t) == *_mtinfo());
    auto pval = (sharedptr_t*)&_buffer[0];
    return (*pval);
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> void setShared(std::shared_ptr<T> ptr) {
    typedef std::shared_ptr<T> sharedptr_t;
    static_assert(sizeof(std::shared_ptr<T>) <= ksize, "static_variant size violation");
    _destroy();
    auto pval = (sharedptr_t*)&_buffer[0];
    new (pval) sharedptr_t;
    (*pval) = ptr;
    assignDescriptor<sharedptr_t>();
    assert(typeid(sharedptr_t) == *_mtinfo());
  }
  //////////////////////////////////////////////////////////////
  // construct a T and return by reference
  //////////////////////////////////////////////////////////////
  template <typename T, typename... A> T& reifyAs() {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    auto pval = (T*)&_buffer[0];
    if (not isA<T>()) {
      _destroy();
      new (pval) T();
      assignDescriptor<T>();
    }
    assert(typeid(T) == *_mtinfo());
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // construct a T and return by reference
  //////////////////////////////////////////////////////////////
  template <typename T, typename... A> T& make(A&&... args) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    _destroy();
    auto pval = (T*)&_buffer[0];
    new (pval) T(std::forward<A>(args)...);
    assignDescriptor<T>();
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // construct and return a reference to a shared_ptr<T>
  //////////////////////////////////////////////////////////////
  template <typename T, typename... A> std::shared_ptr<T>& makeShared(A&&... args) {
    static_assert(sizeof(std::shared_ptr<T>) <= ksize, "static_variant size violation");
    _destroy();
    auto pval = (std::shared_ptr<T>*)&_buffer[0];
    new (pval) std::shared_ptr<T>;
    (*pval) = std::make_shared<T>(std::forward<A>(args)...);
    assignDescriptor<std::shared_ptr<T>>();
    return (*pval);
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast<T> tryAs() {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    auto mtinfo  = _mtinfo();
    bool type_ok = (mtinfo != nullptr) ? (typeid(T) == *mtinfo) : false;
    return attempt_cast<T>((T*)(type_ok ? &_buffer[0] : nullptr));
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast<std::shared_ptr<T>> tryAsShared() {
    static_assert(sizeof(std::shared_ptr<T>) <= ksize, "static_variant size violation");
    auto mtinfo  = _mtinfo();
    bool type_ok = (mtinfo != nullptr) ? (typeid(std::shared_ptr<T>) == *mtinfo) : false;
    return attempt_cast<std::shared_ptr<T>>((std::shared_ptr<T>*)(type_ok ? &_buffer[0] : nullptr));
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast_const<std::shared_ptr<T>> tryAsShared() const {
    using ptr_t = std::shared_ptr<T>;
    static_assert(sizeof(ptr_t) <= ksize, "static_variant size violation");
    auto mtinfo  = _mtinfo();
    bool type_ok = (mtinfo != nullptr) ? (typeid(ptr_t) == *mtinfo) : false;
    if (type_ok) {
      auto as_mut = (ptr_t*)&_buffer[0];
      return attempt_cast_const<ptr_t>(as_mut);
    } else {
      return attempt_cast_const<ptr_t>(nullptr);
    }
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast_const<T> tryAs() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    auto mtinfo  = _mtinfo();
    bool type_ok = (mtinfo != nullptr) ? (typeid(T) == *mtinfo) : false;
    return attempt_cast_const<T>((const T*)(type_ok ? &_buffer[0] : nullptr));
  }
  //////////////////////////////////////////////////////////////
  // return true if the variant is capable of containing an object of type T
  //////////////////////////////////////////////////////////////
  template <typename T> static constexpr bool isTypeOk() {
    int isize = sizeof(T);
    bool rval = (isize <= ksize);
    return rval;
  }
  //////////////////////////////////////////////////////////////
private:
  size_t _capacity() const final {
    return ksize;
  }

  const void* _data() const final {
    return _buffer;
  }
  __attribute((aligned(16))) char _buffer[ksize];
  //////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> void SvarDescriptor::destroy_impl(static_variant_base& var) {
  auto pval = (T*)var.data();
  pval->~T();
}

///////////////////////////////////////////////////////////////////////////////
// from rhs's descriptor, copy the value into lhs
///////////////////////////////////////////////////////////////////////////////

template <typename T> void SvarDescriptor::copy_impl(static_variant_base& lhs, const static_variant_base& rhs) {
  auto lhs_typed = (T*)lhs.data();
  auto rhs_typed = (const T*)rhs.data();
  OrkAssert(lhs.capacity() >= sizeof(T));
  new (lhs_typed) T(*rhs_typed);
  lhs._descriptorFactory.store(rhs._descriptorFactory.load());
}

///////////////////////////////////////////////////////////////////////////////
// from lhs's descriptor, compare the value with rhs
///////////////////////////////////////////////////////////////////////////////

template <typename T> bool SvarDescriptor::equals_impl(const static_variant_base& lhs, const static_variant_base& rhs) {
  const auto lhs_typed = (const T*)lhs.data();
  const auto rhs_typed = (const T*)rhs.data();
  return __svartraits::__equal_to::compare<T>((*lhs_typed), (*rhs_typed));
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> const std::type_info* SvarDescriptor::mtinfo_impl() {
  return &typeid(T);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> std::string SvarDescriptor::typstr_impl() {
  return demangled_typename<T>();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> size_t SvarDescriptor::getlength_impl() {
  return sizeof(T);
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void SvarDescriptor::assign() {

  _destroyer = &destroy_impl<T>;
  _copier    = &copy_impl<T>;
  _equals    = &equals_impl<T>;
  _mtinfo    = &mtinfo_impl<T>;
  _getlength = &getlength_impl<T>;
  _typstr    = &typstr_impl<T>;
}

///////////////////////////////////////////////////////////////////////////////

static const int kptrsize   = sizeof(void*);
static const int kshptrsize = sizeof(std::shared_ptr<char>);

typedef static_variant<4> svar4_t;
typedef static_variant<8> svar8_t;
typedef static_variant<16> svar16_t;
typedef static_variant<32> svar32_t;
typedef static_variant<64> svar64_t;
typedef static_variant<96> svar96_t;
typedef static_variant<128> svar128_t;
typedef static_variant<160> svar160_t;
typedef static_variant<192> svar192_t;
typedef static_variant<256> svar256_t;
typedef static_variant<512> svar512_t;
typedef static_variant<1024> svar1024_t;
typedef static_variant<2048> svar2048_t;
typedef static_variant<4096> svar4096_t;
typedef static_variant<kptrsize> svarp_t;
typedef static_variant<kshptrsize> svarshp_t;

} // namespace ork
