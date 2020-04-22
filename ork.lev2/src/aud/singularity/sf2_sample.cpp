///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/math/audiomath.h>
#include <ork/lev2/aud/singularity/sf2.h>
#include <ork/kernel/string/string.h>

using namespace ork::audiomath;

////////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity::sf2 {

SF2Sample::SF2Sample(sfontsample* smp)
    : start(smp ? smp->dwStart : 0)
    , end(smp ? smp->dwEnd : 0)
    , loopstart(smp ? smp->dwStartloop : 0)
    , loopend(smp ? smp->dwEndloop : 0)
    , samplerate(smp ? smp->dwSampleRate : 0)
    , originalpitch(smp ? smp->byOriginalPitch : 0)
    , pitchcorrection(smp ? smp->chPitchCorrection : 0)
    , samplelink(smp ? smp->wSampleLink : 0)
    , sampletype(smp ? smp->sfSampleType : 0) {
  char namebuf[21];

  for (U32 i = 0; i < 20; i++)
    namebuf[i] = smp ? smp->achSampleName[i] : 0;

  namebuf[20] = 0;

  name = (std::string)namebuf;
}

} // namespace ork::audio::singularity::sf2
