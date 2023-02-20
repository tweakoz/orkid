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

  PlugInst(moduleinst_ptr_t minst, plugdata_ptr_t plugdata);

  const std::type_info& GetDataTypeId() const {
    return _plugdata->mTypeId;
  }
  virtual void _doSetDirty(bool bv) {
  }

  plugdata_ptr_t _plugdata;
  moduleinst_ptr_t _owning_module;
  bool mbDirty;
};

///////////////////////////////////////////////////////////////////////////////

struct InPlugInst : public PlugInst {

  InPlugInst(moduleinst_ptr_t pmod, inplugdata_ptr_t data);
  ~InPlugInst();
  outpluginst_ptr_t connected() const;
  bool isConnected() const;

  void _doSetDirty(bool bv) override; // virtual
};

///////////////////////////////////////////////////////////////////////////////

struct OutPlugInst : public PlugInst {

  OutPlugInst(moduleinst_ptr_t pmod, outplugdata_ptr_t data);
  ~OutPlugInst();

  bool isConnected() const;
  size_t numConnections() const;
  inpluginst_ptr_t connected(size_t idx) const;
  void _doSetDirty(bool bv) override; // virtual

  outplugdata_ptr_t _outplugdata;
  dataflow::node_hash _outputhash;
  dgregister* _register = nullptr;
};
} // namespace ork::dataflow
