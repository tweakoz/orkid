////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

#include <ork/kernel/atomic.h>
#include <ork/orkstd.h>
#include <ork/util/crc64.h>
#include <cxxabi.h>

//#if !defined(__APPLE__)
#define SVAR_DEBUG
//#endif

namespace ork {
#if defined(SVAR_DEBUG)
template <typename T> inline std::string demangled_typename() {
  auto typestr          = typeid(T).name();
  int status            = 0;
  const char* demangled = abi::__cxa_demangle(typestr, 0, 0, &status);
  return std::string(demangled);
}
#endif

///////////////////////////////////////////////////////////////////////////////

template <int tsize> class static_variant;

///////////////////////////////////////////////////////////////////////////////
// templatized destruction delegate for static_variants

template <int tsize, typename T> struct static_variant_destroyer_t {
  typedef T MyType;

  static void destroy(static_variant<tsize>& var);
};

///////////////////////////////////////////////////////////////////////////////

template <int tsize, typename T> struct static_variant_copier_t {
  typedef T MyType;

  static void copy(static_variant<tsize>& lhs, const static_variant<tsize>& rhs);
};

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

///////////////////////////////////////////////////////////////////////////////

template <int tsize> class static_variant {
public:
  typedef void (*destroyer_t)(static_variant<tsize>& var);
  typedef void (*copier_t)(static_variant<tsize>& lhs, const static_variant<tsize>& rhs);
  static const int ksize = tsize;

  //////////////////////////////////////////////////////////////
  // default constuctor
  //////////////////////////////////////////////////////////////
  static_variant()
      : mtinfo(nullptr) {
    mDestroyer = nullptr;
    mCopier    = nullptr;
    _curlength = 0;
  }
  //////////////////////////////////////////////////////////////
  // copy constuctor
  //////////////////////////////////////////////////////////////
  static_variant(const static_variant& oth)
      : mtinfo(nullptr) {
    mDestroyer = nullptr;
    mCopier    = nullptr;

    auto c = oth.mCopier.load();
    if (c)
      c(*this, oth);
  }
  //////////////////////////////////////////////////////////////
  static_variant& operator=(const static_variant& oth) {
    auto c = oth.mCopier.load();
    if (c)
      c(*this, oth);
    _curlength = oth._curlength;
    return *this;
  }
  //////////////////////////////////////////////////////////////
  // typed constructor
  //////////////////////////////////////////////////////////////
  template <typename T> static_variant(const T& value) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    memset(mbuffer, 0, ksize);
    T* pval = (T*)&mbuffer[0];
    new (pval) T(value);

    AssignDestroyer<T>();
    AssignCopier<T>();

    mtinfo = &typeid(T);
#if defined(SVAR_DEBUG)
    _typestr = demangled_typename<T>();
#endif
  }
  //////////////////////////////////////////////////////////////
  // destructor, delegate destuction of the contained object to the destroyer
  //////////////////////////////////////////////////////////////
  ~static_variant() {
    Destroy();
    mDestroyer = nullptr;
    mCopier    = nullptr;
    mtinfo     = nullptr;
  }
  //////////////////////////////////////////////////////////////
  // call the destroyer on contained object
  //////////////////////////////////////////////////////////////
  void Destroy() {
    destroyer_t pdestr = mDestroyer.exchange(nullptr);
    if (pdestr)
      pdestr(*this);
    _curlength = 0;
  }
  //////////////////////////////////////////////////////////////
  //	assign a destroyer
  //////////////////////////////////////////////////////////////
  template <typename T> void AssignDestroyer() {
    mDestroyer.store(&static_variant_destroyer_t<tsize, T>::destroy);
  }
  //////////////////////////////////////////////////////////////
  //	assign a copier
  //////////////////////////////////////////////////////////////
  template <typename T> void AssignCopier() {
    mCopier.store(&static_variant_copier_t<tsize, T>::copy);
    _curlength = sizeof(T);
  }
  //////////////////////////////////////////////////////////////
  // return true if the contained object is a T
  //////////////////////////////////////////////////////////////
  template <typename T> bool IsA() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    return (mtinfo != 0) ? (*mtinfo) == typeid(T) : false;
  }
  //////////////////////////////////////////////////////////////
  // return true if the contained object is a T
  //////////////////////////////////////////////////////////////
  template <typename T> bool IsShared() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    return (mtinfo != 0) ? (*mtinfo) == typeid(std::shared_ptr<T>) : false;
  }
  //////////////////////////////////////////////////////////////
  // assign an object to the variant, assert if it does not fit
  //////////////////////////////////////////////////////////////
  template <typename T> void Set(const T& value) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    Destroy();
    T* pval = (T*)&mbuffer[0];
    new (pval) T(value);
    mtinfo = &typeid(T);
#if defined(SVAR_DEBUG)
    _typestr = demangled_typename<T>();
#endif
    AssignDestroyer<T>();
    AssignCopier<T>();
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> T& Get() {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    assert(typeid(T) == *mtinfo);
    T* pval = (T*)&mbuffer[0];
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> const T& Get() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    assert(typeid(T) == *mtinfo);
    const T* pval = (const T*)&mbuffer[0];
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // return the type T object by const reference, assert if the types dont match
  //////////////////////////////////////////////////////////////
  template <typename T> std::shared_ptr<T>& getShared() const {
    typedef std::shared_ptr<T> sharedptr_t;
    static_assert(sizeof(sharedptr_t) <= ksize, "static_variant size violation");
    assert(typeid(sharedptr_t) == *mtinfo);
    auto pval = (sharedptr_t*)&mbuffer[0];
    return (*pval);
  }
  //////////////////////////////////////////////////////////////
  // construct a T and return by reference
  //////////////////////////////////////////////////////////////
  template <typename T, typename... A> T& Make(A&&... args) {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    Destroy();
    auto pval = (T*)&mbuffer[0];
    new (pval) T(std::forward<A>(args)...);
    mtinfo = &typeid(T);
#if defined(SVAR_DEBUG)
    _typestr = demangled_typename<T>();
#endif
    AssignDestroyer<T>();
    AssignCopier<T>();
    assert(typeid(T) == *mtinfo);
    return *pval;
  }
  //////////////////////////////////////////////////////////////
  // construct and return a reference to a shared_ptr<T>
  //////////////////////////////////////////////////////////////
  template <typename T, typename... A> std::shared_ptr<T>& makeShared(A&&... args) {
    using sharedptr_t = std::shared_ptr<T>;
    static_assert(sizeof(sharedptr_t) <= ksize, "static_variant size violation");
    Destroy();
    auto pval = (sharedptr_t*)&mbuffer[0];
    new (pval) sharedptr_t;
    (*pval) = std::make_shared<T>(std::forward<A>(args)...);
    mtinfo  = &typeid(sharedptr_t);
#if defined(SVAR_DEBUG)
    _typestr = demangled_typename<T>();
#endif
    AssignDestroyer<sharedptr_t>();
    AssignCopier<sharedptr_t>();
    assert(typeid(sharedptr_t) == *mtinfo);
    return (*pval);
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast<T> TryAs() {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    bool type_ok = (mtinfo != nullptr) ? (typeid(T) == *mtinfo) : false;
    return attempt_cast<T>((T*)(type_ok ? &mbuffer[0] : nullptr));
  }
  //////////////////////////////////////////////////////////////
  //
  //////////////////////////////////////////////////////////////
  template <typename T> attempt_cast_const<T> TryAs() const {
    static_assert(sizeof(T) <= ksize, "static_variant size violation");
    bool type_ok = (mtinfo != nullptr) ? (typeid(T) == *mtinfo) : false;
    return attempt_cast_const<T>((const T*)(type_ok ? &mbuffer[0] : nullptr));
  }
  //////////////////////////////////////////////////////////////
  // return true if the variant is capable of containing an object of type T
  //////////////////////////////////////////////////////////////
  template <typename T> static bool IsTypeOk() {
    int isize = sizeof(T);
    bool rval = (isize <= ksize);
    return rval;
  }
  //////////////////////////////////////////////////////////////
  const std::type_info* GetTypeInfo() const {
    return mtinfo;
  }
  //////////////////////////////////////////////////////////////
  const char* GetTypeName() const {
    return mtinfo ? mtinfo->name() : "";
  }
  //////////////////////////////////////////////////////////////
  // return true if the variant has been set to something
  //////////////////////////////////////////////////////////////
  bool IsSet() const {
    return (mtinfo != 0);
  }
#if defined(SVAR_DEBUG)
  std::string typestr() const {
    return _typestr;
  }
#endif
  //////////////////////////////////////////////////////////////
  uint64_t hash() const {
    boost::Crc64 crcgen;
    crcgen.init();
    crcgen.accumulate((const void*)mbuffer, ksize);
    crcgen.finish();
    return crcgen.result();
  }
  //////////////////////////////////////////////////////////////
  TypeId getOrkTypeId() const {
    return TypeId::fromStdTypeInfo(mtinfo);
  }
  //////////////////////////////////////////////////////////////
private:
  char mbuffer[ksize];
  size_t _curlength;
  ork::atomic<destroyer_t> mDestroyer;
  ork::atomic<copier_t> mCopier;
  const std::type_info* mtinfo;
#if defined(SVAR_DEBUG)
  std::string _typestr;
#endif
  //////////////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

template <int tsize, typename T> inline void static_variant_destroyer_t<tsize, T>::destroy(static_variant<tsize>& var) {
  var.template Get<T>().~T();
  // just call T's destructor, as opposed to delete
  //	because the variant owns the memory.
  //  aka 'placement delete'
};

template <int tsize, typename T>
inline void static_variant_copier_t<tsize, T>::copy(static_variant<tsize>& lhs, const static_variant<tsize>& rhs) {
  const T& src = rhs.template Get<T>();
  lhs.template Set<T>(src);
};

///////////////////////////////////////////////////////////////////////////////

static const int kptrsize = sizeof(void*);

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

} // namespace ork
