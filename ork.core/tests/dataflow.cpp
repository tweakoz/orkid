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

#if 1

////////////////////////////////////////////////////////////

namespace ork { namespace dataflow {
namespace test {
////////////////////////////////////////////////////////////
struct ImgBase {};
struct Img32 : public ImgBase {};
struct Img64 : public ImgBase {};
struct Buffer {};
Buffer gBuffer;
////////////////////////////////////////////////////////////
} // namespace test
////////////////////////////////////////////////////////////

template <> void outplugdata<ork::dataflow::test::ImgBase>::describeX(class_t* clazz) {
}
template <> void inplugdata<ork::dataflow::test::ImgBase>::describeX(class_t* clazz) {
}
template <> int MaxFanout<ork::dataflow::test::ImgBase>() {
  return 0;
}

////////////////////////////////////////////////////////////
namespace test {
////////////////////////////////////////////////////////////

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
    createOutputPlug<float>(gmd, EPR_UNIFORM, gmd->_outputA, "OutputA");
    createOutputPlug<float>(gmd, EPR_UNIFORM, gmd->_outputB, "OutputB");
    createOutputPlug<float>(gmd, EPR_UNIFORM, gmd->_outputC, "OutputC");
    return gmd;
  }

  float_ptr_t _outputA;
  float_ptr_t _outputB;
  float_ptr_t _outputC;
};

void GlobalModuleData::describeX(class_t* clazz) {
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
      //printf( "MOD<%p> WBI<%d>\n", this, base.miBufferIndex );
      return gBuffer;
      //return ptex.GetBuffer(outplug->GetValue().miBufferIndex);
  }*/
};

Img32 ImgModuleData::g_no_connection;

void ImgModuleData::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////

using img32_ptr_t         = std::shared_ptr<Img32>;
using img32_outplug_t     = outplugdata<Img32>;
using img32_outplug_ptr_t = std::shared_ptr<img32_outplug_t>;
using img32_inplug_t      = inplugdata<Img32>;
using img32_inplug_ptr_t  = std::shared_ptr<img32_inplug_t>;

////////////////////////////////////////////////////////////

using img64_ptr_t         = std::shared_ptr<Img64>;
using img64_outplug_t     = outplugdata<Img64>;
using img64_outplug_ptr_t = std::shared_ptr<img64_outplug_t>;
using img64_inplug_t      = inplugdata<Img64>;
using img64_inplug_ptr_t  = std::shared_ptr<img64_inplug_t>;

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
    createOutputPlug<Img32>(subclass_instance, EPR_UNIFORM, as_im32mod->_image_out, "Output");
  }
};

void Img32ModuleData::describeX(class_t* clazz) {
}

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
    createOutputPlug<Img64>(subclass_instance, EPR_UNIFORM, as_im64mod->_image_out, "Output");
  }
};

void Img64ModuleData::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////

struct GradientModuleData : public Img32ModuleData {
  DeclareConcreteX(GradientModuleData, Img32ModuleData);

public: //
  GradientModuleData()
      : Img32ModuleData() {

    _image_input_A = std::shared_ptr<Img32>();
    _image_input_B = std::shared_ptr<Img32>();
  }

  static std::shared_ptr<GradientModuleData> createShared() {
    auto gmd = std::make_shared<GradientModuleData>();
    Img32ModuleData::sharedConstructor(gmd);
    createInputPlug<Img32>(gmd, EPR_UNIFORM, gmd->_image_input_A, "InputA");
    createInputPlug<Img32>(gmd, EPR_UNIFORM, gmd->_image_input_B, "InputB");
    return gmd;
  }

  img32_ptr_t _image_input_A;
  img32_ptr_t _image_input_B;
};

void GradientModuleData::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////

struct Op1ModuleData : public Img32ModuleData {
  DeclareConcreteX(Op1ModuleData, Img32ModuleData);

public: //
  Op1ModuleData()
      : Img32ModuleData() {
    _image_input = std::shared_ptr<Img32>();
    _paramA      = std::make_shared<float>(0.0f);
    _paramB      = std::make_shared<float>(0.0f);
  }

  static std::shared_ptr<Op1ModuleData> createShared() {
    auto gmd = std::make_shared<Op1ModuleData>();
    Img32ModuleData::sharedConstructor(gmd);
    createInputPlug<Img32>(gmd, EPR_UNIFORM, gmd->_image_input, "Input");
    createInputPlug<float>(gmd, EPR_UNIFORM, gmd->_paramA, "ParamA");
    createInputPlug<float>(gmd, EPR_UNIFORM, gmd->_paramB, "ParamB");
    return gmd;
  }

  img32_ptr_t _image_input;
  float_ptr_t _paramA;
  float_ptr_t _paramB;
};

void Op1ModuleData::describeX(class_t* clazz) {}

////////////////////////////////////////////////////////////

struct Op2ModuleData : public Img32ModuleData
{
  DeclareConcreteX( Op2ModuleData, Img32ModuleData );

public: //

  Op2ModuleData()
      : Img32ModuleData() {
    _image_inputA = std::shared_ptr<Img32>();
    _image_inputB = std::shared_ptr<Img32>();
    _paramA      = std::make_shared<float>(0.0f);
    _paramB      = std::make_shared<float>(0.0f);
  }

  static std::shared_ptr<Op2ModuleData> createShared() {
    auto gmd = std::make_shared<Op2ModuleData>();
    Img32ModuleData::sharedConstructor(gmd);
    createInputPlug<Img32>(gmd, EPR_UNIFORM, gmd->_image_inputA, "InputA");
    createInputPlug<Img32>(gmd, EPR_UNIFORM, gmd->_image_inputB, "InputB");
    createInputPlug<float>(gmd, EPR_UNIFORM, gmd->_paramA, "ParamA");
    createInputPlug<float>(gmd, EPR_UNIFORM, gmd->_paramB, "ParamB");
    return gmd;
  }

  img32_ptr_t _image_inputA;
  img32_ptr_t _image_inputB;
  float_ptr_t _paramA;
  float_ptr_t _paramB;

};

void Op2ModuleData::describeX(class_t* clazz) {}

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

////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////

TEST(dflow_a) {

  /////////////////////////////////
  // create dataflow graph
  /////////////////////////////////

  auto gdata = std::make_shared<TestGraphData>();

  /////////////////////////////////
  // create dataflow modules
  /////////////////////////////////

  auto gl  = GlobalModuleData::createShared();
  auto grA = GradientModuleData::createShared();
  auto grB = GradientModuleData::createShared();
  auto op1 = Op1ModuleData::createShared();
  auto op2 = Op2ModuleData::createShared();
  auto op3 = Op2ModuleData::createShared();

  GraphData::addModule(gdata, "op1", op1);
  GraphData::addModule(gdata, "op2", op2);
  GraphData::addModule(gdata, "op3", op3);
  GraphData::addModule(gdata, "gradientB", grB);
  GraphData::addModule(gdata, "gradientA", grA);
  GraphData::addModule(gdata, "globals", gl);

  /////////////////////////////////
  // get dataflow plugs from modules
  /////////////////////////////////

  auto gl_outA     = gl->outputNamed("OutputA");
  auto gl_outB     = gl->outputNamed("OutputB");
  auto gl_outC     = gl->outputNamed("OutputC");
  //
  auto gra_out    = grA->outputNamed("Output");
  //
  auto grb_inp_a  = grB->inputNamed("InputA");
  auto grb_out    = grB->outputNamed("Output");
  //
  auto op1_inp    = op1->inputNamed("Input");
  auto op1_out    = op1->outputNamed("Output");
  auto op1_parama = op1->inputNamed("ParamA");
  auto op1_paramb = op1->inputNamed("ParamB");
  //
  auto op2_inpA    = op2->inputNamed("InputA");
  auto op2_inpB    = op2->inputNamed("InputB");
  auto op2_out    = op2->outputNamed("Output");
  auto op2_parama = op2->inputNamed("ParamA");
  auto op2_paramb = op2->inputNamed("ParamB");
  //
  auto op3_inpA    = op3->inputNamed("InputA");
  auto op3_inpB    = op3->inputNamed("InputB");
  auto op3_out    = op3->outputNamed("Output");
  auto op3_parama = op3->inputNamed("ParamA");
  auto op3_paramb = op3->inputNamed("ParamB");

  /////////////////////////////////
  // link dataflow graph
  /////////////////////////////////

  gdata->safeConnect(grb_inp_a, gra_out);
  //
  gdata->safeConnect(op1_inp, grb_out);
  gdata->safeConnect(op1_parama, gl_outA);
  gdata->safeConnect(op1_paramb, gl_outB);
  //
  gdata->safeConnect(op2_inpA, op1_out);
  gdata->safeConnect(op2_inpB, op1_out);
  gdata->safeConnect(op2_parama, gl_outC);
  gdata->safeConnect(op2_paramb, gl_outA);
  //
  gdata->safeConnect(op3_inpA, grb_out);
  gdata->safeConnect(op3_inpB, op2_out);
  gdata->safeConnect(op3_parama, gl_outC);
  gdata->safeConnect(op3_paramb, gl_outA);
  //
  /////////////////////////////////
  // create a dependency graph context
  /////////////////////////////////

  printf( "////////////////////////////////////////////////////////////\n");
  printf( "////// ORDERING TEST\n");
  printf( "////////////////////////////////////////////////////////////\n");

  {
    auto dgctx = std::make_shared<dgcontext>();

    // create dg register sets

    dgregisterblock float_regs("ptex_float", 4);  // 4 float32 registers
    dgregisterblock img32_regs("ptex_img32", 16); // 16 Image32 registers
    dgregisterblock img64_regs("ptex_img64", 4);  // 4 Image64 registers

    /////////////////////////////////
    // assign dg register sets to context
    /////////////////////////////////

    dgctx->setRegisters<float>(&float_regs);
    dgctx->setRegisters<Img32>(&img32_regs);
    dgctx->setRegisters<Img64>(&img64_regs);

    /////////////////////////////////
    // generate a topology sorted execution list
    //  from dataflow graph
    /////////////////////////////////

    auto dgsorter                       = std::make_shared<DgSorter>(gdata.get(), dgctx);
    dgsorter->_logchannel->_enabled     = true;
    dgsorter->_logchannel_reg->_enabled = true;

    auto topo = dgsorter->generateTopology();

    std::vector<dgmoduledata_ptr_t> expected_order{
      gl, grA, grB, op1, op2, op3
    };

    CHECK(expected_order==topo->_flattened);
  }

  /////////////////////////////////
  // reordered
  /////////////////////////////////

  printf( "////////////////////////////////////////////////////////////\n");
  printf( "////// ORDERING TEST 2\n");
  printf( "////////////////////////////////////////////////////////////\n");

  {

    // reorder so OP2 comes after OP3
    gdata->disconnect(op2_inpA);
    gdata->disconnect(op3_inpA);
    gdata->disconnect(op3_inpB);
    gdata->safeConnect(op3_inpA, grb_out);
    gdata->safeConnect(op3_inpB, grb_out);
    gdata->safeConnect(op2_inpA,op3_out);

    auto dgctx = std::make_shared<dgcontext>();

    // create dg register sets

    dgregisterblock float_regs("ptex_float", 4);  // 4 float32 registers
    dgregisterblock img32_regs("ptex_img32", 16); // 16 Image32 registers
    dgregisterblock img64_regs("ptex_img64", 4);  // 4 Image64 registers

    /////////////////////////////////
    // assign dg register sets to context
    /////////////////////////////////

    dgctx->setRegisters<float>(&float_regs);
    dgctx->setRegisters<Img32>(&img32_regs);
    dgctx->setRegisters<Img64>(&img64_regs);

    /////////////////////////////////
    // generate a topology sorted execution list
    //  from dataflow graph
    /////////////////////////////////

    auto dgsorter                       = std::make_shared<DgSorter>(gdata.get(), dgctx);
    dgsorter->_logchannel->_enabled     = true;
    dgsorter->_logchannel_reg->_enabled = true;

    auto topo = dgsorter->generateTopology();

    std::vector<dgmoduledata_ptr_t> expected_order{
      gl, grA, grB, op1, op3, op2
    };

    CHECK(expected_order==topo->_flattened);

    /////////////////////////////////
    // misc dump output
    /////////////////////////////////

    dgsorter->dumpInputs(gl);
    dgsorter->dumpOutputs(gl);

    dgsorter->dumpInputs(grA);
    dgsorter->dumpOutputs(grA);

    dgsorter->dumpInputs(grB);
    dgsorter->dumpOutputs(grB);

    dgsorter->dumpInputs(op1);
    dgsorter->dumpOutputs(op1);
  }


}

////////////////////////////////////////////////////////////

} // namespace test
}} // namespace ork::dataflow

////////////////////////////////////////////////////////////

template <> int ork::dataflow::MaxFanout<ork::dataflow::test::Img32>() {
  return 0;
}

template <> void ork::dataflow::outplugdata<ork::dataflow::test::Img32>::describeX(class_t* clazz) {
}

template <> void ork::dataflow::inplugdata<ork::dataflow::test::Img32>::describeX(class_t* clazz) {
}

template <> int ork::dataflow::MaxFanout<ork::dataflow::test::Img64>() {
  return 0;
}

template <> void ork::dataflow::outplugdata<ork::dataflow::test::Img64>::describeX(class_t* clazz) {
}

template <> void ork::dataflow::inplugdata<ork::dataflow::test::Img64>::describeX(class_t* clazz) {
}

////////////////////////////////////////////////////////////

ImplementTemplateReflectionX(ork::dataflow::outplugdata<ork::dataflow::test::Img32>, "dflowtest/outplugimg32");
ImplementTemplateReflectionX(ork::dataflow::inplugdata<ork::dataflow::test::Img32>, "dflowtest/inplugimg32");

ImplementTemplateReflectionX(ork::dataflow::outplugdata<ork::dataflow::test::Img64>, "dflowtest/outplugimg64");
ImplementTemplateReflectionX(ork::dataflow::inplugdata<ork::dataflow::test::Img64>, "dflowtest/inplugimg64");

ImplementReflectionX(ork::dataflow::test::BaseModuleData, "dflowtest/BaseModule");
ImplementReflectionX(ork::dataflow::test::GlobalModuleData, "dflowtest/GlobalModule");

ImplementReflectionX(ork::dataflow::test::ImgModuleData, "dflowtest/ImgModule");
ImplementReflectionX(ork::dataflow::test::Img32ModuleData, "dflowtest/Img32Module");
ImplementReflectionX(ork::dataflow::test::Img64ModuleData, "dflowtest/Img64Module");

ImplementReflectionX(ork::dataflow::test::TestGraphData, "dflowtest/TestGraphData");
ImplementReflectionX(ork::dataflow::test::GradientModuleData, "dflowtest/GradientModule");
ImplementReflectionX(ork::dataflow::test::Op1ModuleData, "dflowtest/Op1Module");
ImplementReflectionX(ork::dataflow::test::Op2ModuleData, "dflowtest/Op2Module");

////////////////////////////////////////////////////////////

#endif
