#include <ork/lev2/config.h>
#if defined(ENABLE_CORE_AUDIO)

#include "CoreAudioBuffer.h"
#include "CoreAudioBuffer.hpp"
#include <libkern/OSAtomic.h>
#include <stdio.h>
#include <math.h>
// #include <OpenGL/gl.h>
// #include <OpenGL/glu.h>
#include <ork/kernel/fixedstring.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::lev2::ca {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
LayerFragment::LayerFragment() {
}
///////////////////////////////////////////////////////////////////////////////
void LayerFragment::Init(int inumchannels, int inumfr) {
  mNumFrames = inumfr;
  mFrameAccum += mNumFrames;

  if (inumchannels != mNumChannels) {
    if (mChannels)
      delete[] mChannels;

    // printf( "Allocating LayerFragment<%p> numch<%d> framesize<%d>\n", this, inumchannels, inumfr );

    mChannels    = new Fragment[inumchannels];
    mNumChannels = inumchannels;
  }

  for (int i = 0; i < inumchannels; i++) {
    mChannels[i].mNumFrames  = inumfr;
    mChannels[i].mFrameAccum = mFrameAccum;
    mChannels[i].Clear();
  }
}
///////////////////////////////////////////////////////////////////////////////
void LayerFragment::Copy(const LayerFragment* from) {
  Init(from->mNumChannels, from->mNumFrames);
  mFrameAccum = from->mFrameAccum;

  auto copy = [](const Fragment& src, Fragment& dst) {
    assert(dst.mNumFrames == src.mNumFrames);
    dst.mFrameAccum = src.mFrameAccum;
    memcpy((void*)dst.mSampleData, (const void*)src.mSampleData, src.mNumFrames * sizeof(float));
  };

  for (int ich = 0; ich < mNumChannels; ich++) {
    const auto& src = from->mChannels[ich];
    auto& dst       = this->mChannels[ich];
    copy(src, dst);
  }
  // copy(*from->mMixLeft,*mMixLeft);
  // copy(*from->mMixRight,*mMixRight);
}
///////////////////////////////////////////////////////////////////////////////
void LayerFragment::MixDown(StereoFragment* dest) const {
  float baselevel = 1.0f;

  if (mLayer)
    baselevel = mLayer->mAmplitude;

  for (int ch = 0; ch < mNumChannels; ch++) {
    bool osel  = ch & 1;
    float LMIX = osel ? 0.0f : baselevel;
    float RMIX = osel ? baselevel : 0.0f;

    auto& input_ch = this->mChannels[ch];
    auto trk       = input_ch.mTrack;

    if (trk) {
      LMIX *= trk->mAmplitude;
      RMIX *= trk->mAmplitude;
    }

    dest->mMixLeft.Sum(input_ch, LMIX);
    dest->mMixRight.Sum(input_ch, RMIX);
  }
}
///////////////////////////////////////////////////////////////////////////////
LayerFragment::~LayerFragment() {
  delete[] mChannels;
}
///////////////////////////////////////////////////////////////////////////////
Layer::Layer()
    : mName("layer") {
}
Layer::~Layer() {
  printf("deleting layer<%p>\n", this);
}
///////////////////////////////////////////////////////////////////////////////
void Layer::Init(int defsize) {
  printf("layer<%p> init size<%d>\n", this, defsize);
  mFragments.resize(defsize);
}
///////////////////////////////////////////////////////////////////////////////
const LayerFragment* Layer::GetFragment(int idx) const {
  if (idx >= mFragments.size()) {
    printf("Lyr<%p> getfrag<%d> nfrags<%d>\n", this, idx, (int)mFragments.size());
  }
  assert(idx < mFragments.size());
  return mFragments[idx];
}
///////////////////////////////////////////////////////////////////////////////
LayerFragment* Layer::GetFragment(int idx) {
  if (idx >= mFragments.size()) {
    printf("Lyr<%p> getfrag<%d> nfrags<%d>\n", this, idx, (int)mFragments.size());
  }
  assert(idx < mFragments.size());
  return mFragments[idx];
}
///////////////////////////////////////////////////////////////////////////////
void Layer::SetFragment(int idx, LayerFragment* f) {
  assert(f != nullptr);

  f->mLayer = this;

  if (idx == mFragments.size())
    mFragments.push_back(f);
  else if (idx < mFragments.size())
    mFragments[idx] = f;
  else {
    printf("layt<%p> setfrg<%d> cnt<%d>\n", this, idx, (int)mFragments.size());
    assert(false);
  }
}
///////////////////////////////////////////////////////////////////////////////
void Layer::AppendFragment(LayerFragment* f) {
  assert(f != nullptr);
  mFragments.push_back(f);
  f->mLayer = this;
}
///////////////////////////////////////////////////////////////////////////////
void Layer::ClearFragments() {
  for (auto f : mFragments)
    f->mLayer = nullptr;

  mFragments.clear();
}
///////////////////////////////////////////////////////////////////////////////
void Layer::ResizeFragments(int newsize) {
  printf("layer<%p> resize<%d>\n", this, newsize);
  mFragments.resize(newsize, nullptr);
}
///////////////////////////////////////////////////////////////////////////////
Layer* Layer::Double() const {
  auto pret      = new Layer;
  pret->mName    = this->mName + "_D";
  pret->mEnabled = mEnabled;

  int inumfrgs = mFragments.size();
  printf("doubling layer<%p:%s> inumfrgs<%d>\n", this, this->mName.c_str(), inumfrgs);
  for (int j = 0; j < 2; j++)
    for (int ifrg = 0; ifrg < inumfrgs; ifrg++) {
      const auto f = mFragments[ifrg];
      if (f) {
        auto nf = new LayerFragment;
        // printf( "ifrg<%d> inumfrgs<%d> f<%p> nf<%p>\n", ifrg, inumfrgs, f, nf );
        nf->Copy(f);
        pret->AppendFragment(nf);
      } else {
        // printf( "ifrg<%d> inumfrgs<%d> f<%p>\n", ifrg, inumfrgs, f );
      }
    }
  inumfrgs = pret->mFragments.size();
  printf("doubled layer<%p> new<%s> inumfrgs<%d>\n", this, pret->mName.c_str(), inumfrgs);
  return pret;
}
///////////////////////////////////////////////////////////////////////////////
Layer* Layer::Clone() const {
  auto pret      = new Layer;
  pret->mName    = this->mName + "_C";
  pret->mEnabled = mEnabled;

  int inumfrgs = mFragments.size();
  printf("cloning layer<%p:%s> inumfrgs<%d>\n", this, this->mName.c_str(), inumfrgs);

  for (int ifrg = 0; ifrg < inumfrgs; ifrg++) {
    const auto f = mFragments[ifrg];
    if (f) {
      auto nf = new LayerFragment;
      // printf( "ifrg<%d> inumfrgs<%d> f<%p> nf<%p>\n", ifrg, inumfrgs, f, nf );
      nf->Copy(f);
      pret->AppendFragment(nf);
    } else {
      // printf( "ifrg<%d> inumfrgs<%d> f<%p>\n", ifrg, inumfrgs, f );
    }
  }
  inumfrgs = pret->mFragments.size();
  printf("cloned layer<%p> new<%s> inumfrgs<%d>\n", this, pret->mName.c_str(), inumfrgs);
  pret->mTextureObject = mTextureObject;
  return pret;
}
///////////////////////////////////////////////////////////////////////////////
void Layer::Trackify() {
  ////////////
  // count tracks
  ////////////

  int inumtracks = 0;

  for (auto lf : mFragments) {
    int inumch = lf->mNumChannels;
    if (inumch > inumtracks)
      inumtracks = inumch;
  }

  ////////////
  // create tracks
  ////////////

  for (int i = 0; i < inumtracks; i++)
    mTracks.push_back(new Track);

  ////////////

  for (auto lf : mFragments) {
    int inumch = lf->mNumChannels;

    ///////////////////
    // it wouldnt really make sense
    //  if all fragments did not have the
    //  same # of channels
    ///////////////////

    assert(inumch == inumtracks);

    for (int ch = 0; ch < inumch; ch++) {
      auto trkout   = mTracks[ch];
      auto chbuf    = lf->mChannels + ch;
      chbuf->mTrack = trkout;
      trkout->mFragments.push_back(chbuf);
    }
  }
}
///////////////////////////////////////////////////////////////////////////////
void Layer::MakeTexture() { /*
                               glGenTextures( 1, (GLuint*) & mTextureObject );
                               glBindTexture(GL_TEXTURE_2D,mTextureObject);
                               glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);
                               glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
                               glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                               glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                               //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                               //glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
                               glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                               glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

                               int inumlayerf = GetNumFragments();
                               int height = 32;
                               int hm1 = height-1;

                               auto pdata = (uint32_t*) calloc(inumlayerf*height,4);


                               for( int x=0; x<inumlayerf; x++ )
                               {
                                   auto lf = GetFragment(x);
                                   int inumch = lf->mNumChannels;
                                   float pwraccum = 0.0f;
                                   for( int ich=0; ich<inumch; ich++ )
                                   {
                                       const auto& ch = lf->mChannels[ich];
                                       pwraccum += ch.ComputePower();
                                   }
                                   //printf( "x<%d> pwraccum<%f>\n", x, pwraccum );
                                   int hd2 = height*0.5f;
                                   int ipwr = pwraccum*hd2;
                                   int y0 = hd2-ipwr;
                                   int y1 = hd2+ipwr;
                                   if( y0<0 ) y0=0;
                                   if( y1>hm1 ) y1=hm1;
                                   for( int iy=y0; iy<=y1; iy++ )
                                   {
                                       int addr = iy*inumlayerf+x;
                                       pdata[addr]=0xffffffff;
                                   }
                                   //printf( "pwraccum<%f>\n", pwraccum );
                               }

                               glTexImage2D(	GL_TEXTURE_2D,
                                               0,
                                               GL_RGBA,
                                               inumlayerf,
                                               height,
                                               0,
                                               GL_RGBA,
                                               GL_UNSIGNED_BYTE,
                                               pdata );

                               printf( "Layer<%p> MakeTexture inumlayerf<%d>\n", this, inumlayerf );

                               auto mip = gluBuild2DMipmaps(	GL_TEXTURE_2D,
                                                               GL_RGBA,
                                                               inumlayerf,
                                                               128,
                                                               GL_RGBA,
                                                               GL_UNSIGNED_BYTE,
                                                               pdata );

                               glBindTexture(GL_TEXTURE_2D,0);
                           */
}
///////////////////////////////////////////////////////////////////////////////
template struct ObjectPool<LayerFragment>;
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::lev2::ca
///////////////////////////////////////////////////////////////////////////////

#endif // #if defined(ENABLE_CORE_AUDIO)
