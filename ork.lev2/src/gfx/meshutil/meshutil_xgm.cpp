////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/file/chunkfile.h>
#include <ork/application/application.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {

///////////////////////////////////////////////////////////////////////////////

void Mesh::ReadFromXGM(const file::Path& BasePath) {
  lev2::ContextDummy DummyTarget;

  lev2::XgmModel* mdl = new lev2::XgmModel;

  asset::vars_t no_vars;
  bool bOK = lev2::XgmModel::LoadUnManaged(mdl, BasePath,no_vars);

  if (bOK) {
    ////////////////////////////////////////////////////////
    int inummesh = mdl->numMeshes();
    for (int imesh = 0; imesh < inummesh; imesh++) {
      lev2::XgmMesh& mesh = *mdl->mesh(imesh);
      int inumcs          = mesh.numSubMeshes();

      for (int ics = 0; ics < inumcs; ics++) {
        std::string mesh_name      = CreateFormattedString("xgm_import_mesh<%d>", ics);
        const lev2::XgmSubMesh& cs = *mesh.subMesh(ics);

        submesh& outsub = MergeSubMesh(mesh_name.c_str());

        int inumclus = cs.GetNumClusters();

        for (int ic = 0; ic < inumclus; ic++) {
          auto clus             = cs.cluster(ic);
          auto pvb              = clus->GetVertexBuffer();
          int inumv             = pvb->GetMax();
          int isrcsize          = inumv * pvb->GetVtxSize();
          const void* pvertbase = static_cast<lev2::Context&>(DummyTarget).GBI()->LockVB(*pvb);
          OrkAssert(pvertbase != 0);
          // pvb->GetVertexPointer();
          {
            int inumpg = clus->numPrimGroups();
            for (int ipg = 0; ipg < inumpg; ipg++) {
              auto primgroup = clus->primgroup(ipg);
              if (primgroup->GetPrimType() == lev2::PrimitiveType::TRIANGLES) {
                const lev2::IndexBufferBase* pidxbuf          = primgroup->GetIndexBuffer();
                const lev2::StaticIndexBuffer<U16>* pidxbuf16 = (const lev2::StaticIndexBuffer<U16>*)pidxbuf;
                // const U16* pidx16 = pidxbuf16->GetIndexPointer();
                const U16* pidx16 = (const U16*)static_cast<lev2::Context&>(DummyTarget).GBI()->LockIB(*pidxbuf);

                OrkAssert(pidx16 != 0);

                int inumidx = pidxbuf16->GetNumIndices();

                switch (pvb->GetStreamFormat()) {
                  //////////////////////////////////////////////////////////////////
                  case lev2::EVtxStreamFormat::V12N12T8I4W4: {
                    printf( "V12N12T8I4W4\n");
                    auto ptypedsource = (const lev2::SVtxV12N12T8I4W4*)pvertbase;
                    OrkAssert(0 == (inumidx % 3));
                    vertex_ptr_t vertexcache[3];
                    for (int ii = 0; ii < inumidx; ii++) {
                      U16 uidx                            = pidx16[ii];
                      const lev2::SVtxV12N12T8I4W4& InVtx = ptypedsource[uidx];
                      vertex ToolVertex;
                      ToolVertex.mPos                = fvec3_to_dvec3(InVtx.mPosition);
                      ToolVertex.mNrm                = fvec3_to_dvec3(InVtx.mNormal);
                      ToolVertex.mUV[0].mMapTexCoord = InVtx.mUV0;
                      ToolVertex.mCol[0]             = fcolor4::White();
                      vertexcache[(ii % 3)]          = outsub.mergeVertex(ToolVertex);
                      if (2 == (ii % 3)) {
                        Polygon ToolPoly(vertexcache[0], vertexcache[1], vertexcache[2]);
                        outsub.mergePoly(ToolPoly);
                      }
                    }
                    break;
                  }
                  //////////////////////////////////////////////////////////////////
                  case lev2::EVtxStreamFormat::V12N12B12T8C4: {
                    printf( "V12N12B12T8C4\n");
                    auto ptypedsource = (const lev2::SVtxV12N12B12T8C4*)pvertbase;
                    OrkAssert(0 == (inumidx % 3));
                    vertex_ptr_t vertexcache[3];
                    for (int ii = 0; ii < inumidx; ii++) {
                      U16 uidx                             = pidx16[ii];
                      const lev2::SVtxV12N12B12T8C4& InVtx = ptypedsource[uidx];
                      vertex ToolVertex;
                      ToolVertex.mPos                = fvec3_to_dvec3(InVtx._position);
                      ToolVertex.mNrm                = fvec3_to_dvec3(InVtx._normal);
                      ToolVertex.mUV[0].mMapTexCoord = InVtx._uv;
                      ToolVertex.mCol[0]             = fcolor4::White();
                      vertexcache[(ii % 3)]          = outsub.mergeVertex(ToolVertex);
                      if (2 == (ii % 3)) {
                        Polygon ToolPoly(vertexcache[0], vertexcache[1], vertexcache[2]);
                        outsub.mergePoly(ToolPoly);
                      }
                    }
                    break;
                  }
                  //////////////////////////////////////////////////////////////////
                  case lev2::EVtxStreamFormat::V12N12B12T16: {
                    printf( "V12N12B12T16\n");
                    auto ptypedsource = (const lev2::SVtxV12N12B12T16*)pvertbase;
                    OrkAssert(0 == (inumidx % 3));
                    vertex_ptr_t vertexcache[3];
                    for (int ii = 0; ii < inumidx; ii++) {
                      U16 uidx                             = pidx16[ii];
                      const lev2::SVtxV12N12B12T16& InVtx = ptypedsource[uidx];
                      vertex ToolVertex;
                      ToolVertex.mPos                = fvec3_to_dvec3(InVtx.mPosition);

                      printf( "pos<%g %g %g>\n", InVtx.mPosition.x, InVtx.mPosition.y, InVtx.mPosition.z);
                      ToolVertex.mNrm                = fvec3_to_dvec3(InVtx.mNormal);
                      ToolVertex.mUV[0].mMapBiNormal = InVtx.mBiNormal;
                      ToolVertex.mUV[0].mMapTexCoord = InVtx.mUV0;
                      ToolVertex.mCol[0]             = fcolor4::White();
                      vertexcache[(ii % 3)]          = outsub.mergeVertex(ToolVertex);
                      if (2 == (ii % 3)) {
                        Polygon ToolPoly(vertexcache[0], vertexcache[1], vertexcache[2]);
                        outsub.mergePoly(ToolPoly);
                      }
                    }
                    break;
                  }
                  //////////////////////////////////////////////////////////////////
                  case lev2::EVtxStreamFormat::V12N12T16C4: {
                    printf( "V12N12T16C4\n");
                    auto ptypedsource = (const lev2::SVtxV12N12T16C4*)pvertbase;
                    OrkAssert(0 == (inumidx % 3));
                    vertex_ptr_t vertexcache[3];
                    printf("scanning numindices<%d>\n", inumidx);
                    for (int ii = 0; ii < inumidx; ii++) {
                      U16 uidx                           = pidx16[ii];
                      const lev2::SVtxV12N12T16C4& InVtx = ptypedsource[uidx];
                      vertex ToolVertex;
                      ToolVertex.mPos                = fvec3_to_dvec3(InVtx.mPosition);
                      ToolVertex.mNrm                = fvec3_to_dvec3(InVtx.mNormal);
                      ToolVertex.mUV[0].mMapTexCoord = InVtx.mUV0;
                      ToolVertex.mCol[0]             = fcolor4::White();
                      vertexcache[(ii % 3)]          = outsub.mergeVertex(ToolVertex);
                      if (2 == (ii % 3)) {
                        Polygon ToolPoly(vertexcache[0], vertexcache[1], vertexcache[2]);
                        outsub.mergePoly(ToolPoly);
                      }
                    }
                    break;
                  }
                  default: {
                    orkprintf("Mesh::ReadFromXGM() vtxfmt<%08x> not supported\n", uint32_t(pvb->GetStreamFormat()));
                    OrkAssert(false);
                    break;
                  }
                }
              }
            }
          }
          static_cast<lev2::Context&>(DummyTarget).GBI()->UnLockVB(*pvb);
        }
      }
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
