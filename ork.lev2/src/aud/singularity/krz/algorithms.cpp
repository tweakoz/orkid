////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

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

algdata_ptr_t configureKrzAlgorithm(int algid) {

  auto algdout   = std::make_shared<AlgData>();
  algdout->_name = ork::FormatString("KrzALG%d", algid);
  switch (algid) {
    case 1: { // KRZ1 (PCH->DSP->AMP->MONO)
      auto stage_dsp = algdout->appendStage("DSP");
      auto stage_amp = algdout->appendStage("AMP");
      stage_dsp->_ioconfig->_inputs.push_back(0);  // 1 input
      stage_dsp->_ioconfig->_outputs.push_back(0); // 1 output
      stage_amp->_ioconfig->_inputs.push_back(0);  // 1 input
      stage_amp->_ioconfig->_outputs.push_back(0); // 1 output
      break;
    }
    case 2: { // KRZ2 (PCH->DSP1->DSP2->PANNER->AMP->STEREO)
      auto stage_dsp    = algdout->appendStage("DSP");
      auto stage_panner = algdout->appendStage("AMP");
      stage_dsp->_ioconfig->_inputs.push_back(0);    // 1 input
      stage_dsp->_ioconfig->_outputs.push_back(0);   // 1 output
      stage_panner->_ioconfig->_inputs.push_back(0); // 1 input
      stage_panner->_ioconfig->_outputs.push_back(0);
      stage_panner->_ioconfig->_outputs.push_back(1); // 2 outputs
      break;
    }
    default:
      OrkAssert(false);
      break;

      /*// KRZ3 (PCH->DSP2->DSP1->PANNER->AMP->STEREO)
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
      algmap[26]._ioMasks[4]._inputMask  = kmaskBOTH;*/
  }
  return algdout;
}

} // namespace ork::audio::singularity
