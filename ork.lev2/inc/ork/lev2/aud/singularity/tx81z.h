#pragma once
#include <ork/kernel/svariant.h>
#include "synthdata.h"
#include "dspblocks.h"
#include "fmosc.h"
#include "dsp_pmx.h"
#include <ork/file/path.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void configureTx81zAlgorithm(lyrdata_ptr_t layerdata, pm4prgdata_ptr_t fmdata);

///////////////////////////////////////////////////////////////////////////////

struct Tx81zData : public SynthData {
  Tx81zData();
  ~Tx81zData();
  void loadBank(const file::Path& syxpath);

  int _lastprg;
};

} // namespace ork::audio::singularity
