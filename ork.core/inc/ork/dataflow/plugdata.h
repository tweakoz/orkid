////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork::dataflow {

typedef std::string MorphKey;
typedef std::string MorphGroup;

class morph_event : public event::Event {
public:
  EMorphEventType meType;
  float mfMorphValue;
  MorphGroup mMorphGroup;

  morph_event()
      : meType(EMET_END)
      , mfMorphValue(0.0f) {
  }
};

struct imorphtarget {
  virtual void Apply() = 0;
};
struct float_morphtarget : public imorphtarget {
  float mfValue;
};

struct morphitem {
  MorphKey mKey;
  float mWeight;

  morphitem()
      : mWeight(0.0f) {
  }
};

struct morphable {
  void HandleMorphEvent(const morph_event* me);
  virtual void WriteMorphTarget(MorphKey name, float flerpval) = 0;
  virtual void RecallMorphTarget(MorphKey name)                = 0;
  virtual void Morph1D(const morph_event* pevent)              = 0;

  static const int kmaxweights = 2;
  morphitem mMorphItems[kmaxweights];
};

///////////////////////////////////////////////////////////////////////////////

template <typename T> int MaxFanout(void);

///////////////////////////////////////////////////////////////////////////////

struct PlugData : public ork::Object {
  DeclareAbstractX(PlugData, ork::Object);

public:

  PlugData(moduledata_ptr_t pmod, EPlugDir edir, EPlugRate epr, const std::type_info& tid, const char* pname);

  const std::type_info& GetDataTypeId() const {
    return mTypeId;
  }

  template <typename T> std::shared_ptr<T> typedModuleData(){
    return std::dynamic_pointer_cast<T>(_parent_module);
  }

  moduledata_ptr_t _parent_module;
  EPlugDir _plugdir;
  EPlugRate _plugrate;
  const std::type_info& mTypeId;
  std::string _name;

};

template <typename T> std::shared_ptr<T> typedPlugData(plugdata_ptr_t p){
    return std::dynamic_pointer_cast<T>(p);
}


///////////////////////////////////////////////////////////////////////////////

struct InPlugData : public PlugData {

  DeclareAbstractX(InPlugData, PlugData);

public:

  InPlugData(moduledata_ptr_t pmod, EPlugRate epr, const std::type_info& tid, const char* pname);
  ~InPlugData();

  bool isConnected() const;
  bool isMorphable() const;

  outplugdata_ptr_t _connectedOutput;                       // which EXTERNAL output plug are we connected to
  orkvector<outplugdata_ptr_t> _internalOutputConnections; // which output plugs IN THE SAME MODULE are connected to me ?
  morphable_ptr_t mpMorphable;
};

///////////////////////////////////////////////////////////////////////////////

struct OutPlugData : public PlugData {

  DeclareAbstractX(OutPlugData, PlugData);

public:

  OutPlugData(moduledata_ptr_t pmod, EPlugRate epr, const std::type_info& tid, const char* pname);
  ~OutPlugData();

  virtual int MaxFanOut() const {
    return 0;
  }

  bool isConnected() const {
    return (numConnections() != 0);
  }

  size_t numConnections() const {
    return _connections.size();
  }
  inplugdata_ptr_t connected(size_t idx) const {
    return _connections[idx];
  }

  void _disconnect(inplugdata_ptr_t pinplug);

  mutable orkvector<inplugdata_ptr_t> _connections;

};

///////////////////////////////////////////////////////////////////////////////

template <typename vartype> //
class outplugdata : public OutPlugData {
  DeclareTemplateConcreteX(outplugdata<vartype>, OutPlugData);

public:
  void operator=(const outplugdata<vartype>& oth) {
    new (this) outplugdata<vartype>(oth);
  }
  outplugdata(const outplugdata<vartype>& oth)
      : OutPlugData(oth._parent_module, oth._plugrate, oth.GetDataTypeId(), oth._name.c_str())
      {
  }

  outplugdata()
      : OutPlugData(0, EPR_EVENT, typeid(vartype), 0)
      {
  }

  outplugdata(moduledata_ptr_t pmod, EPlugRate epr, const vartype default_value, const char* pname)
      : OutPlugData(pmod, epr, typeid(vartype), pname)
      , _default(default_value)
      {
  }
  outplugdata(moduledata_ptr_t pmod, EPlugRate epr, const vartype default_value, const std::type_info& tinfo, const char* pname)
      : OutPlugData(pmod, epr, tinfo, pname)
      , _default(default_value) {
  }
  int MaxFanOut() const override {
    return MaxFanout<vartype>();
  }
  ///////////////////////////////////////////////////////////////
  //void connectData(const vartype* pd) {
    //mOutputData = pd;
  //}
  ///////////////////////////////////////////////////////////////
  // Internal value access (will not ever give effective value)
 // const vartype& internalData() const;
  ///////////////////////////////////////////////////////////////
  //const vartype&  value() const; // virtual
                                   ///////////////////////////////////////////////////////////////

  vartype _default;
  //const vartype* mOutputData;
};

///////////////////////////////////////////////////////////////////////////////

template <typename vartype> struct inplugdata : public InPlugData {
  DeclareTemplateAbstractX(inplugdata<vartype>, InPlugData);

public:

  explicit inplugdata(moduledata_ptr_t pmod, EPlugRate epr, vartype& def, const char* pname)
      : InPlugData(pmod, epr, typeid(vartype), pname)
      , _default(def) {
  }

  ////////////////////////////////////////////

  void connect(outplugdata<vartype>* vt) {
    connectExternal(vt);
  }
  void connect(outplugdata<vartype>& vt) {
    connectExternal(&vt);
  }

  ///////////////////////////////////////////////////////////////

  //template <typename T> std::shared_ptr<T> typedOutput() {
    //return std::dynamic_ptr_cast<T>(mExternalOutput);
  //}

  ///////////////////////////////////////////////////////////////

  /*inline const vartype& GetValue() // virtual
  {
    outplugdata<vartype>* connected = 0;
    GetTypedInput(connected);
    return (connected != 0) ? (connected->GetValue()) : mDefault;
  }*/

  ///////////////////////////////////////////////////////////////

  vartype& _default; // its a reference to prevent large plugs taking up memory
                     // in the unconnected state it can connect to a global dummy
};

///////////////////////////////////////////////////////////////////////////////

struct floatinplugdata : public inplugdata<float> {
  DeclareAbstractX(floatinplugdata, inplugdata<float>);

public:
  floatinplugdata(moduledata_ptr_t pmod, EPlugRate epr, float& def, const char* pname)
      : inplugdata<float>(pmod, epr, def, pname) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct vect3inplugdata : public inplugdata<fvec3> {
  DeclareAbstractX(vect3inplugdata, inplugdata<fvec3>);

public:
  vect3inplugdata(moduledata_ptr_t pmod, EPlugRate epr, fvec3& def, const char* pname)
      : inplugdata<fvec3>(pmod, epr, def, pname) {
  }
};

///////////////////////////////////////////////////////////////////////////////

template <typename xf> struct floatinplugxfdata : public floatinplugdata {

  DeclareTemplateAbstractX(floatinplugxfdata<xf>, floatinplugdata);

public:
  explicit floatinplugxfdata(moduledata_ptr_t pmod, EPlugRate epr, float& def, const char* pname)
      : floatinplugdata(pmod, epr, def, pname)
      , mtransform() {
  }
  ///////////////////////////////////////////////////////////////

  /*inline const float& GetValue() // virtual
  {
    outplugdata<float>* connected = 0;
    GetTypedInput(connected);
    mtransformed = mtransform.transform((connected != 0) ? (connected->GetValue()) : mDefault);
    return mtransformed;
  }*/

  xf mtransform;
  //mutable float mtransformed;
};

///////////////////////////////////////////////////////////////////////////////

template <typename xf> struct vect3inplugxfdata : public vect3inplugdata {
  DeclareTemplateAbstractX(vect3inplugxfdata<xf>, vect3inplugdata);

public:
  explicit vect3inplugxfdata(moduledata_ptr_t pmod, EPlugRate epr, fvec3& def, const char* pname)
      : vect3inplugdata(pmod, epr, def, pname)
      , mtransform() {
  }

  ///////////////////////////////////////////////////////////////

  xf& GetTransform() {
    return mtransform;
  }

  ///////////////////////////////////////////////////////////////

  /*inline const fvec3& GetValue() // virtual
  {
    outplugdata<fvec3>* connected = 0;
    typedInput(connected);
    mtransformed = mtransform.transform((connected != 0) ? (connected->GetValue()) : mDefault);
    return mtransformed;
  }*/

private:
  xf mtransform;
  mutable fvec3 mtransformed;
  ork::Object* XfAccessor() {
    return &mtransform;
  }
};

///////////////////////////////////////////////////////////////////////////////
struct modscabias : public ork::Object {
  DeclareConcreteX(modscabias, ork::Object);

public:

  modscabias()
      : _mod(1.0f)
      , _scale(1.0f)
      , _bias(0.0f) {
  }

  float _mod;
  float _scale;
  float _bias;
};

///////////////////////////////////////////////////////////////////////////////

struct floatxfitembase : public ork::Object {
  DeclareAbstractX(floatxfitembase, ork::Object);

public:
  virtual float transform(float inp) const = 0;
};

///////////////////////////////////////////////////////////////////////////////

struct floatxfmsbcurve : public floatxfitembase {
  DeclareConcreteX(floatxfmsbcurve, floatxfitembase);

public:
  float mod() const {
    return mModScaleBias._mod;
  }
  float scale() const {
    return mModScaleBias._scale;
  }
  float bias() const {
    return mModScaleBias._bias;
  }

  void detMod(float val) {
    mModScaleBias._mod = val;
  }
  void detScale(float val) {
    mModScaleBias._scale = (val);
  }
  void detBias(float val) {
    mModScaleBias._bias = (val);
  }

  floatxfmsbcurve()
      : mbDoModScaBia(false)
      , mbDoCurve(false) {
  }

  float transform(float input) const override; // virtual

  ork::MultiCurve1D mMultiCurve1d;
  modscabias mModScaleBias;
  bool mbDoModScaBia;
  bool mbDoCurve;
};

///////////////////////////////////////////////////////////////////////////////

class floatxfmodstep : public floatxfitembase {
  DeclareConcreteX(floatxfmodstep, floatxfitembase);

public:
  floatxfmodstep()
      : mMod(1.0f)
      , miSteps(4)
      , mOutputScale(1.0f)
      , mOutputBias(1.0f) {
  }

  float transform(float input) const override; // virtual

private:
  float mMod;
  int miSteps;
  float mOutputScale;
  float mOutputBias;
};

///////////////////////////////////////////////////////////////////////////////

class floatxf : public ork::Object {
  DeclareAbstractX(floatxf, ork::Object);

public:
  float transform(float inp) const;
  floatxf();
  ~floatxf();

private:
  orklut<std::string, ork::Object*> mTransforms;
  int miTest;
};

///////////////////////////////////////////////////////////////////////////////

class vect3xf : public ork::Object {
  DeclareConcreteX(vect3xf, ork::Object);

public:
  const floatxf& GetTransformX() const {
    return mTransformX;
  }
  const floatxf& GetTransformY() const {
    return mTransformY;
  }
  const floatxf& GetTransformZ() const {
    return mTransformZ;
  }

  fvec3 transform(const fvec3& input) const;

private:
  ork::Object* TransformXAccessor() {
    return &mTransformX;
  }
  ork::Object* TransformYAccessor() {
    return &mTransformY;
  }
  ork::Object* TransformZAccessor() {
    return &mTransformZ;
  }

  floatxf mTransformX;
  floatxf mTransformY;
  floatxf mTransformZ;
};

using floatxfinplug = floatinplugxfdata<floatxf> ;
using vect3xfinplug = vect3inplugxfdata<vect3xf>;

using floatoutplug_ptr_t = std::shared_ptr<ork::dataflow::outplugdata<float>>;


} //namespace ork { namespace dataflow {
