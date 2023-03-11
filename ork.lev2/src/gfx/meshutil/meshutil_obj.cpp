////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>
#include <ork/file/chunkfile.h>
#include <ork/lev2/gfx/gfxmaterial.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
struct objpoly {
  std::string group;
  std::string material;
  std::string smoothinggroup;
  orkvector<int> mvtxindices;
  orkvector<int> mnrmindices;
  orkvector<int> muvindices;
};
struct objmesh {
  std::string name;
  std::string matname;
  orkvector<objpoly> mpolys;
};
struct objmat {
  fvec3 mColor;
};
void Mesh::WriteToWavefrontObj(const file::Path& BasePath) const {
  /*ork::file::Path ObjPath = BasePath;
  ork::file::Path MtlPath = BasePath;
  ObjPath.setExtension("obj");
  MtlPath.setExtension("mtl");

  std::string outstr;
  std::string mtloutstr;

  outstr += "# Miniork ToolMesh Dump <Wavefront OBJ format>\n";
  outstr += "\n";

  outstr += CreateFormattedString("mtllib %s.mtl\n", MtlPath.GetName().c_str());

  ///////////////////////////////////////////////////
  ///////////////////////////////////////////////////
  orkvector<fvec3> ObjVertexPool;
  orkvector<fvec3> ObjNormalPool;
  orkvector<fvec2> ObjUv0Pool;
  orkvector<objmesh> ObjMeshPool;
  orkmap<std::string, objmat> ObjMaterialPool;
  ///////////////////////////////////////////////////
  for (auto it : mPolyGroupLut ) {

    const submesh& pgroup      = *it.second;
    const std::string& grpname = it.first;
    const vertexpool& vpool    = pgroup.RefVertexPool();
  }

  int inumv = mvpool.GetNumVertices();
  ///////////////////////////////////////////////////
  objmat OutMaterial;
  ///////////////////////////////////////////////////
  for (int iv0 = 0; iv0 < inumv; iv0++) {
    const vertex& invtx = mvpool.GetVertex(iv0);
    ObjVertexPool.push_back(invtx.mPos);
    ObjNormalPool.push_back(invtx.mNrm);
    ObjUv0Pool.push_back(invtx.mUV[0].mMapTexCoord);
  }
  ///////////////////////////////////////////////////
  OutMaterial.mColor = fcolor3::White();
  for (orkmap<std::string, submesh*>::const_iterator it = mPolyGroupMap.begin(); it != mPolyGroupMap.end(); it++) {
    objmesh OutMesh;
    OutMesh.name          = it->first;
    const submesh* pgroup = it->second;

    int inumtriingroup = pgroup->mGroupPolys.size();

    for (orkset<int>::const_iterator it = pgroup->mGroupPolys.begin(); it != pgroup->mGroupPolys.end(); it++) {
      objpoly outpoly;

      int igti = *it;

      // int iti = TrianglePolyIndices[igti];
      const poly& intri = RefPoly(igti);
      int inumsides     = intri.GetNumSides();
      // OrkAssert( inumsides<3 );
      for (int iv = 0; iv < inumsides; iv++) {
        int idx = intri.GetVertexID(iv);
        outpoly.mvtxindices.push_back(idx);
      }
      OutMesh.mpolys.push_back(outpoly);
    }
    ObjMeshPool.push_back(OutMesh);
    ObjMaterialPool[OutMesh.matname] = OutMaterial;
  }
  ///////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////
  // output Material Pool
  //////////////////////////////////////////////////////////////////////////////

  int inummat = ObjMaterialPool.size();
  for (orkmap<std::string, objmat>::const_iterator itm = ObjMaterialPool.begin(); itm != ObjMaterialPool.end(); itm++) {
    const std::string& matname = (*itm).first;
    const objmat& material     = (*itm).second;

    std::string mayamatname = matname;
    OldStlSchoolFindAndReplace<std::string>(mayamatname, ":", "_");
    OldStlSchoolFindAndReplace<std::string>(mayamatname, "/", "_");

    // newmtl initialShadingGroup
    // illum 4
    // Kd 0.50 0.50 0.50
    // Ka 0.00 0.00 0.00
    // Tf 1.00 1.00 1.00
    // Ni 1.00
    mtloutstr += CreateFormattedString("newmtl %s\n", mayamatname.c_str());
    mtloutstr += CreateFormattedString("Kd %f %f %f\n", material.mColor.x, material.mColor.y, material.mColor.z);
  }

  //////////////////////////////////////////////////////////////////////////////
  // output Vertex Pool
  //////////////////////////////////////////////////////////////////////////////

  int inumobjv = ObjVertexPool.size();

  for (int i = 0; i < inumobjv; i++) {
    outstr += CreateFormattedString("v %f %f %f\n", ObjVertexPool[i].x, ObjVertexPool[i].y, ObjVertexPool[i].z);
  }

  //////////////////////////////////////////////////////////////////////////////
  // output UV Pool
  //////////////////////////////////////////////////////////////////////////////

  int inumobju = ObjUv0Pool.size();

  for (int i = 0; i < inumobju; i++) {
    outstr += CreateFormattedString("vt %f %f\n", ObjUv0Pool[i].x, ObjUv0Pool[i].y);
  }

  //////////////////////////////////////////////////////////////////////////////
  // output Normal Pool
  //////////////////////////////////////////////////////////////////////////////

  int inumobjn = ObjNormalPool.size();

  for (int i = 0; i < inumobjn; i++) {
    outstr += CreateFormattedString("vn %f %f %f\n", ObjNormalPool[i].x, ObjNormalPool[i].y, ObjNormalPool[i].z);
  }

  //////////////////////////////////////////////////////////////////////////////
  // output Mesh Pool
  //////////////////////////////////////////////////////////////////////////////

  for (int ie = 0; ie < int(ObjMeshPool.size()); ie++) {
    const objmesh& omesh = ObjMeshPool[ie];

    std::string mayaname = omesh.name;
    OldStlSchoolFindAndReplace<std::string>(mayaname, ":", "_");
    OldStlSchoolFindAndReplace<std::string>(mayaname, "/", "_");
    std::string mayamatname = omesh.matname;
    OldStlSchoolFindAndReplace<std::string>(mayamatname, ":", "_");
    OldStlSchoolFindAndReplace<std::string>(mayamatname, "/", "_");

    outstr += CreateFormattedString("g %s\n", mayaname.c_str());
    outstr += CreateFormattedString("usemtl %s\n", mayamatname.c_str());

    int inump = omesh.mpolys.size();

    for (int i = 0; i < inump; i++) {
      outstr += CreateFormattedString("f ");

      const objpoly& poly = omesh.mpolys[i];

      int inumi = poly.mvtxindices.size();

      for (int ii = 0; ii < inumi; ii++) {
        int idx = poly.mvtxindices[ii];
        outstr += CreateFormattedString("%d/%d/%d ", idx + 1, idx + 1, idx + 1);
      }

      outstr += CreateFormattedString("\n");
    }
  }

  //////////////////////////////////////////////////////////////////////////////
  // Write Files
  //////////////////////////////////////////////////////////////////////////////

  File outfile;
  outfile.OpenFile(ObjPath, EFM_WRITE);
  outfile.Write(outstr.c_str(), outstr.size());
  outfile.Close();

  File mtloutfile;
  mtloutfile.OpenFile(MtlPath, EFM_WRITE);
  mtloutfile.Write(mtloutstr.c_str(), mtloutstr.size());
  mtloutfile.Close();*/
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct dos2unix_Pred {
  char* pred;
  dos2unix_Pred(char& pred)
      : pred(&pred) {
  }
  bool operator()(char cur) const {
    bool result(cur == '\n' && *pred == '\r');
    *pred = cur;
    return result;
  }
};

void Mesh::ReadFromWavefrontObj(const file::Path& BasePath) {
  ork::file::Path ObjPath = BasePath;
  ork::file::Path MtlPath = BasePath;
  ork::file::Path FxmPath = BasePath;

  ObjPath.setExtension("obj");
  MtlPath.setExtension("mtl");
  FxmPath.setExtension("fxm");

  File objfile(ObjPath, EFM_READ);
  // objfile.Open(  );
  size_t ifilelen         = 0;
  ork::EFileErrCode ecode = objfile.GetLength(ifilelen);
  char* pmem              = new char[ifilelen + 1];
  ecode                   = objfile.Read(pmem, ifilelen);
  pmem[ifilelen]          = 0;
  std::string InObjText(pmem);
  InObjText += "\n\0";
  delete[] pmem;

  char pred('\n');
  InObjText.erase(std::remove_if(InObjText.begin(), InObjText.end(), dos2unix_Pred(pred)), InObjText.end());
  std::replace(InObjText.begin(), InObjText.end(), '\r', '\n');
  std::replace(InObjText.begin(), InObjText.end(), '\t', ' ');

  // assert(s == "This is\na two line thing\nwith a third line\n");

  orkvector<std::string> ObjLines;

  ork::SplitString(InObjText, ObjLines, "\n");

  int inumlines = int(ObjLines.size());

  orkvector<fvec3> ObjV;
  orkvector<fvec3> ObjVN;
  orkvector<fvec2> ObjVT;
  orkvector<objpoly> ObjPolys;

  std::string curgroup;
  std::string cursmoothinggroup;
  std::string curmaterial;
  std::string materiallib;

  for (int il = 0; il < inumlines; il++) {
    const std::string& curline = ObjLines[il];

    int ilinelen = curline.length();

    orkvector<std::string> Tokens;
    ork::SplitString(curline, Tokens, " ");

    if (ilinelen >= 1) {
      char firstch = curline[0];

      switch (firstch) {
        case 'u': // usemtl
        {
          OrkAssert(Tokens[0] == "usemtl");
          if (Tokens.size() == 2) {
            curmaterial = Tokens[1];
          } else {
            curmaterial = "default";
          }
          break;
        }
        case 'm': // mtllib
        {
          OrkAssert(Tokens[0] == "mtllib");
          OrkAssert(Tokens.size() == 2);
          materiallib = Tokens[1];
          break;
        }
        case 'g': // group
        {
          OrkAssert(Tokens[0] == "g");
          // OrkAssert( Tokens.size() > 1 );
          if (Tokens.size() > 1) {
            curgroup = Tokens[1];
          } else {
            curgroup = "default";
          }
          break;
        }
        case 'v': // vertex data
        {
          OrkAssert((Tokens[0] == "v") || (Tokens[0] == "vt") || (Tokens[0] == "vn"));
          if (Tokens[0] == "v") {
            OrkAssert(Tokens.size() == 4);
            fvec3 v;
            sscanf(Tokens[1].c_str(), "%f", v.asArray() + 0);
            sscanf(Tokens[2].c_str(), "%f", v.asArray() + 1);
            sscanf(Tokens[3].c_str(), "%f", v.asArray() + 2);
            ObjV.push_back(v);
          }
          if (Tokens[0] == "vn") {
            OrkAssert(Tokens.size() == 4);
            fvec3 vn;
            sscanf(Tokens[1].c_str(), "%f", vn.asArray() + 0);
            sscanf(Tokens[2].c_str(), "%f", vn.asArray() + 1);
            sscanf(Tokens[3].c_str(), "%f", vn.asArray() + 2);
            ObjVN.push_back(vn);
          }
          if (Tokens[0] == "vt") {
            OrkAssert(Tokens.size() == 3);
            fvec2 vt;
            sscanf(Tokens[1].c_str(), "%f", vt.asArray() + 0);
            sscanf(Tokens[2].c_str(), "%f", vt.asArray() + 1);
            ObjVT.push_back(vt);
          }

          break;
        }
        case 's': // smoothing group
        {
          OrkAssert(Tokens[0] == "s");
          OrkAssert(Tokens.size() == 2);
          cursmoothinggroup = Tokens[1];
          break;
        }
        case 'f': // face
        {
          OrkAssert(Tokens[0] == "f");

          int icount = int(Tokens.size());

          objpoly opoly;
          opoly.group          = curgroup;
          opoly.material       = curmaterial;
          opoly.smoothinggroup = cursmoothinggroup;

          int inumfaceverts = icount - 1;

          for (int ivtx = 0; ivtx < inumfaceverts; ivtx++) {
            const std::string tok = Tokens[ivtx + 1];

            orkvector<std::string> indices;
            ork::SplitString(tok, indices, "/");

            int inumindices = indices.size();

            switch (inumindices) {
              case 3: // vtn
              {
                int iv, it, in;
                sscanf(indices[0].c_str(), "%d", &iv);
                sscanf(indices[1].c_str(), "%d", &it);
                sscanf(indices[2].c_str(), "%d", &in);
                opoly.mvtxindices.push_back(iv);
                opoly.mnrmindices.push_back(in);
                opoly.muvindices.push_back(it);
                break;
              }
              case 2: // vtn
              {
                if (tok.find("//")) {
                  int iv, it, in;
                  sscanf(indices[0].c_str(), "%d", &iv);
                  sscanf(indices[1].c_str(), "%d", &in);
                  it = 1;
                  opoly.mvtxindices.push_back(iv);
                  opoly.mnrmindices.push_back(in);
                  opoly.muvindices.push_back(it);
                  break;
                } else {
                  OrkAssert(false);
                }
              }
              case 1: // v
              {
                if (tok.find("//")) {
                  int iv, it, in;
                  sscanf(indices[0].c_str(), "%d", &iv);
                  it = 1;
                  in = 1;
                  opoly.mvtxindices.push_back(iv);
                  opoly.mnrmindices.push_back(in);
                  opoly.muvindices.push_back(it);
                  break;
                } else {
                  OrkAssert(false);
                }
              }
              default:
                OrkAssert(false);
                break;
            }
          }

          ObjPolys.push_back(opoly);

          break;
        }
        case '#':
          break;
        case 'o': // object name
          break;
        default: {
          printf("unknown obj line<%d> token <%c>\n", il, firstch);
          OrkAssert(false);
          break;
        }
      }
    }
  }

  int inumvertices = ObjV.size();
  int inumnormals  = ObjVN.size();
  int inumuvs      = ObjVT.size();
  int inumpolys    = ObjPolys.size();

 // printf("readobj inumverts<%d>\n", inumvertices);
 // printf("readobj inumnormals<%d>\n", inumnormals);
 // printf("readobj inumuvs<%d>\n", inumuvs);
 // printf("readobj inumpolys<%d>\n", inumpolys);

  /////////////////////////////////////////////
  /////////////////////////////////////////////

  int inumnrm = ObjVN.size();
  int inumuv0 = ObjVT.size();

  for (int ip = 0; ip < inumpolys; ip++) {
    const objpoly& opoly = ObjPolys[ip];
    submesh& smesh       = MergeSubMesh(opoly.group.c_str());
    /////////////////////////////////////////////
    smesh.typedAnnotation<lev2::EVtxStreamFormat>("OutVtxFormat") = ork::lev2::EVtxStreamFormat::V12N12B12T16;
    /////////////////////////////////////////////

    size_t inumv = opoly.mvtxindices.size();
    OrkAssert(opoly.mnrmindices.size() == inumv);
    OrkAssert(opoly.muvindices.size() == inumv);

    if (inumv <= 4) {
      static const int kmax = 4;
      vertex_ptr_t vertex_cache[kmax];
      OrkAssert(inumv <= kmax)

          for (size_t iv = 0; iv < inumv; iv++) {
        int iposidx = opoly.mvtxindices[iv] - 1;
        int inrmidx = opoly.mnrmindices[iv] - 1;
        int iuv0idx = opoly.muvindices[iv] - 1;

        static fvec3 NoUv0(0.0f, 0.0f, 0.0f);
        static fvec3 NoNrm(0.0f, 1.0f, 0.0f);

        const fvec3& pos = ObjV[iposidx];
        const fvec3& nrm = inumnrm ? ObjVN[inrmidx] : NoNrm;
        const fvec3 uv0  = (inumuv0 != 0) ? fvec3(ObjVT[iuv0idx]) : fvec3(NoUv0);

        vertex ToolVertex;

        ToolVertex.mPos                = pos;
        ToolVertex.mNrm                = nrm;
        ToolVertex.mUV[0].mMapTexCoord = uv0;

        vertex_cache[iv] = smesh.mergeVertex(ToolVertex);
      }

      switch (inumv) {
        case 3: {
          bool bzeroarea = (vertex_cache[0] == vertex_cache[1]) || //
                           (vertex_cache[1] == vertex_cache[2]) || //
                           (vertex_cache[2] == vertex_cache[0]);

          if (false == bzeroarea) {
            bool ok = (vertex_cache[0] != nullptr);
            ok &= (vertex_cache[1] != nullptr);
            ok &= (vertex_cache[2] != nullptr);
            if (ok) {
              poly ToolPoly(vertex_cache[0], vertex_cache[1], vertex_cache[2]);
              smesh.MergePoly(ToolPoly);
            }
          }
          break;
        }
        case 4: {
          bool ok = (vertex_cache[0] != nullptr);
          ok &= (vertex_cache[1] != nullptr);
          ok &= (vertex_cache[2] != nullptr);
          ok &= (vertex_cache[3] != nullptr);
          if (ok) {
            poly ToolPoly(vertex_cache[0], vertex_cache[1], vertex_cache[2], vertex_cache[3]);
            smesh.MergePoly(ToolPoly);
          }
          break;
        }
        default:
          OrkAssert(false);
          break;
      }
    }
  }

  /////////////////////////////////////////////
  /////////////////////////////////////////////

  bool bfxmOK = lev2::LoadMaterialMap(FxmPath, mFxmMaterialMap);
  if (bfxmOK) {
    int inummaterialsinmap = mFxmMaterialMap.size();
    printf("Fxm MaterialMap Loaded nummtls<%d>\n", inummaterialsinmap);
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
