////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/dataflow/all.h>
#include <ork/dataflow/plug_inst.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

PlugInst::PlugInst(const PlugData* plugdata)
  : _plugdata(plugdata) {

}

PlugInst::~PlugInst(){

}

InPlugInst::InPlugInst(const PlugData* plugdata)
    : PlugInst(plugdata) {

}

InPlugInst::~InPlugInst(){

}

outpluginst_ptr_t InPlugInst::connected() const{
    return _connectedOutput;
}
bool InPlugInst::isConnected() const{
    return (_connectedOutput!=nullptr);
}
bool InPlugInst::isDirty() const{
    return false;
}
void InPlugInst::_doSetDirty(bool bv) { // override

}

OutPlugInst::OutPlugInst(const PlugData* plugdata)
    : PlugInst(plugdata) {

}
OutPlugInst::~OutPlugInst(){

}

  bool OutPlugInst::isConnected() const{
    return false;
  }
  size_t OutPlugInst::numConnections() const{
    return 0;
  }
  inpluginst_ptr_t OutPlugInst::connected(size_t idx) const{
    return nullptr;
  }
  void OutPlugInst::_doSetDirty(bool bv) {

  }
  bool OutPlugInst::isDirty() const {
    return false;
  }


  floatxfinpluginst::floatxfinpluginst(const floatxfinplugdata* d)
    : inpluginst<FloatPlugTraits>(d)
    ,_data(d) {

    }

  const float& floatxfinpluginst::value() const {
    _xfvalue = inpluginst<FloatPlugTraits>::value();
    return _xfvalue;
  }

  fvec3xfinpluginst::fvec3xfinpluginst(const fvec3xfinplugdata* d)
    : inpluginst<Vec3fPlugTraits>(d)
    , _data(d) {

    }

  const fvec3& fvec3xfinpluginst::value() const {

    _xfvalue = inpluginst<Vec3fPlugTraits>::value();

    return _xfvalue;
  }

///////////////////////////////////////////////////////////////////////////////
} //namespace ork::dataflow {
