////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////

struct PlugInst {

  PlugInst(const PlugData* plugdata);
  virtual ~PlugInst();
  
  const std::type_info& GetDataTypeId() const {
    return _plugdata->mTypeId;
  }
  virtual void _doSetDirty(bool bv) {
  }

  const PlugData* _plugdata;
  bool mbDirty;
};

template <typename T> std::shared_ptr<T> typedPlugInst(pluginst_ptr_t p){
    return std::dynamic_pointer_cast<T>(p);
}

///////////////////////////////////////////////////////////////////////////////

struct InPlugInst : public PlugInst {

  InPlugInst(const PlugData* plugdata);
  ~InPlugInst();
  outpluginst_ptr_t connected() const;
  bool isConnected() const;
  bool isDirty() const;

  void _doSetDirty(bool bv) override; // virtual
};

///////////////////////////////////////////////////////////////////////////////

struct OutPlugInst : public PlugInst {

  OutPlugInst(const PlugData* plugdata);
  ~OutPlugInst();

  bool isConnected() const;
  size_t numConnections() const;
  inpluginst_ptr_t connected(size_t idx) const;
  void _doSetDirty(bool bv) override; // virtual
  bool isDirty() const;

  dataflow::node_hash _outputhash;
  dgregister* _register = nullptr;
};
} // namespace ork::dataflow
