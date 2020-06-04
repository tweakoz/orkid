#include "harness.h"
#include <ork/kernel/timer.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_amp.h>

using namespace ork::audio::singularity;

int main(int argc, char** argv) {
  auto the_synth = synth::instance();
  double SR      = getSampleRate();
  the_synth->setSampleRate(SR);
  the_synth->_masterGain = 0.5f;
  //////////////////////////////////////////////////////////////////////////////
  // allocate program/layer data
  //////////////////////////////////////////////////////////////////////////////
  auto program   = std::make_shared<ProgramData>();
  auto layerdata = program->newLayer();
  auto czoscdata = std::make_shared<CzOscData>();
  program->_role = "czx";
  program->_name = "test";
  //////////////////////////////////////
  // setup dsp graph
  //////////////////////////////////////
  layerdata->_algdata = configureCz1Algorithm(layerdata, 1);
  auto dcostage       = layerdata->stageByName("DCO");
  auto ampstage       = layerdata->stageByName("AMP");
  auto osc            = dcostage->appendTypedBlock<CZX>(czoscdata, 0);
  auto amp            = ampstage->appendTypedBlock<AMP_MONOIO>();
  //////////////////////////////////////
  // setup modulators
  //////////////////////////////////////
  auto DCAENV = layerdata->appendController<RateLevelEnvData>("DCAENV");
  auto DCWENV = layerdata->appendController<RateLevelEnvData>("DCWENV");
  auto LFO2   = layerdata->appendController<LfoData>("MYLFO2");
  auto LFO1   = layerdata->appendController<LfoData>("MYLFO1");
  //////////////////////////////////////
  // setup envelope
  //////////////////////////////////////
  DCAENV->_ampenv = true;
  DCAENV->addSegment("seg0", .2, .7);
  DCAENV->addSegment("seg1", .2, .7);
  DCAENV->addSegment("seg2", 1, 1);
  DCAENV->addSegment("seg3", 120, .3);
  DCAENV->addSegment("seg4", 120, 0);
  //
  DCWENV->_ampenv = false;
  DCWENV->addSegment("seg0", 0.1, .7);
  DCWENV->addSegment("seg1", 1, 1);
  DCWENV->addSegment("seg2", 2, .5);
  DCWENV->addSegment("seg3", 2, 1);
  DCWENV->addSegment("seg4", 2, 1);
  DCWENV->addSegment("seg5", 40, 1);
  DCWENV->addSegment("seg6", 40, 0);
  //////////////////////////////////////
  // setup LFO
  //////////////////////////////////////
  LFO1->_minRate = 0.25;
  LFO1->_maxRate = 0.25;
  LFO1->_shape   = "Sine";
  LFO2->_minRate = 3.3;
  LFO2->_maxRate = 3.3;
  LFO2->_shape   = "Sine";
  //////////////////////////////////////
  // setup modulation routing
  //////////////////////////////////////
  auto& modulation_index_param      = osc->_paramd[0]._mods;
  modulation_index_param._src1      = DCWENV;
  modulation_index_param._src1Depth = 1.0;
  // modulation_index_param._src2      = LFO1;
  // modulation_index_param._src2DepthCtrl = LFO2;
  modulation_index_param._src2MinDepth = 0.5;
  modulation_index_param._src2MaxDepth = 0.1;
  //////////////////////////////////////
  czoscdata->_dcoBaseWaveA = 6;
  czoscdata->_dcoBaseWaveB = 7;
  czoscdata->_dcoWindow    = 2;
  //////////////////////////////////////
  auto& amp_param   = amp->_paramd[0];
  amp_param._coarse = 0.0f;
  amp_param.useDefaultEvaluator();
  amp_param._mods._src1      = DCAENV;
  amp_param._mods._src1Depth = 1.0;
  //////////////////////////////////////////////////////////////////////////////
  // benchmark
  //////////////////////////////////////////////////////////////////////////////

  // enqueue test notes
  constexpr int numvoices = 128;
  for (int i = 0; i < numvoices; i++) {
    enqueue_audio_event(program, double(i) * 0.001, 240.0, 48);
  }

  ork::Timer timer;
  constexpr int KNUMFRAMES = 256;
  float inpbuf[KNUMFRAMES * 2];
  memset(inpbuf, 0, KNUMFRAMES * 2 * sizeof(float));
  const auto& obuf = the_synth->_obuf;

  int iblockcount   = 0;
  int numiters      = 0;
  double total_time = 0.0;
  timer.Start();
  bool done               = false;
  double voicebench_accum = 0;
  while (done == false) {
    the_synth->compute(KNUMFRAMES, inpbuf);
    iblockcount++;
    double cur_time = timer.SecsSinceStart();
    if (cur_time > 1.0f) {

      double maxSR      = (double(iblockcount) * double(KNUMFRAMES)) / cur_time;
      double voicebench = double(numvoices) * maxSR;
      voicebench_accum += voicebench;
      printf(
          "time<%g> iblockcount<%d> maxSR<%d> numvoices<%d> voicebench<%d>\n", //
          cur_time,
          iblockcount,
          int(maxSR),
          numvoices,
          int(voicebench));

      timer.Start();
      iblockcount = 0;
      total_time += cur_time;
      numiters++;
    }
    done = total_time > 10.0;
  }
  int voicebench_avg = voicebench_accum / double(numiters);
  printf("voicebench total<%d> average<%d>\n", int(voicebench_accum), voicebench_avg);
  return 0;
}
