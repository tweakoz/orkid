#include "krzio.h"
#include <audiofile.h>

// extern gig::File dls_file;

///////////////////////////////////////////////////////////////////////////////

int GenSampleKey(int iobjid, int isubi) {
  int idlsk = (iobjid << 16) | isubi;
  return idlsk;
}

///////////////////////////////////////////////////////////////////////////////

Krz::Krz()
    : miFileHeaderAndPRAMLength(0)
    , mpSampleData(0)
    , miSampleDataLength(0) {
}

///////////////////////////////////////////////////////////////////////////////

float log_base(float base, float inp) {
  float rval = log(inp) / log(base);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

RegionInst::RegionInst()
    : miLoKey(0)
    , miHiKey(0)
    , miLoVel(0)
    , miHiVel(0) {
}
RegionInst::RegionInst(const RegionInst& oth)
    : miLoKey(oth.miLoKey)
    , miHiKey(oth.miHiKey)
    , miLoVel(oth.miLoVel)
    , miHiVel(oth.miHiVel) {
  mData.miSampleId    = oth.mData.miSampleId;
  mData.miSubSample   = oth.mData.miSubSample;
  mData.miTuning      = oth.mData.miTuning;
  mData.mVolumeAdjust = oth.mData.mVolumeAdjust;
}

bool RegionInst::operator<(const RegionInst& rhs) const {
  return (miLoKey < rhs.miLoKey) ? true : ((miLoVel < rhs.miLoVel) ? true : false);
}

///////////////////////////////////////////////////////////////////////////////

Keymap::Keymap()
    : miKeymapID(0)
    , miKeymapSampleId(0)
    , miKeymapBasePitch(0)
    , miKeymapCentsPerEntry(0)
    , miKrzMethod(0)
    , miKrzNumEntries(0)
    , miKrzEntrySize(0) {
  memset(&mRgnPtrMap[0], 0, sizeof(RegionData*) * kNKEYS * kNVELS);
}

///////////////////////////////////////////////////////////////////////////////

SampleFile::SampleFile() {
  auto base      = ork::audio::singularity::basePath() / "kurzweil";
  auto rompath   = base / "k2v3internalsamplerom.bin";
  FILE* fsamprom = fopen(rompath.c_str(), "rb");
  mpSampleData   = new short[8 << 20];
  fread((void*)mpSampleData, 1, 8 << 20, fsamprom);
  fclose(fsamprom);
}

float compute_slopeDBPerSample(float dbpsec, float samplerate) {
  float rval = powf(powf(10.0f, (dbpsec / 20.0f)), 1.0f / samplerate);
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void SampleFile::WriteSample(const std::string& fname, const SampleOpts& opts) {
  int inumframes = (opts.end - opts.start);
  assert(opts.start >= 0);
  assert(opts.end >= 0);
  assert(opts.start < (8 << 20));
  assert(opts.end < (8 << 20));
  assert(inumframes >= 0);
  assert(inumframes < (8 << 20));

  bool looped = opts.loopstart >= 0;

  auto af_setup = afNewFileSetup();

  afInitFileFormat(af_setup, AF_FILE_AIFF);
  afInitChannels(af_setup, AF_DEFAULT_TRACK, 1);
  afInitSampleFormat(af_setup, AF_DEFAULT_TRACK, AF_SAMPFMT_TWOSCOMP, 16);

  ///////////////////////////////////

  int markerIDs[]                           = {1, 2, 3, 4};
  const int numMarkers                      = sizeof(markerIDs) / sizeof(int);
  const char* const markerNames[numMarkers] = {"sustain loop start", "sustain loop end", "release loop start", "release loop end"};
  int loopIDs[]                             = {1, 2};

  if (looped) {
    afInitMarkIDs(af_setup, AF_DEFAULT_TRACK, markerIDs, 2);
    for (int i = 0; i < 2; i++)
      afInitMarkName(af_setup, AF_DEFAULT_TRACK, markerIDs[i], markerNames[i]);
    afInitLoopIDs(af_setup, AF_DEFAULT_INST, loopIDs, 1);
  }

  ///////////////////////////////////

  auto af_file = afOpenFile(fname.c_str(), "w", af_setup);
  afFreeFileSetup(af_setup);

  // SndfileHandle file ;
  int channels = opts.inumchans;
  int srate    = opts.samplerate;

  // printf ("Creating file named '%s' nch<%d> len<%d>\n", fname.c_str(),channels,inumframes) ;

  // auto format = (SF_FORMAT_AIFF | SF_FORMAT_PCM_16);
  // file = SndfileHandle (fname, SFM_WRITE, format, channels, srate) ;

  auto buffer = (short*)(mpSampleData + opts.start);

  ///////////////
  const auto& slopevec = opts.natenvSlopes;
  const auto& stimevec = opts.natenvSegTimes;

  const int segment_count          = slopevec.size();
  int segment_index                = 0;
  int samplecounter                = 0;
  bool done                        = false;
  float curamp                     = 1.0f;
  float curslope                   = slopevec[segment_index];
  float curslopepersample          = compute_slopeDBPerSample(curslope, float(srate));
  int segment_time                 = stimevec[segment_index];
  int samples_remaining_in_segment = segment_time;

  /////////////////////////////////////////////
  // scale wave by natural envelope (to test natenv correctness)
  /////////////////////////////////////////////

  if (false)
    for (int i = 0; i < inumframes; i++) {
      short* ptr2val = buffer + i;
      float getval   = float(*ptr2val) * curamp;
      (*ptr2val)     = short(getval);
      curamp *= curslopepersample;

      samples_remaining_in_segment--;
      if (samples_remaining_in_segment <= 0) {
        segment_index++;
        if (segment_index >= (segment_count - 2)) {
          segment_index                = (segment_count - 2);
          samples_remaining_in_segment = 1 << 20;
        } else {

          samples_remaining_in_segment = stimevec[segment_index];
        }
        curslope = slopevec[segment_index];

        curslopepersample = compute_slopeDBPerSample(curslope, float(srate));
      }
    }

  auto af_written = afWriteFrames(af_file, AF_DEFAULT_TRACK, buffer, inumframes);

  if (looped) {
    printf("opts.loopstart<%d>\n", opts.loopstart);
    afSetMarkPosition(af_file, AF_DEFAULT_TRACK, markerIDs[0], opts.loopstart);
    afSetMarkPosition(af_file, AF_DEFAULT_TRACK, markerIDs[1], inumframes - 1);
    // afSetMarkPosition(af_file, AF_DEFAULT_TRACK, markerIDs[2], opts.loopstart);
    // afSetMarkPosition(af_file, AF_DEFAULT_TRACK, markerIDs[3], inumframes);

    afSetLoopStart(af_file, AF_DEFAULT_INST, 1, markerIDs[0]);
    afSetLoopEnd(af_file, AF_DEFAULT_INST, 1, markerIDs[1]);
    // afSetLoopStart(af_file, AF_DEFAULT_INST, 2, markerIDs[2]);
    // afSetLoopEnd(af_file, AF_DEFAULT_INST, 2, markerIDs[3]);
  }
  // file.write (buffer, inumframes) ;

  afCloseFile(af_file);
}

///////////////////////////////////////////////////////////////////////////////

size_t rdhasher::operator()(const RegionData& r) const {
  size_t h1 = std::hash<int>()(r.miSampleId);
  size_t h2 = std::hash<int>()(r.miSubSample);
  // size_t h3 = std::hash<int>()(r.miTuning);
  // size_t h4 = std::hash<float>()(r.mVolumeAdjust);
  // size_t h5 = std::hash<SampleItem*>()(r.mpSample);
  return h1 ^ (h2 << 1); // ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
}

//////////////////////////////////////////////////////

bool rdequer::operator()(const RegionData& a, const RegionData& b) const {
  return a == b;
}
