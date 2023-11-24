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
#include <ork/kernel/variant_common.h>

namespace ork {

///////////////////////////////////////////////////////////////////////////////

struct static_variant_base;
template <int tsize> struct static_variant;

///////////////////////////////////////////////////////////////////////////////

struct SvarDescriptorBase {
  virtual ~SvarDescriptorBase() = default;
#if defined(VAR_DEBUG)
  std::string _typestr;
#endif
  size_t _curlength;
};

template <int tsize> struct SvarDescriptor : public SvarDescriptorBase {

  using destroyer_t = std::function<void(static_variant<tsize>& var)>;
  using copier_t    = std::function<void(static_variant<tsize>& lhs, const static_variant<tsize>& rhs)>;
  using equals_t    = std::function<bool(const static_variant<tsize>& lhs, const static_variant<tsize>& rhs)>;

  /////////////////////////////////////////

  template <typename T> void assign() {

    _destroyer = [](static_variant<tsize>& var) {
      if (var._assert_on_destroy) {
        OrkAssert(false);
      }

      // just call T's destructor, as opposed to delete
      //  because the variant owns the memory.
      //  aka 'placement delete'
      var.template get<T>().~T();
    };

    _copier = [](static_variant<tsize>& lhs, const static_variant<tsize>& rhs) {
      const T& typed_right = rhs.template get<T>();
      lhs.template set<T>(typed_right);
    };

    _equals = [](const static_variant<tsize>& lhs, const static_variant<tsize>& rhs) -> bool {
      return __vartraits::__equal_to::compare<T>(lhs.template get<T>(), rhs.template get<T>());
    };

    _curlength = sizeof(T);

#if defined(VAR_DEBUG)
    _typestr = demangled_typename<T>();
#endif
  }

  /////////////////////////////////////////

  destroyer_t _destroyer;
  copier_t _copier;
  equals_t _equals;
};

///////////////////////////////////////////////////////////////////////////////

template <int tsize, typename T> struct SvarDescriptorFactory {
  static SvarDescriptor<tsize> create() {
    SvarDescriptor<tsize> rval;
    rval.template assign<T>();
    return rval;
  }
};

///////////////////////////////////////////////////////////////////////////////

struct static_variant_base {

  template <int size> friend struct SvarDescriptor;

  virtual size_t capacity() const                                   = 0;
  virtual size_t size() const                                       = 0;
  virtual bool canConvertFrom(const static_variant_base& oth) const = 0;

protected:
  static_variant_base()
      : _mtinfo(nullptr) {
  }
  virtual ~static_variant_base() = default;
  const std::type_info* _mtinfo; // TODO: should this go into _descriptorFactory ?
  bool _assert_on_destroy = false;
};

///////////////////////////////////////////////////////////////////////////////

template <int tsize> struct static_variant : public static_variant_base {
public:
  using descriptor_factory_t = SvarDescriptor<tsize> (*)();

  static constexpr size_t ksize = tsize;

  //////////////////////////////////////////////////////////////

  size_t capacity() const final {
    return ksize;
  }
  size_t size() const final {
    auto descriptor = (_descriptorFactory.load())();
    return descriptor._curlength;
  }

  bool canConvertFrom(const static_variant_base& oth) const {
    return capacity() >= oth.size();
  }

  //////////////////////////////////////////////////////////////
  // default constuctor
  //////////////////////////////////////////////////////////////
  static_variant()
      : static_variant_base() {
    _descriptorFactory = nullptr;
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
  bool operator==(const static_variant& oth) const {
    auto descriptor = (_descriptorFactory.load())();
    auto e          = descriptor._equals;
    return e(*this, oth);
  }
  //////////////////////////////////////////////////////////////
  // typed constructor
  //////////////////////////////////////////////////////////////
  template <typename T> static_variant(const T& value) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    memset(_buffer, 0, ksize);
    T* pval = (T*)&_buffer[0];
    new (pval) T(value);
    _mtinfo = &typeid(T);
    assignDescriptor<T>();
  }
  //////////////////////////////////////////////////////////////
  // destructor, delegate destuction of the contained object to the destroyer
  //////////////////////////////////////////////////////////////
  ~static_variant() {
    _destroy();
  }
  //////////////////////////////////////////////////////////////
  // call the destroyer on contained object
  //////////////////////////////////////////////////////////////
  void _destroy() {
    auto descriptor_factory = _descriptorFactory.exchange(nullptr);
    if (descriptor_factory) {
      OrkAssert(_mtinfo != nullptr);
      auto descriptor = descriptor_factory();
      OrkAssert(descriptor._destroyer);
      descriptor._destroyer(*this);
    }
    _mtinfo = nullptr;
  }
  //////////////////////////////////////////////////////////////
  //	assign descriptor
  //////////////////////////////////////////////////////////////
  template <typename T> void assignDescriptor() {
    _descriptorFactory.store(&SvarDescriptorFactory<tsize, T>::create);
  }
  //////////////////////////////////////////////////////////////
  // return true if the contained object is a T
  //////////////////////////////////////////////////////////////
  template <typename T> bool isA() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    return (_mtinfo != 0) ? (*_mtinfo) == typeid(T) : false;
  }
  //////////////////////////////////////////////////////////////
  // return true if the contained object is a T
  //////////////////////////////////////////////////////////////
  template <typename T> bool isShared() const {
    static_assert(sizeof(std::shared_ptr<T>) <= ksize, "static_variant size violation");
    return (_mtinfo != 0) ? (*_mtinfo) == typeid(std::shared_ptr<T>) : false;
  }
  //////////////////////////////////////////////////////////////
  // assign an object to the variant, assert if it does not fit
  //////////////////////////////////////////////////////////////
  template <typename T> void set(const T& value) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    _destroy();
    T* pval = (T*)&_buffer[0];
    new (pval) T(value);
    _mtinfo = &typeid(T);
    assignDescriptor<T>();
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> T& get() {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    OrkAssert(_mtinfo != nullptr);
    OrkAssert(typeid(T) == *_mtinfo);
    T* pval = (T*)&_buffer[0];
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> const T& get() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    auto& tinfo = typeid(T);
    if (tinfo != *_mtinfo) {
      auto typestr = demangled_typename<T>();
      printf("T<%s>\n", typestr.c_str());
      printf("tinfo: %p:%s\n", (void*)&tinfo, tinfo.name());
      printf("_mtinfo: %p:%s\n", (void*)_mtinfo, _mtinfo->name());
      fflush(stdout);
    }
    assert(tinfo == *_mtinfo);
    const T* pval = (const T*)&_buffer[0];
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> std::shared_ptr<T>& getShared() const {
    typedef std::shared_ptr<T> sharedptr_t;
    static_assert(sizeof(sharedptr_t) <= ksize, "static_variant size violation");
    assert(typeid(sharedptr_t) == *_mtinfo);
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
    _mtinfo = &typeid(sharedptr_t);
    assignDescriptor<sharedptr_t>();
    assert(typeid(sharedptr_t) == *_mtinfo);
  }
  //////////////////////////////////////////////////////////////
  // construct a T and return by reference
  //////////////////////////////////////////////////////////////
  template <typename T, typename... A> T& make(A&&... args) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    _destroy();
    auto pval = (T*)&_buffer[0];
    new (pval) T(std::forward<A>(args)...);
    _mtinfo = &typeid(T);
    assignDescriptor<T>();
    assert(typeid(T) == *_mtinfo);
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
    _mtinfo = &typeid(std::shared_ptr<T>);
    assignDescriptor<std::shared_ptr<T>>();
    assert(typeid(std::shared_ptr<T>) == *_mtinfo);
    return (*pval);
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast<T> tryAs() {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    bool type_ok = (_mtinfo != nullptr) ? (typeid(T) == *_mtinfo) : false;
    return attempt_cast<T>((T*)(type_ok ? &_buffer[0] : nullptr));
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast<std::shared_ptr<T>> tryAsShared() {
    static_assert(sizeof(std::shared_ptr<T>) <= ksize, "static_variant size violation");
    bool type_ok = (_mtinfo != nullptr) ? (typeid(std::shared_ptr<T>) == *_mtinfo) : false;
    return attempt_cast<std::shared_ptr<T>>((std::shared_ptr<T>*)(type_ok ? &_buffer[0] : nullptr));
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast_const<std::shared_ptr<T>> tryAsShared() const {
    using ptr_t = std::shared_ptr<T>;
    static_assert(sizeof(ptr_t) <= ksize, "static_variant size violation");
    bool type_ok = (_mtinfo != nullptr) ? (typeid(ptr_t) == *_mtinfo) : false;
    if (type_ok) {
      auto as_mut   = (ptr_t*)&_buffer[0];
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
    bool type_ok = (_mtinfo != nullptr) ? (typeid(T) == *_mtinfo) : false;
    return attempt_cast_const<T>((const T*)(type_ok ? &_buffer[0] : nullptr));
  }
  //////////////////////////////////////////////////////////////
  // return true if the variant is capable of containing an object of type T
  //////////////////////////////////////////////////////////////
  template <typename T> static bool isTypeOk() {
    int isize = sizeof(T);
    bool rval = (isize <= ksize);
    return rval;
  }
  //////////////////////////////////////////////////////////////
  const std::type_info* typeInfo() const {
    return _mtinfo;
  }
  //////////////////////////////////////////////////////////////
  const char* typeName() const {
    return _mtinfo ? _mtinfo->name() : "";
  }
  //////////////////////////////////////////////////////////////
  // return true if the variant has been set to something
  //////////////////////////////////////////////////////////////
  bool isSet() const {
    return (_mtinfo != 0);
  }
#if defined(VAR_DEBUG)
  std::string typestr() const {
    auto descriptor_factory = _descriptorFactory.load();
    if (descriptor_factory) {
      OrkAssert(_mtinfo != nullptr);
      auto descriptor = descriptor_factory();
      return descriptor._typestr;
    }
    return std::string();
  }
#endif
  //////////////////////////////////////////////////////////////
  uint64_t hash() const {
    boost::Crc64 crcgen;
    crcgen.init();
    crcgen.accumulate((const void*)_buffer, ksize);
    crcgen.finish();
    return crcgen.result();
  }
  //////////////////////////////////////////////////////////////
  TypeId getOrkTypeId() const {
    return TypeId::fromStdTypeInfo(_mtinfo);
  }
  //////////////////////////////////////////////////////////////
private:
  char _buffer[ksize];
  alignas(16) ork::atomic<descriptor_factory_t> _descriptorFactory;
  //////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

static constexpr size_t kptrsize   = sizeof(void*);
static constexpr size_t kshptrsize = sizeof(std::shared_ptr<int>);

typedef static_variant<4> svar4_t;
typedef static_variant<8> svar8_t;
typedef static_variant<16> svar16_t;
typedef static_variant<32> svar32_t;
typedef static_variant<48> svar48_t;
typedef static_variant<64> svar64_t;
typedef static_variant<80> svar80_t;
typedef static_variant<96> svar96_t;
typedef static_variant<112> svar112_t;
typedef static_variant<128> svar128_t;
typedef static_variant<144> svar144_t;
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
