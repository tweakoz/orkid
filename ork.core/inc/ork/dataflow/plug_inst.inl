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
outpluginst<traits>::outpluginst(const outplugdata<traits>* data, ModuleInst* minst) //
    : OutPlugInst(data,minst)                                     //
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
  printf( "outplug<%s> getvalue\n", _plugdata->_name.c_str() );
  return (*_value);
}

template <typename traits> //
void outpluginst<traits>::setValue(const data_type_t& v) {
  (*_value) = v;
}

///////////////////////////////////////////////////////////////////////////////

template <typename traits>
inpluginst<traits>::inpluginst(const inplugdata<traits>* data, ModuleInst* minst) //
    : InPlugInst(data,minst)                                   //
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
    auto inp_plug_data = dynamic_cast<const inplugdata<traits>*>(_plugdata);
    auto out_plug = typedPlugInst<outpluginst<typename traits::out_traits_t>>(_connectedOutput);
    OrkAssert(out_plug);
    *_value = out_plug->value();
    auto as_txflist = std::dynamic_pointer_cast<typename traits::xformer_t>(inp_plug_data->_transformer);
    if(as_txflist){
      for( auto xf : as_txflist->_transforms ){
        //xf->transform( *_value );
      }
    }
  }

  return (*_value);
}

template <typename traits> void inpluginst<traits>::setValue(const data_type_t& v) {
  (*_value) = v;
}

} // namespace ork::dataflow
