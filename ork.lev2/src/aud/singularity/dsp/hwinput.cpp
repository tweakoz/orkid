#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>

ImplementReflectionX(ork::audio::singularity::HwInputData, "DspHwInput");

///////////////////////////////////////////////////////////////////////////////
namespace ork::audio::singularity {
///////////////////////////////////////////////////////////////////////////////

void HwInputData::describeX(class_t* clazz) {}

HwInputData::HwInputData(std::string name)
    : DspBlockData(name) {
  _blocktype            = "HwInput";
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t HwInputData::createInstance() const { // override
  return createDspInstance<HwInput>(this);
}

///////////////////////////////////////////////////////////////////////////////

HwInput::HwInput(const HwInputData* dbd)
    : DspBlock(dbd) {
  auto syni = synth::instance();
}
HwInput::~HwInput(){
  auto syni = synth::instance();
}
///////////////////////////////////////////////////////////////////////////////

void HwInput::compute(DspBuffer& dspbuf) { // final

  auto syni = synth::instance();
  auto input_left   = syni->_ibuf._leftBuffer + _layer->_dspwritebase;
  auto outputchan = getOutBuf(dspbuf, 0) + _layer->_dspwritebase;
  int inumframes = _layer->_dspwritecount;
  //printf("inumframes<%d>\n", inumframes);
  float scale = _key_down ? 1.0f : 0.0f;
  for (int i = 0; i < inumframes; i++) {
    outputchan[i] = input_left[i]*scale;
  }
}

///////////////////////////////////////////////////////////////////////////////

void HwInput::doKeyOn(const KeyOnInfo& koi) { // final
  _key_down = true;
}

///////////////////////////////////////////////////////////////////////////////

void HwInput::doKeyOff() {
  _key_down = false;
}

///////////////////////////////////////////////////////////////////////////////

}
