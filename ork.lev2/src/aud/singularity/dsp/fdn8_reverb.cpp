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

namespace ork::audio::singularity {

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

vec8f ParallelDelay::output(float fi){
  vec8f rval;
  for( int j=0; j<8; j++ ){
    rval._elements[j] = _delay[j].out(fi);
  }
  return rval;
}
void ParallelDelay::input(const vec8f& input){
  for( int j=0; j<8; j++ ){
    float x = input._elements[j];
    float y = _dcblock2[j].compute(x);
    _delay[j].inp(y);
  }
}
ParallelDelay::ParallelDelay(){
  for( int j=0; j<8; j++ ){
    _dcblock[j].Clear();
    _dcblock[j].SetHpf(120.0f);
    _dcblock2[j].clear();
    _dcblock2[j].set(120.0f,1.0f/getInverseSampleRate());
  }
}
///////////////////////////////////////////////////////////////////////////////

Fdn8ReverbData::Fdn8ReverbData(std::string name)
    : DspBlockData(name) {
  _blocktype     = "Fdn8Reverb";
  auto mix_param = addParam();
  mix_param->useDefaultEvaluator();
  ////////////////////////////////////
  // generate matrices
  ////////////////////////////////////
  _hadamard = mtx8f::generateHadamard();
  vec8f unit_x;
  unit_x._elements[0] = 1.0f;
  _householder        = mtx8f::householder(unit_x);
  _permute            = mtx8f::permute(0);
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

dspblk_ptr_t Fdn8ReverbData::createInstance() const { // override
  return std::make_shared<Fdn8Reverb>(this);
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8ReverbData::update() {
}

///////////////////////////////////////////////////////////////////////////////

Fdn8Reverb::Fdn8Reverb(const Fdn8ReverbData* dbd)
    : DspBlock(dbd)
    , _mydata(dbd) {
  ///////////////////////////
  float input_g  = 1.0/256.0; // dbd->_input_gain;
  float output_g = 0.5f; // dbd->_output_gain;
  float tbase    = 0.02; // dbd->_time_base;
  ///////////////////////////
  // matrixHadamard(0.0);
  ///////////////////////////
  _inputGain  = input_g;
  _outputGain = output_g;
  ///////////////////////////
  ///////////////////////////
  for( int i=0; i<k_num_diffusers; i++ ){
    float this_time = tbase*powf(2.0f,float(i*0.5));
    _diffuser[i].setTime(this_time);
  }
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8Reverb::compute(DspBuffer& dspbuf) // final
{
  int inumframes = _layer->_dspwritecount;
  int ibase      = _layer->_dspwritebase;
  float mix      = _param[0].eval();

  auto ilbuf = getInpBuf(dspbuf, 0) + ibase;
  auto irbuf = getInpBuf(dspbuf, 1) + ibase;
  auto olbuf = getOutBuf(dspbuf, 0) + ibase;
  auto orbuf = getOutBuf(dspbuf, 1) + ibase;

  float invfr = 1.0f / inumframes;

  vec8f split;

  for( int i=0; i<k_num_diffusers; i++ ){
    _diffuser[i].tick();
  }
  float val = sinf(_fbmodphase);
  for( int i=0; i<8; i++ ){
    float fbtime = _fbbasetimes[i];
    fbtime *= (1.0f+val*_fbmodulations[i]);
    _fbdelay._delay[i].setNextDelayTime(fbtime);
  }
  _fbmodphase += 0.0017f;

  for (int i = 0; i < inumframes; i++) {
    float fi = float(i) * invfr;

    /////////////////////////////////////
    // input/split from dsp channels
    /////////////////////////////////////

    float inl  = ilbuf[i] * _inputGain;
    float inr  = irbuf[i] * _inputGain;
    float finl = inl*_hipassfilterL.compute(inl);
    float finr = inr*_hipassfilterR.compute(inr);

    split.broadcast(fvec2(finl,finr));

    /////////////////////////////////////
    // diffusion step
    /////////////////////////////////////

    vec8f d0 = _diffuser[0].process(split, fi);
    vec8f d1 = _diffuser[1].process(d0, fi);
    vec8f d2 = _diffuser[2].process(d1, fi);
    vec8f d3 = _diffuser[3].process(d2, fi);
    vec8f d4 = _diffuser[4].process(d3, fi);
    vec8f d5 = _diffuser[5].process(d4, fi);
    vec8f d6 = _diffuser[6].process(d5, fi);
    vec8f d7 = _diffuser[7].process(d6, fi);

    /////////////////////////////////////
    // early reflections
    /////////////////////////////////////

    _early_refl.input( //
        (d0*0.5f) + //
        (d1*0.4f) + //
        (d2*0.3f) + //
        (d3*0.2f) + //
        (d4*0.1f) + //
        (d5*0.05f) + //
        (d6*0.025f) + //
        (d7*0.0125f) //
    );

    /////////////////////////////////////
    // feedback step
    /////////////////////////////////////

    vec8f fbout = _fbdelay.output(fi);
    auto x = _householder.transform(fbout*0.85);
    auto I = (x+d7)*0.99999f;
    _fbdelay.input(I + _nodenorm);

    /////////////////////////////////////
    // mix down
    /////////////////////////////////////

    fvec2 erefl = _stereomix.mixdown(_early_refl.output(fi));

    fvec2 stereo =  _stereomix.mixdown(fbout) + erefl;

    olbuf[i] = ilbuf[i]+stereo.x*_outputGain;
    orbuf[i] = irbuf[i]+stereo.y*_outputGain;
  }
}

///////////////////////////////////////////////////////////////////////////////

Fdn8Reverb::DiffuserStep::DiffuserStep() {
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8Reverb::DiffuserStep::setTime(float time) {
  float tstep = time / float(8.0);
  for (int i = 0; i < 8; i++) {
    int irand    = rand() & 0xffff - 0x8000;
    float f      = float(irand) / float(0x8000);
    float tdelta = tstep * f;
    _basetimes[i] = (tstep * (float(i) + 0.5) + tdelta);
    _modulation[i] = _basetimes[i]*0.03f;
  }
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

void Fdn8Reverb::DiffuserStep::tick(){
  float s = sinf(_phase);
  for (int i = 0; i < 8; i++) {
    float t = _basetimes[i] + s*_modulation[i];
    _delays._delay[i].setNextDelayTime(t);
  }
  _phase += 0.001f;
}

vec8f Fdn8Reverb::DiffuserStep::process(const vec8f& input, float fi) {

  vec8f delay_outs = _delays.output(fi);
  vec8f I = input*0.99999f;
  _delays.input(I + _nodenorm);
  vec8f permuted = _permute.transform(delay_outs); // shuffle and invert (permute)
  vec8f h = _hadamard.transform(permuted);         // hadamard
  return h;
}

///////////////////////////////////////////////////////////////////////////////

void Fdn8Reverb::doKeyOn(const KeyOnInfo& koi) { // final

  for (int i = 0; i < k_num_diffusers; i++) {
    _diffuser[i]._hadamard    = _mydata->_hadamard;
    _diffuser[i]._permute     = _mydata->_permute;
    _diffuser[i]._nodenorm.broadcast(1e-6);
  }
  _householder = _mydata->_householder;

  _nodenorm.broadcast(1e-6);


  float basetime = 0.15;
  for( int j=0; j<8; j++ ){
    int i = rand() & 0x8000;
    float fi = float(i) / float(0x4000);
    fi = (fi-1.0f)*0.1;
    _fbbasetimes[j] = basetime+fi*basetime;
    _fbmodulations[j] = _fbbasetimes[j]*0.03;
    //_fbdelay._delay[j].setStaticDelayTime(basetime+fi*basetime);
    //_fbdelay._delay[j].setStaticDelayTime(basetime);
  }
  for( int j=0; j<8; j++ ){
    _early_refl._delay[j].setStaticDelayTime(0.08);
  }

  _hipassfilterL.Clear();
  _hipassfilterR.Clear();
  _hipassfilterL.SetHpf(_mydata->_hipass_cutoff);
  _hipassfilterR.SetHpf(_mydata->_hipass_cutoff);
}

///////////////////////////////////////////////////////////////////////////////

} // namespace ork::audio::singularity
