#include <ork/lev2/ezapp.h>
#include <ork/lev2/aud/singularity/krzdata.h>
#include <ork/lev2/aud/singularity/tx81z.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/kernel/string/string.h>
#include <ork/reflect/serialize/JsonDeserializer.h>
#include <ork/reflect/serialize/JsonSerializer.h>

using namespace ork;
using namespace ork::lev2;
using namespace ork::reflect::serdes;
using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto app = EzApp::get(argc, argv); // reflection init
  //////////////////////////////////////////////////////////////////////////////
  // allocate program/layer data
  //////////////////////////////////////////////////////////////////////////////
  auto bank                              = std::make_shared<Tx81zData>();
  auto program                           = std::make_shared<ProgramData>();
  bank->_bankdata->_programs[0]          = program;
  bank->_bankdata->_programsByName["yo"] = program;
  auto layerdata                         = program->newLayer();
  auto prgdata81z                        = std::make_shared<Tx81zProgData>();
  program->_tags                         = "fm4";
  program->_name                         = "test";
  prgdata81z->_alg                       = 0;
  auto& opd0                             = prgdata81z->_ops[0];
  auto& opd1                             = prgdata81z->_ops[1];
  auto& opd2                             = prgdata81z->_ops[2];
  auto& opd3                             = prgdata81z->_ops[3];
  //////////////////////////////////////
  // setup dsp graph
  //////////////////////////////////////
  configureTx81zAlgorithm(layerdata, prgdata81z);
  auto ops_stage = layerdata->stageByName("OPS");
  auto op0       = ops_stage->_blockdatas[3];
  auto op1       = ops_stage->_blockdatas[2];
  auto op2       = ops_stage->_blockdatas[1];
  auto op3       = ops_stage->_blockdatas[0];
  //////////////////////////////////////
  op0->param(0)->_coarse   = 6000.0f; // op0 pitch
  op1->param(0)->_coarse   = 7200.0f; // op1 pitch
  op2->param(0)->_coarse   = 7200.0f; // op2 pitch
  op3->param(0)->_coarse   = 8400.0f; // op3 pitch
  op0->param(0)->_keyTrack = -100.0f; // op0 pitch keytrack
  op1->param(0)->_keyTrack = 100.0f;  // op1 pitch keytrack
  op2->param(0)->_keyTrack = 100.0f;  // op2 pitch keytrack
  op3->param(0)->_keyTrack = 100.0f;  // op3 pitch keytrack
  //////////////////////////////////////
  op0->param(1)->_coarse = 0.0f; // op0 amp
  op1->param(1)->_coarse = 0.0f; // op1 amp
  op2->param(1)->_coarse = 0.0f; // op2 amp
  op3->param(1)->_coarse = 0.0f; // op3 amp
  //////////////////////////////////////
  op3->param(2)->_coarse = 1.0f; // feedback = 2PI

  //////////////////////////////////////
  // setup modulators
  //////////////////////////////////////
  auto modop = [layerdata](
                   std::string opname, //
                   dspblkdata_ptr_t op) -> controllerdata_ptr_t {
    auto pitch_param             = op->param(0);
    auto amp_param               = op->param(1);
    auto feedback_param          = op->param(2);
    auto envname                 = FormatString("%sENV", opname.c_str());
    auto ENV                     = layerdata->appendController<YmEnvData>(envname);
    amp_param->_mods->_src1      = ENV;
    amp_param->_mods->_src1Depth = 1.0;
    return ENV;
  };
  auto op0env = modop("OP0", op0);
  auto op1env = modop("OP1", op1);
  auto op2env = modop("OP2", op2);
  auto op3env = modop("OP3", op3);
  //////////////////////////////////////////////////////////////////////////////
  JsonSerializer ser;
  auto topnode    = ser.serializeRoot(bank->_bankdata);
  auto jsonresult = ser.output();
  printf("jsonresult:\n%s\n", jsonresult.c_str());
  //////////////////////////////////////////////////////////////////////////////
  return 0;
}
