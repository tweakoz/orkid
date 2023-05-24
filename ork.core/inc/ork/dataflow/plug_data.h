////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/enum.h>

namespace ork::dataflow {

typedef std::string MorphKey;
typedef std::string MorphGroup;

struct nullpassthrudata;
struct floatpassthrudata;
struct fvec3passthrudata;
struct floatxfdata;
struct fvec3xfdata;

using floatxfdata_ptr_t = std::shared_ptr<floatxfdata>;
using fvec3xfdata_ptr_t = std::shared_ptr<fvec3xfdata>;

struct FloatPlugTraits {
  using elemental_data_type = float;
  using elemental_inst_type = float;
  using xformer_t                    = nullpassthrudata;
  using range_type = float_range;
  using out_traits_t = FloatPlugTraits;
  static constexpr size_t max_fanout = 0;
  static std::shared_ptr<float> data_to_inst(std::shared_ptr<float> inp);
};
struct Vec3fPlugTraits {
  using elemental_data_type = fvec3;
  using elemental_inst_type = fvec3;
  using xformer_t           = nullpassthrudata;
  using range_type = float_range;
  using out_traits_t = Vec3fPlugTraits;
  static constexpr size_t max_fanout = 0;
  static std::shared_ptr<fvec3> data_to_inst(std::shared_ptr<fvec3> inp);
};
struct FloatXfPlugTraits {
  using elemental_data_type = float;
  using elemental_inst_type = float;
  using xformer_t                    = floatxfdata;
  using range_type = float_range;
  using out_traits_t = FloatPlugTraits;
  static constexpr size_t max_fanout = 0;
  static std::shared_ptr<float> data_to_inst(std::shared_ptr<float> inp);
};
struct Vec3XfPlugTraits {
  using elemental_data_type = fvec3;
  using elemental_inst_type = fvec3;
  using xformer_t                    = fvec3xfdata;
  using range_type = float_range;
  using out_traits_t = Vec3fPlugTraits;
  static constexpr size_t max_fanout = 0;
  static std::shared_ptr<fvec3> data_to_inst(std::shared_ptr<fvec3> inp);
};

struct morph_event : public event::Event {
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

struct PlugData : public ork::Object {
  DeclareAbstractX(PlugData, ork::Object);

public:
  PlugData(moduledata_ptr_t pmod, EPlugDir edir, EPlugRate epr, const std::type_info& tid, const char* pname);

  const std::type_info& GetDataTypeId() const;

  template <typename T> std::shared_ptr<T> typedModuleData();

  moduledata_ptr_t _parent_module;
  EPlugDir _plugdir;
  EPlugRate _plugrate;
  const std::type_info& _typeID;
  std::string _name;
};

///////////////////////////////////////////////////////////////////////////////

struct InPlugData : public PlugData {

  DeclareAbstractX(InPlugData, PlugData);

  static constexpr size_t NOPATH = 0xffffffffffffffff;

public:
  InPlugData(moduledata_ptr_t pmod, EPlugRate epr, const std::type_info& tid, const char* pname);
  ~InPlugData();

  bool isConnected() const;
  bool isMorphable() const;
  size_t computeMinDepth(dgmoduledata_constptr_t to_module) const;
  virtual inpluginst_ptr_t createInstance(ModuleInst* minst) const;

  outplugdata_ptr_t _connectedOutput;                      // which EXTERNAL output plug are we connected to
  orkvector<outplugdata_ptr_t> _internalOutputConnections; // which output plugs IN THE SAME MODULE are connected to me ?
  morphable_ptr_t mpMorphable;
  mutable sigslot2::signal_void_t _sigPlugConnectionChanged;
};

///////////////////////////////////////////////////////////////////////////////

struct OutPlugData : public PlugData {

  DeclareAbstractX(OutPlugData, PlugData);

public:
  OutPlugData(moduledata_ptr_t pmod, EPlugRate epr, const std::type_info& tid, const char* pname);
  ~OutPlugData();

  virtual size_t maxFanOut() const;
  bool isConnected() const;
  size_t numConnections() const;
  inplugdata_ptr_t connected(size_t idx) const;

  void _disconnect(inplugdata_ptr_t pinplug);

  virtual outpluginst_ptr_t createInstance(ModuleInst* minst) const;

  mutable orkvector<inplugdata_ptr_t> _connections;
};

///////////////////////////////////////////////////////////////////////////////

template <typename traits> //
struct outplugdata : public OutPlugData {
  DeclareTemplateAbstractX(outplugdata<traits>, OutPlugData);

public:
  using traits_t        = traits;
  using data_type_t     = typename traits_t::elemental_data_type;
  using data_type_ptr_t = std::shared_ptr<data_type_t>;

  explicit outplugdata(
      moduledata_ptr_t pmod, //
      EPlugRate epr,
      const char* pname);

  size_t maxFanOut() const override;
  const data_type_t& value() const;
  void setValue(const data_type_t& v);

  outpluginst_ptr_t createInstance(ModuleInst* minst) const override;

  data_type_ptr_t _value;
};

///////////////////////////////////////////////////////////////////////////////

template <typename traits> struct inplugdata : public InPlugData {
  DeclareTemplateAbstractX(inplugdata<traits>, InPlugData);

public:
  using traits_t        = traits;
  using data_type_t     = typename traits_t::elemental_data_type;
  using data_type_ptr_t = std::shared_ptr<data_type_t>;
  using range_t = typename traits_t::range_type;

  explicit inplugdata(
      moduledata_ptr_t pmod, //
      EPlugRate epr,         //
      const char* pname);

  const data_type_t& value() const;
  void setValue(const data_type_t& v);

  data_type_ptr_t _value;
  object_ptr_t _transformer;
  range_t _range;

  inpluginst_ptr_t createInstance(ModuleInst* minst) const override;
};

///////////////////////////////////////////////////////////////////////////////

struct floatinplugdata : public inplugdata<FloatPlugTraits> {
  DeclareAbstractX(floatinplugdata, inplugdata<FloatPlugTraits>);

public:
  floatinplugdata(moduledata_ptr_t pmod, EPlugRate epr, const char* pname)
      : inplugdata<FloatPlugTraits>(pmod, epr, pname) {
  }
};

///////////////////////////////////////////////////////////////////////////////

struct vect3inplugdata : public inplugdata<Vec3fPlugTraits> {
  DeclareAbstractX(vect3inplugdata, inplugdata<Vec3fPlugTraits>);

public:
  vect3inplugdata(moduledata_ptr_t pmod, EPlugRate epr, const char* pname)
      : inplugdata<Vec3fPlugTraits>(pmod, epr, pname) {
  }
};

///////////////////////////////////////////////////////////////////////////////
struct modscabiasdata : public ork::Object {
  DeclareConcreteX(modscabiasdata, ork::Object);

public:
  float _mod   = 0.0f;
  float _scale = 1.0f;
  float _bias  = 0.0f;
};

using modscabiasdata_ptr_t = std::shared_ptr<modscabiasdata>;

///////////////////////////////////////////////////////////////////////////////

struct floatxfitembasedata : public ork::Object {
  DeclareAbstractX(floatxfitembasedata, ork::Object);

public:
  virtual float transform(float inp) const = 0;
};

using floatxfitembasedata_ptr_t = std::shared_ptr<floatxfitembasedata>;

///////////////////////////////////////////////////////////////////////////////

struct floatxfmsbcurvedata : public floatxfitembasedata {
  DeclareConcreteX(floatxfmsbcurvedata, floatxfitembasedata);

public:
  floatxfmsbcurvedata();
  float transform(float input) const final; // virtual

  multicurve1d_ptr_t _multicurve;
  modscabiasdata_ptr_t _modscalebias;
  bool _domodscalebias = false;
  bool _docurve        = false;
};

///////////////////////////////////////////////////////////////////////////////

struct floatxfmodstepdata : public floatxfitembasedata {
  DeclareConcreteX(floatxfmodstepdata, floatxfitembasedata);

public:
  float transform(float input) const final; // virtual

  float _mod         = 1.0f;
  int _steps         = 4;
  float _outputScale = 1.0f;
  float _outputBias  = 1.0f;
};

///////////////////////////////////////////////////////////////////////////////

struct floatxfdata : public ork::Object {
  DeclareAbstractX(floatxfdata, ork::Object);

public:
  float transform(float inp) const;
  floatxfdata();
  ~floatxfdata();

  orklut<std::string, floatxfitembasedata_ptr_t> _transforms;
  int _test = 0;
};

struct nullpassthrudata : public ork::Object {
  DeclareAbstractX(nullpassthrudata, ork::Object);
  orklut<std::string, floatxfitembasedata_ptr_t> _transforms;
};
struct floatpassthrudata : public ork::Object {
  DeclareAbstractX(floatpassthrudata, ork::Object);
  float transform(float inp) const {
    return inp;
  }
};
struct fvec3passthrudata : public ork::Object {
  DeclareAbstractX(fvec3passthrudata, ork::Object);
  fvec3 transform(fvec3 inp) const {
    return inp;
  }
};

using fvec3passthrudata_ptr_t = std::shared_ptr<fvec3passthrudata>;
///////////////////////////////////////////////////////////////////////////////

struct fvec3xfdata : public ork::Object {
  DeclareConcreteX(fvec3xfdata, ork::Object);

public:
  fvec3xfdata();
  fvec3 transform(const fvec3& input) const;
  floatxfdata_ptr_t _transformX;
  floatxfdata_ptr_t _transformY;
  floatxfdata_ptr_t _transformZ;
  orklut<std::string, fvec3passthrudata_ptr_t> _transforms;
};

///////////////////////////////////////////////////////////////////////////////

using floatoutplug_t = outplugdata<FloatPlugTraits>;
using floatoutplug_ptr_t = std::shared_ptr<floatoutplug_t>;
using floatinpplug_ptr_t = std::shared_ptr<inplugdata<FloatPlugTraits>>;

using floatxfinplugdata_t = inplugdata<FloatXfPlugTraits>;
using fvec3xfinplugdata_t = inplugdata<Vec3XfPlugTraits>;

using floatxfinplugdata_ptr_t = std::shared_ptr<floatxfinplugdata_t>;
using fvec3xfinplugdata_ptr_t = std::shared_ptr<fvec3xfinplugdata_t>;

} // namespace ork::dataflow
