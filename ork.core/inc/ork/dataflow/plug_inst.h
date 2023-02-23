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

  outpluginst_ptr_t _connectedOutput;                       

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
};

///////////////////////////////////////////////////////////////////////////////

template <typename vartype> //
class outpluginst : public OutPlugInst {

public:

  using data_type_t = vartype;
  using data_type_ptr_t = std::shared_ptr<vartype>;

  inline explicit outpluginst( const outplugdata<vartype>* data ) //
      : OutPlugInst(data) //
      , _typed_plugdata(data) { //

      _value = std::make_shared<data_type_t>();
  }

  data_type_ptr_t _value;
  const outplugdata<vartype>* _typed_plugdata;
};

///////////////////////////////////////////////////////////////////////////////

template <typename vartype> struct inpluginst : public InPlugInst {

public:

  using data_type_t = vartype;
  using data_type_ptr_t = std::shared_ptr<vartype>;

  inline explicit inpluginst( const inplugdata<vartype>* data ) //
      : InPlugInst(data) //
      , _typed_plugdata(data) { //
  }

  virtual data_type_ptr_t value() const;

  data_type_ptr_t _default;
  const inplugdata<vartype>* _typed_plugdata;

};

} // namespace ork::dataflow
