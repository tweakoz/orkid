////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once
#include <ork/kernel/svariant.h>
#include "synthdata.h"
#include "dspblocks.h"
#include "fmosc.h"
#include "dsp_pmx.h"
#include <ork/file/path.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void configureTx81zAlgorithm(lyrdata_ptr_t layerdata, tx81zprgdata_ptr_t fmdata);

struct Tx81zProgData {
  std::string _name;
  int _alg            = 0;
  bool _lfoSync       = false;
  int _lfoSpeed       = 0;
  int _lfoDepth       = 0;
  int _pchDepth       = 0;
  int _ampDepth       = 0;
  int _lfoWave        = 0;
  int _ampSensa       = 0;
  int _pchSensa       = 0;
  int _pitchBendRange = 0;
  bool _mono          = false;
  bool _portMode      = false;
  int _portRate       = 0;
  PmOscData _ops[4];
};

///////////////////////////////////////////////////////////////////////////////

struct Tx81zData : public SynthData {
  Tx81zData();
  ~Tx81zData();
  void loadBank(const file::Path& syxpath);

  int _lastprg;
};

} // namespace ork::audio::singularity
