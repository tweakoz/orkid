#include <assert.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/konoff.h>
#include <ork/lev2/aud/singularity/dspblocks.h>

namespace ork::audio::singularity {

static synth_ptr_t the_synth = synth::instance();

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
              bool ena = the_synth->_stageEnable[i];
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
          block->_iomask         = stagedata->_iomask;
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
  int inumframes = the_synth->_numFrames;
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
  int inumframes = the_synth->_numFrames;
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
  auto syn        = the_synth;
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

} // namespace ork::audio::singularity
