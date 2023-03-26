////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once
#include "spline.h"

//////////////////////////////////////////////////////////
namespace ork::math {
//////////////////////////////////////////////////////////

template <typename T>
bool MapU(
    float fu,
    const float fmax,
    const typename orkmap<float, T>& themap,
    float& flower,
    float& fupper,
    float& frange,
    float& flerp,
    int& indexl) {
  bool brv = false;

  if (fu < 0.0f)
    fu = 0.0f;
  if (fu > fmax)
    fu = 1.0f;

  typename orkmap<float, T>::const_iterator itu = themap.upper_bound(fu);

  indexl = -1;

  if (itu == themap.end()) {
    OrkAssert(false);
    itu--;
    // rval = itu->second;
    flerp  = 1.0f;
    flower = -1.0f;
    fupper = -1.0f;
    frange = 0.0f;
  } else {
    typename orkmap<float, T>::const_iterator itl = itu;
    itl--;

    if (itl != themap.end()) {
      indexl = std::distance(themap.begin(), itl);
      fupper = itu->first;
      flower = itl->first;
      frange = fupper - flower;
      flerp  = (fu - flower) / frange;
      brv    = true;
    }
  }

  // orkprintf( "MapU<%f> flower<%f> fupper<%f> flerp<%f> indexl<%d>\n", fu, flower, fupper, flerp, indexl );

  return brv;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T>
bool VecU(
    float fu,
    const float fmax,
    const typename orkvector<T>& themap,
    float& flower,
    float& fupper,
    float& frange,
    float& flerp,
    int& indexl) {
  bool brv = false;

  // if( fu<0.0f ) fu = 0.0f;
  // if( fu>fmax ) fu -= fmax;

  indexl = int(fu);

  fupper = float(indexl + 1);
  flower = float(indexl);
  frange = fupper - flower;
  flerp  = (fu - flower) / frange;
  brv    = true;

  // orkprintf( "VecU<%f> flower<%f> fupper<%f> flerp<%f> indexl<%d>\n", fu, flower, fupper, flerp, indexl );

  return brv;
}

//////////////////////////////////////////////////////////

template <typename T>
CatmullRomSpline<T>::CatmullRomSpline()
    : mbClosed(true) {
}

//////////////////////////////////////////////////////////

template <typename T> void CatmullRomSpline<T>::AddCV(const T& cv) {
  mSeqVertices.push_back(cv);

  int inumcv = int(mSeqVertices.size());

  mBases.resize(inumcv * knumcomponents);
  mBasesDeriv.resize(inumcv * knumcomponents);

  for (int i = 0; i < inumcv; i++) {
    GenBases(i);
  }
}

//////////////////////////////////////////////////////////

template <typename T> void CatmullRomSpline<T>::GetCV(int idx, T& out) const {
  out = mSeqVertices[idx];
}

//////////////////////////////////////////////////////////

template <typename T> void CatmullRomSpline<T>::ClearCVS(void) {
  mSeqVertices.clear();
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> void CatmullRomSpline<T>::GenBases(int pt) const {
  OrkAssert(pt >= 0);
  OrkAssert(pt < NumCVs());

  static T t0, t1, t2, t3;
  float c[4];

  GetCV(GetCVIndex(pt - 1), t0);
  GetCV(GetCVIndex(pt + 0), t1);
  GetCV(GetCVIndex(pt + 1), t2);
  GetCV(GetCVIndex(pt + 2), t3);

  // q(t) = 0.5 *(    	(2 * P1) +
  //		(-P0 + P2) * t +
  //		(2*P0 - 5*P1 + 4*P2 - P3) * t2 +
  //		(-P0 + 3*P1- 3*P2 + P3) * t3)

  for (int i = 0; i < knumcomponents; i++) {

    float cv0 = t0.GetComponent(i);
    float cv1 = t1.GetComponent(i);
    float cv2 = t2.GetComponent(i);
    float cv3 = t3.GetComponent(i);

    c[0] = (-cv0) + (3.0f * cv1) + (-3.0f * cv2) + (cv3);
    c[1] = (2.0f * cv0) + (-5.0f * cv1) + (4.0f * cv2) + (-cv3);
    c[2] = (-cv0) + (cv2);
    c[3] = 2.0f * cv1;

    mBases[BaseIndex(pt, i)].SetCoefs(c);

    mBasesDeriv[BaseIndex(pt, i)] = mBases[BaseIndex(pt, i)].Differentiate();
  }
}

//////////////////////////////////////////////////////////

template <typename T> void CatmullRomSpline<T>::SampleAt(float fU, T& res, T& deriv) const {
  float fmax = float(NumCVs());

  float flower, fupper, frange, flerp;
  int indexl;
  bool bOK = VecU<T>(fU, fmax, mSeqVertices, flower, fupper, frange, flerp, indexl);

  OrkAssert(bOK);
  OrkAssert(indexl >= 0);

  for (int ic = 0; ic < knumcomponents; ic++) {
    int ibi               = BaseIndex(indexl, ic);
    Polynomial& ply       = mBases[ibi];
    Polynomial& ply_deriv = mBasesDeriv[ibi];

    float fval = 0.5f * ply(flerp);
    float fder = 0.5f * ply_deriv(flerp);

    res.SetComponent(ic, fval);
    deriv.SetComponent(ic, fder);
  }
}

//////////////////////////////////////////////////////////

template <typename T> int CatmullRomSpline<T>::GetCVIndex(int in) const {
  int isiz = int(mSeqVertices.size());

  int out = -1;

  if (mbClosed) {
    out = in;

    while (out >= isiz) {
      out -= isiz;
    }
    while (out < 0) {
      out += isiz;
    }
  } else {
    OrkAssert(false);
    if (in >= isiz) {
      in = isiz - 1;
    } else if (in < 0) {
      in = 0;
    }
  }
  // orkprintf( "GetCVIndex(%d)=%d\n", in, out );
  OrkAssert(out != -1);
  OrkAssert(out < isiz);
  return out;
}

///////////////////////////////////////////////////////////////////////////////

template <typename T> int CatmullRomSpline<T>::BaseIndex(int ipoint, int icomponent) const {
  int ipt = GetCVIndex(ipoint);
  return (ipt * knumcomponents) + icomponent;
}

//////////////////////////////////////////////////////////
} // namespace ork::math
//////////////////////////////////////////////////////////
