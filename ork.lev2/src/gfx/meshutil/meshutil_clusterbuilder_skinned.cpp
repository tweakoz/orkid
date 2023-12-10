///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2004, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>
#include <ork/lev2/gfx/meshutil/meshutil_stripper.h>
#include <ork/lev2/gfx/meshutil/meshutil_fixedgrid.h>
#include <ork/util/logger.h>
#include <ork/lev2/gfx/gfxvtxbuf.inl>

const bool gbFORCEDICE = true;
const int kDICESIZE    = 512;

namespace ork::meshutil {

///////////////////////////////////////////////////////////////////////////////

XgmSkinnedClusterBuilder::XgmSkinnedClusterBuilder(const XgmClusterizer& clusterizer)
    : XgmClusterBuilder(clusterizer) {
}

///////////////////////////////////////////////////////////////////////////////

int XgmSkinnedClusterBuilder::findNewJointIndex(const std::string& joint_path) {
  int rval                                        = -1;
  auto itJ = _jointRegisterMapX.find(joint_path);
  if (_jointRegisterMapX.end() != itJ) {
    rval = (*itJ).second;
  }
  //	OrkAssert( rval>=0 );
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool XgmSkinnedClusterBuilder::addTriangle(const XgmClusterTri& Triangle) {
  ///////////////////////////////////////
  // make sure triangle will absolutely fit in the vertex buffer
  ///////////////////////////////////////

  size_t ivcount = _submesh.numVertices();

  static const size_t kvtresh = (2 << 16) - 4;

  if (ivcount > kvtresh) {
    return false;
  }

  ///////////////////////////////////////
  // make sure triangle will absolutely fit in the vertex buffer
  ///////////////////////////////////////

  bool do_add_triangle             = false;
  const int kMaxBonesPerCluster = _clusterizer._policy._maxBonesPerCluster;
  orkset<std::string> AddThisRun;
  for (int i = 0; i < 3; i++) {
    int inumw = Triangle._vertex[i].miNumWeights;
    for (int iw = 0; iw < inumw; iw++) {
      const std::string& joint_path         = Triangle._vertex[i]._jointpaths[iw];
      bool already_resident = _jointRegisterMapX.find(joint_path) != _jointRegisterMapX.end();
      if (already_resident) {
      } else if (AddThisRun.find(joint_path) == AddThisRun.end()) {
        AddThisRun.insert(joint_path);
      }
    }
  }
  size_t NumBonesToAllocate = AddThisRun.size();
  if (0 == NumBonesToAllocate) {
    do_add_triangle = true;
  } else {
    size_t NumBonesAlreadyAllocated = _jointRegisterMapX.size();
    size_t NumBonesFreeInCluster    = (size_t)kMaxBonesPerCluster - NumBonesAlreadyAllocated;
    if (NumBonesFreeInCluster <= 0) { // orkprintf( "Current Cluster [%08x] Is Full\n", this );
      return false;
    } else if (NumBonesToAllocate <= NumBonesFreeInCluster) {
      for (auto joint_path : AddThisRun ) {
        int register_index                = (int)_jointRegisterMapX.size();
        // orkprintf( "SKIN: <Cluster %08x> <Adding New Bone %d> <Reg%02d> <Bone:%s>\n", this, AddThisRun.size(), register_index,
        // joint_path.c_str() );
        if (_jointRegisterMapX.find(joint_path) == _jointRegisterMapX.end()) {
          std::pair<std::string, int> new_joint(joint_path, register_index);
          _jointRegisterMapX.insert(new_joint);
          // orkprintf( "Cluster[%08x] Adding BoneRec [Reg%02d:Bone:%s]\n", this, iBoneREG, BoneName.c_str() );
        }
      }
      do_add_triangle = true;
    }
  }
  if (do_add_triangle) {
    auto v0 = _submesh.mergeVertex(Triangle._vertex[0]);
    auto v1 = _submesh.mergeVertex(Triangle._vertex[1]);
    auto v2 = _submesh.mergeVertex(Triangle._vertex[2]);
    _submesh.mergePoly(Polygon(v0, v1, v2));
  }
  return do_add_triangle;
}

void XgmSkinnedClusterBuilder::buildVertexBuffer(lev2::Context& context, lev2::EVtxStreamFormat format) {
  switch (format) {
    case lev2::EVtxStreamFormat::V12N12T8I4W4: // PC skinned format
    {
      BuildVertexBuffer_V12N12T8I4W4(context);
      break;
    }
    case lev2::EVtxStreamFormat::V12N12B12T8I4W4: // PC binormal skinned format
    {
      BuildVertexBuffer_V12N12B12T8I4W4(context);
      break;
    }
    case lev2::EVtxStreamFormat::V12N6I1T4: // WII skinned format
    {
      BuildVertexBuffer_V12N6I1T4(context);
      break;
    }
    default: {
      assert(false);
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N12B12T8I4W4(lev2::Context& context) // binormal pc skinned
{
  using vtx_t    = lev2::SVtxV12N12B12T8I4W4;
  using vtxbuf_t = lev2::StaticVertexBuffer<vtx_t>;
  lev2::VtxWriter<vtx_t> vwriter;
  const double kVertexScale(1.0f);
  const fvec2 UVScale(1.0f, 1.0f);
  int NumVertexIndices = _submesh.numVertices();
  _vertexBuffer        = std::make_shared<vtxbuf_t>(NumVertexIndices, 0);
  vwriter.Lock(&context, _vertexBuffer.get(), NumVertexIndices);

  for (int iv = 0; iv < NumVertexIndices; iv++) {
    vtx_t OutVtx;
    auto InVtx = *_submesh.vertex(iv);
    const auto pos     = InVtx.mPos * kVertexScale;
    const auto nrm     = InVtx.mNrm;
    OutVtx.mPosition              = fvec3(pos.x, pos.y, pos.z);
    OutVtx.mNormal                = fvec3(nrm.x, nrm.y, nrm.z);
    OutVtx.mUV0                   = InVtx.mUV[0].mMapTexCoord * UVScale;
    OutVtx.mBiNormal              = InVtx.mUV[0].mMapBiNormal;

    const std::string& jn0 = InVtx._jointpaths[0];
    const std::string& jn1 = InVtx._jointpaths[1];
    const std::string& jn2 = InVtx._jointpaths[2];
    const std::string& jn3 = InVtx._jointpaths[3];

    int index0 = findNewJointIndex(jn0);
    int index1 = findNewJointIndex(jn1);
    int index2 = findNewJointIndex(jn2);
    int index3 = findNewJointIndex(jn3);

    index0 = (index0 == -1) ? 0 : index0;
    index1 = (index1 == -1) ? 0 : index1;
    index2 = (index2 == -1) ? 0 : index2;
    index3 = (index3 == -1) ? 0 : index3;

    int W0 = round(InVtx.mJointWeights[0] * 256.0f);
    int W1 = round(InVtx.mJointWeights[1] * 256.0f);
    int W2 = round(InVtx.mJointWeights[2] * 256.0f);
    int W3 = round(InVtx.mJointWeights[3] * 256.0f);

    // printf("W0<%f>\n", InVtx.mJointWeights[0]);
    // printf("W1<%f>\n", InVtx.mJointWeights[1]);
    // printf("W2<%f>\n", InVtx.mJointWeights[2]);
    // printf("W3<%f>\n", InVtx.mJointWeights[3]);

    typedef std::pair<int, int> intpair_t;
    std::vector<intpair_t> wvec;
    wvec.push_back(std::make_pair(W0, index0));
    wvec.push_back(std::make_pair(W1, index1));
    wvec.push_back(std::make_pair(W2, index2));
    wvec.push_back(std::make_pair(W3, index3));

    int points_remaining = 255;
    int sequence         = 0;
    for (auto item : wvec) {
      int w     = item.first;
      int index = item.second;
      // printf("seq<%d> w<%d>\n", sequence, w);
      if (w) {
        if (w > points_remaining) {
          w = points_remaining;
        }
        if ((points_remaining - w) >= 0) {
          int shift = sequence * 8;
          OutVtx.mBoneIndices |= (index << shift);
          OutVtx.mBoneWeights |= (w << shift);
          points_remaining -= w;
        }
      }
      sequence++;
    }
    if(points_remaining > 0){
      logerrchannel()->log(" skinned cluster points_remaining<%d> (check numweights < 4)\n", points_remaining);
    }

    vwriter.AddVertex(OutVtx);
  }
  vwriter.UnLock(&context);
  _vertexBuffer->SetNumVertices(NumVertexIndices);
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N12T8I4W4(lev2::Context& context) // basic pc skinned
{
  using vtx_t    = lev2::SVtxV12N12T8I4W4;
  using vtxbuf_t = lev2::StaticVertexBuffer<vtx_t>;
  lev2::VtxWriter<vtx_t> vwriter;
  const double kVertexScale(1.0);
  const fvec2 UVScale(1.0f, 1.0f);
  int NumVertexIndices = _submesh.numVertices();

  _vertexBuffer = std::make_shared<vtxbuf_t>(NumVertexIndices, 0);
  vwriter.Lock(&context, _vertexBuffer.get(), NumVertexIndices);
  for (int iv = 0; iv < NumVertexIndices; iv++) {
    vtx_t OutVtx;
    const meshutil::vertex& InVtx = *_submesh.vertex(iv);
    const auto pos     = InVtx.mPos * kVertexScale;
    const auto nrm     = InVtx.mNrm;
    OutVtx.mPosition              = fvec3(pos.x, pos.y, pos.z);
    OutVtx.mNormal               = fvec3(nrm.x, nrm.y, nrm.z);
    OutVtx.mUV0                   = InVtx.mUV[0].mMapTexCoord * UVScale;
    const std::string& jn0 = InVtx._jointpaths[0];
    const std::string& jn1 = InVtx._jointpaths[1];
    const std::string& jn2 = InVtx._jointpaths[2];
    const std::string& jn3 = InVtx._jointpaths[3];

    int index0 = findNewJointIndex(jn0);
    int index1 = findNewJointIndex(jn1);
    int index2 = findNewJointIndex(jn2);
    int index3 = findNewJointIndex(jn3);

    index0 = (index0 == -1) ? 0 : index0;
    index1 = (index1 == -1) ? 0 : index1;
    index2 = (index2 == -1) ? 0 : index2;
    index3 = (index3 == -1) ? 0 : index3;

    OutVtx.mBoneIndices = (index0) | (index1 << 8) | (index2 << 16) | (index3 << 24);

    fvec4 vw;
    vw.x = (InVtx.mJointWeights[3]);
    vw.y = (InVtx.mJointWeights[2]);
    vw.z = (InVtx.mJointWeights[1]);
    vw.w = (InVtx.mJointWeights[0]);

    OutVtx.mBoneWeights = vw.RGBAU32();
    vwriter.AddVertex(OutVtx);
  }
  vwriter.UnLock(&context);
  _vertexBuffer->SetNumVertices(NumVertexIndices);
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkinnedClusterBuilder::BuildVertexBuffer_V12N6I1T4(lev2::Context& context) // basic wii skinned
{
  using vtx_t    = lev2::SVtxV12N6I1T4;
  using vtxbuf_t = lev2::StaticVertexBuffer<vtx_t>;
  lev2::VtxWriter<vtx_t> vwriter;
  const float kVertexScale(1.0f);
  const fvec2 UVScale(1.0f, 1.0f);
  int NumVertexIndices = _submesh.numVertices();
  _vertexBuffer        = std::make_shared<vtxbuf_t>(NumVertexIndices, 0);
  vwriter.Lock(&context, _vertexBuffer.get(), NumVertexIndices);
  for (int iv = 0; iv < NumVertexIndices; iv++) {
    vtx_t OutVtx;
    const meshutil::vertex& InVtx = *_submesh.vertex(iv);

    OutVtx.mX = InVtx.mPos.x * kVertexScale;
    OutVtx.mY = InVtx.mPos.y * kVertexScale;
    OutVtx.mZ = InVtx.mPos.z * kVertexScale;

    OutVtx.mNX = s16(InVtx.mNrm.x * float(32767.0f));
    OutVtx.mNY = s16(InVtx.mNrm.y * float(32767.0f));
    OutVtx.mNZ = s16(InVtx.mNrm.z * float(32767.0f));

    OutVtx.mU = s16(InVtx.mUV[0].mMapTexCoord.x * float(1024.0f));
    OutVtx.mV = s16(InVtx.mUV[0].mMapTexCoord.y * float(1024.0f));

    ///////////////////////////////////////

    const std::string& jn0 = InVtx._jointpaths[0];
    const std::string& jn1 = InVtx._jointpaths[1];
    const std::string& jn2 = InVtx._jointpaths[2];
    const std::string& jn3 = InVtx._jointpaths[3];

    int index0 = findNewJointIndex(jn0);
    int index1 = findNewJointIndex(jn1);
    int index2 = findNewJointIndex(jn2);
    int index3 = findNewJointIndex(jn3);

    index0 = (index0 == -1) ? 0 : index0;
    index1 = (index1 == -1) ? 0 : index1;
    index2 = (index2 == -1) ? 0 : index2;
    index3 = (index3 == -1) ? 0 : index3;

    orkset<int> BoneSet;
    BoneSet.insert(index0);
    BoneSet.insert(index1);
    BoneSet.insert(index2);
    BoneSet.insert(index3);

    OrkAssertI(BoneSet.size() == 1, "Sorry, wii does not support hardware weighting!!!");
    OrkAssertI(index0 < 8, "Sorry, wii only has 8 matrix registers!!!");

    OutVtx.mBone = u8(index0);
    vwriter.AddVertex(OutVtx);
  }
  vwriter.UnLock(&context);
  _vertexBuffer->SetNumVertices(NumVertexIndices);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
