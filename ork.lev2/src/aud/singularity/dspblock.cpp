#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_eq.h>

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////

alg_ptr_t AlgData::createAlgInst() const {
  auto alg = std::make_shared<Alg>(*this);
  return alg;
}

dspstagedata_ptr_t AlgData::appendStage() {
  OrkAssert((_numstages + 1) <= kmaxdspstagesperlayer);
  auto stage            = std::make_shared<DspStageData>();
  _stages[_numstages++] = stage;
  return stage;
}

///////////////////////////////////////////////////////////////////////////////

DspParamData& DspBlockData::addParam() {
  OrkAssert(_numParams < kmaxparmperblock - 1);
  return _paramd[_numParams++];
}

DspParamData& DspBlockData::getParam(int index) {
  return _paramd[index];
}

///////////////////////////////////////////////////////////////////////////////

DspBuffer::DspBuffer()
    : _maxframes(0)
    , _numframes(0) {
  for (int i = 0; i < kmaxchans; i++)
    _channels[i] = nullptr;
}

///////////////////////////////////////////////////////////////////////////////

void DspBuffer::resize(int inumframes) {
  if (inumframes > _maxframes) {
    for (int i = 0; i < kmaxchans; i++) {
      if (_channels[i])
        delete[] _channels[i];
      _channels[i] = new float[inumframes];
    }
    _maxframes = inumframes;
  }
  _numframes = inumframes;
}

float* DspBuffer::channel(int ich) {
  return _channels[ich % kmaxchans];
}

///////////////////////////////////////////////////////////////////////////////

DspStageData::DspStageData() {
  _iomask = std::make_shared<IoMask>();
}

dspblkdata_ptr_t DspStageData::appendBlock() {
  OrkAssert(_numblocks < kmaxdspblocksperstage);
  auto blk                  = std::make_shared<DspBlockData>();
  _blockdatas[_numblocks++] = blk;
  return blk;
}

///////////////////////////////////////////////////////////////////////////////

DspBlock::DspBlock(dspblkdata_constptr_t dbd)
    : _dbd(dbd)
    , _numParams(dbd->_numParams)
    , _numFrames(0) {

  printf("NUMP<%d>\n", _numParams);
}

size_t DspBlock::numFrames() const {
  return _numFrames;
}

void DspBlock::resize(int inumframes) {
  _numFrames = inumframes;
  if (auto try_hsync = _vars.typedValueForKey<oschardsynctrack_ptr_t>("HSYNC")) {
    try_hsync.value()->resize(inumframes);
  }
}

///////////////////////////////////////////////////////////////////////////////

FPARAM DspBlock::initFPARAM(const DspParamData& dpd) {
  FPARAM rval;
  rval._coarse    = dpd._coarse;
  rval._fine      = dpd._fine;
  rval._C1        = _layer->getSRC1(dpd._mods);
  rval._C2        = _layer->getSRC2(dpd._mods);
  rval._evaluator = dpd._mods._evaluator;

  rval._keyTrack      = dpd._keyTrack;
  rval._velTrack      = dpd._velTrack;
  rval._kstartNote    = dpd._keystartNote;
  rval._kstartBipolar = dpd._keystartBipolar;

  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void DspBlock::keyOn(const DspKeyOnInfo& koi) {
  _layer = koi._layer;
  printf("keyOn NUMP<%d>\n", _numParams);

  for (int i = 0; i < _numParams; i++) {
    _param[i] = initFPARAM(_dbd->_paramd[i]);
    _param[i].keyOn(koi._key, koi._vel);
  }

  doKeyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

const float* DspBlock::getInpBuf(DspBuffer& obuf, int index) {
  int inpidx = _iomask->_inputs[index];
  return obuf.channel(inpidx);
}

///////////////////////////////////////////////////////////////////////////////

float* DspBlock::getOutBuf(DspBuffer& obuf, int index) {
  int outidx = _iomask->_outputs[index];
  return obuf.channel(outidx);
}

///////////////////////////////////////////////////////////////////////////////

// void DspBlock::output(DspBuffer& obuf, int chanidx, int sampleindex, float val) {
// int outidx     = _iomask->_outputs[chanidx];
// float* A       = obuf.channel(outidx);
// A[sampleindex] = val;
//}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numOutputs() const {
  return _iomask->numOutputs();
}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numInputs() const {
  return _iomask->numInputs();
}

} // namespace ork::audio::singularity
