////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <utpp/UnitTest++.h>
#include <cmath>
#include <limits>

#include <ork/kernel/timer.h>
#include <ork/dataflow/dataflow.h>
#include <ork/application/application.h>
#include <ork/reflect/properties/register.h>

////////////////////////////////////////////////////////////

namespace ork::dataflow {
namespace test {

////////////////////////////////////////////////////////////
// create a bunch of mock procedural-image payload and plug types
// They are not actually functional, but serve to test the 
//   the dataflow infrastructure
////////////////////////////////////////////////////////////

struct ImgBase {};
struct Img32 : public ImgBase {};
struct Img64 : public ImgBase {};
struct Buffer {};

struct ImgBasePlugTraits{
  using data_type_t = ImgBase;
  using inst_type_t = ImgBase;
  static constexpr size_t max_fanout = 0;
};
struct Img32PlugTraits{
  using data_type_t = Img32;
  using inst_type_t = Img32;
  static constexpr size_t max_fanout = 0;
};
struct Img64PlugTraits{
  using data_type_t = Img64;
  using inst_type_t = Img64;
  static constexpr size_t max_fanout = 0;
};

////////////////////////////////////////////////////////////

using img32_ptr_t         = std::shared_ptr<Img32>;
using img32_outplug_t     = outplugdata<Img32PlugTraits>;
using img32_outplug_ptr_t = std::shared_ptr<img32_outplug_t>;
using img32_inplug_t      = inplugdata<Img32PlugTraits>;
using img32_inplug_ptr_t  = std::shared_ptr<img32_inplug_t>;

////////////////////////////////////////////////////////////

using img64_ptr_t         = std::shared_ptr<Img64>;
using img64_outplug_t     = outplugdata<Img64PlugTraits>;
using img64_outplug_ptr_t = std::shared_ptr<img64_outplug_t>;
using img64_inplug_t      = inplugdata<Img64PlugTraits>;
using img64_inplug_ptr_t  = std::shared_ptr<img64_inplug_t>;

struct ImageGenTestImpl {};

typedef ork::dataflow::outplugdata<ImgBase> ImgOutPlug;
typedef ork::dataflow::inplugdata<ImgBase> ImgInPlug;

struct BaseModuleData : public DgModuleData {
  DeclareAbstractX(BaseModuleData, DgModuleData);

public:
  BaseModuleData() {
  }
};

void BaseModuleData::describeX(class_t* clazz) {
}

struct BaseModuleInst : public DgModuleInst {

  BaseModuleInst(const BaseModuleData* data)
      : DgModuleInst(data) {
  }
};

////////////////////////////////////////////////////////////

using float_ptr_t = std::shared_ptr<float>;

struct GlobalModuleData : public BaseModuleData {
  DeclareConcreteX(GlobalModuleData, BaseModuleData);

public: //
  GlobalModuleData() {
    _outputA = std::make_shared<float>(0.0f);
    _outputB = std::make_shared<float>(0.5f);
    _outputC = std::make_shared<float>(1.0f);
  }

  static std::shared_ptr<GlobalModuleData> createShared() {
    auto gmd = std::make_shared<GlobalModuleData>();
    createOutputPlug<FloatPlugTraits>(gmd, EPR_UNIFORM, gmd->_outputA, "OutputA");
    createOutputPlug<FloatPlugTraits>(gmd, EPR_UNIFORM, gmd->_outputB, "OutputB");
    createOutputPlug<FloatPlugTraits>(gmd, EPR_UNIFORM, gmd->_outputC, "OutputC");
    return gmd;
  }

  dgmoduleinst_ptr_t createInstance() const final;

  float_ptr_t _outputA;
  float_ptr_t _outputB;
  float_ptr_t _outputC;
};

void GlobalModuleData::describeX(class_t* clazz) {
}

struct GlobalModuleInst : public BaseModuleInst {
  GlobalModuleInst(const GlobalModuleData* data)
      : BaseModuleInst(data) {
  }
  void onLink(GraphInst* inst) final {
    int numinp = numInputs();
    int numout = numOutputs();

    auto impl = inst->_impl.getShared<ImageGenTestImpl>();

    printf("LINK GlobalModuleInst<%p:%s> numinp<%d> numout<%d>\n", (void*)this, _dgmodule_data->_name.c_str(), numinp, numout);

    _output_A = outputNamed("OutputA");
    _output_B = outputNamed("OutputB");
    _output_C = outputNamed("OutputC");
  }
  void compute(GraphInst* inst) final {

    printf("COMPUTE GlobalModuleInst<%p:%s>\n", (void*)this, _dgmodule_data->_name.c_str());
  }
  outpluginst_ptr_t _output_A;
  outpluginst_ptr_t _output_B;
  outpluginst_ptr_t _output_C;
};

dgmoduleinst_ptr_t GlobalModuleData::createInstance() const {
  return std::make_shared<GlobalModuleInst>(this);
}

////////////////////////////////////////////////////////////

struct ImgModuleData : public BaseModuleData {

  DeclareConcreteX(ImgModuleData, BaseModuleData);

public: //
  static Img32 g_no_connection;

  /*Buffer& GetWriteBuffer( graphinst_ptr_t ptex )
  {	ImgOutPlug* outplug = 0;
      GetTypedOutput<ImgBase>(0,outplug);
      const ImgBase& base = outplug->GetValue();
      //printf( "MOD<%p> WBI<%d>\n", (void*) this, base.miBufferIndex );
      return gBuffer;
      //return ptex.GetBuffer(outplug->GetValue().miBufferIndex);
  }*/
};

Img32 ImgModuleData::g_no_connection;

void ImgModuleData::describeX(class_t* clazz) {
}

struct ImgModuleInst : public BaseModuleInst {
  ImgModuleInst(const ImgModuleData* data)
      : BaseModuleInst(data) {
  }
};

////////////////////////////////////////////////////////////

struct Img32ModuleData : public ImgModuleData {
  DeclareConcreteX(Img32ModuleData, ImgModuleData);

public: //
  Img32ModuleData()
      : ImgModuleData() {

    _image_out = std::shared_ptr<Img32>();
  }

  img32_ptr_t _image_out;

protected:
  static void sharedConstructor(moduledata_ptr_t subclass_instance) {
    auto as_im32mod = std::dynamic_pointer_cast<Img32ModuleData>(subclass_instance);
    createOutputPlug<Img32PlugTraits>(subclass_instance, EPR_UNIFORM, as_im32mod->_image_out, "Output");
  }
};

void Img32ModuleData::describeX(class_t* clazz) {
}

struct Img32ModuleInst : public ImgModuleInst {
  Img32ModuleInst(const Img32ModuleData* data)
      : ImgModuleInst(data) {
  }
};

////////////////////////////////////////////////////////////

struct Img64ModuleData : public ImgModuleData {
  DeclareConcreteX(Img64ModuleData, ImgModuleData);

public: //
  Img64ModuleData()
      : ImgModuleData() {

    _image_out = std::shared_ptr<Img64>();
  }

  img64_ptr_t _image_out;

protected:
  static void sharedConstructor(moduledata_ptr_t subclass_instance) {
    auto as_im64mod = std::dynamic_pointer_cast<Img64ModuleData>(subclass_instance);
    createOutputPlug<Img64PlugTraits>(subclass_instance, EPR_UNIFORM, as_im64mod->_image_out, "Output");
  }
};

void Img64ModuleData::describeX(class_t* clazz) {
}

struct Img64ModuleInst : public ImgModuleInst {
  Img64ModuleInst(const Img64ModuleData* data)
      : ImgModuleInst(data) {
  }
};

////////////////////////////////////////////////////////////

struct GradientModuleData : public Img32ModuleData {
  DeclareConcreteX(GradientModuleData, Img32ModuleData);

public: //
  GradientModuleData()
      : Img32ModuleData() {
  }

  static std::shared_ptr<GradientModuleData> createShared() {
    auto gmd = std::make_shared<GradientModuleData>();
    Img32ModuleData::sharedConstructor(gmd);
    createInputPlug<Img32PlugTraits>(gmd, EPR_UNIFORM, "InputA");
    createInputPlug<Img32PlugTraits>(gmd, EPR_UNIFORM, "InputB");
    return gmd;
  }

  dgmoduleinst_ptr_t createInstance() const final;

};

void GradientModuleData::describeX(class_t* clazz) {
}

struct GradientModuleInst : public Img32ModuleInst {
  GradientModuleInst(const GradientModuleData* data)
      : Img32ModuleInst(data) {
  }
  void onLink(GraphInst* inst) final {
    auto impl  = inst->_impl.getShared<ImageGenTestImpl>();
    int numinp = numInputs();
    int numout = numOutputs();
    printf("LINK GradientModuleInst<%p:%s> numinp<%d> numout<%d>\n", (void*)this, _dgmodule_data->_name.c_str(), numinp, numout);
    _input_imageA = inputNamed("InputA");
    _input_imageB = inputNamed("InputB");
  }
  void compute(GraphInst* inst) final {
    printf("COMPUTE GradientModuleInst<%p:%s>\n", (void*)this, _dgmodule_data->_name.c_str());
  }
  inpluginst_ptr_t _input_imageA;
  inpluginst_ptr_t _input_imageB;
};

dgmoduleinst_ptr_t GradientModuleData::createInstance() const {
  return std::make_shared<GradientModuleInst>(this);
}

////////////////////////////////////////////////////////////

struct Op1ModuleData : public Img32ModuleData {
  DeclareConcreteX(Op1ModuleData, Img32ModuleData);

public: //
  Op1ModuleData()
      : Img32ModuleData() {
  }

  static std::shared_ptr<Op1ModuleData> createShared() {
    auto gmd = std::make_shared<Op1ModuleData>();
    Img32ModuleData::sharedConstructor(gmd);
    createInputPlug<Img32PlugTraits>(gmd, EPR_UNIFORM, "Input");
    createInputPlug<FloatPlugTraits>(gmd, EPR_UNIFORM, "ParamA");
    createInputPlug<FloatPlugTraits>(gmd, EPR_UNIFORM, "ParamB");
    return gmd;
  }

  dgmoduleinst_ptr_t createInstance() const final;

};

void Op1ModuleData::describeX(class_t* clazz) {
}

struct Op1ModuleInst : public Img32ModuleInst {
  Op1ModuleInst(const Op1ModuleData* data)
      : Img32ModuleInst(data) {
  }
  void onLink(GraphInst* inst) final {
    auto impl  = inst->_impl.getShared<ImageGenTestImpl>();
    int numinp = numInputs();
    int numout = numOutputs();
    _input_image = inputNamed("Input");
    _input_paramA = inputNamed("ParamA");
    _input_paramB = inputNamed("ParamB");
    printf("LINK Op1ModuleInst<%p:%s> numinp<%d> numout<%d>\n", (void*)this, _dgmodule_data->_name.c_str(), numinp, numout);
  }
  void compute(GraphInst* inst) final {
    printf("COMPUTE Op1ModuleInst<%p:%s>\n", (void*)this, _dgmodule_data->_name.c_str());
  }
  inpluginst_ptr_t _input_image;
  inpluginst_ptr_t _input_paramA;
  inpluginst_ptr_t _input_paramB;
};

dgmoduleinst_ptr_t Op1ModuleData::createInstance() const {
  return std::make_shared<Op1ModuleInst>(this);
}

////////////////////////////////////////////////////////////

struct Op2ModuleData : public Img32ModuleData {
  DeclareConcreteX(Op2ModuleData, Img32ModuleData);

public: //
  Op2ModuleData()
      : Img32ModuleData() {
  }

  static std::shared_ptr<Op2ModuleData> createShared() {
    auto gmd = std::make_shared<Op2ModuleData>();
    Img32ModuleData::sharedConstructor(gmd);
    createInputPlug<Img32PlugTraits>(gmd, EPR_UNIFORM, "InputA");
    createInputPlug<Img32PlugTraits>(gmd, EPR_UNIFORM, "InputB");
    createInputPlug<FloatPlugTraits>(gmd, EPR_UNIFORM, "ParamA");
    createInputPlug<FloatPlugTraits>(gmd, EPR_UNIFORM, "ParamB");
    return gmd;
  }

  dgmoduleinst_ptr_t createInstance() const final;

};

void Op2ModuleData::describeX(class_t* clazz) {
}

struct Op2ModuleInst : public Img32ModuleInst {
  Op2ModuleInst(const Op2ModuleData* data)
      : Img32ModuleInst(data) {
  }
  void onLink(GraphInst* inst) final {
    auto impl  = inst->_impl.getShared<ImageGenTestImpl>();
    int numinp = numInputs();
    int numout = numOutputs();
    printf("LINK Op2ModuleInst<%p:%s> numinp<%d> numout<%d>\n", (void*)this, _dgmodule_data->_name.c_str(), numinp, numout);
    _input_imageA = inputNamed("InputA");
    _input_imageB = inputNamed("InputB");
    _input_paramA = inputNamed("ParamA");
    _input_paramB = inputNamed("ParamB");
  }
  void compute(GraphInst* inst) final {
    printf("COMPUTE Op2ModuleInst<%p:%s>\n", (void*)this, _dgmodule_data->_name.c_str());
  }
  inpluginst_ptr_t _input_imageA;
  inpluginst_ptr_t _input_imageB;
  inpluginst_ptr_t _input_paramA;
  inpluginst_ptr_t _input_paramB;
};

dgmoduleinst_ptr_t Op2ModuleData::createInstance() const {
  return std::make_shared<Op2ModuleInst>(this);
}

////////////////////////////////////////////////////////////

class TestGraphData : public GraphData {
public:
  DeclareConcreteX(TestGraphData, GraphData);

  bool canConnect(inplugdata_constptr_t pin, outplugdata_constptr_t pout) const final {
    return true;
  }
};

void TestGraphData::describeX(class_t* clazz) {
}

using testgraphdata_ptr_t = std::shared_ptr<TestGraphData>;

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

struct TestDataSet {

  TestDataSet() {

    /////////////////////////////////
    // create dataflow graph
    /////////////////////////////////

    _testgraphdata = std::make_shared<TestGraphData>();

    /////////////////////////////////
    // create dataflow modules
    /////////////////////////////////

    _gl   = GlobalModuleData::createShared();
    _grA  = GradientModuleData::createShared();
    _grB  = GradientModuleData::createShared();
    _op1  = Op1ModuleData::createShared();
    _op2A = Op2ModuleData::createShared();
    _op2B = Op2ModuleData::createShared();

    GraphData::addModule(_testgraphdata, "op1", _op1);
    GraphData::addModule(_testgraphdata, "op2A", _op2A);
    GraphData::addModule(_testgraphdata, "op2B", _op2B);
    GraphData::addModule(_testgraphdata, "gradientB", _grB);
    GraphData::addModule(_testgraphdata, "gradientA", _grA);
    GraphData::addModule(_testgraphdata, "globals", _gl);

    /////////////////////////////////
    // get dataflow plugs from modules
    /////////////////////////////////

    _gl_outA = _gl->outputNamed("OutputA");
    _gl_outB = _gl->outputNamed("OutputB");
    _gl_outC = _gl->outputNamed("OutputC");
    //
    _gra_inp_a = _grA->inputNamed("Input");
    _gra_inp_b = _grA->inputNamed("Output");
    _gra_out   = _grA->outputNamed("Output");
    //
    _grb_inp_a = _grB->inputNamed("InputA");
    _grb_inp_b = _grB->inputNamed("InputB");
    _grb_out   = _grB->outputNamed("Output");
    //
    _op1_inp    = _op1->inputNamed("Input");
    _op1_out    = _op1->outputNamed("Output");
    _op1_parama = _op1->inputNamed("ParamA");
    _op1_paramb = _op1->inputNamed("ParamB");
    //
    _op2_inpA   = _op2A->inputNamed("InputA");
    _op2_inpB   = _op2A->inputNamed("InputB");
    _op2_out    = _op2A->outputNamed("Output");
    _op2_parama = _op2A->inputNamed("ParamA");
    _op2_paramb = _op2A->inputNamed("ParamB");
    //
    _op2B_inpA   = _op2B->inputNamed("InputA");
    _op2B_inpB   = _op2B->inputNamed("InputB");
    _op2B_out    = _op2B->outputNamed("Output");
    _op2B_parama = _op2B->inputNamed("ParamA");
    _op2B_paramb = _op2B->inputNamed("ParamB");

    /////////////////////////////////
    // create a dependency graph context
    /////////////////////////////////

    _dgcontext = std::make_shared<dgcontext>();

    /////////////////////////////////
    // assign/create dg register sets to context
    /////////////////////////////////

    _dgcontext->createRegisters<float>("ptex_float", 4);  // 4 float32 registers
    _dgcontext->createRegisters<Img32>("ptex_img32", 16); // 16 Image32 registers
    _dgcontext->createRegisters<Img64>("ptex_img32", 16); // 4 Image64 registers

  }

  void initialSort(){
    _dgsorter                            = std::make_shared<DgSorter>(_testgraphdata.get(), _dgcontext);
    _dgsorter->_logchannel->_enabled     = true;
    _dgsorter->_logchannel_reg->_enabled = true;
  }

  /////////////////////////////////////////////////////

  void linkConfig1() {
    _testgraphdata->safeConnect(_grb_inp_a, _gra_out);
    //
    _testgraphdata->safeConnect(_op1_inp, _grb_out);
    _testgraphdata->safeConnect(_op1_parama, _gl_outA);
    _testgraphdata->safeConnect(_op1_paramb, _gl_outB);
    //
    _testgraphdata->safeConnect(_op2_inpA, _op1_out);
    _testgraphdata->safeConnect(_op2_inpB, _op1_out);
    _testgraphdata->safeConnect(_op2_parama, _gl_outC);
    _testgraphdata->safeConnect(_op2_paramb, _gl_outA);
    //
    _testgraphdata->safeConnect(_op2B_inpA, _grb_out);
    _testgraphdata->safeConnect(_op2B_inpB, _op2_out);
    _testgraphdata->safeConnect(_op2B_parama, _gl_outC);
    _testgraphdata->safeConnect(_op2B_paramb, _gl_outA);
  }

  /////////////////////////////////////////////////////

  void linkConfig2() {

    linkConfig1(); // start with config1 as a reference

    _testgraphdata->disconnect(_op2_inpA);
    _testgraphdata->disconnect(_op2B_inpA);
    _testgraphdata->disconnect(_op2B_inpB);
    _testgraphdata->safeConnect(_op2B_inpA, _grb_out);
    _testgraphdata->safeConnect(_op2B_inpB, _grb_out);
    _testgraphdata->safeConnect(_op2_inpA, _op2B_out);
  }

  /////////////////////////////////////////////////////

  testgraphdata_ptr_t _testgraphdata;

  dgcontext_ptr_t _dgcontext;
  dgsorter_ptr_t _dgsorter;

  dgmoduledata_ptr_t _gl;
  dgmoduledata_ptr_t _grA;
  dgmoduledata_ptr_t _grB;
  dgmoduledata_ptr_t _op1;
  dgmoduledata_ptr_t _op2A;
  dgmoduledata_ptr_t _op2B;

  outplugdata_ptr_t _gl_outA;
  outplugdata_ptr_t _gl_outB;
  outplugdata_ptr_t _gl_outC;

  inplugdata_ptr_t _gra_inp_a;
  inplugdata_ptr_t _gra_inp_b;
  outplugdata_ptr_t _gra_out;

  inplugdata_ptr_t _grb_inp_a;
  inplugdata_ptr_t _grb_inp_b;
  outplugdata_ptr_t _grb_out;

  inplugdata_ptr_t _op1_parama;
  inplugdata_ptr_t _op1_paramb;
  inplugdata_ptr_t _op1_inp;
  outplugdata_ptr_t _op1_out;

  inplugdata_ptr_t _op2_parama;
  inplugdata_ptr_t _op2_paramb;
  inplugdata_ptr_t _op2_inpA;
  inplugdata_ptr_t _op2_inpB;
  outplugdata_ptr_t _op2_out;

  inplugdata_ptr_t _op2B_parama;
  inplugdata_ptr_t _op2B_paramb;
  inplugdata_ptr_t _op2B_inpA;
  inplugdata_ptr_t _op2B_inpB;
  outplugdata_ptr_t _op2B_out;
};

TEST(dflow_depthcalc1) {
  //printf("////////////////////////////////////////////////////////////\n");
  //printf("////// DEPTHCALC TEST 1\n");
  //printf("////////////////////////////////////////////////////////////\n");

  auto test_data = std::make_shared<TestDataSet>();
  test_data->linkConfig1();
  test_data->initialSort();
  size_t depth = test_data->_op2B_inpA->computeMinDepth(test_data->_gl);
  CHECK(depth==InPlugData::NOPATH);
}

TEST(dflow_depthcalc2) {
  //printf("////////////////////////////////////////////////////////////\n");
  //printf("////// DEPTHCALC TEST 2\n");
  //printf("////////////////////////////////////////////////////////////\n");

  auto test_data = std::make_shared<TestDataSet>();
  test_data->linkConfig1();
  test_data->initialSort();
  size_t depth = test_data->_op2B_inpB->computeMinDepth(test_data->_gl);
  CHECK(depth==2);
}

TEST(dflow_depthcalc3) {
  //printf("////////////////////////////////////////////////////////////\n");
  //printf("////// DEPTHCALC TEST 3\n");
  //printf("////////////////////////////////////////////////////////////\n");

  auto test_data = std::make_shared<TestDataSet>();
  test_data->linkConfig1();
  test_data->_testgraphdata->disconnect(test_data->_op2_parama);
  test_data->_testgraphdata->disconnect(test_data->_op2_paramb);
  test_data->initialSort();
  size_t depth = test_data->_op2B_inpB->computeMinDepth(test_data->_gl);
  CHECK(depth==3);
}

TEST(dflow_order1) {

  printf("////////////////////////////////////////////////////////////\n");
  printf("////// ORDERING TEST\n");
  printf("////////////////////////////////////////////////////////////\n");

  auto test_data = std::make_shared<TestDataSet>();
  test_data->linkConfig1();
  test_data->initialSort();

  /////////////////////////////////
  // generate a topology sorted execution list
  //  from dataflow graph
  /////////////////////////////////

  auto topo = test_data->_dgsorter->generateTopology();
  std::vector<dgmoduledata_ptr_t> expected_order{
      test_data->_gl,   //
      test_data->_grA,  //
      test_data->_grB,  //
      test_data->_op1,  //
      test_data->_op2A, //
      test_data->_op2B};
  CHECK(expected_order == topo->_flattened);

  /////////////////////////////////
  // misc dump output
  /////////////////////////////////

  test_data->_dgsorter->dumpInputs(test_data->_gl);
  test_data->_dgsorter->dumpOutputs(test_data->_gl);

  test_data->_dgsorter->dumpInputs(test_data->_grA);
  test_data->_dgsorter->dumpOutputs(test_data->_grA);

  test_data->_dgsorter->dumpInputs(test_data->_grB);
  test_data->_dgsorter->dumpOutputs(test_data->_grB);

  test_data->_dgsorter->dumpInputs(test_data->_op1);
  test_data->_dgsorter->dumpOutputs(test_data->_op1);
}

TEST(dflow_order2) {
  printf("////////////////////////////////////////////////////////////\n");
  printf("////// ORDERING TEST 2\n");
  printf("////////////////////////////////////////////////////////////\n");

  auto test_data = std::make_shared<TestDataSet>();
  test_data->linkConfig2();
  test_data->initialSort();
  auto topo = test_data->_dgsorter->generateTopology();
  std::vector<dgmoduledata_ptr_t> expected_order{
      test_data->_gl,   //
      test_data->_grA,  //
      test_data->_grB,  //
      test_data->_op1,  //
      test_data->_op2B, //
      test_data->_op2A};
  CHECK(expected_order == topo->_flattened);
}

TEST(dflow_compute1) {
  printf("////////////////////////////////////////////////////////////\n");
  printf("////// GRAPHINST COMPUTE TEST\n");
  printf("////////////////////////////////////////////////////////////\n");

  auto test_data = std::make_shared<TestDataSet>();
  test_data->linkConfig2();
  test_data->initialSort();
  auto gi   = std::make_shared<GraphInst>(test_data->_testgraphdata);
  auto impl = gi->_impl.makeShared<ImageGenTestImpl>();
  auto topo = test_data->_dgsorter->generateTopology();
  gi->updateTopology(topo);


  printf("////// computing.. \n");

  gi->compute();

  printf("////// computing.. \n");

  gi->compute();

  printf("////// computing.. \n");

  gi->compute();

  printf("////// computing.. \n");

  gi->compute();
}

////////////////////////////////////////////////////////////

} // namespace ork::dataflow::test -> ork::dataflow

///////////////////////////////////////////////////////////////////////////////
// plugdata<ImgBase>
///////////////////////////////////////////////////////////////////////////////

template <> void outplugdata<test::ImgBasePlugTraits>::describeX(class_t* clazz) {
}
template <> void inplugdata<test::ImgBasePlugTraits>::describeX(class_t* clazz) {
}

///////////////////////////////////////////////////////////////////////////////
// plugdata<Img32>
///////////////////////////////////////////////////////////////////////////////

template <> void outplugdata<test::Img32PlugTraits>::describeX(class_t* clazz) {
}

template <> void inplugdata<test::Img32PlugTraits>::describeX(class_t* clazz) {
}
template <> inpluginst_ptr_t inplugdata<test::Img32PlugTraits>::createInstance() const {
  return std::make_shared<inpluginst<test::Img32PlugTraits>>(this);
}
template <> outpluginst_ptr_t outplugdata<test::Img32PlugTraits>::createInstance() const {
  return std::make_shared<outpluginst<test::Img32PlugTraits>>(this);
}

template struct outplugdata<test::Img32PlugTraits>;
template struct inplugdata<test::Img32PlugTraits>;

///////////////////////////////////////////////////////////////////////////////
// plugdata<Img64>
///////////////////////////////////////////////////////////////////////////////

template <> void outplugdata<test::Img64PlugTraits>::describeX(class_t* clazz) {
}

template <> void inplugdata<test::Img64PlugTraits>::describeX(class_t* clazz) {
}
template <> inpluginst_ptr_t inplugdata<test::Img64PlugTraits>::createInstance() const {
  return std::make_shared<inpluginst<test::Img64PlugTraits>>(this);
}
template <> outpluginst_ptr_t outplugdata<test::Img64PlugTraits>::createInstance() const {
  return std::make_shared<outpluginst<test::Img64PlugTraits>>(this);
}
template struct outplugdata<test::Img64PlugTraits>;
template struct inplugdata<test::Img64PlugTraits>;
} // namespace ork::dataflow

////////////////////////////////////////////////////////////

using namespace ork::dataflow;

ImplementTemplateReflectionX(outplugdata<test::Img32PlugTraits>, "dflowtest/outplugimg32");
ImplementTemplateReflectionX(inplugdata<test::Img32PlugTraits>, "dflowtest/inplugimg32");

ImplementTemplateReflectionX(outplugdata<test::Img64PlugTraits>, "dflowtest/outplugimg64");
ImplementTemplateReflectionX(inplugdata<test::Img64PlugTraits>, "dflowtest/inplugimg64");

ImplementReflectionX(test::BaseModuleData, "dflowtest/BaseModule");
ImplementReflectionX(test::GlobalModuleData, "dflowtest/GlobalModule");

ImplementReflectionX(test::ImgModuleData, "dflowtest/ImgModule");
ImplementReflectionX(test::Img32ModuleData, "dflowtest/Img32Module");
ImplementReflectionX(test::Img64ModuleData, "dflowtest/Img64Module");

ImplementReflectionX(test::TestGraphData, "dflowtest/TestGraphData");
ImplementReflectionX(test::GradientModuleData, "dflowtest/GradientModule");
ImplementReflectionX(test::Op1ModuleData, "dflowtest/Op1Module");
ImplementReflectionX(test::Op2ModuleData, "dflowtest/Op2Module");

////////////////////////////////////////////////////////////
