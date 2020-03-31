///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyrigh 1996-2004, Michael T. Mayers
// See License at OrkidRoot/license.html or http://www.tweakoz.com/orkid/license.html
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/meshutil/submesh.h>
#include <ork/lev2/gfx/meshutil/meshutil_fixedgrid.h>
#include <ork/lev2/gfx/meshutil/clusterizer.h>

const bool gbFORCEDICE = true;
const int kDICESIZE    = 512;

namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerDiced::XgmClusterizerDiced() {
}

///////////////////////////////////////////////////////////////////////////////

XgmClusterizerDiced::~XgmClusterizerDiced() {
}

///////////////////////////////////////////////////////////////////////////////

void XgmClusterizerDiced::Begin() {
}

///////////////////////////////////////////////////////////////////////////////

bool XgmClusterizerDiced::addTriangle(const XgmClusterTri& Triangle, const MeshConfigurationFlags& flags) {
  int iv0 = _preDicedMesh.MergeVertex(Triangle._vertex[0]);
  int iv1 = _preDicedMesh.MergeVertex(Triangle._vertex[1]);
  int iv2 = _preDicedMesh.MergeVertex(Triangle._vertex[2]);
  poly the_poly(iv0, iv1, iv2);
  _preDicedMesh.MergePoly(the_poly);
  return true;
}

///////////////////////////////////////////////////////////////////////////////

void XgmClusterizerDiced::End() {

  ///////////////////////////////////////////////
  // compute ideal dice size
  ///////////////////////////////////////////////

  AABox aab     = _preDicedMesh.aabox();
  fvec3 extents = aab.Max() - aab.Min();

#if 0

	int isize = 1<<20;
	bool bdone = false;

	while( false == bdone )
	{
		int idimX = int(extents.GetX())/isize;
		int idimY = int(extents.GetY())/isize;
		int idimZ = int(extents.GetZ())/isize;

		int inumnodes = idimX*idimY*idimZ;

		if( inumnodes > 16 )
		{
			bdone = true;
		}
		else
		{
			isize >>= 1;
			orkprintf( "idim<%d %d %d> dice size<%d>\n", idimX, idimY, idimZ, isize );
		}

	}
#else
  int isize = kDICESIZE;
  int idimX = int(extents.GetX()) / isize;
  int idimY = int(extents.GetY()) / isize;
  int idimZ = int(extents.GetZ()) / isize;
  if (idimX == 0)
    idimX = 1;
  if (idimY == 0)
    idimY = 1;
  if (idimZ == 0)
    idimZ = 1;
  int inumnodes = idimX * idimY * idimZ;
#endif

  ///////////////////////////////////////////////
  // END compute ideal dice size
  ///////////////////////////////////////////////

  Mesh DicedMesh;

  DicedMesh.SetMergeEdges(false);

  if (gbFORCEDICE || _preDicedMesh.GetNumPolys() > 10000) {
    float ftimeA = float(OldSchool::GetRef().GetLoResTime());

    GridGraph thegraph(isize);
    thegraph.BeginPreMerge();
    thegraph.PreMergeMesh(_preDicedMesh);
    thegraph.EndPreMerge();
    thegraph.MergeMesh(_preDicedMesh, DicedMesh);

    float ftimeB = float(OldSchool::GetRef().GetLoResTime());

    float ftime = (ftimeB - ftimeA);
    orkprintf("<<PROFILE>> <<dicemesh %f seconds>>\n", ftime);

  } else {
    DicedMesh.MergeSubMesh(_preDicedMesh);
  }
  int inumpacc = 0;

  const orklut<std::string, submesh*>& pgmap = DicedMesh.RefSubMeshLut();

  size_t inumgroups = pgmap.size();
  static int igroup = 0;

  float ftimeC = float(OldSchool::GetRef().GetLoResTime());
  for (orklut<std::string, submesh*>::const_iterator it = pgmap.begin(); it != pgmap.end(); it++) {
    const std::string& pgname = it->first;
    const submesh& pgrp       = *it->second;
    int inumpolys             = pgrp.GetNumPolys();

    inumpacc += inumpolys;

    auto new_builder = std::make_shared<XgmRigidClusterBuilder>(*this);
    _clusters.push_back(new_builder);

    for (int ip = 0; ip < inumpolys; ip++) {
      const poly& ply = pgrp.RefPoly(ip);

      OrkAssert(ply.GetNumSides() == 3);

      XgmClusterTri ClusTri;

      ClusTri._vertex[0] = pgrp.RefVertexPool().GetVertex(ply.GetVertexID(0));
      ClusTri._vertex[1] = pgrp.RefVertexPool().GetVertex(ply.GetVertexID(1));
      ClusTri._vertex[2] = pgrp.RefVertexPool().GetVertex(ply.GetVertexID(2));

      bool bOK = new_builder->addTriangle(ClusTri);

      if (false == bOK) // cluster full, make new cluster
      {
        new_builder = std::make_shared<XgmRigidClusterBuilder>(*this);
        _clusters.push_back(new_builder);
        bOK = new_builder->addTriangle(ClusTri);
        OrkAssert(bOK);
      }
    }
  }
  float ftimeD = float(OldSchool::GetRef().GetLoResTime());
  float ftime  = (ftimeD - ftimeC);
  orkprintf("<<PROFILE>> <<clusterize %f seconds>>\n", ftime);

  float favgpolyspergroup = float(inumpacc) / float(inumgroups);

  orkprintf("dicer NumGroups<%d> AvgPolysPerGroup<%d>\n", inumgroups, int(favgpolyspergroup));

  size_t inumclus = _clusters.size();
  for (size_t ic = 0; ic < inumclus; ic++) {
    auto builder = _clusters[ic];
    AABox bbox   = builder->_submesh.aabox();
    fvec3 vmin   = bbox.Min();
    fvec3 vmax   = bbox.Max();
    float fdist  = (vmax - vmin).Mag();

    int inumv = (int)builder->_submesh.RefVertexPool().GetNumVertices();
    orkprintf(
        "clus<%d> inumv<%d> bbmin<%g %g %g> bbmax<%g %g %g> diag<%g>\n",
        ic,
        inumv,
        vmin.GetX(),
        vmin.GetY(),
        vmin.GetZ(),
        vmax.GetX(),
        vmax.GetY(),
        vmax.GetZ(),
        fdist);
  }
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
