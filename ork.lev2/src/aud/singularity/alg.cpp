////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <assert.h>
#include <ork/lev2/aud/singularity/synth.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/konoff.h>
#include <ork/lev2/aud/singularity/dspblocks.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedMap.hpp>

ImplementReflectionX(ork::audio::singularity::AlgData, "SynAlgorithm");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

void AlgData::describeX(class_t* clazz) {
  clazz->directProperty("Name", &AlgData::_name);
  clazz->directObjectMapProperty("Stages", &AlgData::_stageByName);
}

algdata_ptr_t AlgData::clone() const {
  auto rval = std::make_shared<AlgData>();
  rval->_name = _name;
  rval->_numstages = _numstages;
  for (size_t i=0; i<_numstages; i++) {
    auto stage = _stages[i];
    auto clone = stage->clone();
    rval->_stages[i] = clone;
    rval->_stageByName[clone->_name] = clone;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool AlgData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) { // override
  for (auto item : _stageByName) {
    auto stage     = item.second;
    int index      = stage->_stageIndex;
    _stages[index] = stage;
  }
  _numstages = _stageByName.size();
  return true;
}

///////////////////////////////////////////////////////////////////////////////

alg_ptr_t AlgData::createAlgInst() const {
  alg_ptr_t rval = nullptr;
  if(_voicecache.empty()){
    rval = std::make_shared<Alg>(*this);
  }
  else{
    rval = _voicecache.back();
    _voicecache.pop_back();
  }
  return rval;
}
void AlgData::returnAlgInst(alg_ptr_t alg) const{
  _voicecache.push_back(alg);
}

///////////////////////////////////////////////////////////////////////////////

dspstagedata_ptr_t AlgData::appendStage(const std::string& named) {
  OrkAssert((_numstages + 1) <= kmaxdspstagesperlayer);
  auto stage            = std::make_shared<DspStageData>();
  stage->_name          = named;
  stage->_stageIndex    = _numstages;
  _stages[_numstages++] = stage;
  _stageByName[named]   = stage;
  return stage;
}

///////////////////////////////////////////////////////////////////////////////

dspstagedata_ptr_t AlgData::stageByName(const std::string& named) {
  auto it = _stageByName.find(named);
  return (it == _stageByName.end()) ? nullptr : it->second;
}

///////////////////////////////////////////////////////////////////////////////

dspstagedata_ptr_t AlgData::stageByIndex(int index) {
  OrkAssert(index < _numstages);
  OrkAssert(index >= 0);
  auto stage = _stages[index];
  return stage;
}

///////////////////////////////////////////////////////////////////////////////

Alg::Alg(const AlgData& algd)
    : _algdata(algd) {
  assert((&algd)!=nullptr);
}

///////////////////////////////////////////////////////////////////////////////

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

void Alg::keyOn(KeyOnInfo& koi) {
  auto l = koi._layer;
  assert(l != nullptr);
  //const auto ld = l->_layerdata;

  ///////////////////////////////////////////////////
  // instantiate dspblock grid
  ///////////////////////////////////////////////////

  auto& out_stages = _stageblock._stages;
  int numstages = 0;
  for (int istage = 0; istage < kmaxdspstagesperlayer; istage++) {
    auto stagedata = _algdata._stages[istage];
    if (stagedata) {
      auto stage      = std::make_shared<DspStage>();
      out_stages[istage] = stage;
      numstages++;
      for (int iblock = 0; iblock < kmaxdspblocksperstage; iblock++) {
        auto blockdata = stagedata->_blockdatas[iblock];
        if (blockdata) {
          auto block             = blockdata->createInstance();
          stage->_blocks[iblock] = block;
          block->_verticalIndex  = iblock;
          block->_ioconfig         = stagedata->_ioconfig;
        }
      }
    } else
      out_stages[istage] = nullptr;
  }
  // printf("ALG<%p> numstages<%d>\n", this, numstages);
  ///////////////////////////////////////////////////

  // if (i == 0) // pitch block ?
  //{
  //_block[i]->_ioconfig._outputMask = 3;
  //}
  //} else
  //_block[i] = nullptr;

  doKeyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

void Alg::forEachStage(stagefn_t fn) {
  auto& out_stages = _stageblock._stages;
  for (int istage = 0; istage < kmaxdspstagesperlayer; istage++) {
    auto stage = out_stages[istage];
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
void Alg::beginCompute() {
}
///////////////////////////////////////////////////////////////////////////////
void Alg::doComputePass() {
  ////////////////////////////////////////////////
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  ////////////////////////////////////////////////
  // compute dsp stages
  ////////////////////////////////////////////////
  auto& dspbuf = *_layer->_dspbuffer;
  int istage   = 0;
  auto syn     = synth::instance();
  forEachStage([&](dspstage_ptr_t stage) {
    bool ena = syn->_stageEnable[istage];
    if (ena)
      stage->forEachBlock([&](dspblk_ptr_t block) {
        block->compute(dspbuf);
        //////////////////////////////////////
        // SignalScope
        //////////////////////////////////////
        auto scopesrc = block->_dbd->_scopesource;
        if (scopesrc) {
          if (scopesrc->_cursrcimpl == (void*)block.get()) {
            auto data = dspbuf.channel(scopesrc->_dspchannel) + ibase;
            scopesrc->updateMono(inumframes, data, true);
          }
        }
        /////////////////////////////
      });
    istage++;
  });
}
///////////////////////////////////////////////////////////////////////////////
void Alg::endCompute() {
}

///////////////////////////////////////////////////////////////////////////////

void Alg::doKeyOn(KeyOnInfo& koi) {
  auto l = koi._layer;
  assert(l != nullptr);

  _layer = l;

  forEachStage([&](dspstage_ptr_t stage) {
    stage->forEachBlock([&](dspblk_ptr_t block) {
      block->keyOn(koi);
      //////////////////////////////////////
      // SignalScope
      //////////////////////////////////////
      auto scopesrc = block->_dbd->_scopesource;
      if (scopesrc) {
        scopesrc->_cursrcimpl = block.get();
        scopesrc->notifySinksKeyOn(koi);
      }
      /////////////////////////////
    });
  });
}

///////////////////////////////////////////////////////////////////////////////

void Alg::keyOff() {
  forEachStage([&](dspstage_ptr_t stage) {        //
    stage->forEachBlock([&](dspblk_ptr_t block) { //
      block->doKeyOff();
      //////////////////////////////////////
      // SignalScope
      //////////////////////////////////////
      auto scopesrc = block->_dbd->_scopesource;
      if (scopesrc) {
        scopesrc->notifySinksKeyOff();
      }
      /////////////////////////////
    });
  });
}

} // namespace ork::audio::singularity
