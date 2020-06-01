#include <assert.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/konoff.h>
#include <ork/lev2/aud/singularity/dspblocks.h>

namespace ork::audio::singularity {

static synth_ptr_t the_synth = synth::instance();

dspblk_ptr_t createDspBlock(const DspBlockData* dbd);

///////////////////////////////////////////////////////////////////////////////

alg_ptr_t AlgData::createAlgInst() const {
  auto alg = std::make_shared<Alg>(*this);
  return alg;
}
dspstagedata_ptr_t AlgData::appendStage(const std::string& named) {
  OrkAssert((_numstages + 1) <= kmaxdspstagesperlayer);
  auto stage            = std::make_shared<DspStageData>();
  _stages[_numstages++] = stage;
  _stageByName[named]   = stage;
  return stage;
}
dspstagedata_ptr_t AlgData::stageByName(const std::string& named) {
  auto it = _stageByName.find(named);
  return (it == _stageByName.end()) ? nullptr : it->second;
}
dspstagedata_ptr_t AlgData::stageByIndex(int index) {
  OrkAssert(index < _numstages);
  OrkAssert(index >= 0);
  auto stage = _stages[index];
  return stage;
}

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

  const auto ld = l->_layerdata;

  ///////////////////////////////////////////////////
  // instantiate dspblock grid
  ///////////////////////////////////////////////////

  int numstages = 0;
  for (int istage = 0; istage < kmaxdspstagesperlayer; istage++) {
    auto stagedata = _algdata._stages[istage];
    if (stagedata) {
      auto stage      = std::make_shared<DspStage>();
      _stages[istage] = stage;
      numstages++;
      for (int iblock = 0; iblock < kmaxdspblocksperstage; iblock++) {
        auto blockdata = stagedata->_blockdatas[iblock];
        if (blockdata) {
          auto block             = createDspBlock(blockdata.get());
          stage->_blocks[iblock] = block;
          block->_verticalIndex  = iblock;
          block->_iomask         = stagedata->_iomask;
        }
      }
    } else
      _stages[istage] = nullptr;
  }
  // printf("ALG<%p> numstages<%d>\n", this, numstages);
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

void Alg::compute() {
  ////////////////////////////////////////////////
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  ////////////////////////////////////////////////
  // clear dsp buffers
  ////////////////////////////////////////////////
  if (_algdata._cleardspblock)
    for (int ich = 0; ich < kmaxdspblocksperstage; ich++) {
      float* dst = _layer->_dspbuffer->channel(ich) + ibase;
      memset(dst, inumframes * sizeof(float), 0);
    }
  ////////////////////////////////////////////////
  // compute dsp stages
  ////////////////////////////////////////////////
  auto& dspbuf    = *_layer->_dspbuffer;
  bool touched    = false;
  int inumoutputs = 1;
  int istage      = 0;
  auto syn        = the_synth;
  forEachStage([&](dspstage_ptr_t stage) {
    bool ena = syn->_stageEnable[istage];
    if (ena)
      stage->forEachBlock([&](dspblk_ptr_t block) {
        block->compute(dspbuf);
        inumoutputs = block->numOutputs();
        touched     = true;
      });
    istage++;
  });
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
