#pragma once

#include "krzdata.h"
//#include "modulation.h"

namespace ork::audio::singularity {

struct outputBuffer;
struct layer;
struct DspBlock;
struct DspBuffer;
struct DspKeyOnInfo;

///////////////////////////////////////////////////////////////////////////////

struct Alg final {
  Alg(const AlgData& algd);
  virtual ~Alg();

  void keyOn(DspKeyOnInfo& koi);
  void keyOff();

  void compute(synth& syn, outputBuffer& obuf);

  virtual void doKeyOn(DspKeyOnInfo& koi);
  void intoDspBuf(const outputBuffer& obuf);
  void intoOutBuf(outputBuffer& obuf, int inumo);
  dspblk_ptr_t lastBlock() const;

  dspblk_ptr_t _block[kmaxdspblocksperlayer];
  AlgConfig _algConfig;

  layer* _layer;
};

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
