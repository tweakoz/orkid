////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/alg_eq.h>
#include <ork/lev2/aud/singularity/alg_oscil.h>
#include <ork/lev2/aud/singularity/alg_nonlin.h>
#include <ork/lev2/aud/singularity/alg_filters.h>
#include <ork/lev2/aud/singularity/alg_amp.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/dsp_ringmod.h>
#include <ork/lev2/aud/singularity/cz1.h>
#include <ork/lev2/aud/singularity/sampler.h>
#include <ork/lev2/aud/singularity/hud.h>
#include <ork/lev2/aud/singularity/keyon_prof.h>
#include <ork/reflect/properties/registerX.inl>

ImplementReflectionX(ork::audio::singularity::DspBlockData, "SynDspBlock");

namespace ork::audio::singularity {

//////////////////////////////////////////////////////////////////////////////
// dsp instance storage recycler (see dspblocks.h). one lock-free free list per
//  size class; a class hands back blocks of the FULL class size, so any request
//  that maps to the class can be served by any of its entries.
//////////////////////////////////////////////////////////////////////////////

static constexpr size_t kinstclassshift = 8;                    // 256 byte granularity
static constexpr size_t kinstnumclasses = 64;                   // ... up to 16KB
static constexpr size_t kinstclasscap   = 1024;                 // entries per class
static constexpr size_t kinstmaxbytes   = kinstnumclasses << kinstclassshift;

using instfreelist_t = MpMcBoundedQueue<void*, kinstclasscap>;
static instfreelist_t _instfreelists[kinstnumclasses];
static std::atomic<size_t> _instpoolmisses(0);

void* dspInstanceAlloc(size_t nbytes) {
  if (0 == nbytes or nbytes > kinstmaxbytes) {
    _instpoolmisses.fetch_add(1, std::memory_order_relaxed);
    return ::operator new(nbytes, std::align_val_t(kdspinstancealign));
  }
  size_t klass = (nbytes - 1) >> kinstclassshift;
  void* recycled = nullptr;
  if (_instfreelists[klass].try_pop(recycled))
    return recycled;
  _instpoolmisses.fetch_add(1, std::memory_order_relaxed);
  return ::operator new((klass + 1) << kinstclassshift, std::align_val_t(kdspinstancealign));
}

void dspInstanceFree(void* ptr, size_t nbytes) {
  if (nbytes > 0 and nbytes <= kinstmaxbytes) {
    size_t klass = (nbytes - 1) >> kinstclassshift;
    if (_instfreelists[klass].try_push(ptr))
      return;
  }
  ::operator delete(ptr, std::align_val_t(kdspinstancealign));
}

size_t dspInstancePoolMisses() {
  return _instpoolmisses.load(std::memory_order_relaxed);
}

//////////////////////////////////////////////////////////////////////////////

void DspBlockData::describeX(class_t* clazz) {
  clazz->directProperty("Name", &DspBlockData::_name);
  clazz->directProperty("BlockIndex", &DspBlockData::_blockIndex);
  clazz->directProperty("InputPad", &DspBlockData::_inputPad);
  clazz->directVectorProperty("Params", &DspBlockData::_paramd);
  clazz->directVectorProperty("DspChannels", &DspBlockData::_dspchannels);
}

//////////////////////////////////////////////////////////////////////////////

DspBlockData::DspBlockData(std::string name)
    : _name(name) {
}

int DspBlockData::addDspChannel(int channel) {
  int chanindex = _dspchannels.size();
  _dspchannels.push_back(channel);
  return chanindex;
}

//////////////////////////////////////////////////////////////////////////////

bool DspBlockData::postDeserialize(reflect::serdes::IDeserializer&, object_ptr_t shared) { // override
  _numParams = _paramd.size();
  return true;
}

//////////////////////////////////////////////////////////////////////////////

scopesource_ptr_t DspBlockData::createScopeSource() {
  auto src     = std::make_shared<ScopeSource>();
  _scopesource = src;
  return src;
}

///////////////////////////////////////////////////////////////////////////////

dspparam_ptr_t DspBlockData::addParam(std::string named, std::string units) {
  OrkAssert(_numParams < kmaxparmperblock - 1);
  auto param = std::make_shared<DspParamData>(named);
  param->_units = units;
  _paramd.push_back(param);
  _numParams = _paramd.size();
  return param;
}

dspparam_ptr_t DspBlockData::param(int index) {
  return _paramd[index];
}
dspparam_ptr_t DspBlockData::paramByName(std::string named) {
  for( auto it : _paramd )
    if( it->_name == named )
      return it;
  return nullptr;
}

///////////////////////////////////////////////////////////////////////////////

DspBlock::DspBlock(const DspBlockData* dbd)
    : _dbd(dbd)
    , _numParams(dbd->_numParams) {
  int numchannels = dbd->_dspchannels.size();
  for (int i = 0; i < numchannels; i++)
    _dspchannel[i] = dbd->_dspchannels[i];
  for (int i = numchannels; i < kmaxdspblocksperstage; i++) {
    _dspchannel[i] = i;
  }
}

///////////////////////////////////////////////////////////////////////////////

DspParam DspBlock::initDspParam(dspparam_constptr_t dpd) {
  DspParam rval;
  rval._data      = dpd;
  rval._C1        = _layer->getSRC1(dpd->_mods);
  rval._C2        = _layer->getSRC2(dpd->_mods);
  rval._evaluator = dpd->_mods->_evaluator;
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

void DspBlock::keyOn(const KeyOnInfo& koi) {
  _layer = koi._layer;
  {
    KeyOnProfScope profparam(KOP_BLKPARAM);
    for (int i = 0; i < _numParams; i++) {
      _param[i] = initDspParam(_dbd->_paramd[i]);
      _param[i].keyOn(koi._key, koi._vel);
    }
  }
  KeyOnProfScope profdko(KOP_BLKDOKEYON);
  doKeyOn(koi);
}

///////////////////////////////////////////////////////////////////////////////

float* DspBlock::getRawBuf(DspBuffer& dspbuf, int chanindex){
  return dspbuf.channel(chanindex);
}

const float* DspBlock::getInpBuf(DspBuffer& dspbuf, int index) {
  int chan   = _dspchannel[index];
  int inpidx = _ioconfig->_inputs[chan];
  return dspbuf.channel(inpidx);
}

///////////////////////////////////////////////////////////////////////////////

float* DspBlock::getOutBuf(DspBuffer& dspbuf, int index) {
  int chan   = _dspchannel[index];
  int outidx = _ioconfig->_outputs[chan];
  return dspbuf.channel(outidx);
}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numOutputs() const {
  return _ioconfig->numOutputs();
}

///////////////////////////////////////////////////////////////////////////////

int DspBlock::numInputs() const {
  return _ioconfig->numInputs();
}

///////////////////////////////////////////////////////////////////////////////
NOPDATA::NOPDATA() {
  _blocktype = "NOP";
}
dspblk_ptr_t NOPDATA::createInstance() const { // override
  return createDspInstance<NOP>(this);
}

NOP::NOP(const DspBlockData* dbd)
    : DspBlock(dbd) {
  _numParams = 0;
}
void NOP::compute(DspBuffer& dspbuf) { // override
}

} // namespace ork::audio::singularity
