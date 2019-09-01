///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/math/audiomath.h>
#include "sf2.h"
#include <ork/kernel/string/string.h>

using namespace ork::audiomath;

////////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity::sf2 {

CSF2Sample::CSF2Sample( Ssfontsample* smp )
    : start( smp ? smp->dwStart : 0 )
    , end( smp ? smp->dwEnd : 0 )
    , loopstart( smp ? smp->dwStartloop : 0 )
    , loopend( smp ? smp->dwEndloop : 0 )
    , samplerate( smp ? smp->dwSampleRate : 0 )
    , originalpitch( smp ? smp->byOriginalPitch : 0 )
    , pitchcorrection( smp ? smp->chPitchCorrection : 0 )
    , samplelink( smp ? smp->wSampleLink : 0 )
    , sampletype( smp ? smp->sfSampleType : 0 )
{
    char namebuf[21];

    for( U32 i=0; i<20; i++ )
        namebuf[i] = smp
                   ? smp->achSampleName[i]
                   : 0;

    namebuf[20] = 0;

    name = (std::string) namebuf;
}


} // namespace ork::audio::singularity::sf2 {
