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
  , _ringBuffer(1048576) {
  _streamingdata = dynamic_cast<const STREAMING_OSCILLATOR_DATA*>(dbd);
}
void StreamingOscillatorBlock::compute(DspBuffer& dspbuf){

  auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  int inumframes = _layer->_dspwritecount;
  double phaseinc = PI2 * 440.0f / synth::instance()->_sampleRate;

  auto source = _streamingdata->_source;
  if(source){

    ///////////////////////////////
    // transfer from input chunk queue to ring buffer
    ///////////////////////////////

    lev2::audioinputchunk_ptr_t chunk;
    while(source->_inputqueue.try_pop(chunk)){
      auto& chan0 = chunk->_channels[0];
      size_t num_samples_this_chunk = chan0.size();
      const float* src = chan0.data();
      size_t num_samples = num_samples_this_chunk;
      _ringBuffer.push_many(src, num_samples);
    }

    ///////////////////////////////
    // throttle - attempt to keep the ring buffer somewhat full
    //  to account for timing instabilities
    ///////////////////////////////

    if(_ringBuffer.size()<_streamingdata->_low_watermark){
      for (int i = 0; i < inumframes; i++) {
        outputchan[i] = 0.0f;
      }
    }
    else{

      ///////////////////////////////
      // ok, we have enough data in the ring buffer
      //  to fill the output buffer
      ///////////////////////////////

      int num_frames_pushed=0;
      size_t failed = 0;
      while((num_frames_pushed<inumframes) and (failed<10)){
        size_t num_enqueued = _ringBuffer.size();
        if(num_enqueued>0){
          size_t num_frames_to_push = std::min(num_enqueued, size_t(inumframes-num_frames_pushed));
          _ringBuffer.pop_many(outputchan+num_frames_pushed, num_frames_to_push);
          num_frames_pushed += num_frames_to_push;
        }
        else{
          failed++;
        }
      }
    }
  }
  else{
    // no source, just zero output
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
