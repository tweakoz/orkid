////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <sndfile.h>
#include <string>
#include <assert.h>
#include <unistd.h>
#include <math.h>

#include <ork/lev2/aud/singularity/synthdata.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::STREAMING_OSCILLATOR_DATA, "DspStreamingOscillator");

namespace ork::audio::singularity {

void STREAMING_OSCILLATOR_DATA::describeX(class_t* clazz) {
}

STREAMING_OSCILLATOR_DATA::STREAMING_OSCILLATOR_DATA(std::string name){

}
dspblk_ptr_t STREAMING_OSCILLATOR_DATA::createInstance() const {
  auto instance = std::make_shared<StreamingOscillatorBlock>(this);
  return instance;
}

StreamingOscillatorBlock::StreamingOscillatorBlock(const DspBlockData* dbd)
  : DspBlock(dbd)
  , _ringBuffer(8192) {

  _streamingdata = dynamic_cast<const STREAMING_OSCILLATOR_DATA*>(dbd);
}
void StreamingOscillatorBlock::compute(DspBuffer& dspbuf){

  auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  int inumframes = _layer->_dspwritecount;
  double phaseinc = PI2 * 440.0f / synth::instance()->_sampleRate;

  auto source = _streamingdata->_source;
  if(source){
    int num_frames_pushed=0;
    bool keepgoing = true;
    size_t failed = 0;
    while((num_frames_pushed<inumframes) and (failed<10)){
      lev2::audioinputchunk_ptr_t chunk;
      if(source->_inputqueue.try_pop(chunk)){
        auto& chan0 = chunk->_channels[0];
        size_t num_samples_this_chunk = chan0.size();
        for (int i = 0; i < num_samples_this_chunk; i++) {
          int j = i+num_frames_pushed;
          outputchan[j] = chan0[i];
        }
        num_frames_pushed += inumframes;
      }
      else{
        failed++;
      }
    }
  }
  else{
    for (int i = 0; i < inumframes; i++) {
      outputchan[i] = 0.0f;
    }
  }

}
void StreamingOscillatorBlock::doKeyOn(const KeyOnInfo& koi){

}
void StreamingOscillatorBlock::doKeyOff(){

}

} // namespace ork::audio::singularity
