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

const bool gbFORCEDICE = true;
const int kDICESIZE    = 512;

namespace ork::meshutil {

///////////////////////////////////////////////////////////////////////////////

XgmRigidClusterBuilder::XgmRigidClusterBuilder(const XgmClusterizer& clusterizer)
    : XgmClusterBuilder(clusterizer) {
}

///////////////////////////////////////////////////////////////////////////////

bool XgmRigidClusterBuilder::addTriangle(const XgmClusterTri& Triangle) {
  size_t ivcount = _submesh.numVertices();
  int iicount    = (int)_submesh.numPolys();

  static const int kvtresh  = (1 << 16) - 4;
  static const int kithresh = (1 << 16) / 3;

  if (ivcount > kvtresh) {
    return false;
  }
  if (iicount > kithresh) {
    return false;
  }

  auto v0 = _submesh.mergeVertex(Triangle._vertex[0]);
  auto v1 = _submesh.mergeVertex(Triangle._vertex[1]);
  auto v2 = _submesh.mergeVertex(Triangle._vertex[2]);
  _submesh.mergePoly(Polygon(v0, v1, v2));

  return true;
}

///////////////////////////////////////////////////////////////////////////////

template <typename vtx_t>
lev2::vtxbufferbase_ptr_t buildTypedVertexBuffer(
    lev2::Context& context,
    const meshutil::submesh& inp_submesh,
    std::function<vtx_t(const meshutil::vertex&)> genOutVertex) {
  using vtxbuf_t       = lev2::StaticVertexBuffer<vtx_t>;
  int NumVertexIndices = inp_submesh.numVertices();
  auto out_vbuf        = std::make_shared<vtxbuf_t>(NumVertexIndices, 0);
  lev2::VtxWriter<vtx_t> vwriter;
  vwriter.Lock(&context, out_vbuf.get(), NumVertexIndices);
  for (int iv = 0; iv < NumVertexIndices; iv++)
    vwriter.AddVertex(genOutVertex(*inp_submesh.vertex(iv)));
  vwriter.UnLock(&context);
  out_vbuf->SetNumVertices(NumVertexIndices);
  return std::static_pointer_cast<lev2::VertexBufferBase>(out_vbuf);
}

///////////////////////////////////////////////////////////////////////////////

void XgmRigidClusterBuilder::buildVertexBuffer(lev2::Context& context, lev2::EVtxStreamFormat format) {
  switch (format) {
    ////////////////////////////////////////////////////////////////////////////
    case lev2::EVtxStreamFormat::V12: {
      _vertexBuffer = buildTypedVertexBuffer<lev2::VtxV12>(context, _submesh, [](const meshutil::vertex& inpvtx) {
        return lev2::VtxV12( inpvtx.mPos.x, inpvtx.mPos.y, inpvtx.mPos.z );
      });
      break;
    }
    ////////////////////////////////////////////////////////////////////////////
    case lev2::EVtxStreamFormat::V12T8: {
      _vertexBuffer = buildTypedVertexBuffer<lev2::VtxV12T8>(context, _submesh, [](const meshutil::vertex& inpvtx) {
        const auto& POS = inpvtx.mPos;
        const auto& UV0 = inpvtx.mUV[0].mMapTexCoord;
        return lev2::VtxV12T8( POS.x, POS.y, POS.z, 
                                UV0.x, UV0.y );
      });
      break;
    }
    ////////////////////////////////////////////////////////////////////////////
    case lev2::EVtxStreamFormat::V12C4T16: {
      _vertexBuffer = buildTypedVertexBuffer<lev2::SVtxV12C4T16>(context, _submesh, [](const meshutil::vertex& inpvtx) {
        lev2::SVtxV12C4T16 out_vtx;
        const auto& pos     = inpvtx.mPos;
        out_vtx._position = fvec3(pos.x, pos.y, pos.z);
        out_vtx._uv0      = inpvtx.mUV[0].mMapTexCoord;
        out_vtx._color    = inpvtx.mCol[0].ABGRU32();
        return out_vtx;
      });
      break;
    }
    ////////////////////////////////////////////////////////////////////////////
    case lev2::EVtxStreamFormat::V12N6C2T4: {
      _vertexBuffer = buildTypedVertexBuffer<lev2::SVtxV12N6C2T4>(context, _submesh, [](const meshutil::vertex& inpvtx) {
        lev2::SVtxV12N6C2T4 out_vtx;
        out_vtx.mX = inpvtx.mPos.x;
        out_vtx.mY = inpvtx.mPos.y;
        out_vtx.mZ = inpvtx.mPos.z;

        out_vtx.mNX = s16(inpvtx.mNrm.x * float(32767.0f));
        out_vtx.mNY = s16(inpvtx.mNrm.y * float(32767.0f));
        out_vtx.mNZ = s16(inpvtx.mNrm.z * float(32767.0f));

        out_vtx.mU = s16(inpvtx.mUV[0].mMapTexCoord.x * float(1024.0f));
        out_vtx.mV = s16(inpvtx.mUV[0].mMapTexCoord.y * float(1024.0f));

        int ir = int(inpvtx.mCol[0].y * 255.0f);
        int ig = int(inpvtx.mCol[0].z * 255.0f);
        int ib = int(inpvtx.mCol[0].w * 255.0f);

        out_vtx.mColor = U16(((ir >> 3) << 11) | ((ig >> 2) << 5) | ((ib >> 3) << 0));
        return out_vtx;
      });
      break;
    }
    ////////////////////////////////////////////////////////////////////////////
    case lev2::EVtxStreamFormat::V12N12T16C4: {
      _vertexBuffer = buildTypedVertexBuffer<lev2::SVtxV12N12T16C4>(context, _submesh, [](const meshutil::vertex& inpvtx) {
        lev2::SVtxV12N12T16C4 out_vtx;
        const auto& pos     = inpvtx.mPos;
        const auto& nrm     = inpvtx.mNrm;
        out_vtx.mPosition = fvec3(pos.x, pos.y, pos.z);
        out_vtx.mUV0      = inpvtx.mUV[0].mMapTexCoord;
        out_vtx.mUV1      = inpvtx.mUV[1].mMapTexCoord;
        out_vtx.mNormal   = fvec3(nrm.x, nrm.y, nrm.z);;
        out_vtx.mColor    = inpvtx.mCol[0].ABGRU32();
        return out_vtx;
      });
      break;
    }
    ////////////////////////////////////////////////////////////////////////////
    case lev2::EVtxStreamFormat::V12N12B12T8C4: {
      _vertexBuffer = buildTypedVertexBuffer<lev2::SVtxV12N12B12T8C4>(context, _submesh, [](const meshutil::vertex& inpvtx) {
        lev2::SVtxV12N12B12T8C4 out_vtx;
        const auto& pos     = inpvtx.mPos;
        const auto& nrm     = inpvtx.mNrm;
        out_vtx._position = fvec3(pos.x, pos.y, pos.z);
        out_vtx._uv      = inpvtx.mUV[0].mMapTexCoord;
        out_vtx._normal   = fvec3(nrm.x, nrm.y, nrm.z);;
        out_vtx._binormal = inpvtx.mUV[0].mMapBiNormal;
        out_vtx._color    = inpvtx.mCol[0].ABGRU32();
        return out_vtx;
      });
      break;
    }
    ////////////////////////////////////////////////////////////////////////////
    case lev2::EVtxStreamFormat::V12N12B12T16: {
      _vertexBuffer = buildTypedVertexBuffer<lev2::SVtxV12N12B12T16>(context, _submesh, [](const meshutil::vertex& inpvtx) {
        lev2::SVtxV12N12B12T16 out_vtx;
        const auto& pos     = inpvtx.mPos;
        const auto& nrm     = inpvtx.mNrm;
        out_vtx.mPosition = fvec3(pos.x, pos.y, pos.z);
        out_vtx.mUV0      = inpvtx.mUV[0].mMapTexCoord;
        out_vtx.mUV1      = inpvtx.mUV[1].mMapTexCoord;
        out_vtx.mNormal   = fvec3(nrm.x, nrm.y, nrm.z);;
        out_vtx.mBiNormal = inpvtx.mUV[0].mMapBiNormal;
        return out_vtx;
      });
      break;
    }
    ////////////////////////////////////////////////////////////////////////////
    default: {
      OrkAssert(false);
      break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
