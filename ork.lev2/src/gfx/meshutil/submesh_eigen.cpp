////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <Eigen/Dense>
#include <Eigen/Sparse>
#include<ork/util/logger.h>

namespace ork::meshutil {
/////////////////////////////////////////////////////////////////////////
// eigen to submesh converter for interfacing
//  with various python/numpy packages
/////////////////////////////////////////////////////////////////////////
submesh_ptr_t submeshFromEigen(
    const Eigen::MatrixXd& verts, //
    const Eigen::MatrixXi& faces,
    const Eigen::MatrixXd& uvs,
    const Eigen::MatrixXd& colors,
    const Eigen::MatrixXd& normals,
    const Eigen::MatrixXd& binormals,
    const Eigen::MatrixXd& tangents) {
  auto rval           = std::make_shared<submesh>();
  size_t numVerts     = verts.rows();
  size_t numFaces     = faces.rows();
  size_t sidesPerFace = faces.cols();
  size_t numUvs       = uvs.rows();
  size_t numColors    = colors.rows();
  size_t numNormals   = normals.rows();
  size_t numBinormals = binormals.rows();
  size_t numTangents  = tangents.rows();
  /////////////////////////////////////////////
  OrkAssert(verts.cols() == 3);                                          // make sure we have vec3's
  auto generateVertex = [&](int faceindex, int facevtxindex) -> vertex { //
    vertex outv;
    const Eigen::MatrixXi& face = faces.row(faceindex);
    int per_vert_index          = face(facevtxindex);
    /////////////////////////////////////////////
    // position
    /////////////////////////////////////////////
    auto inp_pos = verts.row(per_vert_index);
    outv.mPos    = dvec3(inp_pos(0), inp_pos(1), inp_pos(2));
    /////////////////////////////////////////////
    // normal
    /////////////////////////////////////////////
    auto donormal = [&](int index) {
      OrkAssert(normals.cols() == 3);
      auto inp  = normals.row(index);
      outv.mNrm = dvec3(inp(0), inp(1), inp(2));
    };
    if (numNormals == numVerts) // per vertex
      donormal(per_vert_index);
    else if (numNormals == numFaces) // per face
      donormal(faceindex);
    else if (numNormals == 0) {
    } // no normals
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // binormal
    /////////////////////////////////////////////
    auto dobinormal = [&](int index) {
      OrkAssert(binormals.cols() == 3);
      auto inp                 = binormals.row(index);
      outv.mUV[0].mMapBiNormal = fvec3(inp(0), inp(1), inp(2));
    };
    if (numBinormals == numVerts) // per vertex
      dobinormal(per_vert_index);
    else if (numBinormals == numFaces) // per face
      dobinormal(faceindex);
    else if (numBinormals == 0) {
    } // no binormals
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // tangent
    /////////////////////////////////////////////
    auto dotangent = [&](int index) {
      OrkAssert(tangents.cols() == 3);
      auto inp                = tangents.row(index);
      outv.mUV[0].mMapTangent = fvec3(inp(0), inp(1), inp(2));
    };
    if (numTangents == numVerts) // per vertex
      dotangent(per_vert_index);
    else if (numTangents == numFaces) // per face
      dotangent(faceindex);
    else if (numTangents == 0) {
    } // no tangents
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // texturecoord
    /////////////////////////////////////////////
    auto dotexcoord = [&](int index) {
      OrkAssert(uvs.cols() == 2);
      auto inp                 = uvs.row(index);
      outv.mUV[0].mMapTexCoord = fvec2(inp(0), inp(1));
    };
    if (numUvs == numVerts) // per vertex
      dotexcoord(per_vert_index);
    else if (numUvs == numFaces) // per face
      dotexcoord(faceindex);
    else if (numUvs == 0) {
    } // no texcoords
    else
      OrkAssert(false);
    /////////////////////////////////////////////
    // color
    /////////////////////////////////////////////
    auto docolor = [&](int index) {
      auto inp = colors.row(index);
      switch (colors.cols()) {
        case 1: // luminance
          outv.mCol[0] = fvec4(inp(0), inp(0), inp(0), 1);
          break;
        case 3: // rgb
          outv.mCol[0] = fvec4(inp(0), inp(1), inp(2), 1);
          break;
        case 4: // rgba
          outv.mCol[0] = fvec4(inp(0), inp(1), inp(2), inp(3));
          break;
        default:
          OrkAssert(false);
          break;
      }
    };
    if (numColors == numVerts)
      docolor(per_vert_index);
    else if (numColors == numFaces)
      docolor(faceindex);
    else if (numColors == 1)
      docolor(0);
    else if (numColors == 0)
      outv.mCol[0] = fvec4(1, 1, 1, 1);
    else
      OrkAssert(false);
    return outv;
  }; // auto generateVertex = [&](int faceindex, int facevtxindex) -> vertex { //
  /////////////////////////////////////////////
  for (int f = 0; f < numFaces; f++) {
    switch (sidesPerFace) {
      case 3: {
        auto o0 = rval->mergeVertex(generateVertex(f, 0));
        auto o1 = rval->mergeVertex(generateVertex(f, 1));
        auto o2 = rval->mergeVertex(generateVertex(f, 2));
        rval->mergePoly(Polygon(o0, o1, o2));
        break;
      }
      case 4: {
        auto o0 = rval->mergeVertex(generateVertex(f, 0));
        auto o1 = rval->mergeVertex(generateVertex(f, 1));
        auto o2 = rval->mergeVertex(generateVertex(f, 2));
        auto o3 = rval->mergeVertex(generateVertex(f, 3));
        rval->mergePoly(Polygon(o0, o1, o2, o3));
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
  }
  return rval;
}
} //    namespace ork::meshutil {