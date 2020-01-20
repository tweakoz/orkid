////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/kernel/orklut.hpp>
#include <ork/file/chunkfile.h>
#include <ork/file/chunkfile.inl>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmodel.h>
#include <ork/lev2/gfx/gfxmaterial_basic.h>
#include <ork/lev2/gfx/gfxmaterial_fx.h>
#include <ork/kernel/string/deco.inl>

using namespace std::string_literals;

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////

XgmSkelNode::XgmSkelNode(const std::string& Name)
    : _name(Name)
    , _parent(0)
    , _numBoundVertices(0) {
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 XgmSkelNode::concatenated() const {
  return _parent ? (_jointMatrix * _parent->concatenated()) : _jointMatrix;
}
fmtx4 XgmSkelNode::concatenatednode() const {
  return _parent ? (_nodeMatrix * _parent->concatenatednode()) : _nodeMatrix;
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 XgmSkelNode::bindMatrix() const {
  return _bindMatrixInverse.inverse();
}

///////////////////////////////////////////////////////////////////////////////

XgmSkelNode::NodeType XgmSkelNode::nodetype() const {
  if (_parent == nullptr) {
    return ENODE_ROOT;
  }
  if (mChildren.size()) {
    return ENODE_NONLEAF;
  }
  return ENODE_LEAF;
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkelNode::visitHierarchy(nodevisitfn_t visitfn) {
  visitfn(this);
  for (auto child : mChildren) {
    child->visitHierarchy(visitfn);
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkelNode::visitHierarchyUp(nodevisitfn_t visitfn) {
  visitfn(this);
  if (_parent) {
    _parent->visitHierarchyUp(visitfn);
  }
}

///////////////////////////////////////////////////////////////////////////////

XgmSkelNode* XgmSkelNode::findCentimeterToMeterNode() {
  XgmSkelNode* rval = nullptr;
  visitHierarchy([&rval](XgmSkelNode* node) {
    if (rval == nullptr) {
      auto parent = node->_parent;
      if (parent) {
        DecompMtx44 pdc, cdc;
        parent->_jointMatrix.decompose(pdc.mTrans, pdc.mRot, pdc.mScale);
        node->_jointMatrix.decompose(cdc.mTrans, cdc.mRot, cdc.mScale);
        printf("parscale<%s:%g>\n", parent->_name.c_str(), pdc.mScale);
        printf("chiscale<%s:%g>\n", node->_name.c_str(), cdc.mScale);
        bool parent_match = math::areValuesClose(pdc.mScale, 1.0, 0.00001);
        bool child_match  = math::areValuesClose(cdc.mScale, 0.01, 0.00001);
        if (parent_match and child_match) {
          rval = node;
          printf("FOUND SCALENODE\n");
        }
      }
    }
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool XgmSkelNode::applyCentimeterToMeterScale() {
  auto cmscalenode = findCentimeterToMeterNode();
  if (cmscalenode) {
    auto d = cmscalenode->concatenated().dump();
    deco::printf(fvec3::Red(), "cmscalenode<%s> %s\n", cmscalenode->_name.c_str(), d.c_str());
  }
  if (cmscalenode) {
    DecompMtx44 ScaleXf;
    cmscalenode->_jointMatrix.decompose(ScaleXf.mTrans, ScaleXf.mRot, ScaleXf.mScale);
    ScaleXf.mTrans *= 0.01f;
    cmscalenode->_jointMatrix.compose(ScaleXf.mTrans, ScaleXf.mRot, 1.0f);
    cmscalenode->visitHierarchy([cmscalenode, &ScaleXf](XgmSkelNode* node) {
      if (node != cmscalenode) {
        DecompMtx44 cdc;
        node->_jointMatrix.decompose(cdc.mTrans, cdc.mRot, cdc.mScale);
        cdc.mTrans *= ScaleXf.mScale;
        cdc.mScale *= ScaleXf.mScale;
        if (node->nodetype() == XgmSkelNode::ENODE_LEAF)
          node->_jointMatrix.compose(cdc.mTrans, cdc.mRot, cdc.mScale);
      }
      fvec3 color;
      switch (node->nodetype()) {
        case XgmSkelNode::ENODE_ROOT:
          color = fvec3::Cyan();
          break;
        case XgmSkelNode::ENODE_NONLEAF:
          color = fvec3::Magenta();
          break;
        case XgmSkelNode::ENODE_LEAF:
          color = fvec3::White();
          break;
      }
      auto d = node->concatenated().dump();
      deco::printf(color, "node<%s> %s\n", node->_name.c_str(), d.c_str());
    });
  }
  return cmscalenode != nullptr;
}

///////////////////////////////////////////////////////////////////////////////
// is this node a parent of testnode
///////////////////////////////////////////////////////////////////////////////

bool XgmSkelNode::isParentOf(XgmSkelNode* testnode) {
  bool rval = false;
  visitHierarchy([this, testnode, &rval](XgmSkelNode* node) {
    if (node != this and node == testnode)
      rval = true;
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// is this node a descendant of testnode
///////////////////////////////////////////////////////////////////////////////

bool XgmSkelNode::isDescendantOf(XgmSkelNode* testnode) {
  bool rval = false;
  visitHierarchyUp([this, testnode, &rval](XgmSkelNode* node) {
    if (node != this and node == testnode)
      rval = true;
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

XgmSkeleton::XgmSkeleton()
    : miNumJoints(0)
    , maJointParents(0)
    , mpUserData(0)
    , miRootNode(-1)
    , mpRootNode(0) {
}

///////////////////////////////////////////////////////////////////////////////

XgmSkeleton::~XgmSkeleton() {
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkeleton::resize(int inumjoints) {
  miNumJoints = inumjoints;

  mvJointNameVect.resize(inumjoints);
  maJointParents.resize(inumjoints);
  _inverseBindMatrices.resize(inumjoints);
  _jointMatrices.resize(inumjoints);
  _nodeMatrices.resize(inumjoints);
}

///////////////////////////////////////////////////////////////////////////////

float XgmSkeleton::boneLength(int ibone) const {

  auto bone = _bones[ibone];
  auto pmtx = _inverseBindMatrices[bone._parentIndex].inverse();
  auto cmtx = _inverseBindMatrices[bone._childIndex].inverse();
  auto ppos = pmtx.GetTranslation();
  auto cpos = cmtx.GetTranslation();
  return (cpos - ppos).length();
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmSkeleton::dumpBind(fvec3 color) const {
  std::string rval;
  if (miRootNode >= 0) {
    int inumjoints = numJoints();
    fvec3 ca       = color;
    fvec3 cb       = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = GetJointName(ij).c_str();
      auto jmtx        = _inverseBindMatrices[ij].inverse();
      rval += deco::asciic_rgb(cc);
      rval += FormatString("%28s", name.c_str());
      rval += ": "s + jmtx.dump4x3(cc) + "\n"s;
      rval += deco::asciic_reset();
    }
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string XgmSkeleton::dumpInvBind(fvec3 color) const {
  std::string rval;
  if (miRootNode >= 0) {
    int inumjoints = numJoints();
    fvec3 ca       = color;
    fvec3 cb       = color * 0.7;

    for (int ij = 0; ij < inumjoints; ij++) {
      fvec3 cc         = (ij & 1) ? cb : ca;
      std::string name = GetJointName(ij).c_str();
      const auto& jmtx = _inverseBindMatrices[ij];
      rval += deco::asciic_rgb(cc);
      rval += FormatString("%28s", name.c_str());
      rval += ": "s + jmtx.dump4x3(cc) + "\n"s;
      rval += deco::asciic_reset();
    }
  }
  return rval;
}

////////////////////////////////////////////////////////////////////////////

std::string XgmSkeleton::dump(fvec3 color) const {
  std::string rval;
  auto color2 = color * 0.5;
  rval += deco::format(color, "XgmSkeleton<%p>\n", this);
  rval += deco::format(color, " numjoints<%d>\n", miNumJoints);
  rval += deco::format(color, " rootindex<%d>\n", miRootNode);

  int i = 0;
  for (orklut<PoolString, int>::const_iterator it = mmJointNameMap.begin(); it != mmJointNameMap.end(); it++) {
    PoolString sidx = (*it).first;
    int idx         = (*it).second;
    // rval += deco::format(color," jointnamemap<%d> <%s>:<%d>\n", i, sidx.c_str(), idx);
    i++;
  }
  i = 0;
  for (orkvector<PoolString>::const_iterator it = mvJointNameVect.begin(); it != mvJointNameVect.end(); it++) {
    const PoolString& s = (*it);
    // rval += deco::format(color," jointnamevect<%d> <%s>\n", i, s.c_str());
    i++;
  }
  i = 0;
  for (orkvector<XgmBone>::const_iterator it = _bones.begin(); it != _bones.end(); it++) {
    const XgmBone& b = (*it);
    rval += deco::format(color, " bone<%d> p<%d> c<%d>\n", i, b._parentIndex, b._childIndex);
    i++;
  }

  rval += deco::format(color, " topmat: ") + mTopNodesMatrix.dump4x3(color) + "\n";
  rval += deco::format(color, " bindmat: ") + mBindShapeMatrix.dump4x3(color) + "\n";

  for (int ij = 0; ij < miNumJoints; ij++) {
    auto name = GetJointName(ij);
    rval += deco::format(color, "   joint<%02d:%s>\n", ij, name.c_str());

    int parent          = maJointParents[ij];
    const char* parname = (parent >= 0) ? GetJointName(parent).c_str() : "none";
    rval += deco::format(color, "     parent<%d:%s>\n", parent, parname);

    rval += deco::format(color, "     ljmat: ") + _jointMatrices[ij].dump4x3(color) + "\n";
    rval += deco::format(color, "     ibmat: ") + _inverseBindMatrices[ij].dump4x3(color) + "\n";
    rval += deco::format(color, "     ndmat: ") + _nodeMatrices[ij].dump4x3(color) + "\n";
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

int XgmSkeleton::jointIndex(const ork::PoolString& Named) const {
  orklut<PoolString, int>::const_iterator it = mmJointNameMap.find(Named);
  int index                                  = (it == mmJointNameMap.end()) ? -1 : it->second;
  if (index == -1) {
    // printf( "find joint<%s> in map\n", Named.c_str() );
    for (auto it : mmJointNameMap) {
      // printf( "in map key<%s>\n", it.first.c_str());
    }
  }
  return index;
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkeleton::AddJoint(int iskelindex, int iparindex, const PoolString& name) {
  mvJointNameVect[iskelindex] = name;
  mmJointNameMap.AddSorted(name, iskelindex);
  maJointParents[iskelindex] = iparindex;
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkeleton::addBone(const XgmBone& bone) {
  _bones.push_back(bone);
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 XgmSkeleton::concatenated(PoolString named) const {
  std::vector<int> walk;
  int index = jointIndex(named);
  while (index != -1) {
    walk.push_back(index);
    index = GetJointParent(index);
  }
  int walklen = sizeof(walk);
  fmtx4 rval;
  for (int i = walklen; i >= 0; i--) {
    int jidx = walk[i];
    auto mtx = RefJointMatrix(jidx);
    rval     = rval * mtx;
  }
  return rval;
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2
