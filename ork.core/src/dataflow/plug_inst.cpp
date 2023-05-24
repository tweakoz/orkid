////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/dataflow/all.h>
#include <ork/dataflow/plug_inst.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

PlugInst::PlugInst(const PlugData* plugdata, ModuleInst* minst)
    : _plugdata(plugdata)
    , _moduleinst(minst) {
}

PlugInst::~PlugInst() {
}

InPlugInst::InPlugInst(const InPlugData* inplugdata, ModuleInst* minst)
    : PlugInst(inplugdata, minst) {
  _connectionPlugConnectionChanged = inplugdata->_sigPlugConnectionChanged.connect([=]() {
    auto conplugdata = inplugdata->_connectedOutput;
    printf(
        "!!! inplugdata<%p:%s> connection changed conplugdata<%p:%s>\n",
        (void*)inplugdata,
        inplugdata->_name.c_str(),
        (void*)conplugdata.get(),
        conplugdata->_name.c_str());

    if (conplugdata) {
      auto ginst = minst->_graphinst;
      auto conmodulename = conplugdata->_parent_module->_name;
      auto it_conmodule = ginst->_module_inst_map.find(conmodulename);
      OrkAssert(it_conmodule != ginst->_module_inst_map.end());
      auto conmoduleinst = it_conmodule->second;
      auto conpluginst = conmoduleinst->outputNamed(conplugdata->_name);
      _connectedOutput = conpluginst;
      printf(
          "!!! inpluginst<%p:%s> connection changed conpluginst<%p:%s>\n",
          (void*)this,
          inplugdata->_name.c_str(),
          (void*)conpluginst.get(),
          conplugdata->_name.c_str());
    
      auto out_plug = typedPlugInst<outpluginst<FloatPlugTraits>>(_connectedOutput);
      OrkAssert(out_plug);
    
    }
  });
}

InPlugInst::~InPlugInst() {
}

outpluginst_ptr_t InPlugInst::connected() const {
  return _connectedOutput;
}
bool InPlugInst::isConnected() const {
  return (_connectedOutput != nullptr);
}
bool InPlugInst::isDirty() const {
  return false;
}
void InPlugInst::_doSetDirty(bool bv) { // override
}

OutPlugInst::OutPlugInst(const OutPlugData* plugdata, ModuleInst* minst)
    : PlugInst(plugdata, minst) {
}
OutPlugInst::~OutPlugInst() {
}

bool OutPlugInst::isConnected() const {
  return false;
}
size_t OutPlugInst::numConnections() const {
  return 0;
}
inpluginst_ptr_t OutPlugInst::connected(size_t idx) const {
  return nullptr;
}
void OutPlugInst::_doSetDirty(bool bv) {
}
bool OutPlugInst::isDirty() const {
  return false;
}

floatxfinpluginst::floatxfinpluginst(const floatxfinplugdata_t* d, ModuleInst* minst)
    : inpluginst<FloatXfPlugTraits>(d, minst)
    , _data(d) {
}

const float& floatxfinpluginst::value() const {
  _xfvalue = inpluginst<FloatXfPlugTraits>::value();
  return _xfvalue;
}

fvec3xfinpluginst::fvec3xfinpluginst(const fvec3xfinplugdata_t* d, ModuleInst* minst)
    : inpluginst<Vec3XfPlugTraits>(d, minst)
    , _data(d) {
}

const fvec3& fvec3xfinpluginst::value() const {

  _xfvalue = inpluginst<Vec3XfPlugTraits>::value();

  return _xfvalue;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
