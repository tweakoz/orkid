#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/cz101.h>
#include <ork/lev2/aud/singularity/konoff.h>
#include <ork/lev2/aud/singularity/sampler.h>

namespace ork::audio::singularity {

dspblk_ptr_t createDspBlock(dspblkdata_constptr_t dbd);

///////////////////////////////////////////////////////////////////////////////

Alg::Alg(const AlgData& algd)
    : _algdata(algd) {
}

Alg::~Alg() {
}

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t Alg::lastBlock() const {
  return nullptr; //
  /*    DspBlock* r = nullptr;
      for( int i=0; i<kmaxdspblocksperlayer; i++ )
          if( _block[i] )
          {
              bool ena = synth::instance()->_stageEnable[i];
              if( ena )
                  r = _block[i];
          }
      return r;*/ // fix _synth::instance
}

///////////////////////////////////////////////////////////////////////////////

void Alg::keyOn(DspKeyOnInfo& koi) {
  auto l = koi._layer;
  assert(l != nullptr);

  const auto ld = l->_LayerData;

  ///////////////////////////////////////////////////
  // instantiate dspblock grid
  ///////////////////////////////////////////////////

  for (int istage = 0; istage < kmaxdspstagesperlayer; istage++) {
    auto stagedata = _algdata._stages[istage];
    if (stagedata) {
      auto stage      = std::make_shared<DspStage>();
      _stages[istage] = stage;
      for (int iblock = 0; iblock < kmaxdspblocksperstage; iblock++) {
        auto blockdata = stagedata->_blockdatas[iblock];
        if (blockdata) {
          auto block             = createDspBlock(blockdata);
          stage->_blocks[iblock] = block;
          block->_verticalIndex  = iblock;
        }
      }
    } else
      _stages[istage] = nullptr;
  }

  ///////////////////////////////////////////////////

  // if (i == 0) // pitch block ?
  //{
  //_block[i]->_iomask._outputMask = 3;
  //}
  //} else
  //_block[i] = nullptr;

  doKeyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::intoDspBuf(const outputBuffer& obuf) {
  int inumframes = synth::instance()->_numFrames;
  _layer->_dspbuffer->resize(inumframes);
  float* lefbuf = obuf._leftBuffer;
  float* rhtbuf = obuf._rightBuffer;
  float* uprbuf = _layer->_dspbuffer->channel(0);
  float* lwrbuf = _layer->_dspbuffer->channel(1);
  memcpy(uprbuf, lefbuf, inumframes * 4);
  memcpy(lwrbuf, lefbuf, inumframes * 4);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::intoOutBuf(outputBuffer& obuf, int inumo) {
  int inumframes = synth::instance()->_numFrames;
  _layer->_dspbuffer->resize(inumframes);
  float* lefbuf = obuf._leftBuffer;
  float* rhtbuf = obuf._rightBuffer;
  float* uprbuf = _layer->_dspbuffer->channel(0);
  float* lwrbuf = _layer->_dspbuffer->channel(1);
  memcpy(lefbuf, uprbuf, inumframes * 4);
  memcpy(rhtbuf, lwrbuf, inumframes * 4);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::forEachStage(stagefn_t fn) {
  for (int istage = 0; istage < kmaxdspstagesperlayer; istage++) {
    auto stage = _stages[istage];
    if (stage) {
      fn(stage);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void DspStage::forEachBlock(blockfn_t fn) {
  for (int iblock = 0; iblock < kmaxdspblocksperstage; iblock++) {
    auto b = _blocks[iblock];
    if (b) {
      fn(b);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void Alg::compute(outputBuffer& obuf) {
  intoDspBuf(obuf);
  auto dspbuf     = _layer->_dspbuffer;
  bool touched    = false;
  int inumoutputs = 1;
  int istage      = 0;
  auto syn        = synth::instance();
  forEachStage([&](dspstage_ptr_t stage) {
    bool ena = syn->_stageEnable[istage];
    if (ena)
      stage->forEachBlock([&](dspblk_ptr_t block) {
        block->compute(*dspbuf.get());
        inumoutputs = block->numOutputs();
        touched     = true;
      });
    istage++;
  });
  // get num outputs for STAGE, not block..
  intoOutBuf(obuf, inumoutputs);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::doKeyOn(DspKeyOnInfo& koi) {
  auto l = koi._layer;
  assert(l != nullptr);

  koi._prv = nullptr;
  _layer   = l;

  forEachStage([&](dspstage_ptr_t stage) {
    stage->forEachBlock([&](dspblk_ptr_t block) {
      // const auto& iomasks = _algConfig._ioMasks;
      //_block[i]->_iomask = iomasks[i];
      koi._prv = block;
      block->keyOn(koi);
    });
  });
}

///////////////////////////////////////////////////////////////////////////////

void Alg::keyOff() {
  forEachStage([&](dspstage_ptr_t stage) {        //
    stage->forEachBlock([&](dspblk_ptr_t block) { //
      block->doKeyOff();
    });
  });
}

///////////////////////////////////////////////////////////////////////////////

alg_ptr_t AlgData::createAlgInst() const {
  auto alg = std::make_shared<Alg>(*this);
  return alg;
}

///////////////////////////////////////////////////////////////////////////////
struct NONE : public DspBlock {
  NONE(dspblkdata_constptr_t dbd)
      : DspBlock(dbd) {
    _numParams = 0;
  }
  void compute(DspBuffer& dspbuf) final {
  }
};

///////////////////////////////////////////////////////////////////////////////

dspblk_ptr_t createDspBlock(dspblkdata_constptr_t dbd) {
  dspblk_ptr_t rval;

  if (dbd->_dspBlock == "NONE")
    rval = std::make_shared<NONE>(dbd);

  ////////////////////////
  // amp/mix
  ////////////////////////

  if (dbd->_dspBlock == "XFADE")
    rval = std::make_shared<XFADE>(dbd);
  if (dbd->_dspBlock == "x GAIN")
    rval = std::make_shared<XGAIN>(dbd);
  if (dbd->_dspBlock == "GAIN")
    rval = std::make_shared<GAIN>(dbd);
  if (dbd->_dspBlock == "AMP_MONOIO")
    rval = std::make_shared<AMP_MONOIO>(dbd);
  if (dbd->_dspBlock == "AMP")
    rval = std::make_shared<AMP>(dbd);
  if (dbd->_dspBlock == "+ AMP")
    rval = std::make_shared<PLUSAMP>(dbd);
  if (dbd->_dspBlock == "x AMP")
    rval = std::make_shared<XAMP>(dbd);
  if (dbd->_dspBlock == "PANNER")
    rval = std::make_shared<PANNER>(dbd);
  if (dbd->_dspBlock == "AMP U   AMP L")
    rval = std::make_shared<AMPU_AMPL>(dbd);
  if (dbd->_dspBlock == "! AMP")
    rval = std::make_shared<BANGAMP>(dbd);

  ////////////////////////
  // osc/gen
  ////////////////////////

  if (dbd->_dspBlock == "SAMPLER")
    rval = std::make_shared<SAMPLER>(dbd);
  if (dbd->_dspBlock == "SINE")
    rval = std::make_shared<SINE>(dbd);
  if (dbd->_dspBlock == "LF SIN")
    rval = std::make_shared<SINE>(dbd);
  if (dbd->_dspBlock == "SAW")
    rval = std::make_shared<SAW>(dbd);
  if (dbd->_dspBlock == "SQUARE")
    rval = std::make_shared<SQUARE>(dbd);
  if (dbd->_dspBlock == "SINE+")
    rval = std::make_shared<SINEPLUS>(dbd);
  if (dbd->_dspBlock == "SAW+")
    rval = std::make_shared<SAWPLUS>(dbd);
  if (dbd->_dspBlock == "SW+SHP")
    rval = std::make_shared<SWPLUSSHP>(dbd);
  if (dbd->_dspBlock == "+ SHAPEMOD OSC")
    rval = std::make_shared<PLUSSHAPEMODOSC>(dbd);
  if (dbd->_dspBlock == "SHAPE MOD OSC")
    rval = std::make_shared<SHAPEMODOSC>(dbd);

  if (dbd->_dspBlock == "SYNC M")
    rval = std::make_shared<SYNCM>(dbd);
  if (dbd->_dspBlock == "SYNC S")
    rval = std::make_shared<SYNCS>(dbd);
  if (dbd->_dspBlock == "PWM")
    rval = std::make_shared<PWM>(dbd);

  if (dbd->_dspBlock == "FM4")
    rval = std::make_shared<FM4>(dbd);
  if (dbd->_dspBlock == "CZX")
    rval = std::make_shared<CZX>(dbd);

  if (dbd->_dspBlock == "NOISE")
    rval = std::make_shared<NOISE>(dbd);

  ////////////////////////
  // EQ
  ////////////////////////

  if (dbd->_dspBlock == "PARA BASS")
    rval = std::make_shared<PARABASS>(dbd);
  if (dbd->_dspBlock == "PARA MID")
    rval = std::make_shared<PARAMID>(dbd);
  if (dbd->_dspBlock == "PARA TREBLE")
    rval = std::make_shared<PARATREBLE>(dbd);
  if (dbd->_dspBlock == "PARAMETRIC EQ")
    rval = std::make_shared<PARAMETRIC_EQ>(dbd);

  ////////////////////////
  // filter
  ////////////////////////

  if (dbd->_dspBlock == "2POLE ALLPASS")
    rval = std::make_shared<TWOPOLE_ALLPASS>(dbd);
  if (dbd->_dspBlock == "2POLE LOWPASS")
    rval = std::make_shared<TWOPOLE_LOWPASS>(dbd);

  if (dbd->_dspBlock == "STEEP RESONANT BASS")
    rval = std::make_shared<STEEP_RESONANT_BASS>(dbd);
  if (dbd->_dspBlock == "4POLE LOPASS W/SEP")
    rval = std::make_shared<FOURPOLE_LOPASS_W_SEP>(dbd);
  if (dbd->_dspBlock == "4POLE HIPASS W/SEP")
    rval = std::make_shared<FOURPOLE_HIPASS_W_SEP>(dbd);
  if (dbd->_dspBlock == "NOTCH FILTER")
    rval = std::make_shared<NOTCH_FILT>(dbd);
  if (dbd->_dspBlock == "NOTCH2")
    rval = std::make_shared<NOTCH2>(dbd);
  if (dbd->_dspBlock == "DOUBLE NOTCH W/SEP")
    rval = std::make_shared<DOUBLE_NOTCH_W_SEP>(dbd);
  if (dbd->_dspBlock == "BANDPASS FILT")
    rval = std::make_shared<BANDPASS_FILT>(dbd);
  if (dbd->_dspBlock == "BAND2")
    rval = std::make_shared<BAND2>(dbd);

  if (dbd->_dspBlock == "LOPAS2")
    rval = std::make_shared<LOPAS2>(dbd);
  if (dbd->_dspBlock == "LP2RES")
    rval = std::make_shared<LP2RES>(dbd);
  if (dbd->_dspBlock == "LOPASS")
    rval = std::make_shared<LOPASS>(dbd);
  if (dbd->_dspBlock == "LPCLIP")
    rval = std::make_shared<LPCLIP>(dbd);
  if (dbd->_dspBlock == "LPGATE")
    rval = std::make_shared<LPGATE>(dbd);

  if (dbd->_dspBlock == "HIPASS")
    rval = std::make_shared<HIPASS>(dbd);
  if (dbd->_dspBlock == "ALPASS")
    rval = std::make_shared<ALPASS>(dbd);

  if (dbd->_dspBlock == "HIFREQ STIMULATOR")
    rval = std::make_shared<HIFREQ_STIMULATOR>(dbd);

  ////////////////////////
  // nonlin
  ////////////////////////

  if (dbd->_dspBlock == "SHAPER")
    rval = std::make_shared<SHAPER>(dbd);
  if (dbd->_dspBlock == "2PARAM SHAPER")
    rval = std::make_shared<TWOPARAM_SHAPER>(dbd);
  if (dbd->_dspBlock == "WRAP")
    rval = std::make_shared<WRAP>(dbd);
  if (dbd->_dspBlock == "DIST")
    rval = std::make_shared<DIST>(dbd);

  if (rval) {
    rval->resize(synth::instance()->_numFrames);
  }

  return rval;
}

} // namespace ork::audio::singularity
