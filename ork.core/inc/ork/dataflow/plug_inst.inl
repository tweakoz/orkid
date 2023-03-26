////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////

template <typename traits>                                        //
outpluginst<traits>::outpluginst(const outplugdata<traits>* data) //
    : OutPlugInst(data)                                           //
    , _typed_plugdata(data) {                                     //

  _value = traits::data_to_inst(data->_value);
}

template <typename traits> //
typename outpluginst<traits>::data_type_const_ptr_t outpluginst<traits>::value_ptr() const {
  return _value;
}

template <typename traits> //
typename outpluginst<traits>::data_type_ptr_t outpluginst<traits>::value_ptr() {
  return _value;
}

template <typename traits> //
const typename outpluginst<traits>::data_type_t& outpluginst<traits>::value() const {
  return (*_value);
}

template <typename traits> //
void outpluginst<traits>::setValue(const data_type_t& v) {
  (*_value) = v;
}

///////////////////////////////////////////////////////////////////////////////

template <typename traits>
inpluginst<traits>::inpluginst(const inplugdata<traits>* data) //
    : InPlugInst(data)                                         //
    , _typed_plugdata(data) {                                  //
  //_value = std::make_shared<data_type_t>();
  _value = traits::data_to_inst(data->_value);
  //_value = data->_value;
}

template <typename traits> typename inpluginst<traits>::data_type_const_ptr_t inpluginst<traits>::value_ptr() const {
  return _value;
}

template <typename traits> typename inpluginst<traits>::data_type_ptr_t inpluginst<traits>::value_ptr() {
  return _value;
}

template <typename traits> const typename inpluginst<traits>::data_type_t& inpluginst<traits>::value() const {

  if (_connectedOutput) {
    auto out_plug = typedPlugInst<outpluginst<traits>>(_connectedOutput);
    OrkAssert(out_plug);
    return out_plug->value();
  }

  return (*_value);
}

template <typename traits> void inpluginst<traits>::setValue(const data_type_t& v) {
  (*_value) = v;
}

} // namespace ork::dataflow
