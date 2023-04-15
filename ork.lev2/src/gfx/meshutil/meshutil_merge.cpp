////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/math/plane.h>
#include <ork/kernel/csystem.h>
#include <ork/kernel/mutex.h>
#include <ork/kernel/thread.h>
#include <ork/lev2/gfx/meshutil/meshutil.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork::meshutil {
///////////////////////////////////////////////////////////////////////////////

void Mesh::MergeMaterialsFromToolMesh(const Mesh& from) {
  for (const auto& item : from.mShadingGroupToMaterialMap) {
    const std::string& key = item.first;
    const auto& val        = item.second;

    const auto& itf = mShadingGroupToMaterialMap.find(key);
    OrkAssert(itf == mShadingGroupToMaterialMap.end());
    mShadingGroupToMaterialMap[key] = val;
  }
  for (auto itm : from._materialsByShadingGroup) {
    const std::string& key = itm.first;
    auto val               = itm.second;

    auto itf = _materialsByShadingGroup.find(key);
    OrkAssert(itf == _materialsByShadingGroup.end());
    _materialsByShadingGroup[key] = val;
  }
  for (auto itm : from._materialsByName) {
    const std::string& key = itm.first;
    auto val               = itm.second;

    auto itf = _materialsByName.find(key);
    OrkAssert(itf == _materialsByName.end());
    _materialsByName[key] = val;
  }
}

///////////////////////////////////////////////////////////////////////////////

void Mesh::MergeSubMesh(const submesh& src) {
  MergeSubMesh(src, "default");
}

///////////////////////////////////////////////////////////////////////////////

void Mesh::MergeSubMesh(const Mesh& src, const submesh& inp_grp, const char* newname) {
  float ftimeA   = float(OldSchool::GetRef().GetLoResTime());
  auto pnewgroup = submeshFromGroupName(newname);
  if (nullptr == pnewgroup) {
    pnewgroup                      = std::make_shared<submesh>();
    _submeshesByPolyGroup[newname] = pnewgroup;
  }
  int inumpingroup = inp_grp.numPolys();
  for (int i = 0; i < inumpingroup; i++) {
    const Polygon& inp_poly = inp_grp.RefPoly(i);
    int inumpv      = inp_poly.numVertices();
    poly_ptr_t new_poly;
    switch(inumpv){
      case 3:{
        auto newvtx0           = pnewgroup->mergeVertex(*inp_grp.vertex(inp_poly.vertexID(0)));
        auto newvtx1           = pnewgroup->mergeVertex(*inp_grp.vertex(inp_poly.vertexID(1)));
        auto newvtx2           = pnewgroup->mergeVertex(*inp_grp.vertex(inp_poly.vertexID(2)));
        new_poly = std::make_shared<Polygon>(newvtx0,newvtx1,newvtx2);
        break;
      }
      case 4:{
        auto newvtx0           = pnewgroup->mergeVertex(*inp_grp.vertex(inp_poly.vertexID(0)));
        auto newvtx1           = pnewgroup->mergeVertex(*inp_grp.vertex(inp_poly.vertexID(1)));
        auto newvtx2           = pnewgroup->mergeVertex(*inp_grp.vertex(inp_poly.vertexID(2)));
        auto newvtx3           = pnewgroup->mergeVertex(*inp_grp.vertex(inp_poly.vertexID(3)));
        new_poly = std::make_shared<Polygon>(newvtx0,newvtx1,newvtx2,newvtx3);
        break;
      }
      default:
        OrkAssert(false);
        break;
    }
    new_poly->SetAnnoMap(inp_poly.GetAnnoMap());
    pnewgroup->mergePoly(*new_poly);
  }
  float ftimeB = float(OldSchool::GetRef().GetLoResTime());
  float ftime  = (ftimeB - ftimeA);
  orkprintf("<<PROFILE>> <<Mesh::MergeSubMesh %f seconds>>\n", ftime);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct MergeToolMeshQueueItem {
  const submesh* mpSourceSubMesh;
  submesh* mpDestSubMesh;
  std::string destname;

  MergeToolMeshQueueItem()
      : mpSourceSubMesh(0)
      , mpDestSubMesh(0) {
  }

  void DoIt(int ithread) const;
};

///////////////////////////////////////////////////////////////////////////////

struct MergeToolMeshQueue {
  int miNumFinished;
  int miNumQueued;
  LockedResource<orkvector<MergeToolMeshQueueItem>> mJobSet;
  ork::mutex mSourceMutex;

  MergeToolMeshQueue()
      : miNumFinished(0)
      , miNumQueued(0)
      , mSourceMutex("srcmutex") {
  }
};

void MergeToolMeshQueueItem::DoIt(int ithread) const {
  float ftimeA = float(OldSchool::GetRef().GetLoResTime());
  int inump    = mpSourceSubMesh->numPolys();
  for (int i = 0; i < inump; i++) {
    const Polygon& ply = mpSourceSubMesh->RefPoly(i);
    int inumv       = ply.numVertices();

    std::vector<vertex_ptr_t> merged;
    for (int i = 0; i < inumv; i++) {

      auto src  = ply.vertex(i);
      merged.push_back(mpDestSubMesh->mergeVertex(*src));
    }
    Polygon polyA(merged);
    polyA.SetAnnoMap(ply.GetAnnoMap());
    mpDestSubMesh->mergePoly(polyA);
  }
  float ftimeB = float(OldSchool::GetRef().GetLoResTime());
  float ftime  = (ftimeB - ftimeA);
  orkprintf(
      "<<PROFILE>> <<Mesh::MergeToolMeshThreaded  Thread<%d> Dest<%s> NumPolys<%d> %f seconds>>\n",
      ithread,
      destname.c_str(),
      inump,
      ftime);
}

struct MergeToolMeshThreadData {
  MergeToolMeshQueue* mQ;
  int miThreadIndex;
};

struct MergeToolMeshJobThread : public ork::Thread {
  MergeToolMeshJobThread(MergeToolMeshThreadData* thread_data)
      : mData(thread_data) {
  }

  MergeToolMeshThreadData* mData;

  void run() override {
    MergeToolMeshQueue* Q = mData->mQ;

    bool bdone = false;
    while (!bdone) {
      orkvector<MergeToolMeshQueueItem>& qq = Q->mJobSet.LockForWrite();
      if (qq.size()) {
        orkvector<MergeToolMeshQueueItem>::iterator it = (qq.end() - 1);
        MergeToolMeshQueueItem qitem                   = *it;
        qq.erase(it);
        Q->mJobSet.UnLock();
        ////////////////////////////////
        qitem.DoIt(mData->miThreadIndex);
        ////////////////////////////////
        Q->mSourceMutex.Lock();
        {
          Q->miNumFinished++;
          bdone = (Q->miNumFinished == Q->miNumQueued);
          // qitem.mpSourceToolMesh->RemoveSubMesh(qitem.mSourceSubName);
        }
        Q->mSourceMutex.UnLock();
        ////////////////////////////////
      } else {
        Q->mJobSet.UnLock();
        bdone = true;
      }
    }
  }
};
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void Mesh::MergeToolMeshThreadedExcluding(const Mesh& sr, int inumthreads, const std::set<std::string>& ExcludeSet) {
  float ftimeA = float(OldSchool::GetRef().GetLoResTime());

  MergeToolMeshQueue Q;
  orkvector<MergeToolMeshQueueItem>& QV = Q.mJobSet.LockForWrite();
  {
    for (auto it : sr._submeshesByPolyGroup) {
      const submesh& src_grp  = *it.second;
      const std::string& name = it.first;

      if (ExcludeSet.find(name) == ExcludeSet.end()) {
        submesh& dest_grp = MergeSubMesh(name.c_str());
        //////////////////////////////
        MergeToolMeshQueueItem qitem;
        qitem.mpSourceSubMesh = &src_grp;
        qitem.mpDestSubMesh   = &dest_grp;
        qitem.destname        = name;
        //////////////////////////////
        QV.push_back(qitem);
        //////////////////////////////
      }
    }
  }
  Q.miNumQueued = (int)QV.size();
  Q.mJobSet.UnLock();
  /////////////////////////////////////////////////////////
  // start threads
  /////////////////////////////////////////////////////////
  orkvector<ork::Thread*> ThreadVect;
  for (int ic = 0; ic < inumthreads; ic++) {
    MergeToolMeshThreadData* thread_data = new MergeToolMeshThreadData;
    thread_data->mQ                      = &Q;
    thread_data->miThreadIndex           = ic;
    auto job_thread                      = new MergeToolMeshJobThread(thread_data);
    ThreadVect.push_back(job_thread);
  }
  /////////////////////////////////////////////////////////
  // wait for threads
  /////////////////////////////////////////////////////////
  for (auto it = ThreadVect.begin(); it != ThreadVect.end(); it++) {
    auto job = (*it);
    job->join();
    delete job;
  }
  /////////////////////////////////////////////////////////
  /////////////////////////////////////////////////////////
  MergeMaterialsFromToolMesh(sr);
  float ftimeB = float(OldSchool::GetRef().GetLoResTime());
  float ftime  = (ftimeB - ftimeA);
  orkprintf("<<PROFILE>> <<Mesh::MergeToolMeshThreaded %f seconds>>\n", ftime);
}

void Mesh::MergeToolMeshThreaded(const Mesh& sr, int inumthreads) {
  const std::set<std::string> EmptySet;
  MergeToolMeshThreadedExcluding(sr, inumthreads, EmptySet);
}

///////////////////////////////////////////////////////////////////////////////

void Mesh::MergeToolMeshAs(const Mesh& sr, const char* pgroupname) {
  submesh& dest_group = MergeSubMesh(pgroupname);
  for (auto itpg = sr._submeshesByPolyGroup.begin(); itpg != sr._submeshesByPolyGroup.end(); itpg++) {
    const submesh& src_group = *itpg->second;
    int inump                = src_group.numPolys();
    for (int ip = 0; ip < inump; ip++) {
      const Polygon& inp_poly = src_group.RefPoly(ip);
      std::vector<vertex_ptr_t> new_vertices;
      for (int iv = 0; iv < inp_poly.numVertices(); iv++) {
        int ivi               = inp_poly.vertexID(iv);
        const vertex& src_vtx = *src_group.vertex(ivi);
        new_vertices.push_back(dest_group.mergeVertex(src_vtx));
      }
      poly_ptr_t new_poly = std::make_shared<Polygon>(new_vertices);
      new_poly->SetAnnoMap(inp_poly.GetAnnoMap());
      dest_group.mergePoly(*new_poly);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

submesh& Mesh::MergeSubMesh(const char* pname) {
  auto itpg = _submeshesByPolyGroup.find(pname);
  submesh_ptr_t subm;
  if (itpg == _submeshesByPolyGroup.end()) {
    subm                         = std::make_shared<submesh>();
    subm->name                   = pname;
    _submeshesByPolyGroup[pname] = subm;
  } else {
    subm = itpg->second;
  }
  return *subm;
}

///////////////////////////////////////////////////////////////////////////////

submesh& Mesh::MergeSubMesh(const char* pname, const AnnotationMap& merge_annos) {
  auto itpg = _submeshesByPolyGroup.find(pname);
  submesh_ptr_t subm;
  if (itpg == _submeshesByPolyGroup.end()) {
    subm       = std::make_shared<submesh>();
    subm->name = pname;
    subm->MergeAnnos(merge_annos, true);
    _submeshesByPolyGroup[pname] = subm;
  } else {
    subm = itpg->second;
  }
  return *subm;
} // namespace ork::meshutil

///////////////////////////////////////////////////////////////////////////////

void Mesh::MergeSubMesh(const submesh& inp_mesh, const char* pasgroup) {
  submesh& sub_mesh = MergeSubMesh(pasgroup);
  sub_mesh.MergeSubMesh(inp_mesh);
}

///////////////////////////////////////////////////////////////////////////////
} // namespace ork::meshutil
///////////////////////////////////////////////////////////////////////////////
