////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::dataflow {

template <typename T> std::shared_ptr<T> PlugData::typedModuleData() {
  return std::dynamic_pointer_cast<T>(_parent_module);
}

template <typename T> std::shared_ptr<T> typedPlugData(plugdata_ptr_t p) {
  return std::dynamic_pointer_cast<T>(p);
}

template <typename traits>
outplugdata<traits>::outplugdata(
    moduledata_ptr_t pmod, //
    EPlugRate epr,
    const char* pname)                                     //
    : OutPlugData(pmod, epr, typeid(data_type_t), pname) { //

  _value = std::make_shared<data_type_t>();
}

template <typename traits> size_t outplugdata<traits>::maxFanOut() const {
  return traits::max_fanout;
}

template <typename traits> const typename outplugdata<traits>::data_type_t& outplugdata<traits>::value() const {
  return (*_value);
}

template <typename traits> void outplugdata<traits>::setValue(const data_type_t& v) {
  (*_value) = v;
}

template <typename traits>
inplugdata<traits>::inplugdata(
    moduledata_ptr_t pmod,                           //
    EPlugRate epr,                                   //
    const char* pname)                               //
    : InPlugData(pmod, epr, typeid(traits), pname) { //
  _value = std::make_shared<data_type_t>();
}

template <typename traits> const typename inplugdata<traits>::data_type_t& inplugdata<traits>::value() const {
  return (*_value);
}

template <typename traits> void inplugdata<traits>::setValue(const data_type_t& v) {
  (*_value) = v;
}

} // namespace ork::dataflow
