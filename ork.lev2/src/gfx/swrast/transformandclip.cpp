////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include "lev3_test.h"
#include <math.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/Array.hpp>
#include <ork/math/collision_test.h>
#include <ork/math/sphere.h>
#include <ork/math/plane.hpp>
#include "render_graph.h"

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

TransformAndClipModule::TransformAndClipModule(const RenderData& rdata)
    : mRenderData(rdata) {
  mPostTransformTriangles.resize(512 << 10);
  mTriangleIndex.Store(0);

  //	for( int i=0; i<kmaxbuckets; i++ )
  //	{
  //		mMetaBuckets[i].Reset();
  //	}
}

void TriangleBucket::Reset() {
  if (0 == mPostTransformTriangles.size()) {
    mPostTransformTriangles.resize(32 << 10);
  }
  mTriangleIndex.Store(0);
}

void TriangleMetaBucket::Reset() {
  /*	for( orkmap<const rend_shader*,TriangleBucket*>::iterator it=mSubBuckets.begin(); it!=mSubBuckets.end(); it++ )
      {
          TriangleBucket* pucket = it->second;
          pucket->Reset();
      }
      mpLastShader = 0;
      mpLastBucket = 0;
  */
  for (int i = 0; i < kmaxbuckets; i++) {
    mBuckets[i].Reset();
  }
}

///////////////////////////////////////////////////////////////////////////////

struct TransformAndClipWuData {
  int miWorkUnit;
};

///////////////////////////////////////////////////////////////////////////////

void TransformAndClipModule::do_divide(ork::threadpool::thread_pool* tpool) {
  int inum = mNumSubTasks.Fetch();
  OrkAssert(inum == 0);
  TransformAndClipWuData std;
  miNumWorkUnits = 64;
  IncNumTasks(miNumWorkUnits);
  for (int i = 0; i < miNumWorkUnits; i++) {
    std.miWorkUnit                  = i;
    ork::threadpool::sub_task* psub = new ork::threadpool::sub_task(this);
    psub->SetData<TransformAndClipWuData>(std);
    tpool->AddSubTask(psub);
  }
}

///////////////////////////////////////////////////////////////////////////////

void TransformAndClipModule::do_onstarted() {
  mTriangleIndex.Store(0);

  for (orkmap<const rend_shader*, TriangleMetaBucket*>::iterator it = mMetaBuckets.begin(); it != mMetaBuckets.end(); it++) {
    TriangleMetaBucket* pmeta = it->second;
    pmeta->Reset();
  }
}

///////////////////////////////////////////////////////////////////////////////

void TransformAndClipModule::do_subtask_finished(const ork::threadpool::sub_task* tsk) {
  delete tsk;
}

///////////////////////////////////////////////////////////////////////////////

void TransformAndClipModule::do_onfinished() {
  //	const ork::RgmModel* pmodel = mRenderData.mpModel;
  //	void* hash = (void*) pmodel;
  //	mSourceHash = hash;
}

///////////////////////////////////////////////////////////////////////////////

bool boxisect(float fax0, float fay0, float fax1, float fay1, float fbx0, float fby0, float fbx1, float fby1) {
  if (fay1 < fby0)
    return false;
  if (fay0 > fby1)
    return false;
  if (fax1 < fbx0)
    return false;
  if (fax0 > fbx1)
    return false;
  return true;
}

///////////////////////////////////////////////////////////////////////////////

struct ivertex {
  float iX, iY;
};
struct irect {
  float l, r, t, b;
};
static bool isIntersecting(const irect& ir, float xa, float ya, float xb, float yb) {
  // Calculate m and c for the equation for the line (y = mx+c)
  float m = (yb - ya) / (xb - xa);
  float c = ya - (m * xa);

  if ((ya - ir.t) * (yb - ir.t) < 0.0f) {
    float s = (ir.t - c) / m;
    if (s > ir.l && s < ir.r)
      return true;
  }

  if ((ya - ir.b) * (yb - ir.b) < 0.0f) {
    float s = (ir.b - c) / m;
    if (s > ir.l && s < ir.r)
      return true;
  }

  if ((xa - ir.l) * (xb - ir.l) < 0.0f) {
    float s = m * ir.l + c;
    if (s > ir.t && s < ir.b)
      return true;
  }

  if ((xa - ir.r) * (xb - ir.r) < 0.0f) {
    float s = m * ir.r + c;
    if (s > ir.t && s < ir.b)
      return true;
  }

  return false;
}

static bool isInsideTriangle(const irect& ir, const ivertex& v0, const ivertex& v1, const ivertex& v2) {
  float l  = ir.l;
  float r  = ir.r;
  float t  = ir.t;
  float b  = ir.b;
  float x0 = v0.iX;
  float y0 = v0.iY;
  float x1 = v1.iX;
  float y1 = v1.iY;
  float x2 = v2.iX;
  float y2 = v2.iY;

  // Calculate m and c for the equation for the line (y = mx+c)
  float m0 = (y2 - y1) / (x2 - x1);
  float c0 = y2 - (m0 * x2);

  float m1 = (y2 - y0) / (x2 - x0);
  float c1 = y0 - (m1 * x0);

  float m2 = (y1 - y0) / (x1 - x0);
  float c2 = y0 - (m2 * x0);

  int t0 = int(x0 * m0 + c0 > y0);
  int t1 = int(x1 * m1 + c1 > y1);
  int t2 = int(x2 * m2 + c2 > y2);

  float lm0 = l * m0 + c0;
  float lm1 = l * m1 + c1;
  float lm2 = l * m2 + c2;

  float rm0 = r * m0 + c0;
  float rm1 = r * m1 + c1;
  float rm2 = r * m2 + c2;

  if (!(t0 ^ int(lm0 > t)) && !(t1 ^ int(lm1 > t)) && !(t2 ^ int(lm2 > t)))
    return true;

  if (!(t0 ^ int(lm0 > b)) && !(t1 ^ int(lm1 > b)) && !(t2 ^ int(lm2 > b)))
    return true;

  if (!(t0 ^ int(rm0 > t)) && !(t1 ^ int(rm1 > t)) && !(t2 ^ int(rm2 > t)))
    return true;

  if (!(t0 ^ int(rm0 > b)) && !(t1 ^ int(rm1 > b)) && !(t2 ^ int(rm2 > b)))
    return true;

  return false;
}

static bool triangleTest(const irect& ir, const ivertex& v0, const ivertex& v1, const ivertex& v2) {
  float x0 = v0.iX;
  float y0 = v0.iY;
  float x1 = v1.iX;
  float y1 = v1.iY;
  float x2 = v2.iX;
  float y2 = v2.iY;
  float l  = ir.l;
  float r  = ir.r;
  float t  = ir.t;
  float b  = ir.b;

  if (x0 > l)
    if (x0 < r)
      if (y0 > t)
        if (y0 < b)
          return true;
  if (x1 > l)
    if (x1 < r)
      if (y1 > t)
        if (y1 < b)
          return true;
  if (x2 > l)
    if (x2 < r)
      if (y2 > t)
        if (y2 < b)
          return true;

  if (isInsideTriangle(ir, v0, v1, v2))
    return true;

  return (isIntersecting(ir, x0, y0, x1, y1) || isIntersecting(ir, x1, y1, x2, y2) || isIntersecting(ir, x0, y0, x2, y2));
  // return false;
}

void TransformAndClipModule::do_process(const ork::threadpool::sub_task* tsk, const ork::threadpool::thread_pool_worker* ptpw) {
  const TransformAndClipWuData& subtaskdata = tsk->GetData<TransformAndClipWuData>();
  int iwu                                   = subtaskdata.miWorkUnit;

  //	const ork::RgmModel* pmodel = mRenderData.mpModel;
  //	void* hash = (void*) pmodel;

  if (0) {
    ork::msleep(1);
    return;
  }

  int inumsub = mRenderData.mpSrcMesh->miNumSubMesh;

  const ork::fmtx4& mtxM   = mRenderData.mMatrixM;
  const ork::fmtx4& mtxMV  = mRenderData.mMatrixMV;
  const ork::fmtx4& mtxMVP = mRenderData.mMatrixMVP;

  float fiW  = float(mRenderData.miImageWidth);
  float fiH  = float(mRenderData.miImageHeight);
  float fwd4 = fiW * 0.25f;
  float fhd4 = fiH * 0.25f;

  for (int is = 0; is < inumsub; is++) {
    const rend_srcsubmesh& Sub = mRenderData.mpSrcMesh->mpSubMeshes[is];
    // const ork::BakeShader* pbakeshader = Sub.mpShader;

    rend_shader* pshader = Sub.mpShader;

    TriangleMetaBucket* pmetabucket = mMetaBuckets.find(pshader)->second;

    int inumtri = (Sub.miNumTriangles);
    for (int it = iwu; it < inumtri; it += miNumWorkUnits) {
      // if( (it%16) < 12 ) continue;

      const rend_srctri& Tri = Sub.mpTriangles[it];
      //			const ork::fplane3& Plane = Tri.mFacePlane;
      const rend_srcvtx& TriV0 = Tri.mpVertices[0];
      const rend_srcvtx& TriV1 = Tri.mpVertices[1];
      const rend_srcvtx& TriV2 = Tri.mpVertices[2];

      //////////////////////////////////////
      // displacement/tesselation shader goes here
      //////////////////////////////////////

      //////////////////////////////////////

      const ork::fvec4& o0  = TriV0.mPos;
      const ork::fvec4& o1  = TriV1.mPos;
      const ork::fvec4& o2  = TriV2.mPos;
      ork::fvec4 w0         = o0.transform(mtxM);
      ork::fvec4 w1         = o1.transform(mtxM);
      ork::fvec4 w2         = o2.transform(mtxM);
      const ork::fvec3& on0 = TriV0.mVertexNormal;
      const ork::fvec3& on1 = TriV1.mVertexNormal;
      const ork::fvec3& on2 = TriV2.mVertexNormal;
      const ork::fvec3 wn0  = on0.transform3x3(mtxM).normalized();
      const ork::fvec3 wn1  = on1.transform3x3(mtxM).normalized();
      const ork::fvec3 wn2  = on2.transform3x3(mtxM).normalized();

      //////////////////////////////////////
      // trivial reject
      //////////////////////////////////////

      ork::fvec4 h0 = o0.transform(mtxMVP);
      ork::fvec4 h1 = o1.transform(mtxMVP);
      ork::fvec4 h2 = o2.transform(mtxMVP);

      ork::fvec4 hd0 = h0;
      ork::fvec4 hd1 = h1;
      ork::fvec4 hd2 = h2;
      hd0.perspectiveDivideInPlace();
      hd1.perspectiveDivideInPlace();
      hd2.perspectiveDivideInPlace();
      ork::fvec3 d0 = (hd1.xyz() - hd0.xyz());
      ork::fvec3 d1 = (hd2.xyz() - hd1.xyz());

      ork::fvec3 dX = d0.crossWith(d1);

      bool bFRONTFACE = (dX.z <= 0.0f);

      if (false == bFRONTFACE)
        continue;

      int inuminside = 0;

      inuminside +=
          (((hd0.x >= -1.0f) || (hd0.x <= 1.0f)) && ((hd0.y >= -1.0f) || (hd0.y <= 1.0f)) && ((hd0.z >= -1.0f) || (hd0.z <= 1.0f)));

      inuminside +=
          (((hd1.x >= -1.0f) || (hd1.x <= 1.0f)) && ((hd1.y >= -1.0f) || (hd1.y <= 1.0f)) && ((hd1.z >= -1.0f) || (hd1.z <= 1.0f)));

      inuminside +=
          (((hd2.x >= -1.0f) || (hd2.x <= 1.0f)) && ((hd2.y >= -1.0f) || (hd2.y <= 1.0f)) && ((hd2.z >= -1.0f) || (hd2.z <= 1.0f)));

      if (0 == inuminside) {
        continue;
      }

      //////////////////////////////////////

      float fX0  = (0.5f + hd0.x * 0.5f) * fiW;
      float fY0  = (0.5f + hd0.y * 0.5f) * fiH;
      float fX1  = (0.5f + hd1.x * 0.5f) * fiW;
      float fY1  = (0.5f + hd1.y * 0.5f) * fiH;
      float fX2  = (0.5f + hd2.x * 0.5f) * fiW;
      float fY2  = (0.5f + hd2.y * 0.5f) * fiH;
      float fZ0  = hd0.z;
      float fZ1  = hd1.z;
      float fZ2  = hd2.z;
      float fiZ0 = 1.0f / fZ0;
      float fiZ1 = 1.0f / fZ1;
      float fiZ2 = 1.0f / fZ2;
      //////////////////////////////////////
      // the triangle passed the backface cull and trivial reject, queue it

      int idx = mTriangleIndex.FetchAndIncrement();
      // continue;
      rend_triangle& rtri = mPostTransformTriangles[idx];

      rtri.mfArea   = Tri.mSurfaceArea;
      rtri.mpShader = pshader;

      rend_ivtx& v0 = rtri.mSVerts[0];
      rend_ivtx& v1 = rtri.mSVerts[1];
      rend_ivtx& v2 = rtri.mSVerts[2];

      v0.mSX           = fX0;
      v0.mSY           = fY0;
      v0.mfDepth       = fZ0;
      v0.mfInvDepth    = fiZ0;
      v0.mRoZ          = fiZ0; // TriV0.uv.x*v0.mfInvDepth;
      v0.mSoZ          = 0.0f; // TriV0.uv.x*v0.mfInvDepth;
      v0.mToZ          = 0.0f; // TriV0.uv.y*v0.mfInvDepth;
      v0.mWldSpacePos  = w0;
      v0.mObjSpacePos  = o0;
      v0.mObjSpaceNrm  = on0;
      v0.mWldSpaceNrm  = wn0;
      v1.mSX           = fX1;
      v1.mSY           = fY1;
      v1.mfDepth       = fZ1;
      v1.mfInvDepth    = fiZ1;
      v1.mRoZ          = 0.0f; // TriV0.uv.x*v0.mfInvDepth;
      v1.mSoZ          = fiZ1; // TriV1.uv.x*v1.mfInvDepth;
      v1.mToZ          = 0.0f; // TriV1.uv.y*v1.mfInvDepth;
      v1.mWldSpacePos  = w1;
      v1.mObjSpacePos  = o1;
      v1.mObjSpaceNrm  = on1;
      v1.mWldSpaceNrm  = wn1;
      v2.mSX           = fX2;
      v2.mSY           = fY2;
      v2.mfDepth       = fZ2;
      v2.mfInvDepth    = fiZ2;
      v2.mRoZ          = 0.0f; // TriV0.uv.x*v0.mfInvDepth;
      v2.mSoZ          = 0.0f; // TriV2.uv.x*v2.mfInvDepth;
      v2.mToZ          = fiZ2; // TriV2.uv.y*v2.mfInvDepth;
      v2.mWldSpacePos  = w2;
      v2.mObjSpacePos  = o2;
      v2.mObjSpaceNrm  = on2;
      v2.mWldSpaceNrm  = wn2;
      rtri.mFaceNormal = Tri.mFaceNormal;

      ///////////////////////////////////////////////////
      // distribute triangle to appropriate buckets
      ///////////////////////////////////////////////////

      ivertex iV0, iV1, iV2;
      iV0.iX     = (fX0);
      iV0.iY     = (fY0);
      iV1.iX     = (fX1);
      iV1.iY     = (fY1);
      iV2.iX     = (fX2);
      iV2.iY     = (fY2);
      float MinX = std::min(fX0, std::min(fX1, fX2));
      float MaxX = std::max(fX0, std::max(fX1, fX2));
      float MinY = std::min(fY0, std::min(fY1, fY2));
      float MaxY = std::max(fY0, std::max(fY1, fY2));

      int iminbx = mRenderData.GetBucketX(MinX);
      int imaxbx = mRenderData.GetBucketX(MaxX);
      int iminby = mRenderData.GetBucketY(MinY);
      int imaxby = mRenderData.GetBucketY(MaxY);

      if (iminbx < 0)
        iminbx = 0;
      if (iminby < 0)
        iminby = 0;
      if (imaxbx >= mRenderData.miNumTilesW)
        imaxbx = mRenderData.miNumTilesW - 1;
      if (imaxby >= mRenderData.miNumTilesH)
        imaxby = mRenderData.miNumTilesH - 1;

      for (int iby = iminby; iby <= imaxby; iby++)
        for (int ibx = iminbx; ibx <= imaxbx; ibx++) {
          int ibucket            = mRenderData.GetBucketIndex(ibx, iby);
          TriangleBucket& pucket = pmetabucket->mBuckets[ibucket];

          const RasterTile& tile = mRenderData.GetTile(ibx, iby);
          float fbucketX0        = float(tile.miScreenXBase);
          float fbucketY0        = float(tile.miScreenYBase);
          float fbucketX1        = fbucketX0 + float(tile.miWidth);
          float fbucketY1        = fbucketY0 + float(tile.miHeight);

          static int accept = 0;
          static int reject = 0;

          if (false == boxisect(MinX, MinY, MaxX, MaxY, fbucketX0, fbucketY0, fbucketX1, fbucketY1)) {
            reject++;
            continue;
          }

          irect ir;
          ir.l        = (fbucketX0);
          ir.r        = (fbucketX1);
          ir.t        = (fbucketY0);
          ir.b        = (fbucketY1);
          bool bsectT = triangleTest(ir, iV0, iV1, iV2);

          if (false == bsectT) {
            reject++;
            continue;
          }

          accept++;
          int idx                             = pucket.mTriangleIndex.FetchAndIncrement();
          pucket.mPostTransformTriangles[idx] = &rtri;
        }

      //////////////////////////////////////
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
