////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
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
#include <ork/kernel/string/deco.inl>
#include <ork/util/logger.h>

using namespace std::string_literals;

namespace ork::lev2 {
///////////////////////////////////////////////////////////////////////////////
static logchannel_ptr_t logchan_skel = logger()->createChannel("gfxanim.skel", fvec3(1, 0.7, 1));

XgmSkelNode::XgmSkelNode(const std::string& Name)
    : _name(Name) {
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 XgmSkelNode::concatenated() const {
  return _parent ? fmtx4::multiply_ltor(_parent->concatenated(),_jointMatrix) //
                 : _jointMatrix;
}
fmtx4 XgmSkelNode::concatenatednode() const {
  return _parent ? fmtx4::multiply_ltor(_nodeMatrix,_parent->concatenatednode()) //
                 : _nodeMatrix;
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 XgmSkelNode::concatenated2() const {
  return _parent ? fmtx4::multiply_ltor(_parent->concatenated2() , _jointMatrix) //
                 : _jointMatrix;
}
fmtx4 XgmSkelNode::concatenatednode2() const {
  return _parent ? fmtx4::multiply_ltor(_parent->concatenatednode2() , _nodeMatrix) //
                 : _nodeMatrix;
}

///////////////////////////////////////////////////////////////////////////////

XgmSkelNode::NodeType XgmSkelNode::nodetype() const {
  if (_parent == nullptr) {
    return ENODE_ROOT;
  }
  if (_children.size()) {
    return ENODE_NONLEAF;
  }
  return ENODE_LEAF;
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkelNode::visitHierarchy(xgmskelnode_ptr_t node,nodevisitfn_t visitfn) {
  visitfn(node);
  for (auto child : node->_children) {
    visitHierarchy(child,visitfn);
  }
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkelNode::visitHierarchyUp(xgmskelnode_ptr_t node,nodevisitfn_t visitfn) {
  visitfn(node);
  if (node->_parent) {
    visitHierarchyUp(node->_parent,visitfn);
  }
}

///////////////////////////////////////////////////////////////////////////////

xgmskelnode_ptr_t XgmSkelNode::findCentimeterToMeterNode(xgmskelnode_ptr_t root) {
  xgmskelnode_ptr_t rval = nullptr;
  XgmSkelNode::visitHierarchy(root,[&rval](xgmskelnode_ptr_t node) {
    if (rval == nullptr) {
      auto parent = node->_parent;
      if (parent) {
        DecompTransform pdc, cdc;
        pdc.decompose(parent->_jointMatrix);
        cdc.decompose(node->_jointMatrix);
        logchan_skel->log("parscale<%s:%g>", parent->_name.c_str(), pdc._uniformScale);
        logchan_skel->log("chiscale<%s:%g>", node->_name.c_str(), cdc._uniformScale);
        constexpr float my_epsilon = 0.00001;
        bool parent_match = math::areValuesClose(pdc._uniformScale, 1.0, my_epsilon);
        bool child_match  = math::areValuesClose(cdc._uniformScale, 0.01, my_epsilon);
        if (parent_match and child_match) {
          rval = node;
          logchan_skel->log("FOUND SCALENODE\n");
          OrkAssert(false);
        }
      }
    }
  });
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool XgmSkelNode::applyCentimeterToMeterScale(xgmskelnode_ptr_t root) {
  auto cmscalenode = findCentimeterToMeterNode(root);
  if (cmscalenode) {
    auto d = cmscalenode->concatenated().dump();
    deco::printf(fvec3::Red(), "cmscalenode<%s> %s\n", cmscalenode->_name.c_str(), d.c_str());
  }
  if (cmscalenode) {
    DecompTransform ScaleXf;
    ScaleXf.decompose(cmscalenode->_jointMatrix);
    ScaleXf._translation *= 0.01f;
    ScaleXf._uniformScale = 1.0f;
    cmscalenode->_jointMatrix = ScaleXf.composed();
    XgmSkelNode::visitHierarchy(cmscalenode,[cmscalenode, &ScaleXf](xgmskelnode_ptr_t node) {
      if (node != cmscalenode) {
        DecompTransform cdc;
        cdc.decompose(node->_jointMatrix);
        cdc._translation *= ScaleXf._uniformScale;
        cdc._uniformScale *= ScaleXf._uniformScale;
        if (node->nodetype() == XgmSkelNode::ENODE_LEAF)
          node->_jointMatrix = cdc.composed();
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
// is parnode a parent of childnode ?
///////////////////////////////////////////////////////////////////////////////

bool XgmSkelNode::isParentOf(xgmskelnode_ptr_t parnode, xgmskelnode_ptr_t childnode) {
  //////////////////////////
  if(parnode==childnode)
    return false;
  if(parnode==nullptr)
    return false;
  if(childnode==nullptr)
    return false;
  //////////////////////////
  bool rval = false;
  //////////////////////////
  visitHierarchyUp(childnode,[parnode, &rval](xgmskelnode_ptr_t node) {
    if (node == parnode)
      rval = true;
  });
  //////////////////////////
  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// is childnode a descendant of parnode ?
///////////////////////////////////////////////////////////////////////////////

bool XgmSkelNode::isDescendantOf(xgmskelnode_ptr_t childnode, xgmskelnode_ptr_t parnode) {
  return isParentOf(parnode,childnode);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

XgmSkeleton::XgmSkeleton() {
}

///////////////////////////////////////////////////////////////////////////////

XgmSkeleton::~XgmSkeleton() {
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkeleton::resize(int inumjoints) {
  miNumJoints = inumjoints;

  mvJointNameVect.resize(inumjoints);
  maJointParents.resize(inumjoints);
  _bindMatrices.resize(inumjoints);
  _inverseBindMatrices.resize(inumjoints);
  _jointMatrices.resize(inumjoints);
  _nodeMatrices.resize(inumjoints);
}

///////////////////////////////////////////////////////////////////////////////

float XgmSkeleton::boneLength(int ibone) const {

  auto bone = _bones[ibone];
  auto pmtx = _bindMatrices[bone._parentIndex];
  auto cmtx = _bindMatrices[bone._childIndex];
  auto ppos = pmtx.translation();
  auto cpos = cmtx.translation();
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
      auto jmtx        = _bindMatrices[ij];
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
  for (auto item : mmJointNameMap ) {
    const std::string& sidx = item.first;
    int idx         = item.second;
    // rval += deco::format(color," jointnamemap<%d> <%s>:<%d>\n", i, sidx.c_str(), idx);
    i++;
  }
  i = 0;
  for (const std::string& name : mvJointNameVect ) {
    // rval += deco::format(color," jointnamevect<%d> <%s>\n", i, s.c_str());
    i++;
  }
  i = 0;
  for (const XgmBone& bone : _bones ) {
    rval += deco::format(color, " bone<%d> p<%d> c<%d>\n", i, bone._parentIndex, bone._childIndex);
    i++;
  }

  rval += deco::format(color, " topmat: ") + mTopNodesMatrix.dump4x3cn() + "\n";
  rval += deco::format(color, " bindmat: ") + mBindShapeMatrix.dump4x3cn() + "\n";

  for (int ij = 0; ij < miNumJoints; ij++) {
    auto name = GetJointName(ij);
    rval += deco::format(color, "   joint<%02d:%s>\n", ij, name.c_str());

    int parent          = maJointParents[ij];
    const char* parname = (parent >= 0) ? GetJointName(parent).c_str() : "none";
    rval += deco::format(color, "     parent<%d:%s>\n", parent, parname);

    rval += deco::format(color, "     ljmat: ") + _jointMatrices[ij].dump4x3cn() + "\n";
    rval += deco::format(color, "     ibmat: ") + _inverseBindMatrices[ij].dump4x3cn() + "\n";
    rval += deco::format(color, "     ndmat: ") + _nodeMatrices[ij].dump4x3cn() + "\n";
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

int XgmSkeleton::jointIndex(const std::string& named) const {
  auto it = mmJointNameMap.find(named);
  int index = (it == mmJointNameMap.end()) ? -1 : it->second;
  if (index == -1) {
    // printf( "find joint<%s> in map\n", named.c_str() );
    for (auto it : mmJointNameMap) {
      // printf( "in map key<%s>\n", it.first.c_str());
    }
  }
  return index;
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkeleton::AddJoint(int iskelindex, int iparindex, const std::string& name) {
  mvJointNameVect[iskelindex] = name;
  mmJointNameMap.AddSorted(name, iskelindex);
  maJointParents[iskelindex] = iparindex;
}

///////////////////////////////////////////////////////////////////////////////

void XgmSkeleton::addBone(const XgmBone& bone) {
  _bones.push_back(bone);
}

///////////////////////////////////////////////////////////////////////////////

fmtx4 XgmSkeleton::concatenated(std::string named) const {
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
    rval     = fmtx4::multiply_ltor(rval,mtx);
  }
  return rval;
}

////////////////////////////////////////////////////////////////
} // namespace ork::lev2
