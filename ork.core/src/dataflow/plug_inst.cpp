////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/dataflow/dataflow.h>
#include <ork/dataflow/scheduler.h>

#include <ork/math/cvector2.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/cvector4.hpp>
#include <ork/math/quaternion.hpp>
#include <ork/math/cmatrix3.hpp>
#include <ork/math/cmatrix4.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

PlugInst::PlugInst(moduleinst_ptr_t minst, plugdata_ptr_t plugdata){

}

PlugInst::~PlugInst(){

}

InPlugInst::InPlugInst(moduleinst_ptr_t pmod, inplugdata_ptr_t data)
    : PlugInst(pmod,data) {

}

InPlugInst::~InPlugInst(){

}

outpluginst_ptr_t InPlugInst::connected() const{
    return nullptr;
}
bool InPlugInst::isConnected() const{
    return false;
}
bool InPlugInst::isDirty() const{
    return false;
}
void InPlugInst::_doSetDirty(bool bv) { // override

}

OutPlugInst::OutPlugInst(moduleinst_ptr_t pmod, outplugdata_ptr_t data)
    : PlugInst(pmod,data) {

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


///////////////////////////////////////////////////////////////////////////////
} //namespace ork::dataflow {
