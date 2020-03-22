////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/kernel/orklut.hpp>
#include <ork/math/plane.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <orktool/orktool_pch.h>

namespace ork { namespace MeshUtil {

/////////////////////////////////////////////////////////////////////////

toolmesh::toolmesh()
    : mbMergeEdges(true) {
}

///////////////////////////////////////////////////////////////////////////////

toolmesh::~toolmesh() {
  int icnt = int(mPolyGroupLut.size());
  for (int i = 0; i < icnt; i++) {
    submesh* psub = mPolyGroupLut.GetIterAtIndex(i)->second;

    if (psub) {
      delete psub;
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::Prune() {
  orkset<std::string> SubsToPrune;
  for (orklut<std::string, submesh*>::const_iterator it = mPolyGroupLut.begin(); it != mPolyGroupLut.end(); it++) {
    const submesh& src_grp  = *it->second;
    const std::string& name = it->first;
    int inump               = src_grp.GetNumPolys();
    if (0 == inump) {
      SubsToPrune.insert(name);
    }
  }
  while (SubsToPrune.empty() == false) {
    const std::string& name = *SubsToPrune.begin();
    RemoveSubMesh(name);
    SubsToPrune.erase(SubsToPrune.begin());
  }
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::Dump(const std::string& comment) const {
  if (1)
    return; //
  /*
    static int icnt   = 0;
    std::string fname = CreateFormattedString("tmeshout%d.txt", icnt);
    icnt++;
    FILE* fout = fopen(fname.c_str(), "wt");
    fprintf(fout, "////////////////////////////////////////////////\n");
    fprintf(fout, "////////////////////////////////////////////////\n");
    fprintf(fout, "// toolmesh dump<%s>\n", comment.c_str());
    fprintf(fout, "////////////////////////////////////////////////\n");
    fprintf(fout, "////////////////////////////////////////////////\n");
    for (const auto& item : mShadingGroupToMaterialMap) {
      const std::string& key = item.first;
      const auto& val        = item.second;
      fprintf(fout,
              "// toolmesh::shadinggroup<%s> material_id<%s> material_name<%s>\n",
              key.c_str(),
              val.mMaterialDaeId.c_str(),
              val.mMaterialName.c_str());
    }
    fprintf(fout, "////////////////////////////////////////////////\n");
    for (orkmap<std::string, std::string>::const_iterator itm = mAnnotations.begin(); itm != mAnnotations.end(); itm++) {
      const std::string& key = itm->first;
      const std::string& val = itm->second;
      fprintf(fout, "// toolmesh::annokey<%s> annoval<%s>\n", key.c_str(), val.c_str());
    }
    fprintf(fout, "////////////////////////////////////////////////\n");
    for (orklut<std::string, submesh*>::const_iterator it = mPolyGroupLut.begin(); it != mPolyGroupLut.end(); it++) {
      const submesh& src_grp  = *it->second;
      const std::string& name = it->first;
      int inump               = src_grp.GetNumPolys();
      fprintf(fout, "// toolmesh::polygroup<%s> numpolys<%d>\n", name.c_str(), inump);
      const submesh::AnnotationMap& subannos = src_grp.RefAnnotations();
      for (orkmap<std::string, std::string>::const_iterator itm = subannos.begin(); itm != subannos.end(); itm++) {
        const std::string& key = itm->first;
        const std::string& val = itm->second;
        fprintf(fout, "//		submesh::annokey<%s> annoval<%s>\n", key.c_str(), val.c_str());
      }
      orkset<const AnnoMap*> annosets;
      for (int ip = 0; ip < inump; ip++) {
        const poly& ply     = src_grp.RefPoly(ip);
        const AnnoMap* amap = ply.GetAnnoMap();
        if (amap)
          annosets.insert(amap);
      }
      for (orkset<const AnnoMap*>::const_iterator it2 = annosets.begin(); it2 != annosets.end(); it2++) {
        const AnnoMap* amapp                         = (*it2);
        const orkmap<std::string, std::string>& amap = amapp->mAnnotations;
        for (orkmap<std::string, std::string>::const_iterator it3 = amap.begin(); it3 != amap.end(); it3++) {
          const std::string& key = it3->first;
          const std::string& val = it3->second;
          fprintf(fout, "//			submeshpoly::annokey<%s> annoval<%s>\n", key.c_str(), val.c_str());
        }
      }
      for (int ip = 0; ip < inump; ip++) {
        fprintf(fout, "poly<%d> <", ip);
        const poly& ply = src_grp.RefPoly(ip);
        int inumv       = ply.GetNumSides();
        for (int iv = 0; iv < inumv; iv++) {
          int i0  = iv;
          int i1  = (iv + 1) % inumv;
          int iv0 = ply.GetVertexID(i0);
          int iv1 = ply.GetVertexID(i1);
          edge Edge(iv0, iv1);
          u64 ue  = src_grp.GetEdgeBetween(iv0, iv1);
          u32 ue0 = u32(ue & 0xffffffff);
          u32 ue1 = u32((ue >> 32) & 0xffffffff);

          fprintf(fout, "%d:%08x%08x ", ply.GetVertexID(iv), ue0, ue1);
        }
        fprintf(fout, ">\n");
      }
    }
    fprintf(fout, "////////////////////////////////////////////////\n");
    fclose(fout);
    */
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::CopyMaterialsFromToolMesh(const toolmesh& from) {
  mMaterialsByShadingGroup   = from.mMaterialsByShadingGroup;
  mMaterialsByName           = from.mMaterialsByName;
  mShadingGroupToMaterialMap = from.mShadingGroupToMaterialMap;
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::SetAnnotation(const char* annokey, const char* annoval) {
  std::string aval = "";
  if (annoval != 0)
    aval = annoval;
  mAnnotations[std::string(annokey)] = aval;
}
void submesh::setStringAnnotation(const char* annokey, std::string annoval) {
  mAnnotations[std::string(annokey)] = annoval;
}

///////////////////////////////////////////////////////////////////////////////

const char* toolmesh::GetAnnotation(const char* annokey) const {
  static const char* defret("");
  orkmap<std::string, std::string>::const_iterator it = mAnnotations.find(std::string(annokey));
  if (it != mAnnotations.end()) {
    return (*it).second.c_str();
  }
  return defret;
}

///////////////////////////////////////////////////////////////////////////////
/*
void submesh::SplitOnAnno(toolmesh& out, const std::string& annokey) const {
  int inumpolys = (int)mMergedPolys.size();
  for (int ip = 0; ip < inumpolys; ip++) {
    const poly& ply    = mMergedPolys[ip];
    std::string pgroup = ply.GetAnnotation(annokey);
    if (pgroup != "") {
      submesh& sub = out.MergeSubMesh(pgroup.c_str());
      int inumpv   = ply.GetNumSides();
      poly NewPoly;
      NewPoly.miNumSides = inumpv;
      for (int iv = 0; iv < inumpv; iv++) {
        int ivi                = ply.GetVertexID(iv);
        const vertex& vtx      = RefVertexPool().GetVertex(ivi);
        int inewvi             = sub.MergeVertex(vtx);
        NewPoly.miVertices[iv] = inewvi;
      }
      NewPoly.SetAnnoMap(ply.GetAnnoMap());
      sub.MergePoly(NewPoly);
    }
  }
}*/

///////////////////////////////////////////////////////////////////////////////
/*
void submesh::SplitOnAnno(toolmesh& out, const std::string& prefix, const std::string& annokey) const {
  int inumpolys = (int)mMergedPolys.size();
  AnnotationMap merge_annos;
  merge_annos["SplitPrefix"] = prefix;
  for (int ip = 0; ip < inumpolys; ip++) {
    const poly& ply         = mMergedPolys[ip];
    std::string pgroup      = ply.GetAnnotation(annokey);
    std::string merged_name = prefix + std::string("_") + pgroup;
    if (pgroup != "") {
      submesh& sub = out.MergeSubMesh(merged_name.c_str(), merge_annos);
      int inumpv   = ply.GetNumSides();
      poly NewPoly;
      NewPoly.miNumSides = inumpv;
      for (int iv = 0; iv < inumpv; iv++) {
        int ivi                = ply.GetVertexID(iv);
        const vertex& vtx      = RefVertexPool().GetVertex(ivi);
        int inewvi             = sub.MergeVertex(vtx);
        NewPoly.miVertices[iv] = inewvi;
      }
      NewPoly.SetAnnoMap(ply.GetAnnoMap());
      sub.MergePoly(NewPoly);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////

void submesh::SplitOnAnno(toolmesh& out, const std::string& annokey, const AnnotationMap& mrgannos) const {
  int inumpolys = (int)mMergedPolys.size();
  for (int ip = 0; ip < inumpolys; ip++) {
    const poly& ply    = mMergedPolys[ip];
    std::string pgroup = ply.GetAnnotation(annokey);
    if (pgroup != "") {
      submesh& sub = out.MergeSubMesh(pgroup.c_str());
      sub.MergeAnnos(mrgannos, true);
      int inumpv = ply.GetNumSides();
      poly NewPoly;
      NewPoly.miNumSides = inumpv;
      for (int iv = 0; iv < inumpv; iv++) {
        int ivi                = ply.GetVertexID(iv);
        const vertex& vtx      = RefVertexPool().GetVertex(ivi);
        int inewvi             = sub.MergeVertex(vtx);
        NewPoly.miVertices[iv] = inewvi;
      }
      NewPoly.SetAnnoMap(ply.GetAnnoMap());
      sub.MergePoly(NewPoly);
    }
  }
}*/

///////////////////////////////////////////////////////////////////////////////

AABox toolmesh::GetAABox() const {
  AABox outp;
  outp.BeginGrow();
  int inumsub = (int)mPolyGroupLut.size();
  for (int is = 0; is < inumsub; is++) {
    const submesh& sub = *mPolyGroupLut.GetItemAtIndex(is).second;
    outp.Grow(sub.aabox().Min());
    outp.Grow(sub.aabox().Max());
  }
  outp.EndGrow();
  return outp;
}

///////////////////////////////////////////////////////////////////////////////

const orklut<std::string, submesh*>& toolmesh::RefSubMeshLut() const {
  return mPolyGroupLut;
}

///////////////////////////////////////////////////////////////////////////////

/*const orkvector<submesh *> &toolmesh::RefPolyGroupByPolyIndex() const
{
    return mPolyGroupByPolyIndex;
}*/

///////////////////////////////////////////////////////////////////////////////

const submesh* toolmesh::FindSubMeshFromMaterialName(const std::string& materialname) const {
  for (const auto& item : mShadingGroupToMaterialMap) {
    if (materialname == item.second.mMaterialDaeId) {
      const std::string& shgrpname                       = item.first;
      orklut<std::string, submesh*>::const_iterator itpg = mPolyGroupLut.find(shgrpname);
      if (itpg != mPolyGroupLut.end()) {
        return itpg->second;
      }
    }
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

submesh* toolmesh::FindSubMeshFromMaterialName(const std::string& materialname) {
  for (const auto& item : mShadingGroupToMaterialMap) {
    if (materialname == item.second.mMaterialDaeId) {
      const std::string& shgrpname                 = item.first;
      orklut<std::string, submesh*>::iterator itpg = mPolyGroupLut.find(shgrpname);
      if (itpg != mPolyGroupLut.end()) {
        return itpg->second;
      }
    }
  }
  return 0;
}

///////////////////////////////////////////////////////////////////////////////

const submesh* toolmesh::FindSubMesh(const std::string& polygroupname) const {
  orklut<std::string, submesh*>::const_iterator it = mPolyGroupLut.find(polygroupname);
  return (it == mPolyGroupLut.end()) ? 0 : it->second;
}

///////////////////////////////////////////////////////////////////////////////

submesh* toolmesh::FindSubMesh(const std::string& polygroupname) {
  orklut<std::string, submesh*>::iterator it = mPolyGroupLut.find(polygroupname);
  return (it == mPolyGroupLut.end()) ? 0 : it->second;
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::SetRangeTransform(const fvec4& vscale, const fvec4& vtrans) {
  fmtx4 MatS, MatT;
  MatS.Scale(vscale.GetX(), vscale.GetY(), vscale.GetZ());
  MatT.SetTranslation(vtrans.GetX(), vtrans.GetY(), vtrans.GetZ());
  mMatRange = MatS * MatT;
}

///////////////////////////////////////////////////////////////////////////////

void toolmesh::RemoveSubMesh(const std::string& pgroup) {
  orklut<std::string, submesh*>::iterator it = mPolyGroupLut.find(pgroup);
  if (it != mPolyGroupLut.end()) {
    submesh* psub = it->second;
    mPolyGroupLut.erase(it);
    delete psub;
  }
}

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork::MeshUtil
