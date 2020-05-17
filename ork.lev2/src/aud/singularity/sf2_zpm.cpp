///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/math/audiomath.h>
#include <ork/lev2/aud/singularity/sf2.h>
#include <ork/lev2/aud/singularity/krzobjects.h>
#include <ork/kernel/string/string.h>

using namespace ork::audiomath;

////////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity::sf2 {

///////////////////////////////////////////////////////////////////////////////

Sf2TestSynthData::Sf2TestSynthData(const file::Path& filename, const std::string& bankname)
    : SynthData() {
  _staticBankName = bankname;

  auto sfpath  = basePath() / "soundfonts" / filename;
  auto abspath = sfpath.ToAbsolute();
  _sfont       = new SoundFont(abspath.c_str(), bankname);
}
Sf2TestSynthData::~Sf2TestSynthData() {
  delete _sfont;
}

const ProgramData* Sf2TestSynthData::getProgram(int progID) const {
  auto ObjDB = _sfont->_zpmDB;
  return ObjDB->findProgram(progID);
}

///////////////////////////////////////////////////////////////////////////////

// SF2Sample<11:piano060v125> opitch<60> sta<9674166> end<10272671> isblklen<59189498>
//_sample<piano060v125>
//_sample blkstart<9674166>
//_sample blkend<10272671>
//_blk_start<634006142976> _blk_end<673229766656>
// iiA<-2147483648> iiB<-2147483648> sblk<0x126aeb000>

void SoundFont::genZpmDB() {
#if 0

   _zpmDB = new VastObjectsDB;

   //////////////////////
   // sf2 samples -> zpm samples
   //////////////////////

   std::map<int,sample*> zpmsamples;

   for( int i=0; i<numsamples; i++ )
   {
        auto s = GetSample(i);

        auto ks = new sample;
        ks->_name = s->name;
        ks->_sampleBlock = _chunkOfSampleData;
        ks->_sampleRate = s->samplerate;
        ks->_rootKey = s->originalpitch;

        ks->_blk_start = s->start;
        ks->_blk_end = s->end-1;
        ks->_blk_loopstart = s->loopstart;
        ks->_blk_loopend = s->loopend-1;
        ks->_blk_alt = s->loopend-1;

        ks->_loopMode = eLoopMode::FROMKM;

        int RKcents = (s->originalpitch)*100;
        float SRratio = 96000.0f/ks->_sampleRate;
        int pitchadjx = s->pitchcorrection;
        int frqerc = linear_freq_ratio_to_cents(SRratio);
        //pitchadjx = frqerc-delcents
        //pitchadjx+delcents = frqerc
        int delcents = frqerc-pitchadjx;
        int highestP = delcents+RKcents; // cents
        ks->_highestPitch = highestP;
        //delcents = highestP-RKcents
        //highestP = delcents+RKcents
        //_baseCents = _kmcents+pitchadjx-1200;

#if 0
        {
            int highestP = sample->_highestPitch;
            float SRratio = 96000.0f/sampsr;
            int RKcents = (_sampleRoot)*100;
            int delcents = highestP-RKcents;
            int frqerc = linear_freq_ratio_to_cents(SRratio);
            int pitchadjx = (frqerc-delcents); //+1200;
            int pitchadj = sample->_pitchAdjust; //+1200;
            _curpitchadj = pitchadj;
            _curpitchadjx = pitchadjx;

            _baseCents = _kmcents+/*_pchcents*/+pitchadjx-1200;
            //_basecentsOSC = 6000;//(note-0)*100;//pitchadjx-1200;
            if(pitchadj)
            {
                _baseCents = _kmcents+/*_pchcents*/+pitchadj;
                //_basecentsOSC = _pchcents+pitchadj;
            }
            _curcents = _baseCents;

        }
#endif

        int isblklen = _sampleDataNumSamples;
        if(false)//i==147)
        printf( "SF2Sample<%d:%s> sr<%d> opitch<%d> pcorr<%d> sta<%d> end<%d> lpst<%d> lpend<%d> isblklen<%d>\n",
                i,
                s->name.c_str(),
                s->samplerate,
                s->originalpitch,
                s->pitchcorrection,
                s->start,
                s->end,
                s->loopstart,
                s->loopend,
                isblklen);
        assert(s->start<=_sampleDataNumSamples);
        assert(s->end<=_sampleDataNumSamples);

        zpmsamples[i] = ks;
   }

   //////////////////////
   // sf2 instruments -> zpm multisamples
   // sf2 instruments -> zpm keymaps
   //////////////////////

   //std::map<int,multisample*> zpm_multisamples;
   //std::map<int,KeyMap*> zpm_keymaps;

   for( int i=0; i<numinst; i++ )
   {
      auto inst = GetInstrument(i);
      int inumz = inst->num_izones-1;

      auto zpm_ms = new multisample;
      _zpmDB->_multisamples[i] = zpm_ms;
      zpm_ms->_name = inst->mName;
      zpm_ms->_objid = i;

      auto zpm_km = new KeyMap;
      zpm_km->_name = inst->mName;
      zpm_km->_kmID = i;
      _zpmDB->_keymaps[i] = zpm_km;

     // printf( "sf2inst<%d:%s> inumz<%d>\n", i, inst->mName.c_str(), inumz);
      for( int j=0; j<inumz; j++)
      {
        auto& zone = inst->GetIZoneFromIndex(j);
        auto itsamp = zpmsamples.find(zone.sampleID);
        assert( itsamp != zpmsamples.end() );

        int coarsetune = zone.coarsetune;
        int finetune = zone.finetune;
        int atten = zone.atten;
        int srootOV = zone.samplerootoverride;
        int kmin = zone.keymin;
        int kmax = zone.keymax;

        auto zpm_s = itsamp->second;
        //printf( "   zone<%d> kmin<%d> kmax<%d> samp<%d> opitch<%d> sr<%0.2f>\n", j, zone.keymin, zone.keymax, zone.sampleID, zpm_s->_rootKey, zpm_s->_sampleRate );
        zpm_ms->_samples[j] = zpm_s;

        auto kmr = new kmregion;
        kmr->_lokey = kmin;
        kmr->_hikey = kmax;
        kmr->_lovel = 0;
        kmr->_hivel = 127;
        kmr->_tuning = -(srootOV-zpm_s->_rootKey-12)*100;
        kmr->_volAdj = 1.0f;
        kmr->_linGain = 1.0f;
        kmr->_multiSample = zpm_ms;
        kmr->_sample = zpm_s;
        kmr->_sampleName = zpm_s->_name;
        kmr->_multsampID = i;
        kmr->_sampID = j;

     /*     The wavetable oscillator is playing
            a digital sample which is described in terms of a start point,
            end point, and two points describing a loop. The sound can be
            flagged as unlooped, in which case the loop points are
            ignored. If the sound is looped, it can be played in two ways.
            If it is flagged as “loop during release”, the sound is played
            from the start point through the loop, and loops until the
            note becomes inaudible. If not, the sound is played from the
            start point through the loop, and loops until the key is
            released. At this point, the next time the loop end point is
            reached, the sound continues through the loop end point and
            plays until the end point is reached, at which time audio is
            terminated.     */


        switch( zone._loopModeOverride )
        {
            case 0: // no loop
            case 2: // reserved - no loop
                kmr->_loopModeOverride = eLoopMode::NONE;
                break;
            case 1: // loops continuously
                kmr->_loopModeOverride = eLoopMode::FWD;
                break;
            case 3: // loop, then release
                kmr->_loopModeOverride = eLoopMode::FWD;
                break;
            case -1:
                kmr->_loopModeOverride = eLoopMode::NOTSET;
                break;
            default:
                printf( "lmov<%d>\n", int(zone._loopModeOverride) );
                assert(false);
                break;
        }
        if( false ) //i==95 )
        {
            printf("/////////////////////////\n");
            printf( "zone<%d> sid<%d> sample<%s>\n",j,zone.sampleID, zpm_s->_name.c_str() );
            printf( "lkey<%d> hkey<%d>\n", kmin, kmax );
            printf( "srootOV<%d> sov<%d>\n", j, srootOV, zpm_s->_rootKey );
            printf( "loopOV<%d> z.coarsetune<%d> z.finetune<%d>\n", j, int(zone._loopModeOverride), coarsetune, finetune );
        }
        zpm_km->_regions.push_back(kmr);
      }
   }


   //////////////////////
   // generate programs

   for( auto it : _zpmDB->_keymaps )
   {
        auto id = it.first;
        auto km = it.second;
        auto prg = new ProgramData;
        _zpmDB->_programs[id] = prg;
        prg->_name = km->_name;
        prg->_role = _bankName;
        auto ld = prg->newLayer();
        ld->_keymap = km;
        auto CB0 = new ControlBlockData;
        ld->_ctrlBlocks[0] = CB0;
        auto AE = new RateLevelEnvData;

        CB0->_cdata[0] = AE;
        ld->_algData->_krzAlgIndex = 1;
        ld->_algData->_name = "ALG1";
        ld->_kmpBlock._keymap = km;
        ld->_fBlock[0]._dspBlock = "SAMPLER";
        ld->_fBlock[0]._paramScheme = "PCH";
        ld->_fBlock[4]._dspBlock = "AMP";
        ld->_fBlock[4]._paramScheme = "AMP";
        AE->_name = "AMPENV";
        AE->_ampenv = true;
        AE->_segments.push_back({0,1}); // atk1
        AE->_segments.push_back({0,0}); // atk2
        AE->_segments.push_back({0,0}); // atk3
        AE->_segments.push_back({0,0}); // dec
        AE->_segments.push_back({3,0}); // rel1
        AE->_segments.push_back({0,0}); // rel2
        AE->_segments.push_back({0,0}); // rel3
   }

   //exit(0);

   //////////////////////
#endif
}

////////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity::sf2
