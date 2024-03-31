////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::dataflow {

///////////////////////////////////////////////////////////////////////////////

struct PlugInst {

  PlugInst(const PlugData* plugdata, ModuleInst* minst);
  virtual ~PlugInst();

  const std::type_info& GetDataTypeId() const {
    return _plugdata->_typeID;
  }
  virtual void _doSetDirty(bool bv) {
  }

  bool connectedIsVarying() const;

  ModuleInst* _moduleinst = nullptr;
  const PlugData* _plugdata;
  bool mbDirty;
};

template <typename T> std::shared_ptr<T> typedPlugInst(pluginst_ptr_t p) {
  return std::dynamic_pointer_cast<T>(p);
}

///////////////////////////////////////////////////////////////////////////////

struct InPlugInst : public PlugInst {

  InPlugInst(const InPlugData* plugdata, ModuleInst* minst);
  ~InPlugInst();
  outpluginst_ptr_t connected() const;
  bool isConnected() const;
  bool isDirty() const;

  void _doSetDirty(bool bv) override; // virtual
  bool connectedIsVarying() const;

  outpluginst_ptr_t _connectedOutput;
  sigslot2::scoped_connection _connectionPlugConnectionChanged;
};

///////////////////////////////////////////////////////////////////////////////

struct OutPlugInst : public PlugInst {

  OutPlugInst(const OutPlugData* plugdata, ModuleInst* minst);
  ~OutPlugInst();

  bool isConnected() const;
  size_t numConnections() const;
  inpluginst_ptr_t connected(size_t idx) const;
  void _doSetDirty(bool bv) override; // virtual
  bool isDirty() const;
  bool isVarying() const;

  const OutPlugData* _outplugdata;

  dataflow::node_hash _outputhash;
};

///////////////////////////////////////////////////////////////////////////////

template <typename traits> //
class outpluginst : public OutPlugInst {

public:
  using data_type_t           = typename traits::elemental_inst_type;
  using data_type_ptr_t       = std::shared_ptr<data_type_t>;
  using data_type_const_ptr_t = std::shared_ptr<const data_type_t>;

  inline explicit outpluginst(const outplugdata<traits>* data, ModuleInst* minst);
  virtual data_type_const_ptr_t value_ptr() const;
  virtual data_type_ptr_t value_ptr();
  virtual const data_type_t& value() const;
  virtual void setValue(const data_type_t& v);

  data_type_ptr_t _value;
  const outplugdata<traits>* _typed_plugdata;
};

///////////////////////////////////////////////////////////////////////////////

template <typename traits> struct inpluginst : public InPlugInst {

public:
  using data_type_t           = typename traits::elemental_inst_type;
  using data_type_ptr_t       = std::shared_ptr<data_type_t>;
  using data_type_const_ptr_t = std::shared_ptr<const data_type_t>;

  explicit inpluginst(const inplugdata<traits>* data, ModuleInst* minst);
  data_type_const_ptr_t value_ptr() const;
  data_type_ptr_t value_ptr();
  virtual const data_type_t& value() const;
  void setValue(const data_type_t& v);

  data_type_ptr_t _value;
  const inplugdata<traits>* _typed_plugdata;
};

///////////////////////////////////////////////////////////////////////////////

struct floatxfinpluginst : public inpluginst<FloatXfPlugTraits> {

  floatxfinpluginst(const floatxfinplugdata_t* d, ModuleInst* minst);

  const float& value() const final;

  const floatxfinplugdata_t* _data = nullptr;
  mutable float _xfvalue;
};

///////////////////////////////////////////////////////////////////////////////

struct fvec3xfinpluginst : public inpluginst<Vec3XfPlugTraits> {

  fvec3xfinpluginst(const fvec3xfinplugdata_t* d, ModuleInst* minst);

  const fvec3& value() const final;

  const fvec3xfinplugdata_t* _data = nullptr;
  mutable fvec3 _xfvalue;
};

///////////////////////////////////////////////////////////////////////////////

struct fquatxfinpluginst : public inpluginst<QuatXfPlugTraits> {

  fquatxfinpluginst(const fquatxfinplugdata_t* d, ModuleInst* minst);

  const fquat& value() const final;

  const fquatxfinplugdata_t* _data = nullptr;
  mutable fquat _xfvalue;
};

///////////////////////////////////////////////////////////////////////////////

using float_inp_pluginst_t     = inpluginst<FloatPlugTraits>;
using float_inp_pluginst_ptr_t = std::shared_ptr<float_inp_pluginst_t>;

using floatxf_inp_pluginst_t     = inpluginst<FloatXfPlugTraits>;
using floatxf_inp_pluginst_ptr_t = std::shared_ptr<floatxf_inp_pluginst_t>;

using float_out_pluginst_t     = outpluginst<FloatPlugTraits>;
using float_out_pluginst_ptr_t = std::shared_ptr<float_out_pluginst_t>;

//

using fvec3_inp_pluginst_t     = inpluginst<Vec3fPlugTraits>;
using fvec3_inp_pluginst_ptr_t = std::shared_ptr<fvec3_inp_pluginst_t>;

using fvec3xf_inp_pluginst_t     = inpluginst<Vec3XfPlugTraits>;
using fvec3xf_inp_pluginst_ptr_t = std::shared_ptr<fvec3xf_inp_pluginst_t>;

using fvec3_out_pluginst_t     = outpluginst<Vec3fPlugTraits>;
using fvec3_out_pluginst_ptr_t = std::shared_ptr<fvec3_out_pluginst_t>;

//

using fquat_inp_pluginst_t     = inpluginst<QuatfPlugTraits>;
using fquat_inp_pluginst_ptr_t = std::shared_ptr<fquat_inp_pluginst_t>;

using fquatxf_inp_pluginst_t     = inpluginst<QuatXfPlugTraits>;
using fquatxf_inp_pluginst_ptr_t = std::shared_ptr<fquatxf_inp_pluginst_t>;

using fquat_out_pluginst_t     = outpluginst<QuatfPlugTraits>;
using fquat_out_pluginst_ptr_t = std::shared_ptr<fquat_out_pluginst_t>;

} // namespace ork::dataflow
