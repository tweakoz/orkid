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
////////////////////////////////////////////////////////////////////////////////

InstrumentZone::InstrumentZone()
    : miBaseModulator( 0 )
    , mbGlobalZone( false )
    , num_generators( 0 )
    , base_generator( 0 )
    , sampleID( 0 )
    , presetnum( 0xffffffff )
    , _loopModeOverride( -1 )
    , index( 0xffffffff )
    , is_zone_used( true )
    , strip_order( 0 )
    , ipan( 0 )
{
}

////////////////////////////////////////////////////////////////////////////////

void InstrumentZone::ApplyGenerator( ESF2Generators egen, S16 GenVal )
{
    const float kfrqbase = 8.1757989156f;

    switch( egen )
    {
        ////////////////////////////////////////////////////
        case ESF2GEN_KEYRANGE:
        {   keymax = ( (GenVal&0xff00)>>8 );
            keymin = ( (GenVal&0x00ff) );
            break;
        }
        case ESF2GEN_VELRANGE:
        {   velmax = ((GenVal&0xff00)>>8);
            velmin = (GenVal&0x00ff);
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_OVERRIDEROOTKEY: //    overriding ROOT key
        {
            samplerootoverride = ( (u8) GenVal );
            break;
        }
        case ESF2GEN_TUNE_SEMIS: // coarse tune (semitones)
        {   coarsetune = (GenVal);
            break;
        }
        case ESF2GEN_TUNE_CENTS://  fine tune (cents)
        {
            finetune = (GenVal);
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_SAMPLE_LOOPMODE: //    loopsample
        {   _loopModeOverride = GenVal;
            //assert( IsGlobalZone() == false );
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_BASE_ATTENUATION: //   initial Attenuation (centibels)
        {
            float fatten = (float(GenVal)/2.6f);
            atten = (short(fatten));
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_ZONE_PAN: //   Zone Pan
        {   ipan = GenVal;
            pan = ( float(GenVal) * 0.1f );
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_BASE_FILTER_CUTOFF:
        {
            float frqratio = cents_to_linear_freq_ratio(float(GenVal));
            float freq = kfrqbase*frqratio;
            filterBaseCutoff = (freq);
            break;
        }
        case ESF2GEN_BASE_FILTER_Q:
        {
            //float flevel = centibel_to_linear_amp_ratio(float(GenVal));
            filterBaseQ = ( float(GenVal) );
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_MODLFO_DELAY:
        {
            float ftime = timecent_to_linear_time(GenVal);
            modLfoDelay = (ftime);
            break;
        }
        case ESF2GEN_MODLFO_FREQ:
        {
            float frqratio = cents_to_linear_freq_ratio(float(GenVal));
            float freq = kfrqbase*frqratio;
            modLfoFreq = (freq);
            break;
        }
        case ESF2GEN_MODLFO_TO_PITCH:
        {
            float fval = float(GenVal);
            modLfoToPitch = (fval);
            break;
        }
        case ESF2GEN_MODLFO_TO_FC:
        {
            float fval = float(GenVal);
            modLfoToCutoff = (fval);
            break;
        }
        case ESF2GEN_MODLFO_TO_AMP:
        {
            float fval = float(GenVal);
            modLfoToAmp = (fval);
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_VIBLFO_DELAY:
        {
            float ftime = timecent_to_linear_time(GenVal);
            vibLfoDelay = (ftime);
            break;
        }
        case ESF2GEN_VIBLFO_FREQ:
        {
            float frqratio = cents_to_linear_freq_ratio(float(GenVal));
            float freq = kfrqbase*frqratio;
            vibLfoFreq = (freq);
            break;
        }
        case ESF2GEN_VIBLFO_TO_PITCH:
        {
            float fval = float(GenVal);
            vibLfoToPitch =(fval);
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_AMPENV_DELAY: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            ampEnvDelay = (ftime);
            break;
        }
        case ESF2GEN_AMPENV_ATTACK: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            ampEnvAttack = (ftime);
            break;
        }
        case ESF2GEN_AMPENV_HOLD: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            ampEnvHold = (ftime);
            break;
        }
        case ESF2GEN_AMPENV_DECAY: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            ampEnvDecay = (ftime);
            break;
        }
        case ESF2GEN_AMPENV_SUSTAIN: // (centibels)
        {
            float flevel = centibel_to_linear_amp_ratio(-float(GenVal));
            ampEnvSustain = (flevel);
            break;
        }
        case ESF2GEN_AMPENV_RELEASE: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            ampEnvRelease = (ftime);
            break;
        }
        case ESF2GEN_AMPENV_KEYNUMTOHOLD:
        {
            float fval = float(GenVal);
            ampEnvKeyTrackHold = (fval);
            break;
        }
        case ESF2GEN_AMPENV_KEYNUMTODECAY:
        {
            float fval = float(GenVal);
            ampEnvKeyTrackDecay = (fval);
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_MODENV_DELAY: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            modEnvDelay = (ftime);
            break;
        }
        case ESF2GEN_MODENV_ATTACK: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            modEnvAttack = (ftime);
            break;
        }
        case ESF2GEN_MODENV_HOLD: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            modEnvHold = (ftime);
            break;
        }
        case ESF2GEN_MODENV_DECAY: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            modEnvDecay = (ftime);
            break;
        }
        case ESF2GEN_MODENV_SUSTAIN: // (-.1% units, 0==100%)
        {   int ival = GenVal;
            float flevel = 1.0f - float(ival)*0.001f;
            modEnvSustain = (flevel);
            break;
        }
        case ESF2GEN_MODENV_RELEASE: // (timecents)
        {
            float ftime = timecent_to_linear_time(GenVal);
            modEnvRelease = (ftime);
            break;
        }
        case ESF2GEN_MODENV_KEYNUMTOHOLD:
        {
            float fval = float(GenVal);
            modEnvKeyTrackHold = (fval);
            break;
        }
        case ESF2GEN_MODENV_KEYNUMTODECAY:
        {
            float fval = float(GenVal);
            modEnvKeyTrackDecay = (fval);
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_MUTEX_GROUP:
        {   mutexgroup = ( GenVal );
            break;
        }
        ////////////////////////////////////////////////////
        case ESF2GEN_SAMPLE_ID: //  sampleID (terminal generator)
        {   sampleID = GenVal;
            break;
        }
        ////////////////////////////////////////////////////
    }
}

////////////////////////////////////////////////////////////////////////////////

void SoundFont::ProcessInstruments( void )
{
    numinst = (int) mPXMInstruments.size();
    numizones = (int) mPXMInstrumentZones.size();
    numigen = (int) mPXMInstrumentGen.size();
    numsamples = (int) mPXMSamples.size();

    //////////////////////////////////////////////////
    // scan Instrument Zone Generators

    for( int j=1; j<=numizones; j++ )
    {   int i = j-1;

        InstrumentZone *izone = mPXMInstrumentZones[i];
        //InstrumentZone *izone2 = mPXMInstrumentZones[j];

        int iBASEGEN = izone->base_generator;
        int iLASTGEN = ( j<=(numizones-1) ) ? mPXMInstrumentZones[j]->base_generator : numigen;

        izone->num_generators = (iLASTGEN-iBASEGEN);

        izone->index = i;
        izone->sampleID = 0xffff;

        int isize = (int) mPXMInstrumentGen.size();

        for( int g=0; g<izone->num_generators; g++ )
        {
            int iGenINDEX = iBASEGEN + g;

            SSoundFontGenerator *igen = mPXMInstrumentGen[iGenINDEX];

            ESF2Generators egen = ESF2Generators(igen->muGeneratorID);

            std::map<ESF2Generators,S16>::const_iterator it=izone->mGenerators.find(egen);
            assert(it==izone->mGenerators.end());

            izone->mGenerators.insert(std::make_pair(egen,igen->miGeneratorValue));

            ///////////////////////////////////////////
            // is keyrange first generator ?
            ///////////////////////////////////////////

            if( egen == ESF2GEN_KEYRANGE )
            {
                //assert(g==0);
            }

            ///////////////////////////////////////////
            // is sampleid last generator ?
            ///////////////////////////////////////////

            if( egen == ESF2GEN_SAMPLE_ID )
            {
                assert(g==(izone->num_generators-1));
            }

            ///////////////////////////////////////////
            // is this a global zone ?
            ///////////////////////////////////////////

            if( g==izone->num_generators-1 )
            {
                if( egen != ESF2GEN_SAMPLE_ID )
                {
                    izone->SetGlobalZone();
                }
            }

            ///////////////////////////////////////////
        }

        for( auto it : izone->mGenerators )
        {
            ESF2Generators egen = it.first;
            S16 GenVal = it.second;
            izone->ApplyGenerator( egen, GenVal );
        }
    }

    //////////////////////////////////////////////////
    // setup Instruments

    for( int j=1; j<=numinst; j++ )
    {
        int i=j-1;

        SF2Instrument *inst = mPXMInstruments[i];

        int iz1 = (int) inst->izone_base;
        int iz2 = (j<=(numinst-1)) ? (int) mPXMInstruments[j]->izone_base : (numizones);

        inst->num_izones = (iz2-iz1);

        /////////////////////////////////////////////////////////
        // find wacky SF2 "Global Split" Data
        /////////////////////////////////////////////////////////

        InstrumentZone* pGlobalIZone = 0;
        bool bHasGlobalZone = false;

        for( size_t iz=0; iz<inst->num_izones; iz++ )
        {
            int izone_index = int(inst->izone_base) + iz ;

            InstrumentZone *pZ = mPXMInstrumentZones[ izone_index ];

            if( pZ->IsGlobalZone() )
            {
                assert( bHasGlobalZone==false );
                bHasGlobalZone = true;
                pGlobalIZone = pZ;
            }
        }

        /////////////////////////////////////////////////////////
        // Distrubute wacky SF2 "Global Split" Data
        /////////////////////////////////////////////////////////

        for( size_t iz=0; iz<inst->num_izones; iz++ )
        {
            InstrumentZone *pZ = mPXMInstrumentZones[ inst->izone_base + iz ];

            if( bHasGlobalZone )
            {
                if( pZ != pGlobalIZone )
                {
                    for( auto it : pGlobalIZone->mGenerators )
                    {
                        ESF2Generators egen = it.first;
                        S16 GenVal = it.second;

                        auto itf = pZ->mGenerators.find(egen);

                        if( itf==pZ->mGenerators.end() )
                        {
                            pZ->ApplyGenerator( egen, GenVal );
                            pZ->mGenerators.insert( std::make_pair( egen, GenVal ) );
                        }
                    }

                    inst->mIZones.push_back( *pZ );
                }
            }
            else
            {
                inst->mIZones.push_back( *pZ );
            }
        }

        /////////////////////////////////////////////////////////

        //printf( "inst %d [%s]\n", i, inst->GetName().c_str() );
    }

    //////////////////////////////////////////////////
    // Finish Up Samples

    U32 samplen = 0;

    int imaxsamp = (int) mPXMSamples.size();

    //printf( "NumSamples %d : %d\n", numsamples, imaxsamp );

    for( int i=0; i<numsamples; i++ )
    {
        auto smp = mPXMSamples[ i ];

        //printf( "Sample %d %08x %s [start %d] [end %d] [loopstart %d] [loopend %d]\n", i, smp, smp->name.c_str(), smp->start, smp->end, smp->loopstart, smp->loopend );

        assert(smp->start < _sampleDataNumSamples);
        assert(smp->end <= _sampleDataNumSamples);
        assert(smp->loopstart < _sampleDataNumSamples);
        assert(smp->loopend <= _sampleDataNumSamples);

        ///////////////////////////////////


    }

    //////////////////////////////////////////////////
}

void SoundFont::ProcessPresets( void )
{
    int inumpresets = (int) mPXMPrograms.size();
    int inumprez = (int) this->mPXMProgramZones.size();
    int inumpgen = (int) this->mPXMPresetGen.size();

    //printf( "post_process_pzones()\n" );

    ////////////////////////

    for( int j=1; j<=inumprez; j++ )
    {
        int i = j-1;

        SF2ProgramZone *pre = mPXMProgramZones[i];

        int pg1 = pre->base_generator;
        int pg2 = ( j<=inumprez-1 ) ? mPXMProgramZones[j]->base_generator : inumpgen;

        pre->num_generators = (pg2-pg1);
        pre->instrumentID = -1;

        //printf( "//////////////////////////////\n" );
        //printf( "// pzone %d numpregens %d genbase %d pg1 %d pg2 %d\n", i, pre->num_generators, pre->base_generator, pg1, pg2 );

        for( int g=0; g<pre->num_generators; g++ )
        {
            int gnum = g+pre->base_generator;
            SSoundFontGenerator *pgen = mPXMPresetGen[gnum];

            if( pgen->muGeneratorID == 41 ) // instrument
            {
                pre->instrumentID = pgen->miGeneratorValue;
            }
            else if( pgen->muGeneratorID == 43 ) // key range
            {
                pre->key_max = (pgen->miGeneratorValue&0xff00)>>8;
                pre->key_min = (pgen->miGeneratorValue&0x00ff);
            }
            else if( pgen->muGeneratorID == 44 ) // vel range
            {
                pre->vel_max = (pgen->miGeneratorValue&0xff00)>>8;
                pre->vel_min = (pgen->miGeneratorValue&0x00ff);

                if( (pre->vel_min == 0) && (pre->vel_max == 0) ) // 00 .. 00 really means 00 .. 7f ?
                {
                    pre->vel_max = 0x7f;
                }

            }
            else
            {
                //printf( "pgen: %d val: %d\n", pgen->muGeneratorID, pgen->miGeneratorValue );
            }
        }

        //printf( "pzone: %d instrument %d keymin: %d keymax: %d\n", i, pre->instrumentID, pre->key_min, pre->key_max );
    }
    ////////////////////////

    //printf( "post_process_presets()\n" );

    for( int j=1; j<=inumpresets; j++ )
    {
        int i=j-1;

        SF2Program *preset = mPXMPrograms[i];
        U32 pr1 = preset->pbag_base;
        U32 pr2 = ( j<=inumpresets-1 ) ? mPXMPrograms[j]->pbag_base : numprebags;

        preset->num_pbags = (pr2-pr1);

        //printf( "//////////////////////////\n" );
        //printf( "// program: %03d <%s> pbag_base %d num_pbags %d pr1 %d pr2 %d mapped %d\n", i, preset->GetName().c_str(), preset->pbag_base, preset->num_pbags, pr1, pr2, preset->mapped_preset );

//      SF2Program *mapped = mPXMPrograms[ preset->mapped_preset ];

        for( int ipbag=0; ipbag<preset->num_pbags; ipbag++ )
        {
            int zoneidx = preset->pbag_base + ipbag;

            SF2ProgramZone *pzone = mPXMProgramZones[zoneidx];

            if( pzone->instrumentID>=0 )
            {
                SF2Instrument *inst = mPXMInstruments[ pzone->instrumentID ];
                preset->AddZone( *pzone );
                //printf( "// [zone %d of %d] [zoneID %d] [name %s]\n", ipbag, preset->num_pbags, pzone->instrumentID, inst->GetName().c_str() );
            }
        }
    }

}


////////////////////////////////////////////////////////////////////////////////
} //namespace ork { namespace sf2 {
