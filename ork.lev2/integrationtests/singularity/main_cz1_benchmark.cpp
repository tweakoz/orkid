#include <map>
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
  constexpr int KNUMFRAMES = 512;
  float inpbuf[KNUMFRAMES * 2];
  memset(inpbuf, 0, KNUMFRAMES * 2 * sizeof(float));
  const auto& obuf = the_synth->_obuf;

  int numiters      = 0;
  double total_time = 0.0;
  timer.Start();
  bool done               = false;
  double voicebench_accum = 0;
  double cur_time         = timer.SecsSinceStart();
  double prev_time        = cur_time;
  /////////////////////////////////////////
  std::multimap<double, int> time_histogram;
  while (done == false) {
    the_synth->compute(KNUMFRAMES, inpbuf);
    cur_time         = timer.SecsSinceStart();
    double iter_time = cur_time - prev_time;
    ///////////////////////////////////////////
    // histogram for looking for timing spikes
    ///////////////////////////////////////////
    auto hit = time_histogram.find(iter_time);
    if (hit == time_histogram.end()) {
      hit = time_histogram.insert(std::make_pair(iter_time, 0));
    }
    hit->second++;
    ///////////////////////////////////////////
    prev_time = cur_time;
    numiters++;
    done = cur_time > 10.0;
    if (numiters % 256 == 0) {
      printf("numiters<%d>                  \r", numiters);
      fflush(stdout);
    }
  }
  /////////////////////////////////////////
  size_t samples_computed = size_t(numiters) * size_t(KNUMFRAMES);
  double maxSR            = double(samples_computed) / cur_time;
  double voicebench       = double(numvoices) * maxSR;
  /////////////////////////////////////////
  printf("voicebench maxSR<%d> numvoices<%d> voicebench<%d>\n", int(maxSR), numvoices, int(voicebench));
  /////////////////////////////////////////
  // time histogram report
  /////////////////////////////////////////
  double desired_blockperiod = 1000.0 / (48000.0 / double(KNUMFRAMES));
  printf("desired_blockperiod<%g msec>\n", desired_blockperiod);
  /////
  auto it_lo       = time_histogram.begin();
  auto it_hi       = time_histogram.rbegin();
  double lo        = it_lo->first * 1000.0;
  double hi        = it_hi->first * 1000.0;
  double accum     = 0.0;
  int numunderruns = 0;
  for (auto item : time_histogram) {
    double t = item.first;
    accum += t;
    if (t * 1000.0 > desired_blockperiod) {
      numunderruns++;
    }
  }
  double avg = 1000.0 * accum / double(time_histogram.size());
  printf("exectime range lo<%g msec> hi<%g msec> avg<%g msec>\n", lo, hi, avg);
  printf("numunderruns<%d>\n", numunderruns);
  /////////////////////////////////////////
  return 0;
}
