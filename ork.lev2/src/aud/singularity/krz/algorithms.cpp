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
  algdout->_name = ork::FormatString("Krz-ALG-%d", algid);
  //printf("configure IO for algid<%d>\n", algid );
  switch (algid) {
    case 1: { // KRZ1 (PCH->DSP->AMP->MONO)
      auto stage_dsp = algdout->appendStage("DSP");
      auto stage_amp = algdout->appendStage("AMP");
      stage_dsp->setNumIos(1, 1); // 1 in, 1 out
      stage_amp->setNumIos(1, 2); // 1 in, 1 out
      break;
    }
    case 2:{   // KRZ2 (PCH->DSP1->DSP2->PANNER->AMP->STEREO)
      auto stage_dsp    = algdout->appendStage("DSP");
      auto stage_amp = algdout->appendStage("AMP");
      stage_dsp->setNumIos(1, 2); // 1 in, 2 out
      stage_amp->setNumIos(2, 2); // 2 in, 2 out
      break;
    }
    case 3: { // KRZ3 (PCH->DSP2->DSP1->PANNER->AMP->STEREO)
      auto stage_dsp    = algdout->appendStage("DSP");
      auto stage_panner = algdout->appendStage("AMP");
      stage_dsp->setNumIos(1, 1); // 1 in, 1 out
      stage_panner->setNumIos(1, 2); // 1 in, 2 out
      break;
    }
    case 5: { // krz4
      // algmap[4]._ioMasks[0]._inputMask = 0;
      // algmap[5]._ioMasks[0]._inputMask = 0;
    }
    case 6: // krz6
      // algmap[6]._ioMasks[3]._inputMask = kmaskBOTH;
    case 23: // krz23
      // algmap[6]._ioMasks[2]._inputMask = kmaskBOTH;
    case 20: {
      // KRZ20
      //algmap[20]._ioMasks[0]._inputMask = 0;
      //algmap[20]._ioMasks[2]._inputMask = kmaskBOTH;
    }
    case 7: {
      // KRZ7
      //algmap[7]._ioMasks[3]            = IoMask(kmaskLOWER, kmaskLOWER);
      //algmap[7]._ioMasks[4]._inputMask = kmaskBOTH;
    }
    case 10: {
      // KRZ10
      //algmap[10]._ioMasks[2]            = IoMask(kmaskLOWER, kmaskLOWER);
      //algmap[10]._ioMasks[4]._inputMask = kmaskBOTH;
    }
    case 11: {
      // KRZ11
      //algmap[11]._ioMasks[1]._outputMask = kmaskBOTH;
      //algmap[11]._ioMasks[2]._outputMask = kmaskLOWER;
      //algmap[11]._ioMasks[4]._inputMask  = kmaskBOTH;
    }
    case 12: {
      // KRZ12
      //algmap[12]._ioMasks[1]._outputMask = kmaskBOTH;
      //algmap[12]._ioMasks[4]._inputMask  = kmaskBOTH;
    }
    case 24: {
      // KRZ24
      //algmap[24]._ioMasks[2]._inputMask  = kmaskBOTH;
      //algmap[24]._ioMasks[3]._outputMask = kmaskBOTH;
      //algmap[24]._ioMasks[4]._inputMask  = kmaskBOTH;
      //algmap[24]._ioMasks[4]._outputMask = kmaskBOTH;
    }
    case 26: {
      // KRZ26
      //algmap[26]._ioMasks[3]._outputMask = kmaskBOTH;
      //algmap[26]._ioMasks[4]._outputMask = kmaskBOTH;
      //algmap[26]._ioMasks[4]._inputMask  = kmaskBOTH;
    }
    case 15: { 
      //algmap[15]._ioMasks[1]._inputMask = kmaskBOTH;
      auto stage_dsp    = algdout->appendStage("DSP");
      auto stage_panner = algdout->appendStage("AMP");
      stage_dsp->setNumIos(1, 1); // 1 in, 1 out
      stage_panner->setNumIos(1, 2); // 1 in, 2 out
      break;
    }
    case 14: { 
      // KRZ14
      //algmap[14]._ioMasks[2]._inputMask = kmaskLOWER;
      auto stage_dsp    = algdout->appendStage("DSP");
      auto stage_panner = algdout->appendStage("AMP");
      stage_dsp->setNumIos(1, 1); // 1 in, 1 out
      stage_panner->setNumIos(1, 2); // 1 in, 2 out
      break;
    }
    case 9: { 
      //algmap[9]._ioMasks[0]._inputMask = 0;
      auto stage_dsp    = algdout->appendStage("DSP");
      auto stage_panner = algdout->appendStage("AMP");
      stage_dsp->setNumIos(1, 1); // 1 in, 1 out
      stage_panner->setNumIos(1, 2); // 1 in, 2 out
      break;
    }
    case 22: { 
      //algmap[22]._ioMasks[2]._outputMask = kmaskLOWER;
      auto stage_dsp    = algdout->appendStage("DSP");
      auto stage_panner = algdout->appendStage("AMP");
      stage_dsp->setNumIos(1, 1); // 1 in, 1 out
      stage_panner->setNumIos(1, 2); // 1 in, 2 out
      break;
    }
    default:
      auto stage_dsp    = algdout->appendStage("DSP");
      auto stage_panner = algdout->appendStage("AMP");
      stage_dsp->setNumIos(1, 1); // 1 in, 1 out
      stage_panner->setNumIos(1, 2); // 1 in, 2 out
      break;
  }
  return algdout;
}

} // namespace ork::audio::singularity
