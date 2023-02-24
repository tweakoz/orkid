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
#include <ork/reflect/properties/register.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::dataflow::GraphData, "dflow/graphdata");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GraphData::describeX(object::ObjectClass* clazz) {
/*  clazz
      ->directMapProperty(
          "Modules",             //
          &GraphData::_modules) //
      ->annotate<ConstString>(
          "editor.factorylistbase", //
          "dflow/dgmodule");

  clazz
      ->accessorVariant(
          "zzz_connections", //
          &GraphInst::SerializeConnections,
          &GraphInst::DeserializeConnections)
      ->annotate<bool>(
          "editor.visible", //
          false);

  clazz->annotate(
      "editor.object.ops", //
      ConstString("dfgraph:dflowgraphedit import:dflowgraphimport export:dflowgraphexport"));
      */
}
///////////////////////////////////////////////////////////////////////////////
GraphData::GraphData()
    : _topologyDirty(true) {
}
///////////////////////////////////////////////////////////////////////////////
GraphData::~GraphData() {
}
///////////////////////////////////////////////////////////////////////////////
size_t GraphData::numModules() const {
  return _modules.size();
}
///////////////////////////////////////////////////////////////////////////////
void GraphData::markTopologyDirty(bool bv) {
  _topologyDirty = bv;
}
///////////////////////////////////////////////////////////////////////////////
void GraphData::clear() { // virtual
}
///////////////////////////////////////////////////////////////////////////////
bool GraphData::isTopologyDirty() const {
  return _topologyDirty;
}
///////////////////////////////////////////////////////////////////////////////
void GraphData::addModule(graphdata_ptr_t gd, const std::string& named, dgmoduledata_ptr_t pchild) {
  pchild->_name = named;
  gd->_modules.AddSorted(named, pchild);
  pchild->_graphdata = gd;
  gd->_topologyDirty = true;
  gd->OnGraphChanged();
}
///////////////////////////////////////////////////////////////////////////////
void GraphData::removeModule(graphdata_ptr_t gd, dgmoduledata_ptr_t pchild) {
  OrkAssert(false); // not implemented yet
}
///////////////////////////////////////////////////////////////////////////////
dgmoduledata_ptr_t GraphData::module(const std::string& named) const {
  dgmoduledata_ptr_t pret                                 = 0;
  auto it = _modules.find(named);
  if (it != _modules.end()) {
    pret = typedModuleData<DgModuleData>(it->second);
  }
  return pret;
}
///////////////////////////////////////////////////////////////////////////////
dgmoduledata_ptr_t GraphData::module(size_t indexed) const {
  return typedModuleData<DgModuleData>(_modules.GetItemAtIndex(indexed).second);
}
///////////////////////////////////////////////////////////////////////////////
bool GraphData::canConnect(inplugdata_constptr_t pin, outplugdata_constptr_t pout) const {
  return true; // TODO fixme
  //((&pin->GetDataTypeId()) == (&pout->GetDataTypeId()));
}
///////////////////////////////////////////////////////////////////////////////
void GraphData::safeConnect(inplugdata_ptr_t inp,
                             outplugdata_ptr_t outp) { 
  bool ok = canConnect(inp, outp);
  OrkAssert(ok);
  inp->_connectedOutput = outp;
  outp->_connections.push_back(inp);
}
///////////////////////////////////////////////////////////////////////////////
void GraphData::disconnect(inplugdata_ptr_t inp) {
  if (inp->_connectedOutput) {
    inp->_connectedOutput->_disconnect(inp);
  }
  inp->_connectedOutput = nullptr;
}
///////////////////////////////////////////////////////////////////////////////
void GraphData::disconnect(outplugdata_ptr_t output) {
  for(auto input : output->_connections ){
    input->_connectedOutput = nullptr;
  }
  output->_connections.clear();
}
///////////////////////////////////////////////////////////////////////////////
bool GraphData::serializeConnections(graphdata_const_ptr_t gd, ork::reflect::serdes::ISerializer& ser) {
  for (auto it = gd->_modules.begin(); it != gd->_modules.end(); it++) {
    object_ptr_t pobj                  = it->second;
    auto pdgmodule = std::dynamic_pointer_cast<DgModuleData>(pobj);
    if (pdgmodule) {
      pdgmodule->_name = it->first;
    }
  }
  /////////////////////////////////////////////
  int inumlinks = 0;
  for (auto item : gd->_modules ) {
    auto pmodule = std::dynamic_pointer_cast<DgModuleData>(item.second);
    if (pmodule) {
      int inuminputplugs = pmodule->numInputs();
      for (int ip = 0; ip < inuminputplugs; ip++) {
        auto pinput = pmodule->input(ip);

        auto poutput = pinput->_connectedOutput;
        if (poutput) {
          auto poutmodule = poutput->typedModuleData<DgModuleData>();
          inumlinks++;
        }
      }
    }
  }
  // ser.serializeElement(inumlinks);
  /////////////////////////////////////////////
  for (auto item : gd->_modules ) {
    auto pmodule = std::dynamic_pointer_cast<DgModuleData>(item.second);
    if (pmodule) {
      int inuminputplugs = pmodule->numInputs();
      for (int ip = 0; ip < inuminputplugs; ip++) {
        auto pinput         = pmodule->input(ip);
        auto poutput = pinput->_connectedOutput;
        if (poutput) {
          auto poutmodule = poutput->_parent_module;
          // ser.serializeElement(ork::PieceString(pmodule->GetName().c_str()));
          // ser.serializeElement(ork::PieceString(pinput->GetName().c_str()));
          // ser.serializeElement(ork::PieceString(poutput->GetModule()->GetName().c_str()));
          // ser.serializeElement(ork::PieceString(poutput->GetName().c_str()));
        }
      }
    }
  }
  /////////////////////////////////////////////
  return true;
}
///////////////////////////////////////////////////////////////////////////////
bool GraphData::deserializeConnections(graphdata_ptr_t gd, ork::reflect::serdes::IDeserializer& deser) {
  for (auto it = gd->_modules.begin(); it != gd->_modules.end(); it++) {
    object_ptr_t pobj                  = it->second;
    auto pdgmodule = std::dynamic_pointer_cast<DgModuleData>(pobj);
    if (pdgmodule) {
      pdgmodule->_name = it->first;
      pdgmodule->_parent = gd;
    }
  }
  /////////////////////////////////////////////
  // read number of links
  int inumlinks = 0;
  // deser.deserialize(inumlinks);
  /////////////////////////////////////////////
  for (int il = 0; il < inumlinks; il++) {
    std::string inp_mod_name;
    std::string inp_plg_name;
    std::string out_mod_name;
    std::string out_plg_name;
    //deser.deserialize(inp_mod_name);
    //deser.deserialize(inp_plg_name);
    //deser.deserialize(out_mod_name);
    //deser.deserialize(out_plg_name);
    /////////////////////////
    // make the connection
    /////////////////////////
    auto it_inp = gd->_modules.find(inp_mod_name);
    auto it_out = gd->_modules.find(out_mod_name);
    if (it_inp != gd->_modules.end() && it_out != gd->_modules.end()) {
      auto pinp_mod = typedModuleData<DgModuleData>(it_inp->second);
      auto pout_mod = typedModuleData<DgModuleData>(it_out->second);
      auto inp_plug  = typedPlugData<InPlugData>(pinp_mod->inputNamed(inp_plg_name));
      auto out_plug = typedPlugData<OutPlugData>(pout_mod->outputNamed(out_plg_name));
      if (inp_plug != nullptr && out_plug != nullptr) {
        gd->safeConnect(inp_plug, out_plug);
      }
    }
  }
  /////////////////////////////////////////////
  return true;
}
///////////////////////////////////////////////////////////////////////////////
bool GraphData::preDeserialize(reflect::serdes::IDeserializer&) {
  clear();
  _modules.clear();
  return true;
}
///////////////////////////////////////////////////////////////////////////////
bool GraphData::postDeserialize(reflect::serdes::IDeserializer&) {
  /////////////////////////////////
  // remove dangling null modules
  /////////////////////////////////

  auto modules_copy = _modules;
  _modules.clear();
  for (auto item : modules_copy) {
    auto sec = item.second;
    if (sec != nullptr) {
      _modules.AddSorted(item.first, item.second);
    }
  }

  /////////////////////////////////

  OnGraphChanged();
  return true;
}
///////////////////////////////////////////////////////////////////////////////
void GraphData::OnGraphChanged() {

  for (auto item : _modules) {
    auto module = typedModuleData<DgModuleData>(item.second);
    //printf("graph<%p> module<%p> name<%s>\n", (void*) this, (void*) module.get(), item.first.c_str());
    module->_name = item.first
;  }
}
///////////////////////////////////////////////////////////////////////////////
bool GraphData::isComplete() const {
  bool bcomp = true;
  for (auto item : _modules) {
    auto module = typedModuleData<DgModuleData>(item.second);;
    if (nullptr == module) {
      bcomp = false;
    }
  }
  return bcomp;
}
///////////////////////////////////////////////////////////////////////////////
graphinst_ptr_t GraphData::createGraphInst(graphdata_ptr_t gd){
    auto gi = std::make_shared<GraphInst>(gd);
    gi->_sharedThis = gi;
    return gi;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
