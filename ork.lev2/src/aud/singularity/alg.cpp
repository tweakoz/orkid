#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/konoff.h>

namespace ork::audio::singularity {

DspBlock* createDspBlock(const DspBlockData& dbd);

extern synth* the_synth;

///////////////////////////////////////////////////////////////////////////////

Alg::Alg(const AlgData& algd)
    : _algConfig(algd._config)
    , _blockBuf(nullptr) {
  for (int i = 0; i < kmaxdspblocksperlayer; i++)
    _block[i] = nullptr;

  _blockBuf = new DspBuffer;
}

Alg::~Alg() {
  for (int i = 0; i < kmaxdspblocksperlayer; i++)
    if (_block[i])
      delete _block[i];

  delete _blockBuf;
}

///////////////////////////////////////////////////////////////////////////////

DspBlock* Alg::lastBlock() const {
  return nullptr; //
  /*    DspBlock* r = nullptr;
      for( int i=0; i<kmaxdspblocksperlayer; i++ )
          if( _block[i] )
          {
              bool ena = the_synth->_fblockEnable[i];
              if( ena )
                  r = _block[i];
          }
      return r;*/ // fix _the_synth
}

///////////////////////////////////////////////////////////////////////////////

void Alg::keyOn(DspKeyOnInfo& koi) {
  auto l = koi._layer;
  assert(l != nullptr);

  const auto ld = l->_layerData;

  for (int i = 0; i < kmaxdspblocksperlayer; i++) {
    const auto data = ld->_dspBlocks[i];
    if (data and data->_dspBlock.length()) {
      _block[i] = createDspBlock(*data);

      if (i == 0) // pitch block ?
      {
        _block[i]->_iomask._outputMask = 3;
      }
    } else
      _block[i] = nullptr;
  }

  doKeyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::intoDspBuf(const outputBuffer& obuf, DspBuffer& dspbuf) {
  int inumframes = obuf._numframes;
  _blockBuf->resize(inumframes);
  float* lefbuf = obuf._leftBuffer;
  float* rhtbuf = obuf._rightBuffer;
  float* uprbuf = _blockBuf->channel(0);
  float* lwrbuf = _blockBuf->channel(1);
  memcpy(uprbuf, lefbuf, inumframes * 4);
  memcpy(lwrbuf, lefbuf, inumframes * 4);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::intoOutBuf(outputBuffer& obuf, const DspBuffer& dspbuf, int inumo) {
  int inumframes = obuf._numframes;
  _blockBuf->resize(inumframes);
  float* lefbuf = obuf._leftBuffer;
  float* rhtbuf = obuf._rightBuffer;
  float* uprbuf = _blockBuf->channel(0);
  float* lwrbuf = _blockBuf->channel(1);
  memcpy(lefbuf, uprbuf, inumframes * 4);
  memcpy(rhtbuf, lwrbuf, inumframes * 4);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::compute(synth& syn, outputBuffer& obuf) {
  intoDspBuf(obuf, *_blockBuf);

  bool touched = false;

  int inumoutputs = 1;

  for (int i = 0; i < kmaxdspblocksperlayer; i++) {
    auto b = _block[i];
    if (b) {
      bool ena = syn._fblockEnable[i];
      if (ena) {
        b->compute(*_blockBuf);
        inumoutputs = b->numOutputs();
        touched     = true;
      }
    }
  }

  intoOutBuf(obuf, *_blockBuf, inumoutputs);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::doKeyOn(DspKeyOnInfo& koi) {
  auto l = koi._layer;
  assert(l != nullptr);

  koi._prv = nullptr;
  _layer   = l;

  auto procblock = [&koi](DspBlock* thisblock, layer* l) {
    if (thisblock) {
      koi._prv = thisblock;
      thisblock->keyOn(koi);
    }
  };

  const auto& iomasks = _algConfig._ioMasks;
  for (int i = 0; i < kmaxdspblocksperlayer; i++)
    if (_block[i]) {
      _block[i]->_iomask = iomasks[i];
      procblock(_block[i], l);
    }
}

///////////////////////////////////////////////////////////////////////////////

void Alg::keyOff() {
  for (int i = 0; i < kmaxdspblocksperlayer; i++) {
    auto b = _block[i];
    if (b)
      b->doKeyOff();
  }
}

///////////////////////////////////////////////////////////////////////////////

Alg* AlgData::createAlgInst() const {
  auto alg = new Alg(*this);
  return alg;
}

///////////////////////////////////////////////////////////////////////////////
struct NONE : public DspBlock {
  NONE(const DspBlockData& dbd)
      : DspBlock(dbd) {
    _iomask._inputMask  = 0;
    _iomask._outputMask = 0;
    _numParams          = 0;
  }
  void compute(DspBuffer& dspbuf) final {
  }
};

///////////////////////////////////////////////////////////////////////////////

DspBlock* createDspBlock(const DspBlockData& dbd) {
  DspBlock* rval = nullptr;

  if (dbd._dspBlock == "NONE")
    rval = new NONE(dbd);

  ////////////////////////
  // amp/mix
  ////////////////////////

  if (dbd._dspBlock == "XFADE")
    rval = new XFADE(dbd);
  if (dbd._dspBlock == "x GAIN")
    rval = new XGAIN(dbd);
  if (dbd._dspBlock == "GAIN")
    rval = new GAIN(dbd);
  if (dbd._dspBlock == "AMP")
    rval = new AMP(dbd);
  if (dbd._dspBlock == "+ AMP")
    rval = new PLUSAMP(dbd);
  if (dbd._dspBlock == "x AMP")
    rval = new XAMP(dbd);
  if (dbd._dspBlock == "PANNER")
    rval = new PANNER(dbd);
  if (dbd._dspBlock == "AMP U   AMP L")
    rval = new AMPU_AMPL(dbd);
  if (dbd._dspBlock == "! AMP")
    rval = new BANGAMP(dbd);

  ////////////////////////
  // osc/gen
  ////////////////////////

  if (dbd._dspBlock == "SAMPLEPB")
    rval = new SAMPLEPB(dbd);
  if (dbd._dspBlock == "SINE")
    rval = new SINE(dbd);
  if (dbd._dspBlock == "LF SIN")
    rval = new SINE(dbd);
  if (dbd._dspBlock == "SAW")
    rval = new SAW(dbd);
  if (dbd._dspBlock == "SQUARE")
    rval = new SQUARE(dbd);
  if (dbd._dspBlock == "SINE+")
    rval = new SINEPLUS(dbd);
  if (dbd._dspBlock == "SAW+")
    rval = new SAWPLUS(dbd);
  if (dbd._dspBlock == "SW+SHP")
    rval = new SWPLUSSHP(dbd);
  if (dbd._dspBlock == "+ SHAPEMOD OSC")
    rval = new PLUSSHAPEMODOSC(dbd);
  if (dbd._dspBlock == "SHAPE MOD OSC")
    rval = new SHAPEMODOSC(dbd);

  if (dbd._dspBlock == "SYNC M")
    rval = new SYNCM(dbd);
  if (dbd._dspBlock == "SYNC S")
    rval = new SYNCS(dbd);
  if (dbd._dspBlock == "PWM")
    rval = new PWM(dbd);

  if (dbd._dspBlock == "FM4")
    rval = new FM4(dbd);
  if (dbd._dspBlock == "CZX")
    rval = new CZX(dbd);

  if (dbd._dspBlock == "NOISE")
    rval = new NOISE(dbd);

  ////////////////////////
  // EQ
  ////////////////////////

  if (dbd._dspBlock == "PARA BASS")
    rval = new PARABASS(dbd);
  if (dbd._dspBlock == "PARA MID")
    rval = new PARAMID(dbd);
  if (dbd._dspBlock == "PARA TREBLE")
    rval = new PARATREBLE(dbd);
  if (dbd._dspBlock == "PARAMETRIC EQ")
    rval = new PARAMETRIC_EQ(dbd);

  ////////////////////////
  // filter
  ////////////////////////

  if (dbd._dspBlock == "2POLE ALLPASS")
    rval = new TWOPOLE_ALLPASS(dbd);
  if (dbd._dspBlock == "2POLE LOWPASS")
    rval = new TWOPOLE_LOWPASS(dbd);

  if (dbd._dspBlock == "STEEP RESONANT BASS")
    rval = new STEEP_RESONANT_BASS(dbd);
  if (dbd._dspBlock == "4POLE LOPASS W/SEP")
    rval = new FOURPOLE_LOPASS_W_SEP(dbd);
  if (dbd._dspBlock == "4POLE HIPASS W/SEP")
    rval = new FOURPOLE_HIPASS_W_SEP(dbd);
  if (dbd._dspBlock == "NOTCH FILTER")
    rval = new NOTCH_FILT(dbd);
  if (dbd._dspBlock == "NOTCH2")
    rval = new NOTCH2(dbd);
  if (dbd._dspBlock == "DOUBLE NOTCH W/SEP")
    rval = new DOUBLE_NOTCH_W_SEP(dbd);
  if (dbd._dspBlock == "BANDPASS FILT")
    rval = new BANDPASS_FILT(dbd);
  if (dbd._dspBlock == "BAND2")
    rval = new BAND2(dbd);

  if (dbd._dspBlock == "LOPAS2")
    rval = new LOPAS2(dbd);
  if (dbd._dspBlock == "LP2RES")
    rval = new LP2RES(dbd);
  if (dbd._dspBlock == "LOPASS")
    rval = new LOPASS(dbd);
  if (dbd._dspBlock == "LPCLIP")
    rval = new LPCLIP(dbd);
  if (dbd._dspBlock == "LPGATE")
    rval = new LPGATE(dbd);

  if (dbd._dspBlock == "HIPASS")
    rval = new HIPASS(dbd);
  if (dbd._dspBlock == "ALPASS")
    rval = new ALPASS(dbd);

  if (dbd._dspBlock == "HIFREQ STIMULATOR")
    rval = new HIFREQ_STIMULATOR(dbd);

  ////////////////////////
  // nonlin
  ////////////////////////

  if (dbd._dspBlock == "SHAPER")
    rval = new SHAPER(dbd);
  if (dbd._dspBlock == "2PARAM SHAPER")
    rval = new TWOPARAM_SHAPER(dbd);
  if (dbd._dspBlock == "WRAP")
    rval = new WRAP(dbd);
  if (dbd._dspBlock == "DIST")
    rval = new DIST(dbd);

  return rval;
}

} // namespace ork::audio::singularity
