#include <ork/application/application.h>
#include <ork/lev2/aud/audiodevice.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/kernel/timer.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/environment.h>

namespace ork{
  void initModule(ork::appinitdata_ptr_t init_data);
  void exitModule(ork::appinitdata_ptr_t init_data);
  namespace lev2{
    void initModule(ork::appinitdata_ptr_t init_data);
    void exitModule(ork::appinitdata_ptr_t init_data);
  }
}
using namespace ork;

int main(int argc, char** argv){

  ork::genviron.init_from_global_env();

  auto initdata = std::make_shared<AppInitData>();

  initdata->_enable_audio = true;
  initdata->_enable_audio_input = true;
  initdata->_enable_audio_synth = false;
  initdata->_enable_graphics = false;
  initdata->_audio_input_numchannels = 1;

  ::ork::initModule(initdata);
  ::ork::lev2::initModule(initdata);
  initdata->finalizeInitialization();

  auto auddev = lev2::AudioDevice::createInstance(initdata);
  initdata->_miscvars["audiodevice"].set<ork::lev2::audiodevice_ptr_t>(auddev);
  if(initdata->_enable_audio_synth){
    auto synth = audio::singularity::synth::instance();
    initdata->_miscvars["synth"].set<audio::singularity::synth_ptr_t>(synth);
    if(synth){
      synth->mainThreadHandler();
    }
  }

  size_t _frame_counter = 0;
  double _energy_accum = 0.0f;


  if(initdata->_enable_audio_input){
    
    auto input_handler = [&](lev2::audioinputchunk_const_rawptr_t chunk){
      auto num_frames = chunk->_num_frames;
      auto& chan0 = chunk->_channels[0];
      for(size_t f=0; f<num_frames; f++){
        auto frame = chan0[f];
        _energy_accum += std::abs(frame);
      }
      _frame_counter += num_frames;
    };
    auddev->_input_handler = input_handler;
  }

  auddev->startup();

  printf("beginning audio energy accumulation, please wait...\n");
  Timer t;
  t.Start();
  while(t.SecsSinceStart()<10.0){
    ::ork::opq::mainSerialQueue()->Process();
    ::usleep(100);
  }
  double energy_avg = _energy_accum/double(_frame_counter);
  double energy_avg_dB = audiomath::linear_amp_ratio_to_decibel(energy_avg);

  double fps = double(_frame_counter)/t.SecsSinceStart();


  printf("finished audio energy accumulation\n");

  auddev->shutdown();

  printf("frame_counter: %zu\n", _frame_counter);
  printf("measured fps: %f\n", fps);
  printf("energy_accum: %f\n", _energy_accum);
  printf("energy_avg: %f\n", energy_avg);
  printf("energy_avg_dB: %f\n", energy_avg_dB);

  ::ork::lev2::exitModule(initdata);
  ::ork::exitModule(initdata);

  return 0;
}
