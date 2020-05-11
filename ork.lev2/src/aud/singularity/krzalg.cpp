#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/konoff.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

const int kmaskUPPER = 1;
const int kmaskLOWER = 2;
const int kmaskBOTH  = 3;

const std::map<int, AlgConfig>& KrzAlgMap() {
  static bool ginit = true;
  static std::map<int, AlgConfig> algmap;
  if (ginit) {
    ginit = false;
    // KRZ1 (PCH->DSP3->AMP->MONO)
    algmap[1]._ioMasks[0]._inputMask = 0;
    // KRZ2 (PCH->DSP2->DSP1->PANNER->AMP->STEREO)
    algmap[2]._ioMasks[3]._outputMask = kmaskBOTH;
    algmap[2]._ioMasks[4]             = IoMask(kmaskBOTH, kmaskBOTH);
    // KRZ3 (PCH->DSP2->DSP1->PANNER->AMP->STEREO)
    algmap[3]._ioMasks[3] = IoMask(kmaskBOTH, kmaskBOTH);
    // KRZ4
    algmap[4]._ioMasks[0]._inputMask = 0;
    // KRZ5
    algmap[5]._ioMasks[0]._inputMask = 0;
    // KRZ6
    algmap[6]._ioMasks[3]._inputMask = kmaskBOTH;
    // KRZ7
    algmap[7]._ioMasks[3]            = IoMask(kmaskLOWER, kmaskLOWER);
    algmap[7]._ioMasks[4]._inputMask = kmaskBOTH;
    // KRZ9
    algmap[9]._ioMasks[0]._inputMask = 0;
    // KRZ10
    algmap[10]._ioMasks[2]            = IoMask(kmaskLOWER, kmaskLOWER);
    algmap[10]._ioMasks[4]._inputMask = kmaskBOTH;
    // KRZ11
    algmap[11]._ioMasks[1]._outputMask = kmaskBOTH;
    algmap[11]._ioMasks[2]._outputMask = kmaskLOWER;
    algmap[11]._ioMasks[4]._inputMask  = kmaskBOTH;
    // KRZ12
    algmap[12]._ioMasks[1]._outputMask = kmaskBOTH;
    algmap[12]._ioMasks[4]._inputMask  = kmaskBOTH;
    // KRZ14
    algmap[14]._ioMasks[2]._inputMask = kmaskLOWER;
    // KRZ15
    algmap[15]._ioMasks[1]._inputMask = kmaskBOTH;
    // KRZ20
    algmap[20]._ioMasks[0]._inputMask = 0;
    algmap[20]._ioMasks[2]._inputMask = kmaskBOTH;
    // KRZ22
    algmap[22]._ioMasks[2]._outputMask = kmaskLOWER;
    // KRZ23
    algmap[23]._ioMasks[2]._outputMask = kmaskBOTH;
    // KRZ24
    algmap[24]._ioMasks[2]._inputMask  = kmaskBOTH;
    algmap[24]._ioMasks[3]._outputMask = kmaskBOTH;
    algmap[24]._ioMasks[4]._inputMask  = kmaskBOTH;
    algmap[24]._ioMasks[4]._outputMask = kmaskBOTH;
    // KRZ26
    algmap[26]._ioMasks[3]._outputMask = kmaskBOTH;
    algmap[26]._ioMasks[4]._outputMask = kmaskBOTH;
    algmap[26]._ioMasks[4]._inputMask  = kmaskBOTH;
  }
  return algmap;
}

} // namespace ork::audio::singularity
