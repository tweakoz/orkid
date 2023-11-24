////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//
//	dynamic_variants allow you to stick any RAII compliant object
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
#include <ork/kernel/variant_common.h>
#include <ork/kernel/atomic.h>
#include <ork/orkstd.h>
#include <ork/util/crc64.h>

namespace ork {
///////////////////////////////////////////////////////////////////////////////

struct dynamic_variant {
public:
  using destroyer_t = std::function<void(dynamic_variant& var)>;
  using copier_t    = std::function<void(dynamic_variant& lhs, const dynamic_variant& rhs)>;
  using equals_t    = std::function<bool(const dynamic_variant& lhs, const dynamic_variant& rhs)>;

  //////////////////////////////////////////////////////////////

  template <typename T> void assignDescriptor() {

    _destroyer = [](dynamic_variant& var) {
      if (var._assert_on_destroy) {
        OrkAssert(false);
      }

      // just call T's destructor, as opposed to delete
      //  because the variant owns the memory.
      //  aka 'placement delete'
      var.template get<T>().~T();
    };

    _copier = [](dynamic_variant& lhs, const dynamic_variant& rhs) {
      const T& typed_right = rhs.template get<T>();
      lhs.template set<T>(typed_right);
    };

    _equals = [](const dynamic_variant& lhs, const dynamic_variant& rhs) -> bool {
      return __vartraits::__equal_to::compare<T>(lhs.template get<T>(), rhs.template get<T>());
    };

#if defined(VAR_DEBUG)
    _typestr = demangled_typename<T>();
#endif

    _mtinfo = &typeid(T);

  }

  //////////////////////////////////////////////////////////////
  // default constuctor
  //////////////////////////////////////////////////////////////
  dynamic_variant() {
  }
  //////////////////////////////////////////////////////////////
  // destructor, delegate destuction of the contained object to the destroyer
  //////////////////////////////////////////////////////////////
  ~dynamic_variant() {
    _destroy();
  }
  //////////////////////////////////////////////////////////////
  // call the destroyer on contained object
  //////////////////////////////////////////////////////////////
  void _destroy() {
    if(_destroyer){
        _destroyer(*this);
    }
    _mtinfo = nullptr;
    _equals = nullptr;
    _copier = nullptr;
    _destroyer = nullptr;
    _typestr = "";
  }
  //////////////////////////////////////////////////////////////
  dynamic_variant& operator=(const dynamic_variant& oth) {
    _destroy();
    printf( "WTF are you skipping this?\n");
    _assert_on_destroy = oth._assert_on_destroy;
    _buffer.resize(oth._buffer.size());
    memset(_buffer.data(), 0, _buffer.size());
    _mtinfo = oth._mtinfo;
    if (_copier) {
      _copier(*this, oth);
    }
    _destroyer = oth._destroyer;
    _copier    = oth._copier;
    _equals    = oth._equals;
    _typestr   = oth._typestr;
    return *this;
  }
  //////////////////////////////////////////////////////////////
  bool operator==(const dynamic_variant& oth) const {
    if (_equals) {
      return _equals(*this, oth);
    }
    return false;
  }
  //////////////////////////////////////////////////////////////
  // typed constructor
  //////////////////////////////////////////////////////////////
template <typename T, 
          typename std::enable_if<!std::is_same<typename std::decay<T>::type, dynamic_variant>::value, int>::type = 0>
  dynamic_variant(const T& value) {
    _buffer.resize(sizeof(T));
    memset(_buffer.data(), 0, sizeof(T));
    T* pval = (T*)_buffer.data();
    printf( "WTF are you skipping this?\n");
    new (pval) T(value);
    assignDescriptor<T>();
  }
  //////////////////////////////////////////////////////////////
  // copy constuctor
  //////////////////////////////////////////////////////////////
  dynamic_variant(const dynamic_variant& oth) {
    _assert_on_destroy = oth._assert_on_destroy;
    _buffer.resize(oth._buffer.size());
    _mtinfo = oth._mtinfo;
    if (oth._copier) {
      oth._copier(*this, oth);
    }
    _destroyer = oth._destroyer;
    _copier    = oth._copier;
    _equals    = oth._equals;
    _typestr   = oth._typestr;

  }
  //////////////////////////////////////////////////////////////
  // assign an object to the variant, assert if it does not fit
  //////////////////////////////////////////////////////////////
  template <typename T> void set(const T& value) {
    _destroy();
    _buffer.resize(sizeof(T));
    memset(_buffer.data(), 0, sizeof(T));
    T* pval = (T*)_buffer.data();
    printf( "WTF are you skipping this?\n");
    new (pval) T(value);
    assignDescriptor<T>();
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> void setShared(std::shared_ptr<T> ptr) {
    using sharedptr_t = std::shared_ptr<T>;
    _destroy();
    _buffer.resize(sizeof(sharedptr_t));
    memset(_buffer.data(), 0, sizeof(T));
    auto pval = (sharedptr_t*)_buffer.data();
    printf( "WTF are you skipping this?\n");
    new (pval) sharedptr_t;
    (*pval) = ptr;
    assignDescriptor<sharedptr_t>();
    assert(typeid(sharedptr_t) == *_mtinfo);
  }
  //////////////////////////////////////////////////////////////
  // construct a T and return by reference
  //////////////////////////////////////////////////////////////
  template <typename T, typename... A> T& make(A&&... args) {
    _destroy();
    _buffer.resize(sizeof(T));
    memset(_buffer.data(), 0, sizeof(T));
    auto pval = (T*)_buffer.data();
    printf( "WTF are you skipping this?\n");
    new (pval) T(std::forward<A>(args)...);
    assignDescriptor<T>();
    assert(typeid(T) == *_mtinfo);
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // construct and return a reference to a shared_ptr<T>
  //////////////////////////////////////////////////////////////
  template <typename T, typename... A> std::shared_ptr<T>& makeShared(A&&... args) {
    using sharedptr_t = std::shared_ptr<T>;
    _destroy();
    _buffer.resize(sizeof(sharedptr_t));
    memset(_buffer.data(), 0, sizeof(T));
    auto pval = (sharedptr_t*)_buffer.data();
    printf( "WTF are you skipping this?\n");
    new (pval) sharedptr_t;
    (*pval) = std::make_shared<T>(std::forward<A>(args)...);
    assignDescriptor<sharedptr_t>();
    assert(typeid(sharedptr_t) == *_mtinfo);
    return (*pval);
  }  //////////////////////////////////////////////////////////////
  // return the type T object by reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> T& get() {
    OrkAssert(_mtinfo != nullptr);
    OrkAssert(typeid(T) == *_mtinfo);
    T* pval = (T*)_buffer.data();
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> const T& get() const {
    auto& tinfo = typeid(T);
    if (tinfo != *_mtinfo) {
      auto typestr = demangled_typename<T>();
      printf("T<%s>\n", typestr.c_str());
      printf("tinfo: %p:%s\n", (void*)&tinfo, tinfo.name());
      printf("_mtinfo: %p:%s\n", (void*)_mtinfo, _mtinfo->name());
      fflush(stdout);
    }
    assert(tinfo == *_mtinfo);
    const T* pval = (const T*)_buffer.data();
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> std::shared_ptr<T>& getShared() const {
    typedef std::shared_ptr<T> sharedptr_t;
    assert(typeid(sharedptr_t) == *_mtinfo);
    auto pval = (sharedptr_t*)_buffer.data();
    return (*pval);
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast<T> tryAs() {
    bool type_ok = (_mtinfo != nullptr) ? (typeid(T) == *_mtinfo) : false;
    return attempt_cast<T>((T*)(type_ok ? _buffer.data() : nullptr));
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast<std::shared_ptr<T>> tryAsShared() {
    using sharedptr_t = std::shared_ptr<T>;
    bool type_ok = (_mtinfo != nullptr) ? (typeid(sharedptr_t) == *_mtinfo) : false;
    return attempt_cast<sharedptr_t>((sharedptr_t*)(type_ok ? _buffer.data() : nullptr));
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast_const<std::shared_ptr<T>> tryAsShared() const {
    using ptr_t = std::shared_ptr<T>;
    bool type_ok = (_mtinfo != nullptr) ? (typeid(ptr_t) == *_mtinfo) : false;
    if (type_ok) {
      auto as_mut = (ptr_t*)_buffer.data();
      return attempt_cast_const<ptr_t>(as_mut);
    } else {
      return attempt_cast_const<ptr_t>(nullptr);
    }
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast_const<T> tryAs() const {
    bool type_ok = (_mtinfo != nullptr) ? (typeid(T) == *_mtinfo) : false;
    return attempt_cast_const<T>((const T*)(type_ok ? _buffer.data() : nullptr));
  }
  //////////////////////////////////////////////////////////////
  // return true if the contained object is a T
  //////////////////////////////////////////////////////////////
  template <typename T> bool isA() const {
    return (_mtinfo != 0) ? (*_mtinfo) == typeid(T) : false;
  }
  //////////////////////////////////////////////////////////////
  // return true if the contained object is a T
  //////////////////////////////////////////////////////////////
  template <typename T> bool isShared() const {
    return (_mtinfo != 0) ? (*_mtinfo) == typeid(std::shared_ptr<T>) : false;
  }
  //////////////////////////////////////////////////////////////
  // return true if the variant is capable of containing an object of type T
  //////////////////////////////////////////////////////////////
  template <typename T> static bool isTypeOk() {
    return true;
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
    return _typestr;
  }
#endif
  //////////////////////////////////////////////////////////////
  uint64_t hash() const {
    boost::Crc64 crcgen;
    crcgen.init();
    crcgen.accumulateItem(_buffer.size());
    crcgen.accumulate((const void*)_buffer.data(), _buffer.size());
    crcgen.finish();
    return crcgen.result();
  }
  //////////////////////////////////////////////////////////////
  TypeId getOrkTypeId() const {
    return TypeId::fromStdTypeInfo(_mtinfo);
  }
  //////////////////////////////////////////////////////////////
  size_t capacity() const {
    return _buffer.capacity();
  }
  size_t size() const {
    return _buffer.size();
  }
  //////////////////////////////////////////////////////////////
private:
  std::vector<char> _buffer;
  const std::type_info* _mtinfo = nullptr; // TODO: should this go into _descriptorFactory ?
  bool _assert_on_destroy       = false;
  destroyer_t _destroyer = nullptr;
  copier_t _copier = nullptr;
  equals_t _equals = nullptr;
  std::string _typestr;
  //////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

using dvar_t = dynamic_variant;

} // namespace ork
