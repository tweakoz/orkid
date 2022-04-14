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
template class ork::orklut<ork::PoolString, ork::dataflow::module*>;

using OrkDataflowOutPlugFloat    = ork::dataflow::outplug<float>;
using OrkDataflowInpPlugFloat    = ork::dataflow::inplug<float>;
using OrkDataflowOutPlugFloat3   = ork::dataflow::outplug<ork::fvec3>;
using OrkDataflowInpPlugFloat3   = ork::dataflow::inplug<ork::fvec3>;
using OrkDataflowFloatInpPlugXf  = ork::dataflow::floatinplugxf<ork::dataflow::floatxf>;
using OrkDataflowFloat3InpPlugXf = ork::dataflow::vect3inplugxf<ork::dataflow::vect3xf>;
///////////////////////////////////////////////////////////////////////////////
ImplementReflectionX(ork::dataflow::module, "dflow/module");
ImplementReflectionX(ork::dataflow::dgmodule, "dflow/dgmodule");
ImplementReflectionX(ork::dataflow::plugroot, "dflow/plugroot");
ImplementReflectionX(ork::dataflow::inplugbase, "dflow/inplugbase");
ImplementReflectionX(ork::dataflow::outplugbase, "dflow/outplugbase");
ImplementReflectionX(ork::dataflow::floatinplug, "dflow/floatinplug");
ImplementReflectionX(ork::dataflow::vect3inplug, "dflow/vect3inplug");

ImplementReflectionX(ork::dataflow::modscabias, "dflow/ModScaleBias");
ImplementReflectionX(ork::dataflow::floatxfitembase, "dflow/floatxfitembase");
ImplementReflectionX(ork::dataflow::floatxfmsbcurve, "dflow/floatxfmsbcurve");
ImplementReflectionX(ork::dataflow::floatxfmodstep, "dflow/floatxfmodstep");
ImplementReflectionX(ork::dataflow::floatxf, "dflow/floatxf");
ImplementReflectionX(ork::dataflow::vect3xf, "dflow/vect3xf");

///////////////////////////////////////////////////////////////////////////////
namespace ork::dataflow {
///////////////////////////////////////////////////////////////////////////////

bool gbGRAPHLIVE = false;
void plugroot::describeX(class_t* clazz) {
}
void inplugbase::describeX(class_t* clazz) {
}
void outplugbase::describeX(class_t* clazz) {
}
///////////////////////////////////////////////////////////////////////////////
template <> int MaxFanout<float>() {
  return 0;
}
template <> void inplug<float>::describeX(class_t* clazz) {
}
template <> void outplug<float>::describeX(class_t* clazz) {
}
template class outplug<float>;
///////////////////////////////////////////////////////////////////////////////
template <> const float& outplug<float>::GetInternalData() const {
  static const float kdefault = 0.0f;
  if (0 == mOutputData)
    return kdefault;
  return *mOutputData;
}
///////////////////////////////////////////////////////////////////////////////
template <> const float& outplug<float>::GetValue() const {
  return GetInternalData();
}

///////////////////////////////////////////////////////////////////////////////
template <> int MaxFanout<fvec3>() {
  return 0;
}
template <> void inplug<fvec3>::describeX(class_t* clazz) {
}
template <> void outplug<fvec3>::describeX(class_t* clazz) {
}
template class outplug<fvec3>;
///////////////////////////////////////////////////////////////////////////////
template <> const fvec3& outplug<fvec3>::GetInternalData() const {
  static const fvec3 kdefault;
  if (0 == mOutputData) {
    return kdefault;
  }
  return *mOutputData;
}
///////////////////////////////////////////////////////////////////////////////
template <> const fvec3& outplug<fvec3>::GetValue() const {
  return GetInternalData();
}
///////////////////////////////////////////////////////////////////////////////
void floatinplug::describeX(class_t* clazz) {
  ork::reflect::RegisterProperty(
      "value", //
      &floatinplug::GetValAccessor,
      &floatinplug::SetValAccessor);
  ork::reflect::annotatePropertyForEditor<floatinplug>("value", "editor.visible", "false");
}
///////////////////////////////////////////////////////////////////////////////
template <> void floatinplugxf<floatxf>::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("xf", &floatinplugxf<floatxf>::XfAccessor);
}
///////////////////////////////////////////////////////////////////////////////
void vect3inplug::describeX(class_t* clazz) {
  ork::reflect::RegisterProperty(
      "value", //
      &vect3inplug::GetValAccessor,
      &vect3inplug::SetValAccessor);
  ork::reflect::annotatePropertyForEditor<vect3inplug>("value", "editor.visible", "false");
}
///////////////////////////////////////////////////////////////////////////////
template <> void vect3inplugxf<vect3xf>::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("xf", &vect3inplugxf<vect3xf>::XfAccessor);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void modscabias::describeX(object::ObjectClass* clazz) {
  clazz->floatProperty("mod", float_range{0.0f, 16.0f}, &modscabias::mfMod);
  clazz->floatProperty("scale", float_range{-16.0f, 16.0f}, &modscabias::mfScale);
  clazz->floatProperty("bias", float_range{-16.0f, 16.0f}, &modscabias::mfBias);
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
    float fsca    = (GetScale() * input) + GetBias();
    float modout  = (GetMod() > 0.0f) ? fmodf(fsca, GetMod()) : fsca;
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
  clazz->floatProperty("Mod", float_range{0.0f, 16.0f}, &floatxfmodstep::mMod);
  clazz->intProperty("Step", int_range{1, 128}, &floatxfmodstep::miSteps);
  clazz->floatProperty("OutputBias", float_range{-16.0f, 16.0f}, &floatxfmodstep::mOutputBias);
  clazz->floatProperty("OutputScale", float_range{-1600.0f, 1600.0f}, &floatxfmodstep::mOutputScale);
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

/*bool inplugbase::IsDirty() const
{
        bool rv = false;
        const outplugbase* pcon = GetConnected();
        if( pcon )
        {
                rv |= pcon->IsDirty();
        }
        return rv;
}*/

plugroot::plugroot(module* pmod, EPlugDir edir, EPlugRate epr, const std::type_info& tid, const char* pname)
    : mePlugDir(edir)
    , mePlugRate(epr)
    , mModule(pmod)
    , mTypeId(tid)
    , mPlugName(pname ? ork::AddPooledLiteral(pname) : ork::AddPooledLiteral("noname")) {
  printf("plugroot<%p> pmod<%p> construct name<%s>\n", (void*) this, (void*) pmod, mPlugName.c_str());
}
void plugroot::SetDirty(bool bv) {
  mbDirty = bv;
  DoSetDirty(bv);
  // mModule
}
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
inplugbase::inplugbase(module* pmod, EPlugRate epr, const std::type_info& tid, const char* pname)
    : plugroot(pmod, EPD_INPUT, epr, tid, pname)
    , mExternalOutput(0)
    , mpMorphable(0) {
  if (GetModule())
    GetModule()->AddInput(this);
}
///////////////////////////////////////////////////////////////////////////////
inplugbase::~inplugbase() {
  if (GetModule())
    GetModule()->RemoveInput(this);
  Disconnect();
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::DoSetDirty(bool bd) {
  if (bd) {
    GetModule()->SetInputDirty(this);
    for (auto& item : mInternalOutputConnections) {
      item->SetDirty(bd);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::ConnectInternal(outplugbase* vt) {
  mInternalOutputConnections.push_back(vt);
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::ConnectExternal(outplugbase* vt) {
  mExternalOutput = vt;
  vt->mExternalInputConnections.push_back(this);
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::SafeConnect(graph_data& gr,
                             outplugbase* vt) { // OrkAssert( GetDataTypeId() == vt->GetDataTypeId() );
  bool cc = gr.CanConnect(this, vt);
  OrkAssert(cc);
  ConnectExternal(vt);
}
///////////////////////////////////////////////////////////////////////////////
void inplugbase::Disconnect() {
  if (mExternalOutput) {
    mExternalOutput->Disconnect(this);
  }
  mExternalOutput = 0;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
outplugbase::outplugbase(module* pmod, EPlugRate epr, const std::type_info& tid, const char* pname)
    : plugroot(pmod, EPD_OUTPUT, epr, tid, pname)
    , mpRegister(0) {
  if (GetModule())
    GetModule()->AddOutput(this);
}
///////////////////////////////////////////////////////////////////////////////
outplugbase::~outplugbase() {
  if (GetModule())
    GetModule()->RemoveOutput(this);

  while (GetNumExternalOutputConnections()) {
    inplugbase* pcon = GetExternalOutputConnection(GetNumExternalOutputConnections() - 1);
    pcon->Disconnect();
  }
}
///////////////////////////////////////////////////////////////////////////////
void outplugbase::DoSetDirty(bool bd) {
  if (bd) {
    GetModule()->SetOutputDirty(this);
    for (auto& item : mExternalInputConnections) {
      item->SetDirty(bd);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void outplugbase::Disconnect(inplugbase* pinplug) {
  auto it = std::find(mExternalInputConnections.begin(), mExternalInputConnections.end(), pinplug);
  if (it != mExternalInputConnections.end()) {
    mExternalInputConnections.erase(it);
  }
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void module::describeX(class_t* clazz) {
}
module::module()
    : mpMorphable(nullptr)
    , mNumStaticInputs(0)
    , mNumStaticOutputs(0) {
}
module::~module() {
}

///////////////////////////////////////////////////////////////////////////////
void module::SetInputDirty(inplugbase* plg) {
  DoSetInputDirty(plg);
}
///////////////////////////////////////////////////////////////////////////////
void module::SetOutputDirty(outplugbase* plg) {
  DoSetOutputDirty(plg);
}
///////////////////////////////////////////////////////////////////////////////
void module::AddDependency(outplugbase& pout, inplugbase& pin) {
  pin.ConnectInternal(&pout);
  // DepPlugSet::value_type v( & pin, & pout );
  // mDependencies.insert( v );
}
void module::AddInput(inplugbase* plg) {
  auto it = mStaticInputs.find(plg);
  if (it == mStaticInputs.end()) {
    mStaticInputs.insert(plg);
    mNumStaticInputs++;
  }
}
void module::AddOutput(outplugbase* plg) {
  auto it = mStaticOutputs.find(plg);
  if (it == mStaticOutputs.end()) {
    mStaticOutputs.insert(plg);
    mNumStaticOutputs++;
  }
}
void module::RemoveInput(inplugbase* plg) {
  auto it = mStaticInputs.find(plg);
  if (it != mStaticInputs.end()) {
    mStaticInputs.erase(it);
    mNumStaticInputs--;
  }
}
void module::RemoveOutput(outplugbase* plg) {
  auto it = mStaticOutputs.find(plg);
  if (it != mStaticOutputs.end()) {
    mStaticOutputs.erase(it);
    mNumStaticOutputs--;
  }
}
inplugbase* module::GetStaticInput(int idx) const {
  int size = mStaticInputs.size();
  auto it  = mStaticInputs.begin();
  for (int i = 0; i < idx; i++) {
    it++;
  }
  inplugbase* rval = (it != mStaticInputs.end()) ? *it : nullptr;
  return rval;
}
outplugbase* module::GetStaticOutput(int idx) const {
  int size = mStaticOutputs.size();
  auto it  = mStaticOutputs.begin();
  for (int i = 0; i < idx; i++) {
    it++;
  }
  outplugbase* rval = (it != mStaticOutputs.end()) ? *it : nullptr;
  return rval;
}
///////////////////////////////////////////////////////////////////////////////
bool module::IsDirty() const {
  bool rval   = false;
  int inumout = this->GetNumOutputs();
  for (int i = 0; i < inumout; i++) {
    outplugbase* poutput = GetOutput(i);
    rval |= poutput->IsDirty();
  }
  if (false == rval) {
    int inumchi = GetNumChildren();
    for (int ic = 0; ic < inumchi; ic++) {
      module* pchild = GetChild(ic);
      rval |= pchild->IsDirty();
    }
  }
  return rval;
}
inplugbase* GetInput(int idx) {
  return 0;
}
outplugbase* GetOutput(int idx) {
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
inplugbase* module::GetInputNamed(const PoolString& named) {
  int inuminp = GetNumInputs();
  for (int ip = 0; ip < inuminp; ip++) {
    inplugbase* rval = GetInput(ip);
    OrkAssert(rval != nullptr);
    if (named == rval->GetName()) {
      return rval;
    }
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
outplugbase* module::GetOutputNamed(const PoolString& named) {
  int inumout = GetNumOutputs();
  printf("module<%p> numouts<%d>\n", (void*) this, inumout);
  for (int ip = 0; ip < inumout; ip++) {
    outplugbase* rval = GetOutput(ip);
    OrkAssert(rval != nullptr);
    if (named == rval->GetName()) {
      return rval;
    }
  }
  return 0;
}
///////////////////////////////////////////////////////////////////////////////
module* module::GetChildNamed(const ork::PoolString& named) const {
  int inumchi = GetNumChildren();
  for (int ic = 0; ic < inumchi; ic++) {
    module* rval = GetChild(ic);
    if (named == rval->GetName()) {
      return rval;
    }
  }
  return 0;
}
/*bool module::IsOutputDirty(const ork::dataflow::outplugbase *pplug) const
{
        bool bv = false;
        for( DepPlugSet::const_iterator it=mDependencies.begin();
it!=mDependencies.end(); it++ )
        {
                const DepPlugSet::value_type& v = *it;

                inplugbase* pin = v.first;
                const outplugbase* pout = v.second;

                if( pout == pplug )
                {
                        bv |= pin->IsDirty();
                }
        }
        return bv;
}*/
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void dgmodule::describeX(class_t* clazz) {
  // ork::reflect::RegisterProperty("mgvpos", &dgmodule::mgvpos);
  // ork::reflect::annotatePropertyForEditor<dgmodule>("mgvpos", "editor.visible", "false");
}
///////////////////////////////////////////////////////////////////////////////
dgmodule::dgmodule()
    : mAffinity(dataflow::scheduler::CpuAffinity)
    , _parent(0)
    , mKey() {
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::DivideWork(const scheduler& sch, cluster* clus) {
  clus->AddModule(this);
  DoDivideWork(sch, clus);
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::DoDivideWork(const scheduler& sch, cluster* clus) {
  workunit* wu = new workunit(this, clus, 0);
  wu->SetAffinity(GetAffinity());
  clus->AddWorkUnit(wu);
}
///////////////////////////////////////////////////////////////////////////////
void dgmodule::ReleaseWorkUnit(workunit* wu) {
  OrkAssert(wu->GetModule() == this);
  delete wu;
}
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::dataflow
///////////////////////////////////////////////////////////////////////////////
ImplementTemplateReflectionX(OrkDataflowOutPlugFloat, "dflow/outplug<float>");
ImplementTemplateReflectionX(OrkDataflowInpPlugFloat, "dflow/inplug<float>");
ImplementTemplateReflectionX(OrkDataflowOutPlugFloat3, "dflow/outplug<vect3>");
ImplementTemplateReflectionX(OrkDataflowInpPlugFloat3, "dflow/inplug<vect3>");
ImplementTemplateReflectionX(OrkDataflowFloatInpPlugXf, "dflow/inplugxf<float,floatxf>");
ImplementTemplateReflectionX(OrkDataflowFloat3InpPlugXf, "dflow/inplugxf<vect3,vect3xf>");
