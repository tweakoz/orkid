////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/string/ConstString.h>
#include <ork/reflect/BidirectionalSerializer.h>
#include <ork/reflect/properties/ITyped.hpp>

namespace ork::reflect::serdes {

struct EnumRegistrar;
using enumregistrar_ptr_t = std::shared_ptr<EnumRegistrar>;

//////////////////////////////////////////////////////////////
struct EnumValue {
  int _value = 0;
  std::string _name;
};
using enumvalue_ptr_t = std::shared_ptr<EnumValue>;

//////////////////////////////////////////////////////////////
struct EnumType {
  const std::type_info* _mtinfo = nullptr;

  template <typename enum_t>                                 //
  inline void addEnum(std::string name, enum_t enum_value) { //
    _int2strmap[int(enum_value)] = name;
    _str2intmap[name]            = int(enum_value);
  }
  inline std::string findNameFromValue(int ivalue) { //
    auto it = _int2strmap.find(ivalue);
    return it->second;
  }
  inline int findValueFromName(std::string svalue) { //
    auto it = _str2intmap.find(svalue);
    return it->second;
  }

  std::string _name;
  std::map<int, std::string> _int2strmap;
  std::map<std::string, int> _str2intmap;
};
using enumtype_ptr_t = std::shared_ptr<EnumType>;
//////////////////////////////////////////////////////////////

struct EnumRegistrar {
  static enumregistrar_ptr_t instance();
  using typekey_t = const std::type_info*;

  template <typename enumclass> //
  inline                        //
      enumtype_ptr_t
      addEnumClass(std::string named) { //

    typekey_t tkey = &typeid(enumclass);

    auto type       = std::make_shared<EnumType>();
    type->_name     = named;
    _namemap[named] = type;
    _typemap[tkey]  = type;
    return type;
  }
  template <typename enumclass> //
  inline                        //
      enumtype_ptr_t
      findEnumClass() { //
    typekey_t tkey = &typeid(enumclass);
    auto it        = _typemap.find(tkey);
    return (it != _typemap.end()) ? it->second : nullptr;
  }

  std::map<typekey_t, enumtype_ptr_t> _typemap;
  std::map<std::string, enumtype_ptr_t> _namemap;
};
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::reflect::serdes

///////////////////////////////////////////////////////////////////////////////
#define DeclareEnumSerializer(ENUMTYPE)                                                                                            \
  template <> void ::ork::reflect::ITyped<ENUMTYPE>::serialize(serdes::node_ptr_t leafnode) const;                                 \
  template <> void ::ork::reflect::ITyped<ENUMTYPE>::deserialize(serdes::node_ptr_t desernode) const;
///////////////////////////////////////////////////////////////////////////////
#define ImplementEnumSerializer(ENUMTYPE)                                                                                          \
  template <> void ::ork::reflect::ITyped<ENUMTYPE>::serialize(serdes::node_ptr_t leafnode) const {                                \
    auto serializer = leafnode->_serializer;                                                                                       \
    auto instance   = leafnode->_ser_instance;                                                                                     \
    ENUMTYPE value;                                                                                                                \
    get(value, instance);                                                                                                          \
    auto registrar = ::ork::reflect::serdes::EnumRegistrar::instance();                                                            \
    auto enumtype  = registrar->findEnumClass<ENUMTYPE>();                                                                         \
    auto enumname  = enumtype->findNameFromValue(int(value));                                                                      \
    leafnode->_value.template Set<std::string>(enumname);                                                                          \
    serializer->serializeLeaf(leafnode);                                                                                           \
  }                                                                                                                                \
  template <> void ::ork::reflect::ITyped<ENUMTYPE>::deserialize(serdes::node_ptr_t desernode) const {                             \
    auto instance      = desernode->_deser_instance;                                                                               \
    const auto& var    = desernode->_value;                                                                                        \
    const auto& as_str = var.Get<std::string>();                                                                                   \
    auto registrar     = ::ork::reflect::serdes::EnumRegistrar::instance();                                                        \
    auto enumtype      = registrar->findEnumClass<ENUMTYPE>();                                                                     \
    int intval         = enumtype->findValueFromName(as_str);                                                                      \
    auto enumval       = ENUMTYPE(intval);                                                                                         \
    set(enumval, instance);                                                                                                        \
  }

///////////////////////////////////////////////////////////////////////////////
// clang-format off
///////////////////////////////////////////////////////////////////////////////
#define BeginEnumRegistration(X)                                                                                               \
void initenum_##X () {                                                                                                            \
   auto registrar = ::ork::reflect::serdes::EnumRegistrar::instance();                                                            \
   auto enumtype  = registrar->addEnumClass<X>(#X);

#define RegisterEnum(ET, X) enumtype->addEnum(#X, ET::X);

#define EndEnumRegistration() }

#define InvokeEnumRegistration(X) initenum_##X ();
///////////////////////////////////////////////////////////////////////////////
// clang-format on
///////////////////////////////////////////////////////////////////////////////
