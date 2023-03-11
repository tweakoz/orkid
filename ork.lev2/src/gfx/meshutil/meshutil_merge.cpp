////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
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

void Mesh::MergeSubMesh(const Mesh& src, const submesh& pgrp, const char* newname) {
  float ftimeA   = float(OldSchool::GetRef().GetLoResTime());
  auto pnewgroup = submeshFromGroupName(newname);
  if (nullptr == pnewgroup) {
    pnewgroup                      = std::make_shared<submesh>();
    _submeshesByPolyGroup[newname] = pnewgroup;
  }
  int inumpingroup = pgrp.GetNumPolys();
  for (int i = 0; i < inumpingroup; i++) {
    const poly& ply = pgrp.RefPoly(i);
    int inumpv      = ply.GetNumSides();
    poly NewPoly;
    NewPoly.miNumSides = inumpv;
    for (int iv = 0; iv < inumpv; iv++) {
      int ivi               = ply.GetVertexID(iv);
      const vertex& vtx     = pgrp.RefVertexPool().GetVertex(ivi);
      auto newvtx           = pnewgroup->mergeVertex(vtx);
      NewPoly._vertices[iv] = newvtx;
    }
    NewPoly.SetAnnoMap(ply.GetAnnoMap());
    pnewgroup->MergePoly(NewPoly);
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
  int inump    = mpSourceSubMesh->GetNumPolys();
  for (int i = 0; i < inump; i++) {
    if (i == 0x194 && inump == 0x145b) {
      orkprintf("yo\n");
    }
    const poly& ply = mpSourceSubMesh->RefPoly(i);
    int inumv       = ply.GetNumSides();
    if (inumv > kmaxsidesperpoly) {
      OrkAssert(false);
      continue;
    }
    vertex_ptr_t merged[kmaxsidesperpoly];
    for (int i = 0; i < inumv; i++) {

      auto src  = ply._vertices[i];
      merged[i] = mpDestSubMesh->mergeVertex(*src);
    }
    poly polyA(merged, inumv);
    polyA.SetAnnoMap(ply.GetAnnoMap());
    mpDestSubMesh->MergePoly(polyA);
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
    int inump                = src_group.GetNumPolys();
    for (int ip = 0; ip < inump; ip++) {
      const poly& ply = src_group.RefPoly(ip);
      int inumv       = ply.GetNumSides();
      if (inumv > kmaxsidesperpoly) {
        OrkAssert(false);
        continue;
      }
      vertex_ptr_t merged[kmaxsidesperpoly];
      for (int i = 0; i < inumv; i++)
        merged[i] = dest_group.mergeVertex(*ply._vertices[i]);
      poly npoly(merged, inumv);
      npoly.SetAnnoMap(ply.GetAnnoMap());
      dest_group.MergePoly(npoly);
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
