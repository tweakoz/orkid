////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
#include <ork/pch.h>
#include <ork/math/multicurve.h>
#include <ork/kernel/orklut.hpp>
#include <ork/reflect/properties/registerX.inl>
#include <ork/reflect/properties/DirectTypedMap.hpp>
#include <ork/reflect/properties/DirectTypedVector.hpp>
#include <ork/reflect/enum_serializer.inl>
#include <math.h>

ImplementReflectionX(ork::MultiCurve1D, "MultiCurve1D");

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////
template class orklut<std::string,multicurve1d_ptr_t>;

BeginEnumRegistration(MultiCurveSegmentType);
RegisterEnum(MultiCurveSegmentType, LINEAR);
RegisterEnum(MultiCurveSegmentType, BOX);
RegisterEnum(MultiCurveSegmentType, LOG);
RegisterEnum(MultiCurveSegmentType, EXP);
EndEnumRegistration();

ImplementEnumSerializer(MultiCurveSegmentType);

void MultiCurve1D::describeX(object::ObjectClass* clazz) {
  InvokeEnumRegistration(MultiCurveSegmentType);

  clazz
      ->directVectorProperty("Segs", &ork::MultiCurve1D::mSegmentTypes) //
      ->annotate<bool>("editor.visible", false);
  clazz
      ->directMapProperty("Verts", &ork::MultiCurve1D::mVertices) //
      ->annotate<bool>("editor.visible", false);

  clazz->floatProperty("min", float_range{-2000.0f, +2000.0f}, &ork::MultiCurve1D::mMin);
  clazz->floatProperty("max", float_range{-2000.0f, +2000.0f}, &ork::MultiCurve1D::mMax);

  static const char* EdGrpStr = "sort://min max Curve";
  static const char* NodeFactory = "GedNodeFactoryCurve1D";

  clazz->annotate("editor.prop.groups", EdGrpStr);
  clazz->annotate("editor.ged.node.factory", NodeFactory);
}

///////////////////////////////////////////////////////////////////////////////

MultiCurve1D::MultiCurve1D()
    : mMin(0.0f)
    , mMax(1.0f) {
  Init(1);
}

///////////////////////////////////////////////////////////////////////////////

void MultiCurve1D::Init(int inumsegs) {
  OrkAssert(inumsegs > 0);

  mSegmentTypes.reserve(inumsegs);
  mVertices.reserve(inumsegs + 1);

  mSegmentTypes.clear();
  mVertices.clear();

  for (int i = 0; i < inumsegs; i++) {
    mSegmentTypes.push_back(MultiCurveSegmentType::LINEAR);
    float fu = float(i) / float(inumsegs);
    mVertices.AddSorted(fu, 0.0f);
  }
  mVertices.AddSorted(1.0f, 1.0f);

  OrkAssert(IsOk() == true);
}

///////////////////////////////////////////////////////////////////////////////

bool MultiCurve1D::IsOk() const {
  int inumseg = GetNumSegments();
  int inumV   = int(mVertices.size());

  bool bOK = true;

  bOK &= (inumseg == (inumV - 1));
  bOK &= (mVertices.begin()->first == 0.0f);
  bOK &= ((mVertices.end() - 1)->first == 1.0f);

  OrkAssert(bOK); // temporary

  return bOK;
}

///////////////////////////////////////////////////////////////////////////////

int MultiCurve1D::GetNumSegments() const {
  return int(mSegmentTypes.size());
}

///////////////////////////////////////////////////////////////////////////////

float MultiCurve1D::Sample(float fu) const {
  if (fu < 0.0f)
    fu = 0.0f;
  if (fu > 1.0f)
    fu = 1.0f;
  if (isnan(fu))
    fu = 0.0f;

  bool bdone = false;
  int isega  = 0;
  int isegb  = 0;
  int inumv  = int(mVertices.size());
  if (0 == inumv)
    return 0.0f;

  while (false == bdone) {
    isegb = (isega + 1);
    if (isega >= inumv)
      return 0.0f;
    if (isegb >= inumv)
      return 0.0f;

    if ((fu >= mVertices.GetItemAtIndex(isega).first) && (fu <= mVertices.GetItemAtIndex(isegb).first)) {
      bdone = true;
    } else {
      isega++;
    }
  }

  float rval = 0.0f;

  const std::pair<float, float>& VA = mVertices.GetItemAtIndex(isega);
  const std::pair<float, float>& VB = mVertices.GetItemAtIndex(isegb);

  float dU    = VB.first - VA.first;
  float dV    = VB.second - VA.second;
  float Slope = dV / dU;
  float Base  = VA.first;

  float fsu = (fu - Base) / dU;

  switch (mSegmentTypes[isega]) {
    case MultiCurveSegmentType::LINEAR: {
      float fisu = 1.0f - fsu;
      rval       = (VA.second * fisu) + (VB.second * fsu);
      break;
    }
    case MultiCurveSegmentType::LOG: {
      float fsup = fsu + (fsu - (fsu * fsu));
      float fisu = 1.0f - fsup;
      rval       = (VA.second * fisu) + (VB.second * fsup);
      break;
    }
    case MultiCurveSegmentType::EXP: {
      float fsup = (fsu * fsu);
      float fisu = 1.0f - fsup;
      rval       = (VA.second * fisu) + (VB.second * fsup);
      break;
    }
    case MultiCurveSegmentType::BOX: {
      rval = VA.second;
      break;
    }
    default:
      OrkAssert(false);
      break;
  }

  return mMin + rval * (mMax - mMin);
}

///////////////////////////////////////////////////////////////////////////////

void MultiCurve1D::SplitSegment(int iseg) {
  int inumseg = GetNumSegments();
  OrkAssert(iseg < inumseg);
  OrkAssert(iseg >= 0);
  OrkAssert(iseg < int(mVertices.size() + 1));

  int iva                           = iseg;
  int ivb                           = iseg + 1;
  const std::pair<float, float>& VA = mVertices.GetItemAtIndex(iva);
  const std::pair<float, float>& VB = mVertices.GetItemAtIndex(ivb);

  mVertices.AddSorted((VA.first + VB.first) * 0.5f, (VA.second + VB.second) * 0.5f);

  mSegmentTypes.insert(mSegmentTypes.begin() + ivb, MultiCurveSegmentType::LINEAR);

  OrkAssert(IsOk() == true);
}

///////////////////////////////////////////////////////////////////////////////

void MultiCurve1D::MergeSegment(int ifirstseg) {
  int inumseg = GetNumSegments();

  OrkAssert(ifirstseg < inumseg);
  OrkAssert(ifirstseg >= 0);

  orklut<float, float>::iterator it = mVertices.begin() + ifirstseg;
  mVertices.RemoveItem(it);

  orkvector<MultiCurveSegmentType>::iterator it2 = mSegmentTypes.begin() + ifirstseg;
  mSegmentTypes.erase(it2);

  OrkAssert(IsOk() == true);
}

///////////////////////////////////////////////////////////////////////////////

void MultiCurve1D::SetSegmentType(int iseg, MultiCurveSegmentType etype) {
  int inumseg = GetNumSegments();
  OrkAssert(iseg < inumseg);
  OrkAssert(iseg >= 0);

  mSegmentTypes[iseg] = etype;

  OrkAssert(IsOk() == true);
}

///////////////////////////////////////////////////////////////////////////////

void MultiCurve1D::SetPoint(int ipoint, float fu, float fv) {
  int inumV = int(mVertices.size());

  OrkAssert(ipoint <= inumV);
  OrkAssert(ipoint >= 0);

  orklut<float, float>::iterator it = mVertices.begin() + ipoint;

  mVertices.RemoveItem(it);
  mVertices.AddSorted(fu, fv);

  OrkAssert(IsOk() == true);
}

///////////////////////////////////////////////////////////////////////////////

bool MultiCurve1D::preDeserialize(ork::reflect::serdes::IDeserializer& deser) {
  mVertices.clear();
  mSegmentTypes.clear();
  return true;
}

bool MultiCurve1D::postDeserialize(reflect::serdes::IDeserializer&) {
  return (true);
}

} // namespace ork
///////////////////////////////////////////////////////////////////////////////
