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
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/AccessorTyped.hpp>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/registerX.inl>

#include <ork/math/cvector2.hpp>
#include <ork/math/cvector3.hpp>
#include <ork/math/cvector4.hpp>
#include <ork/math/quaternion.hpp>
#include <ork/math/cmatrix3.hpp>
#include <ork/math/cmatrix4.hpp>

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
bool gbGRAPHLIVE = false;
///////////////////////////////////////////////////////////////////////////////
void PlugData::describeX(class_t* clazz) {
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
    : _plugdir(edir)
    , _plugrate(epr)
    , _parent_module(pmod)
    , mTypeId(tid)
    , _name(pname ? pname : "noname") {
  //printf("PlugData<%p> pmod<%p> construct name<%s>\n", (void*) this, (void*) pmod.get(), _name.c_str());
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
InPlugData::~InPlugData(){

}
inpluginst_ptr_t InPlugData::createInstance() const{
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

size_t InPlugData::computeMinDepth(dgmoduledata_constptr_t to_module ) const{
  size_t depth = NOPATH;
  if(to_module==_parent_module){
    depth = 0;
  }
  else if(isConnected()){
    auto upstream_plug = _connectedOutput;
    auto upstream_module = upstream_plug->_parent_module;
    if(upstream_module==to_module){
      depth = 1;
    }
    else{
      size_t min_depth = NOPATH;
      for( auto upstream_input : upstream_module->_inputs ){
        size_t this_path_depth = upstream_input->computeMinDepth(to_module);
        if(this_path_depth!=NOPATH){
          this_path_depth += 1;
          if(this_path_depth<min_depth){
            min_depth = this_path_depth;
          }
        }
      }
      depth = min_depth;
    }
  }
  printf( "InPlugData<%p:%s> this_module<%s> computeMinDepth dest_module<%s>, depth<%zx>\n",
          this, 
          this->_name.c_str(),
          this->_parent_module->_name.c_str(),
          to_module->_name.c_str(),
          depth );
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
//void InPlugData::connectInternal(outplugdata_ptr_t vt) {
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
void OutPlugData::_disconnect(inplugdata_ptr_t pinplug){
  auto it = std::find(_connections.begin(),_connections.end(),pinplug);
  if(it!=_connections.end()){
    _connections.erase(it);
  }
}
outpluginst_ptr_t OutPlugData::createInstance() const{
  return nullptr;
}
///////////////////////////////////////////////////////////////////////////////
// plugdata<float>
///////////////////////////////////////////////////////////////////////////////
template <> int MaxFanout<float>() {
  return 0;
}
template <> void inplugdata<float>::describeX(class_t* clazz) {
}
template <> inpluginst_ptr_t inplugdata<float>::createInstance() const{
  return std::make_shared<inpluginst<float>>(this);
}
template <> void outplugdata<float>::describeX(class_t* clazz) {
}
template <> outpluginst_ptr_t outplugdata<float>::createInstance() const{
  return std::make_shared<outpluginst<float>>(this);
}

template class outplugdata<float>;
///////////////////////////////////////////////////////////////////////////////
// plugdata<fvec3>
///////////////////////////////////////////////////////////////////////////////
template <> int MaxFanout<fvec3>() {
  return 0;
}
template <> void inplugdata<fvec3>::describeX(class_t* clazz) {
}
template <> void outplugdata<fvec3>::describeX(class_t* clazz) {
}
template <> inpluginst_ptr_t inplugdata<fvec3>::createInstance() const{
  return std::make_shared<inpluginst<fvec3>>(this);
}
template <> outpluginst_ptr_t outplugdata<fvec3>::createInstance() const{
  return std::make_shared<outpluginst<fvec3>>(this);
}
template class outplugdata<fvec3>;
///////////////////////////////////////////////////////////////////////////////
void floatinplugdata::describeX(class_t* clazz) {
  /*ork::reflect::RegisterProperty(
      "value", //
      &floatinplugdata::GetValAccessor,
      &floatinplugdata::SetValAccessor);
  ork::reflect::annotatePropertyForEditor<floatinplug>("value", "editor.visible", "false");*/
}
///////////////////////////////////////////////////////////////////////////////
template <> void floatinplugxfdata<floatxf>::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("xf", &floatinplugxfdata<floatxf>::XfAccessor);
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
template <> void vect3inplugxfdata<vect3xf>::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("xf", &vect3inplugxfdata<vect3xf>::XfAccessor);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void modscabias::describeX(object::ObjectClass* clazz) {
  /*clazz->floatProperty("mod", float_range{0.0f, 16.0f}, &modscabias::mfMod);
  clazz->floatProperty("scale", float_range{-16.0f, 16.0f}, &modscabias::mfScale);
  clazz->floatProperty("bias", float_range{-16.0f, 16.0f}, &modscabias::mfBias);
*/
}
void floatxfitembase::describeX(class_t* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
void floatxfmsbcurve::describeX(object::ObjectClass* clazz) {
  /*
  ork::reflect::RegisterProperty("curve", &floatxfmsbcurve::CurveAccessor);
  ork::reflect::RegisterProperty("ModScaleBias", &floatxfmsbcurve::ModScaleBiasAccessor);

  ork::reflect::RegisterProperty("domodscabia", &floatxfmsbcurve::mbDoModScaBia);
  ork::reflect::RegisterProperty("docurve", &floatxfmsbcurve::mbDoCurve);

  static const char* EdGrpStr = "sort:// domodscabia docurve ModScaleBias curve";

  reflect::Description::anno_t annoval;
  annoval.set<const char*>(EdGrpStr);
  reflect::annotateClassForEditor<floatxfmsbcurve>("editor.prop.groups", annoval);
*/
}
///////////////////////////////////////////////////////////////////////////////
float floatxfmsbcurve::transform(float input) const {
  if (mbDoModScaBia || mbDoCurve) {
    float fsca    = (scale() * input) + bias();
    float modout  = (mod() > 0.0f) ? fmodf(fsca, mod()) : fsca;
    float biasout = modout;
    input         = biasout;
  }
  if (mbDoCurve) {
    float clampout = (input < 0.0f) ? 0.0f : (input > 1.0f) ? 1.0f : input;
    input          = mMultiCurve1d.Sample(clampout);
  }
  return input;
}
///////////////////////////////////////////////////////////////////////////////
void floatxfmodstep::describeX(object::ObjectClass* clazz) {
  /*clazz->floatProperty("Mod", float_range{0.0f, 16.0f}, &floatxfmodstep::mMod);
  clazz->intProperty("Step", int_range{1, 128}, &floatxfmodstep::miSteps);
  clazz->floatProperty("OutputBias", float_range{-16.0f, 16.0f}, &floatxfmodstep::mOutputBias);
  clazz->floatProperty("OutputScale", float_range{-1600.0f, 1600.0f}, &floatxfmodstep::mOutputScale);
*/
}
///////////////////////////////////////////////////////////////////////////////
float floatxfmodstep::transform(float input) const {
  int isteps = miSteps > 0 ? miSteps : 1;

  float fclamped = (input < 0.0f) ? 0.0f : (input > 1.0f) ? 1.0f : input;
  input          = (mMod > 0.0f) ? (fmodf(input, mMod) / mMod) : fclamped;
  float finpsc   = input * float(isteps); // 0..1 -> 0..4
  int iinpsc     = int(std::floor(finpsc));
  float fout     = float(iinpsc) / float(isteps);
  return (fout * mOutputScale) + mOutputBias;
}
///////////////////////////////////////////////////////////////////////////////
floatxf::floatxf()
    : miTest(0) {
}
///////////////////////////////////////////////////////////////////////////////
floatxf::~floatxf() {
}
///////////////////////////////////////////////////////////////////////////////
void floatxf::describeX(class_t* clazz) {
  /*
  ork::reflect::RegisterMapProperty("Transforms", &floatxf::mTransforms);
  ork::reflect::annotatePropertyForEditor<floatxf>("Transforms", "editor.factorylistbase", "dflow/floatxfitembase");
  ork::reflect::annotatePropertyForEditor<floatxf>("Transforms", "editor.map.policy.impexp", "true");
  */
}
///////////////////////////////////////////////////////////////////////////////
float floatxf::transform(float input) const {
  if (!mTransforms.Empty()) {
    for (auto itxf : mTransforms) {
      floatxfitembase* pitem = rtti::autocast(itxf.second);
      if (pitem) {
        input = pitem->transform(input);
      }
    }
  }
  return input;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void vect3xf::describeX(class_t* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
fvec3 vect3xf::transform(const fvec3& input) const {
  fvec3 output;
  output.x = (mTransformX.transform(input.x));
  output.y = (mTransformX.transform(input.y));
  output.z = (mTransformX.transform(input.z));
  return output;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
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
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::dataflow::PlugData, "dflow/plugdata");
ImplementReflectionX(ork::dataflow::InPlugData, "dflow/inpplugdata");
ImplementReflectionX(ork::dataflow::OutPlugData, "dflow/outplugdata");

ImplementReflectionX(ork::dataflow::floatxf, "dflow/floatxf");
ImplementReflectionX(ork::dataflow::vect3xf, "dflow/vect3xf");
ImplementReflectionX(ork::dataflow::floatxfitembase, "dflow/floatxfitembase");

ImplementReflectionX(ork::dataflow::floatinplugdata, "dflow/floatinplugdata");
ImplementReflectionX(ork::dataflow::vect3inplugdata, "dflow/vect3inplugdata");

ImplementTemplateReflectionX(ork::dataflow::outplugdata<float>, "dflow/outplugdata<float>");
ImplementTemplateReflectionX(ork::dataflow::inplugdata<float>, "dflow/inplugdata<float>");
ImplementTemplateReflectionX(ork::dataflow::outplugdata<ork::fvec3>, "dflow/outplugdata<vec3>");
ImplementTemplateReflectionX(ork::dataflow::inplugdata<ork::fvec3>, "dflow/inplugdata<vec3>");
ImplementTemplateReflectionX(ork::dataflow::floatxfinplug, "dflow/floatxfinplug");
ImplementTemplateReflectionX(ork::dataflow::vect3xfinplug, "dflow/vect3xfinplug");
