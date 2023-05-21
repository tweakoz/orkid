////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/application/application.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/editorsupport/std_annotations.inl>

#include <ork/dataflow/all.h>
#include <ork/dataflow/plug_data.inl>
#include <ork/dataflow/plug_inst.inl>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
bool gbGRAPHLIVE = false;

std::shared_ptr<float> FloatPlugTraits::data_to_inst(std::shared_ptr<float> inp) {
  return inp;
}
std::shared_ptr<float> FloatXfPlugTraits::data_to_inst(std::shared_ptr<float> inp) {
  return inp;
}
std::shared_ptr<fvec3> Vec3fPlugTraits::data_to_inst(std::shared_ptr<fvec3> inp) {
  return inp;
}
std::shared_ptr<fvec3> Vec3XfPlugTraits::data_to_inst(std::shared_ptr<fvec3> inp) {
  return inp;
}
///////////////////////////////////////////////////////////////////////////////
void PlugData::describeX(class_t* clazz) {
  clazz->template annotateTyped<ConstString>("editor.ged.node.factory", "GedNodeFactoryPlug");
  clazz->template annotateTyped<reflect::obj_to_string_fn_t>("editor.ged.item.namer", [](object_ptr_t o) {
    auto as_plug = std::dynamic_pointer_cast<PlugData>(o);
    return as_plug->_name;
  });
}

/*bool inplugbase::IsDirty() const
{
        bool rv = false;
        const outplugdata_ptr_t pcon = GetConnected();
        if( pcon )
        {
                rv |= pcon->IsDirty();
        }
        return rv;
}*/

PlugData::PlugData(moduledata_ptr_t pmod, EPlugDir edir, EPlugRate epr, const std::type_info& tid, const char* pname)
    : _parent_module(pmod)
    , _plugdir(edir)
    , _plugrate(epr)
    , _typeID(tid)
    , _name(pname ? pname : "noname") {
  printf("PlugData<%p> pmod<%p> construct name<%s>\n", (void*)this, (void*)pmod.get(), _name.c_str());
}

const std::type_info& PlugData::GetDataTypeId() const {
  return _typeID;
}

///////////////////////////////////////////////////////////////////////////////
void InPlugData::describeX(class_t* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
bool InPlugData::isConnected() const {
  return (_connectedOutput != nullptr);
}
bool InPlugData::isMorphable() const {
  return (mpMorphable != 0);
}
///////////////////////////////////////////////////////////////////////////////
InPlugData::InPlugData(moduledata_ptr_t pmod, EPlugRate epr, const std::type_info& tid, const char* pname)
    : PlugData(pmod, EPD_INPUT, epr, tid, pname)
    , _connectedOutput(nullptr)
    , mpMorphable(0) {
}
InPlugData::~InPlugData() {
}
inpluginst_ptr_t InPlugData::createInstance() const {
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// compute minimum depth to target module (recursively)
//  this does not differentiate across different plug types
//  all paths to target module via different plug types are considered
///////////////////////////////////////////////////////////////////////////////

size_t InPlugData::computeMinDepth(dgmoduledata_constptr_t to_module) const {
  size_t depth = NOPATH;
  if (to_module == _parent_module) {
    depth = 0;
  } else if (isConnected()) {
    auto upstream_plug   = _connectedOutput;
    auto upstream_module = upstream_plug->_parent_module;
    if (upstream_module == to_module) {
      depth = 1;
    } else {
      size_t min_depth = NOPATH;
      for (auto upstream_input : upstream_module->_inputs) {
        size_t this_path_depth = upstream_input->computeMinDepth(to_module);
        if (this_path_depth != NOPATH) {
          this_path_depth += 1;
          if (this_path_depth < min_depth) {
            min_depth = this_path_depth;
          }
        }
      }
      depth = min_depth;
    }
  }
  if (0)
    printf(
        "InPlugData<%p:%s> this_module<%s> computeMinDepth dest_module<%s>, depth<%zx>\n",
        this,
        this->_name.c_str(),
        this->_parent_module->_name.c_str(),
        to_module->_name.c_str(),
        depth);
  return depth;
}

///////////////////////////////////////////////////////////////////////////////
/*void InPlugData::DoSetDirty(bool bd) {
  if (bd) {
    _parent_module->SetInputDirty(this);
    for (auto& item : _internalOutputConnections) {
      item->SetDirty(bd);
    }
  }
}*/
///////////////////////////////////////////////////////////////////////////////
// void InPlugData::connectInternal(outplugdata_ptr_t vt) {
//_internalOutputConnections.push_back(vt);
//}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void OutPlugData::describeX(class_t* clazz) {
}
OutPlugData::OutPlugData(moduledata_ptr_t pmod, EPlugRate epr, const std::type_info& tid, const char* pname)
    : PlugData(pmod, EPD_OUTPUT, epr, tid, pname) {
}
///////////////////////////////////////////////////////////////////////////////
OutPlugData::~OutPlugData() {
}
///////////////////////////////////////////////////////////////////////////////
void OutPlugData::_disconnect(inplugdata_ptr_t pinplug) {
  auto it = std::find(_connections.begin(), _connections.end(), pinplug);
  if (it != _connections.end()) {
    _connections.erase(it);
  }
}
outpluginst_ptr_t OutPlugData::createInstance() const {
  return nullptr;
}
size_t OutPlugData::maxFanOut() const {
  return 0;
}

bool OutPlugData::isConnected() const {
  return (numConnections() != 0);
}

size_t OutPlugData::numConnections() const {
  return _connections.size();
}
inplugdata_ptr_t OutPlugData::connected(size_t idx) const {
  return _connections[idx];
}
///////////////////////////////////////////////////////////////////////////////
// plugdata<float>
///////////////////////////////////////////////////////////////////////////////
template <> void inplugdata<FloatPlugTraits>::describeX(class_t* clazz) {
}
template <> inpluginst_ptr_t inplugdata<FloatPlugTraits>::createInstance() const {
  return std::make_shared<inpluginst<FloatPlugTraits>>(this);
}

template <> void outplugdata<FloatPlugTraits>::describeX(class_t* clazz) {
}
template <> outpluginst_ptr_t outplugdata<FloatPlugTraits>::createInstance() const {
  return std::make_shared<outpluginst<FloatPlugTraits>>(this);
}

template struct outplugdata<FloatPlugTraits>;
///////////////////////////////////////////////////////////////////////////////
// plugdata<floatxf>
///////////////////////////////////////////////////////////////////////////////
template <> void inplugdata<FloatXfPlugTraits>::describeX(class_t* clazz) {
}
template <> inpluginst_ptr_t inplugdata<FloatXfPlugTraits>::createInstance() const {
  return std::make_shared<inpluginst<FloatXfPlugTraits>>(this);
}
///////////////////////////////////////////////////////////////////////////////
// plugdata<fvec3>
///////////////////////////////////////////////////////////////////////////////
template <> void inplugdata<Vec3fPlugTraits>::describeX(class_t* clazz) {
}
template <> void outplugdata<Vec3fPlugTraits>::describeX(class_t* clazz) {
}
template <> inpluginst_ptr_t inplugdata<Vec3fPlugTraits>::createInstance() const {
  return std::make_shared<inpluginst<Vec3fPlugTraits>>(this);
}
template <> outpluginst_ptr_t outplugdata<Vec3fPlugTraits>::createInstance() const {
  return std::make_shared<outpluginst<Vec3fPlugTraits>>(this);
}
template struct outplugdata<Vec3fPlugTraits>;
///////////////////////////////////////////////////////////////////////////////
// plugdata<fvec3xf>
///////////////////////////////////////////////////////////////////////////////
template <> void inplugdata<Vec3XfPlugTraits>::describeX(class_t* clazz) {
}
template <> inpluginst_ptr_t inplugdata<Vec3XfPlugTraits>::createInstance() const {
  return std::make_shared<inpluginst<Vec3XfPlugTraits>>(this);
}
///////////////////////////////////////////////////////////////////////////////
void floatinplugdata::describeX(class_t* clazz) {
  /*ork::reflect::RegisterProperty(
      "value", //
      &floatinplugdata::GetValAccessor,
      &floatinplugdata::SetValAccessor);
  ork::reflect::annotatePropertyForEditor<floatinplug>("value", "editor.visible", "false");*/
}
///////////////////////////////////////////////////////////////////////////////
void vect3inplugdata::describeX(class_t* clazz) {
  /*  ork::reflect::RegisterProperty(
        "value", //
        &vect3inplugdata::GetValAccessor,
        &vect3inplugdata::SetValAccessor);
    ork::reflect::annotatePropertyForEditor<vect3inplug>("value", "editor.visible", "false");
  */
}
///////////////////////////////////////////////////////////////////////////////
//void floatxfinplugdata::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("xf", &floatinplugxfdata<floatxf>::XfAccessor);
//}
///////////////////////////////////////////////////////////////////////////////
//void fvec3xfinplugdata::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("xf", &vect3inplugxfdata<vect3xf>::XfAccessor);
//}
///////////////////////////////////////////////////////////////////////////////
// template <> void vect3inplugxfdata<floatxfdata>::describeX(class_t* clazz) {
// }
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void modscabiasdata::describeX(object::ObjectClass* clazz) {
  /*clazz->floatProperty("mod", float_range{0.0f, 16.0f}, &modscabias::mfMod);
  clazz->floatProperty("scale", float_range{-16.0f, 16.0f}, &modscabias::mfScale);
  clazz->floatProperty("bias", float_range{-16.0f, 16.0f}, &modscabias::mfBias);
*/
}
void floatxfitembasedata::describeX(class_t* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
void floatxfmsbcurvedata::describeX(object::ObjectClass* clazz) {
  /*
  ork::reflect::RegisterProperty("curve", &floatxfmsbcurve::CurveAccessor);
  ork::reflect::RegisterProperty("ModScaleBias", &floatxfmsbcurve::ModScaleBiasAccessor);

  ork::reflect::RegisterProperty("domodscabia", &floatxfmsbcurve::_domodscalebias);
  ork::reflect::RegisterProperty("docurve", &floatxfmsbcurve::_docurve);

  static const char* EdGrpStr = "sort:// domodscabia docurve ModScaleBias curve";

  reflect::Description::anno_t annoval;
  annoval.set<const char*>(EdGrpStr);
  reflect::annotateClassForEditor<floatxfmsbcurve>("editor.prop.groups", annoval);
*/
}
///////////////////////////////////////////////////////////////////////////////
float floatxfmsbcurvedata::transform(float input) const {
  if (_domodscalebias || _docurve) {
    float fsca    = (_modscalebias._scale * input) + _modscalebias._bias;
    float modout  = (_modscalebias._mod > 0.0f) ? fmodf(fsca, _modscalebias._mod) : fsca;
    float biasout = modout;
    input         = biasout;
  }
  if (_docurve) {
    float clampout = (input < 0.0f) ? 0.0f : (input > 1.0f) ? 1.0f : input;
    input          = _multicurve.Sample(clampout);
  }
  return input;
}
///////////////////////////////////////////////////////////////////////////////
void floatxfmodstepdata::describeX(object::ObjectClass* clazz) {
  /*clazz->floatProperty("Mod", float_range{0.0f, 16.0f}, &floatxfmodstep::_mod);
  clazz->intProperty("Step", int_range{1, 128}, &floatxfmodstep::_steps);
  clazz->floatProperty("OutputBias", float_range{-16.0f, 16.0f}, &floatxfmodstep::_outputBias);
  clazz->floatProperty("OutputScale", float_range{-1600.0f, 1600.0f}, &floatxfmodstep::_outputScale);
*/
}
///////////////////////////////////////////////////////////////////////////////
float floatxfmodstepdata::transform(float input) const {
  int isteps = _steps > 0 ? _steps : 1;

  float fclamped = (input < 0.0f) ? 0.0f : (input > 1.0f) ? 1.0f : input;
  input          = (_mod > 0.0f) ? (fmodf(input, _mod) / _mod) : fclamped;
  float finpsc   = input * float(isteps); // 0..1 -> 0..4
  int iinpsc     = int(std::floor(finpsc));
  float fout     = float(iinpsc) / float(isteps);
  return (fout * _outputScale) + _outputBias;
}
///////////////////////////////////////////////////////////////////////////////
floatxfdata::floatxfdata()
    : _test(0) {
}
///////////////////////////////////////////////////////////////////////////////
floatxfdata::~floatxfdata() {
}
///////////////////////////////////////////////////////////////////////////////
void floatxfdata::describeX(class_t* clazz) {
  clazz->template annotateTyped<ConstString>("editor.ged.node.factory", "GedNodeFactoryPlugFloatXF");
  
  /*
  ork::reflect::RegisterMapProperty("Transforms", &floatxf::_transforms);
  ork::reflect::annotatePropertyForEditor<floatxf>("Transforms", "editor.factorylistbase", "dflow/floatxfitembase");
  ork::reflect::annotatePropertyForEditor<floatxf>("Transforms", "editor.map.policy.impexp", "true");
  */
}
///////////////////////////////////////////////////////////////////////////////
float floatxfdata::transform(float input) const {
  if (!_transforms.Empty()) {
    for (auto xf : _transforms) {
      input = xf.second->transform(input);
    }
  }
  return input;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void fvec3xfdata::describeX(class_t* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
fvec3xfdata::fvec3xfdata() {
  _transformX = std::make_shared<floatxfdata>();
  _transformY = std::make_shared<floatxfdata>();
  _transformZ = std::make_shared<floatxfdata>();
}
///////////////////////////////////////////////////////////////////////////////
fvec3 fvec3xfdata::transform(const fvec3& input) const {
  fvec3 output;
  output.x = (_transformX->transform(input.x));
  output.y = (_transformY->transform(input.y));
  output.z = (_transformZ->transform(input.z));
  return output;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void morphable::HandleMorphEvent(const morph_event* me) {
  switch (me->meType) {
    case EMET_WRITE:
      break;
    case EMET_MORPH:
      Morph1D(me);
      break;
    case EMET_END:
      break;
  }
}

///////////////////////////////////////////////////////////////////////////////

void nullpassthrudata::describeX(class_t* clazz){
  
}

///////////////////////////////////////////////////////////////////////////////

void floatpassthrudata::describeX(class_t* clazz){
  
}

///////////////////////////////////////////////////////////////////////////////

void fvec3passthrudata::describeX(class_t* clazz){

}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////

namespace dflow = ork::dataflow;

ImplementReflectionX(dflow::PlugData, "dflow::plugdata");
ImplementReflectionX(dflow::InPlugData, "dflow::inpplugdata");
ImplementReflectionX(dflow::OutPlugData, "dflow::outplugdata");

ImplementReflectionX(dflow::floatxfdata, "dflow::floatxfdata");
ImplementReflectionX(dflow::fvec3xfdata, "dflow::fvec3xfdata");
ImplementReflectionX(dflow::floatxfitembasedata, "dflow::floatxfitembasedata");

ImplementReflectionX(dflow::floatinplugdata, "dflow::floatinplugdata");
ImplementReflectionX(dflow::vect3inplugdata, "dflow::vect3inplugdata");

ImplementTemplateReflectionX(dflow::outplugdata<dflow::FloatPlugTraits>, "dflow::outplugdata<float>");
ImplementTemplateReflectionX(dflow::inplugdata<dflow::FloatPlugTraits>, "dflow::inplugdata<float>");

ImplementTemplateReflectionX(dflow::outplugdata<dflow::Vec3fPlugTraits>, "dflow::outplugdata<vec3>");
ImplementTemplateReflectionX(dflow::inplugdata<dflow::Vec3fPlugTraits>, "dflow::inplugdata<vec3>");

ImplementTemplateReflectionX(dflow::inplugdata<dflow::FloatXfPlugTraits>, "dflow::inplugdata<floatxf>");
ImplementTemplateReflectionX(dflow::inplugdata<dflow::Vec3XfPlugTraits>, "dflow::inplugdata<vec3xf>");

ImplementReflectionX(dflow::nullpassthrudata, "dflow::nullpassthrudata");
ImplementReflectionX(dflow::floatpassthrudata, "dflow::floatpassthrudata");
ImplementReflectionX(dflow::fvec3passthrudata, "dflow::fvec3passthrudata");
