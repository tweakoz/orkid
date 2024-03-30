////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/dataflow/enum.h>
#include <ork/math/cvector3.h>
#include <ork/math/quaternion.h>

namespace ork::dataflow {

typedef std::string MorphKey;
typedef std::string MorphGroup;

struct nullpassthrudata;
struct floatpassthrudata;
struct fvec3passthrudata;
struct fquatpassthrudata;
struct floatxfdata;
struct fvec3xfdata;
struct fquatxfdata;

using floatxfdata_ptr_t = std::shared_ptr<floatxfdata>;
using fvec3xfdata_ptr_t = std::shared_ptr<fvec3xfdata>;
using fquatxfdata_ptr_t = std::shared_ptr<fquatxfdata>;

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
struct QuatfPlugTraits {
  using elemental_data_type = fquat;
  using elemental_inst_type = fquat;
  using xformer_t           = nullpassthrudata;
  using range_type = float_range;
  using out_traits_t = QuatfPlugTraits;
  static constexpr size_t max_fanout = 0;
  static std::shared_ptr<fquat> data_to_inst(std::shared_ptr<fquat> inp);
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
struct QuatXfPlugTraits {
  using elemental_data_type = fquat;
  using elemental_inst_type = fquat;
  using xformer_t                    = fquatxfdata;
  using range_type = float_range;
  using out_traits_t = QuatfPlugTraits;
  static constexpr size_t max_fanout = 0;
  static std::shared_ptr<fquat> data_to_inst(std::shared_ptr<fquat> inp);
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

struct quatinplugdata : public inplugdata<QuatfPlugTraits> {
  DeclareAbstractX(quatinplugdata, inplugdata<QuatfPlugTraits>);

public:
  quatinplugdata(moduledata_ptr_t pmod, EPlugRate epr, const char* pname)
      : inplugdata<QuatfPlugTraits>(pmod, epr, pname) {
  }
};

///////////////////////////////////////////////////////////////////////////////
struct modxfdata : public ork::Object {
  DeclareConcreteX(modxfdata, ork::Object);

public:
  float _mod   = 0.0f;
};
using modxfdata_ptr_t = std::shared_ptr<modxfdata>;
///////////////////////////////////////////////////////////////////////////////
struct scalexfdata : public ork::Object {
  DeclareConcreteX(scalexfdata, ork::Object);

public:
  float _scale   = 0.0f;
};
using scalexfdata_ptr_t = std::shared_ptr<scalexfdata>;
///////////////////////////////////////////////////////////////////////////////
struct biasxfdata : public ork::Object {
  DeclareConcreteX(biasxfdata, ork::Object);

public:
  float _bias   = 0.0f;
};
using biasxfdata_ptr_t = std::shared_ptr<biasxfdata>;
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

struct floatxfmoddata : public floatxfitembasedata {
  DeclareConcreteX(floatxfmoddata, floatxfitembasedata);

public:
  floatxfmoddata();
  float transform(float input) const final; // virtual

  modxfdata_ptr_t _moddata;
  bool _domod        = true;
};

using floatxfmoddata_ptr_t = std::shared_ptr<floatxfmoddata>;

///////////////////////////////////////////////////////////////////////////////

struct floatxfscaledata : public floatxfitembasedata {
  DeclareConcreteX(floatxfscaledata, floatxfitembasedata);

public:
  floatxfscaledata();
  float transform(float input) const final; // virtual

  scalexfdata_ptr_t _scaledata;
  bool _doscale        = true;
};

using floatxfscaledata_ptr_t = std::shared_ptr<floatxfscaledata>;

///////////////////////////////////////////////////////////////////////////////

struct floatxfbiasdata : public floatxfitembasedata {
  DeclareConcreteX(floatxfbiasdata, floatxfitembasedata);

public:
  floatxfbiasdata();
  float transform(float input) const final; // virtual

  biasxfdata_ptr_t _biasdata;
  bool _dobias        = true;
};

using floatxfbiasdata_ptr_t = std::shared_ptr<floatxfbiasdata>;

///////////////////////////////////////////////////////////////////////////////

struct floatxfsinedata : public floatxfitembasedata {
  DeclareConcreteX(floatxfsinedata, floatxfitembasedata);

public:
  floatxfsinedata();
  float transform(float input) const final; // virtual

  bool _dosine        = true;
};

using floatxfsinedata_ptr_t = std::shared_ptr<floatxfsinedata>;

///////////////////////////////////////////////////////////////////////////////

struct floatxfpowdata : public floatxfitembasedata {
  DeclareConcreteX(floatxfpowdata, floatxfitembasedata);

public:
  floatxfpowdata();
  float transform(float input) const final; // virtual

  float _power       = 1.0f;
  bool _dopow        = true;
};

using floatxfpowdata_ptr_t = std::shared_ptr<floatxfpowdata>;

///////////////////////////////////////////////////////////////////////////////

struct floatxfabsdata : public floatxfitembasedata {
  DeclareConcreteX(floatxfabsdata, floatxfitembasedata);

public:
  floatxfabsdata();
  float transform(float input) const final; // virtual

  bool _doabs        = true;
};

using floatxfabsdata_ptr_t = std::shared_ptr<floatxfabsdata>;

///////////////////////////////////////////////////////////////////////////////

struct floatxfcurvedata : public floatxfitembasedata {
  DeclareConcreteX(floatxfcurvedata, floatxfitembasedata);

public:
  floatxfcurvedata();
  float transform(float input) const final; // virtual

  multicurve1d_ptr_t _multicurve;
  bool _docurve        = true;
};

using floatxfcurvedata_ptr_t = std::shared_ptr<floatxfcurvedata>;

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
  DeclareConcreteX(floatxfdata, ork::Object);

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
struct fquatpassthrudata : public ork::Object {
  DeclareAbstractX(fquatpassthrudata, ork::Object);
  fquat transform(fquat inp) const {
    return inp;
  }
};

using fvec3passthrudata_ptr_t = std::shared_ptr<fvec3passthrudata>;
using fquatpassthrudata_ptr_t = std::shared_ptr<fquatpassthrudata>;
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

struct fquatxfdata : public ork::Object {
  DeclareConcreteX(fquatxfdata, ork::Object);

public:
  fquatxfdata();
  fquat transform(const fquat& input) const;
  orklut<std::string, fquatpassthrudata_ptr_t> _transforms;
};

///////////////////////////////////////////////////////////////////////////////

using floatoutplug_t = outplugdata<FloatPlugTraits>;
using floatoutplug_ptr_t = std::shared_ptr<floatoutplug_t>;
using floatinpplug_ptr_t = std::shared_ptr<inplugdata<FloatPlugTraits>>;

using floatxfinplugdata_t = inplugdata<FloatXfPlugTraits>;
using fvec3xfinplugdata_t = inplugdata<Vec3XfPlugTraits>;
using fquatxfinplugdata_t = inplugdata<QuatXfPlugTraits>;

using floatxfinplugdata_ptr_t = std::shared_ptr<floatxfinplugdata_t>;
using fvec3xfinplugdata_ptr_t = std::shared_ptr<fvec3xfinplugdata_t>;
using fquatxfinplugdata_ptr_t = std::shared_ptr<fquatxfinplugdata_t>;

} // namespace ork::dataflow
