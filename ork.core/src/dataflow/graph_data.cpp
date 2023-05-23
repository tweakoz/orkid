////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/reflect/properties/register.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/registerX.inl>

#include <ork/dataflow/all.h>
#include <ork/dataflow/module.inl>
#include <ork/dataflow/plug_data.inl>
#include <ork/reflect/IDeserializer.inl>
template class ork::orklut<std::string, ork::dataflow::graphdata_ptr_t>;

///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::dataflow::GraphData, "dflow/graphdata");
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow {
///////////////////////////////////////////////////////////////////////////////
struct ConnectionsProperty : public reflect::ObjectProperty {
  //////////////////////////////////////////////////////////////////
  void deserialize(reflect::serdes::node_ptr_t deser_node) const {

    using namespace ork::reflect;

    auto instance   = deser_node->_deser_instance;
    auto gdata      = std::dynamic_pointer_cast<GraphData>(instance);

    auto num_links_node = serdes::fetchNextMapSubLeaf(deser_node);
    OrkAssert(num_links_node->_key == "numlinks");
    int inumlinks = -1;
    serdes::decode_value<int>(num_links_node->_value, inumlinks);
    for( int i=0; i<inumlinks; i++ ){
      auto conn_node = serdes::fetchNextMapSubLeaf(deser_node);

      auto c1 = conn_node->_deser_blind_children[0];
      auto c2 = conn_node->_deser_blind_children[1];
      auto c3 = conn_node->_deser_blind_children[2];
      auto c4 = conn_node->_deser_blind_children[3];
      auto inp_mod_name = c1->_value.get<std::string>();
      auto inp_plg_name = c2->_value.get<std::string>();
      auto out_mod_name = c3->_value.get<std::string>();
      auto out_plg_name = c4->_value.get<std::string>();

      //printf( "conn_node<%s>\n", conn_node->_key.c_str() );
      //printf( "c1<%s> inp_mod_name<%s>\n", c1->_name.c_str(), inp_mod_name.c_str() );
      //printf( "c2<%s> inp_plg_name<%s>\n", c2->_name.c_str(), inp_plg_name.c_str() );
      //printf( "c3<%s> out_mod_name<%s>\n", c3->_name.c_str(), out_mod_name.c_str() );
      //printf( "c4<%s> out_plg_name<%s>\n", c4->_name.c_str(), out_plg_name.c_str() );

      /////////////////////////
      // make the connection
      /////////////////////////
      auto it_inp = gdata->_modules.find(inp_mod_name);
      auto it_out = gdata->_modules.find(out_mod_name);
      if (it_inp != gdata->_modules.end() and it_out != gdata->_modules.end()) {
        auto pinp_mod = typedModuleData<DgModuleData>(it_inp->second);
        auto pout_mod = typedModuleData<DgModuleData>(it_out->second);
        auto inp_plug  = typedPlugData<InPlugData>(pinp_mod->inputNamed(inp_plg_name));
        auto out_plug = typedPlugData<OutPlugData>(pout_mod->outputNamed(out_plg_name));
        if (inp_plug != nullptr && out_plug != nullptr) {
          gdata->safeConnect(inp_plug, out_plug);
        }
      }
    }
  }
  //////////////////////////////////////////////////////////////////
  void serialize(reflect::serdes::node_ptr_t ser_node) const {
    using namespace ork::reflect;
    auto serializer = ser_node->_serializer;
    auto instance   = ser_node->_ser_instance;
    auto gdata      = std::dynamic_pointer_cast<const GraphData>(instance);
    for (auto it = gdata->_modules.begin(); //
         it != gdata->_modules.end();       //
         it++) {                            //
      object_ptr_t pobj = it->second;
      auto pdgmodule    = std::dynamic_pointer_cast<DgModuleData>(pobj);
      if (pdgmodule) {
        pdgmodule->_name = it->first;
      }
    }
    /////////////////////////////////////////////
    int inumlinks = 0;
    for (auto item : gdata->_modules) {
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
    /////////////////////////////////////////////
    int index = 0;
    auto sub_node           = serializer->pushNode(_name, serdes::NodeType::MAP);
    sub_node->_parent       = ser_node;
    sub_node->_ser_instance = instance;
    sub_node->_serializer   = serializer;
    serializeMapSubLeaf<int>(sub_node, "numlinks", inumlinks );
    /////////////////////////////////////////////
    for (auto item : gdata->_modules) {
      auto pmodule = std::dynamic_pointer_cast<DgModuleData>(item.second);
      if (pmodule) {
        auto inp_module_name = pmodule->_name;
        int inuminputplugs = pmodule->numInputs();
        for (int ip = 0; ip < inuminputplugs; ip++) {
          auto pinput  = pmodule->input(ip);
          auto inp_plug_name = pinput->_name;
          auto poutput = pinput->_connectedOutput;
          if (poutput) {

            auto conn_name = FormatString("conn-%d", index);
            auto poutmodule = poutput->_parent_module;
            auto out_module_name = poutmodule->_name;
            auto out_plug_name = poutput->_name;
            auto conn_node = serializer->pushNode(conn_name, serdes::NodeType::MAP);
            conn_node->_parent       = sub_node;
            conn_node->_ser_instance = instance;
            conn_node->_serializer   = serializer;
              serializeMapSubLeaf<std::string>(conn_node, "inp_module", inp_module_name );
              serializeMapSubLeaf<std::string>(conn_node, "inp_plug", inp_plug_name );
              serializeMapSubLeaf<std::string>(conn_node, "out_module", out_module_name );
              serializeMapSubLeaf<std::string>(conn_node, "out_plug", out_plug_name );
            serializer->popNode(); // pop conn_node

            index++;

          }
        }
      }
    }
    serializer->popNode(); // pop ary_node
    /////////////////////////////////////////////
  }
};
///////////////////////////////////////////////////////////////////////////////
void GraphData::describeX(object::ObjectClass* clazz) {
  clazz
      ->directObjectMapProperty(
          "Modules",                //
          &GraphData::_modules)     //
      ->annotate<ConstString>(      //
          "editor.factorylistbase", //
          "dflow::DgModuleData")
      ->annotate<ConstString>( //
          "editor.object.ops", //
          "dfgraph:dflowgraphedit import:dflowgraphimport export:dflowgraphexport");

  auto con_prop = new ConnectionsProperty();
  con_prop->annotate("editor.visible", false);
  clazz->Description().addProperty("zzz_connections", con_prop);
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
  dgmoduledata_ptr_t pret = 0;
  auto it                 = _modules.find(named);
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
void GraphData::safeConnect(inplugdata_ptr_t inp, outplugdata_ptr_t outp) {
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
  for (auto input : output->_connections) {
    input->_connectedOutput = nullptr;
  }
  output->_connections.clear();
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
  // finish connections
  /////////////////////////////////

  for( auto c : _deser_connections ){
    //
    auto out_mod = module(c->_out_module);
    auto inp_mod = module(c->_inp_module);
    OrkAssert(out_mod!=nullptr);
    OrkAssert(inp_mod!=nullptr);
    //
    auto out_plug = out_mod->outputNamed(c->_out_plug);
    auto inp_plug = inp_mod->inputNamed(c->_inp_plug);
    OrkAssert(out_plug!=nullptr);
    OrkAssert(inp_plug!=nullptr);
    //S
    safeConnect(inp_plug,out_plug);
  }

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
    // printf("graph<%p> module<%p> name<%s>\n", (void*) this, (void*) module.get(), item.first.c_str());
    module->_name = item.first;
  }
}
///////////////////////////////////////////////////////////////////////////////
bool GraphData::isComplete() const {
  bool bcomp = true;
  for (auto item : _modules) {
    auto module = typedModuleData<DgModuleData>(item.second);
    ;
    if (nullptr == module) {
      bcomp = false;
    }
  }
  return bcomp;
}
///////////////////////////////////////////////////////////////////////////////
graphinst_ptr_t GraphData::createGraphInst(graphdata_ptr_t gd) {
  auto gi         = std::make_shared<GraphInst>(gd);
  gi->_sharedThis = gi;
  return gi;
}
///////////////////////////////////////////////////////////////////////////////
}} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
