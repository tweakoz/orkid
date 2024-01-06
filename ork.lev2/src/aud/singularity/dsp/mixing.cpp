////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/aud/singularity/synth.h>
#include <assert.h>
#include <ork/lev2/aud/singularity/filters.h>
#include <ork/lev2/aud/singularity/dsp_mix.h>
#include <ork/lev2/aud/singularity/modulation.h>
#include <ork/lev2/aud/singularity/alg_pan.inl>
#include <ork/reflect/properties/registerX.inl>
#include <iostream>
#include <string>
#include <iomanip> // For std::setw and std::setprecision

ImplementReflectionX(ork::audio::singularity::MonoInStereoOutData, "SynMonoInStereoOut");

namespace ork::audio::singularity {

///////////////////////////////////////////////////////////////////////////////
Sum2Data::Sum2Data(std::string name)
    : DspBlockData(name) {
  _blocktype = "SUM2";
}
dspblk_ptr_t Sum2Data::createInstance() const { // override
  return std::make_shared<SUM2>(this);
}
///////////////////////////////////////////////////////////////////////////////
SUM2::SUM2(const DspBlockData* dbd)
    : DspBlock(dbd) {
}
///////////////////////////////////////////////////////////////////////////////
void SUM2::compute(DspBuffer& dspbuf) { // final
  int inumframes       = _layer->_dspwritecount;
  int ibase            = _layer->_dspwritebase;
  const float* inpbufa = getInpBuf(dspbuf, 0) + ibase;
  const float* inpbufb = getInpBuf(dspbuf, 1) + ibase;
  float* outbufa       = getOutBuf(dspbuf, 0) + ibase;
  // float* outbufb       = getOutBuf(dspbuf, 1) + ibase;
  for (int i = 0; i < inumframes; i++) {
    float inA  = inpbufa[i] * _dbd->_inputPad;
    float inB  = inpbufb[i] * _dbd->_inputPad;
    float res  = (inA + inB);
    res        = clip_float(res, -2, 2);
    outbufa[i] = res;
    // outbufb[i] = res;
  }
}
///////////////////////////////////////////////////////////////////////////////

void MonoInStereoOutData::describeX(class_t* clazz) {
}

MonoInStereoOutData::MonoInStereoOutData(std::string name)
    : DspBlockData(name) {
  _blocktype     = "MonoInStereoOut";
  auto amp_param = addParam();
  amp_param->useAmplitudeEvaluator();
  auto pan_param = addParam();
  pan_param->useDefaultEvaluator();
}
dspblk_ptr_t MonoInStereoOutData::createInstance() const { // override
  return std::make_shared<MonoInStereoOut>(this);
}

MonoInStereoOut::MonoInStereoOut(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void MonoInStereoOut::compute(DspBuffer& dspbuf) // final
{
  float dynamicgain = _param[0].eval() * _dbd->_inputPad;
  float dynamicpan  = _panbase + _param[1].eval();
  int inumframes    = _layer->_dspwritecount;
  int ibase         = _layer->_dspwritebase;
  const auto& LD    = _layer->_layerdata;
  auto l_lrmix      = panBlend(dynamicpan);
  auto lbuf         = getRawBuf(dspbuf, 0) + ibase;
  auto rbuf         = getRawBuf(dspbuf, 1) + ibase;
  float SingleLinG  = decibel_to_linear_amp_ratio(LD->_channelGains[0]);

  for (int i = 0; i < inumframes; i++) {
    // float linG = decibel_to_linear_amp_ratio(dynamicgain);
    // linG *= SingleLinG;
    float inp  = lbuf[i];
    float mono = clip_float(
        inp * dynamicgain, //
        kminclip,
        kmaxclip);
    lbuf[i] = mono * l_lrmix.lmix;
    rbuf[i] = mono * l_lrmix.rmix;
  }
  _fval[0] = _filt;
}

void MonoInStereoOut::doKeyOn(const KeyOnInfo& koi) // final
{
  _filt    = 0.0f;
  auto LD  = koi._layer->_layerdata;
  int chan = _dspchannel[0];
  _panbase = LD->_channelPans[chan];
}
///////////////////////////////////////////////////////////////////////////////

StereoEnhancerData::StereoEnhancerData(std::string name)
    : DspBlockData(name) {
  _blocktype       = "StereoEnhancer";
  auto width_param = addParam();
  width_param->useDefaultEvaluator();
}
dspblk_ptr_t StereoEnhancerData::createInstance() const { // override
  return std::make_shared<StereoEnhancer>(this);
}

StereoEnhancer::StereoEnhancer(const DspBlockData* dbd)
    : DspBlock(dbd) {
}

void StereoEnhancer::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  auto ilbuf     = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf     = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf     = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf     = getOutBuf(dspbuf, 1) + ibase;
  float width    = _param[0].eval();

  for (int i = 0; i < inumframes; i++) {
    float inl    = ilbuf[i];
    float inr    = irbuf[i];
    float mono   = (inl + inr) * 0.5f;
    float stereo = (inl - inr) * width;
    olbuf[i]     = mono + stereo;
    orbuf[i]     = mono - stereo;
  }
}

void StereoEnhancer::doKeyOn(const KeyOnInfo& koi) // final
{
}

void AllpassDelay::setParams(float time, float lingain){
  _delay.setStaticDelayTime(time);
  _gain = lingain;
  clear();
}
float AllpassDelay::compute(float inp){
  float delay_out = _delay.out(0.0f);
  _delay.inp(inp+delay_out*_gain);
  float ap_out = (delay_out*(1.0-_gain*_gain))+(inp*_gain*-1.0f);
  return ap_out;
}
void AllpassDelay::clear(){
  _delay.clear();
}

///////////////////////////////////////////////////////////////////////////////
// Mixdown 8 channels to 2 channels
///////////////////////////////////////////////////////////////////////////////

fvec2 mix8to2::mixdown(const vec8f& input) const {
  fvec2 output;
  for (int j = 0; j < 8; ++j) {
    output.x += _elements[0][j] * input._elements[j];
    output.y += _elements[1][j] * input._elements[j];
  }
  return output;
}

///////////////////////////////////////////////////////////////////////////////

mix8to2::mix8to2() {
  for (int i = 0; i < 2; ++i) {
    for (int j = 0; j < 8; ++j) {
      _elements[i][j] = 0.25f; // Adjust these values based on your needs
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

vec8f::vec8f() {
  for (int i = 0; i < 8; i++) {
    _elements[i] = 0.0f;
  }
}

///////////////////////////////////////////////////////////////////////////////

void vec8f::broadcast(float inp){
  for (int i = 0; i < 8; i++) {
    _elements[i] = inp;
  }
}

///////////////////////////////////////////////////////////////////////////////

void vec8f::broadcast(fvec2 inp){
  for (int i = 0; i < 8; i++) {
    _elements[i] = (i&1) ? inp.y : inp.x;
  }
}

///////////////////////////////////////////////////////////////////////////////

mtx8f mtx8f::generateHadamard() {
  mtx8f result;

  float scale = 1.0f;
  // Initialize H(1)
  int H[1][1] = {{1}};

  // Generate H(8) using Sylvester's construction
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 8; j++) {
      // Determine the sign (plus or minus)
      int sign = 1;
      if (i >= 4 && j >= 4)
        sign = -1;

      result._elements[i][j] = scale * (sign * H[i % 4][j % 4]);
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////

mtx8f mtx8f::householder(const vec8f& v) {
  mtx8f result;

  // Step 1: Calculate the normalized vector u
  vec8f u    = v;
  float norm = 0.0;
  for (int i = 0; i < 8; ++i) {
    norm += u._elements[i] * u._elements[i];
  }
  norm = std::sqrt(norm);

  // Adjust the first component of u
  u._elements[0] += (u._elements[0] > 0) ? norm : -norm;

  // Normalize u
  norm = 0.0;
  for (int i = 0; i < 8; ++i) {
    norm += u._elements[i] * u._elements[i];
  }
  norm = std::sqrt(norm);
  for (int i = 0; i < 8; ++i) {
    u._elements[i] /= norm;
  }

  // Step 2: Construct Householder matrix H = I - 2uu^T
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      result._elements[i][j] = -2.0 * u._elements[i] * u._elements[j];
      if (i == j) {
        result._elements[i][j] += 1.0;
      }
    }
  }

  return result;
}

///////////////////////////////////////////////////////////////////////////////

mtx8f mtx8f::permute(int seed) {
  // create energy preserving (orthogonal) matrix
  // for use in FDN 8 (internal) channel reverb
  //  allowing for negation of a random number of channels
  //  and random permutation of channels
  std::mt19937 rng(seed); // Random number generator with seed
  std::uniform_int_distribution<int> dist(0, 1);

  mtx8f initial;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      initial._elements[i][j] = float(i == j);
    }
  }

  // Step 1: Negation of a Random Number of Channels
  for (int i = 0; i < 8; ++i) {
    if (dist(rng)) { // 50% chance to negate the channel
      for (int j = 0; j < 8; ++j) {
        initial._elements[i][j] *= -1;
      }
    }
  }

  // Step 2: Random Permutation of Channels
  std::vector<int> permutation(8);
  std::iota(permutation.begin(), permutation.end(), 0); // Fill with 0, 1, ..., 7
  std::shuffle(permutation.begin(), permutation.end(), rng);

  mtx8f permuted;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      permuted._elements[i][j] = initial._elements[permutation[i]][j];
    }
  }

  return permuted;
}

///////////////////////////////////////////////////////////////////////////////

vec8f mtx8f::column(int i) const {
  vec8f result;
  for (int j = 0; j < 8; ++j) {
    result._elements[j] = _elements[j][i];
  }
  return result;
}

void mtx8f::setToIdentity(){
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
        _elements[i][j] = (i == j) ? 1.0f : 0.0f;
    }
  }
}
void mtx8f::embed3DRotation(const fmtx3& rotationMatrix, //
                            const int dimsU[3],
                            const int dimsV[3]) {
    // when this matrix is multiplied itself any number of times
    //. it should result in an energy preserving othogonal matrix
    //  that rotates the 3 dimensions specified by dimsU and dimsV
    setToIdentity();

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            _elements[dimsU[i]][dimsV[j]] = rotationMatrix[i][j];
        }
    }
// Zero out the other elements in the affected rows and columns
        for (int i = 0; i < 3; ++i) {
            for (int j = 0; j < 8; ++j) {
                if (std::find(dimsV, dimsV + 3, j) == dimsV + 3) {
                    _elements[dimsU[i]][j] = 0.0f;
                }
                if (std::find(dimsU, dimsU + 3, j) == dimsU + 3) {
                    _elements[j][dimsV[i]] = 0.0f;
                }
            }
        }}

mtx8f mtx8f::concat(const mtx8f& rhs) const{
  mtx8f result;
  for (int i = 0; i < 8; ++i) {
    for (int j = 0; j < 8; ++j) {
      result._elements[i][j] = 0.0f;
      for (int k = 0; k < 8; ++k) {
        result._elements[i][j] += _elements[i][k] * rhs._elements[k][j];
      }
    }
  }
  return result;
}
  void mtx8f::newRot(){

  int i = rand() & 0xffff - 0x8000;
  int j = rand() & 0xffff - 0x8000;
  int k = rand() & 0xffff - 0x8000;
  auto axis = fvec3(float(i) / float(0x8000), //
                    float(j) / float(0x8000), //
                    float(k) / float(0x8000)).normalized();
  float angle = (float(rand() & 0xffff - 0x8000) / float(0x8000))*0.01f;

  // create random sel of 3 axes from 8 space
  int dimsA[3];
  int dimsB[3];
  for (int i = 0; i < 3; ++i) {
    dimsA[i] = rand() & 0x7;
    dimsB[i] = rand() & 0x7;
  }
  auto Q = fquat(axis, angle);
  auto R = Q.toMatrix3();
  this->embed3DRotation(R, dimsA, dimsB);
    }
void mtx8f::scramble(){
  int i = rand() & 0x3;

  switch(i){ 
    case 0: { // rotate left
      mtx8f rr;
      rr.newRot();
      //(*this) = this->concat(rr);
      break;
    }
    /*case 1: { // rotate up
      for( int j=0; j<8; j++ ){
        float tmp = _elements[j][0];
        for( int k=0; k<7; k++ ){
          _elements[j][k] = _elements[j][k+1];
        }
        _elements[j][7] = tmp;
      }
      break;
    }*/
    case 2: { // flip left/right
      for( int j=0; j<4; j++ ){
        for( int k=0; k<8; k++ ){
          float tmp = _elements[j][k];
          _elements[j][k] = _elements[7-j][k];
          _elements[7-j][k] = tmp;
        }
      }
      break;
    }
    
    /*
    case 3: { // flip up/down
      for( int j=0; j<4; j++ ){
        for( int k=0; k<8; k++ ){
          float tmp = _elements[k][j];
          _elements[k][j] = _elements[k][7-j];
          _elements[k][7-j] = tmp;
        }
      }
      break;
    }*/
  }
}

void mtx8f::dump() const{
        std::cout << "Matrix8x8 " << this << "\n{\n";

        for (int i = 0; i < 8; ++i) {
            std::cout << "   |";
            for (int j = 0; j < 8; ++j) {
                std::cout << std::setw(8) << std::setprecision(4) << std::fixed << _elements[i][j] << " |";
            }
            std::cout << "\n";
        }

        std::cout << "}\n";

}

///////////////////////////////////////////////////////////////////////////////

float vec8f::dotWith(const vec8f& oth) const {
  float result = 0.0f;
  for (int i = 0; i < 8; ++i) {
    result += _elements[i] * oth._elements[i];
  }
  return result;
}

///////////////////////////////////////////////////////////////////////////////

vec8f vec8f::operator+(const vec8f& oth) const {
  vec8f rval;
  for( int i=0; i<8; i++ ){
    rval._elements[i] = _elements[i] + oth._elements[i];
  }
  return rval;
}

vec8f vec8f::operator-(const vec8f& oth) const {
  vec8f rval;
  for( int i=0; i<8; i++ ){
    rval._elements[i] = _elements[i] - oth._elements[i];
  }
  return rval;
}

vec8f vec8f::operator*(float scalar) const {
  vec8f rval;
  for( int i=0; i<8; i++ ){
    rval._elements[i] = _elements[i] * scalar;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

vec8f mtx8f::transform(const vec8f& input) const{
  vec8f result;
  for (int j = 0; j < 8; ++j) {
    result._elements[j] = 0.0f;
    for (int k = 0; k < 8; ++k) {
    result._elements[j] += _elements[j][k] * input._elements[k];
    }
  }
  return result;
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::audio::singularity
