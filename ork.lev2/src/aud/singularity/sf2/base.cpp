////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/file/file.h>
#include <ork/math/audiomath.h>
#include <ork/lev2/aud/singularity/sf2.h>
#include <ork/kernel/string/string.h>

////////////////////////////////////////////////////////////////////////////////

namespace ork::audio::singularity::sf2 {

///////////////////////////////////////////////////////////////////////////////

SF2Program::SF2Program() {
  preset        = 0;
  bank          = 0;
  pbag_base     = 0;
  library       = 0;
  genre         = 0;
  morphology    = 0;
  num_pbags     = 0;
  mapped_preset = 0xffffffff;
  //	create_control_callback = create_pxmpresetov;

  std::string name = "preset";

  // add_attributeSTRING( 0, name, ATTR_RW, ATTR_SHOW, "Preset" );
}

////////////////////////////////////////////////////////////////////////////////

SoundFont::SoundFont(const std::string& SoundFontName, const std::string& bankname)
    : _bankName(bankname)
    , numinst(0)
    , numizones(0)
    , numigen(0)
    , numsamples(0)
    , _chunkOfSampleData(nullptr)
    , _sampleDataNumSamples(0) 
    , mSoundFontName(SoundFontName) {
  std::string filename = SoundFontName;

  RIFFFile RiffFile;
  RiffFile.OpenFile(filename);
  RiffFile.LoadChunks();

  GetSBFK(RiffFile.GetChunk("ROOT"));
  Process();

  genZpmDB();
}

////////////////////////////////////////////////////////////////////////////////

SoundFont::~SoundFont() {
  for (auto item : mPXMPrograms)
    delete item;
  for (auto item : mPXMProgramZones)
    delete item;
  for (auto item : mPXMInstruments)
    delete item;
  for (auto item : mPXMInstrumentZones)
    delete item;
  for (auto item : mPXMSamples)
    delete item;
  for (auto item : mDynamicChunks)
    delete item;
}

////////////////////////////////////////////////////////////////////////////////

void SoundFont::AddProgram(sfontpreset* preset) {
  auto cpre = new SF2Program;

  cpre->bank          = preset->wBank;
  cpre->genre         = preset->dwGenre;
  cpre->library       = preset->dwLibrary;
  cpre->morphology    = preset->dwMorphology;
  cpre->pbag_base     = preset->wPresetBagNdx;
  cpre->preset        = preset->wPreset;
  cpre->mapped_preset = cpre->preset;

  cpre->SetName(preset->GetName());

  // printf( "pbase: %d preset %d mapped %d name %s\n", cpre->pbag_base, cpre->preset, cpre->mapped_preset, cpre->GetName().c_str()
  // );

  mPXMPrograms.push_back(cpre);
}

////////////////////////////////////////////////////////////////////////////////

void SoundFont::AddSample(sfontsample* sample) {
  SF2Sample* pxsample = new SF2Sample(sample);
  mPXMSamples.push_back(pxsample);
}

////////////////////////////////////////////////////////////////////////////////

void SoundFont::AddPresetGen(SoundFontGenerator* pgn) {
  mPXMPresetGen.push_back(pgn);
}

////////////////////////////////////////////////////////////////////////////////

void SoundFont::AddInstrumentZone(sfontinstbag* ibg) {
  auto pxmi            = new InstrumentZone;
  pxmi->base_generator = ibg->wInstGenNdx;
  pxmi->SetBaseModulator(ibg->wInstModNdx);
  mPXMInstrumentZones.push_back(pxmi);
}

////////////////////////////////////////////////////////////////////////////////

void SoundFont::AddInstrument(sfontinst* inst) {
  auto pxmi        = new SF2Instrument;
  pxmi->izone_base = inst->wInstBagNdx;
  // strncpy( (char *) & pxmi->name[0], (char *) & inst->achInstName[0], 20 );
  pxmi->SetName(inst->GetName());
  pxmi->SetIndex(mPXMInstruments.size());
  mPXMInstruments.push_back(pxmi);
}

////////////////////////////////////////////////////////////////////////////////

void SoundFont::AddInstrumentGen(SoundFontGenerator* igen) {
  mPXMInstrumentGen.push_back(igen);
}

////////////////////////////////////////////////////////////////////////////////

void SoundFont::AddProgramZone(sfontprebag* pbg) {
  auto pxmp            = new SF2ProgramZone;
  pxmp->base_generator = pbg->wInstGenNdx;
  mPXMProgramZones.push_back(pxmp);
}

} // namespace ork::audio::singularity::sf2
