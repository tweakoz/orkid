////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <ork/lev2/gfx/meshutil/submesh.h>

namespace ork::meshutil {

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

void submeshWriteObj(const submesh& inpsubmesh, const file::Path& BasePath) {
  ork::file::Path ObjPath = BasePath;
  ork::file::Path MtlPath = BasePath;
  ObjPath.setExtension("obj");
  MtlPath.setExtension("mtl");

  std::string outstr;
  std::string mtloutstr;

  outstr += "# Miniork ToolMesh Dump <Wavefront OBJ format>\n";
  outstr += "\n";

  outstr += CreateFormattedString("mtllib %s.mtl\n", MtlPath.getName().c_str());

  ///////////////////////////////////////////////////
  ///////////////////////////////////////////////////
  orkvector<fvec3> ObjVertexPool;
  orkvector<fvec3> ObjNormalPool;
  orkvector<fvec2> ObjUv0Pool;
  orkvector<objmesh> ObjMeshPool;
  orkmap<std::string, objmat> ObjMaterialPool;

  int inumv = inpsubmesh._vtxpool->GetNumVertices();
  ///////////////////////////////////////////////////
  objmat OutMaterial;
  ///////////////////////////////////////////////////
  for (int iv0 = 0; iv0 < inumv; iv0++) {
    const vertex& invtx = inpsubmesh._vtxpool->GetVertex(iv0);
    ObjVertexPool.push_back(invtx.mPos);
    ObjNormalPool.push_back(invtx.mNrm);
    ObjUv0Pool.push_back(invtx.mUV[0].mMapTexCoord);
  }
  ///////////////////////////////////////////////////
  OutMaterial.mColor = fcolor3::White();
  objmesh OutMesh;
  OutMesh.name = "submesh";

  orkvector<int> triangles;
  orkvector<int> quads;
  inpsubmesh.FindNSidedPolys(triangles, 3);
  inpsubmesh.FindNSidedPolys(quads, 4);
  for (int i = 0; i < triangles.size(); i++) {
    objpoly outpoly;
    int igti          = triangles[i];
    const poly& intri = inpsubmesh.RefPoly(igti);
    for (int iv = 0; iv < 3; iv++) {
      int idx = intri.GetVertexID(iv);
      outpoly.mvtxindices.push_back(idx);
    }
    OutMesh.mpolys.push_back(outpoly);
  }
  for (int i = 0; i < quads.size(); i++) {
    objpoly outpoly;
    int igti          = quads[i];
    const poly& intri = inpsubmesh.RefPoly(igti);
    for (int iv = 0; iv < 4; iv++) {
      int idx = intri.GetVertexID(iv);
      outpoly.mvtxindices.push_back(idx);
    }
    OutMesh.mpolys.push_back(outpoly);
  }
  ObjMeshPool.push_back(OutMesh);
  ObjMaterialPool[OutMesh.matname] = OutMaterial;
  //////////////////////////////////////////////////////////////////////////////
  // output Material Pool
  //////////////////////////////////////////////////////////////////////////////

  int inummat = ObjMaterialPool.size();
  for (orkmap<std::string, objmat>::const_iterator itm = ObjMaterialPool.begin(); itm != ObjMaterialPool.end(); itm++) {
    const std::string& matname = (*itm).first;
    const objmat& material     = (*itm).second;

    fxstring<256> mayamatname = matname.c_str();
    mayamatname.replace_in_place(":", "_");
    mayamatname.replace_in_place("/", "_");

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

    fxstring<256> mayaname = omesh.name.c_str();
    mayaname.replace_in_place(":", "_");
    mayaname.replace_in_place("/", "_");
    fxstring<256> mayamatname = omesh.matname.c_str();
    mayamatname.replace_in_place(":", "_");
    mayamatname.replace_in_place("/", "_");

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
  mtloutfile.Close();
}

} // namespace ork::meshutil
