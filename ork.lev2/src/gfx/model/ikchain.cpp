////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/lev2/gfx/gfxanim.h>
#include <ork/lev2/gfx/ikchain.h>
#include <ork/math/math_types.inl>

namespace ork::lev2 {

/////////////////////////////////////////////////////////////////////////////

IkChain::IkChain(xgmskeleton_constptr_t skel)
    : _skeleton(skel) {

  _bindpose = std::make_shared<XgmLocalPose>(_skeleton);
  _bindpose->bindPose();
  _bindpose->blendPoses();
  _bindpose->concatenate();
}

/////////////////////////////////////////////////////////////////////////////

void IkChain::bindToJointNamed(std::string named) {
  int index = _skeleton->jointIndex(named);
  _jointindices.push_back(index);
}
void IkChain::bindToJointPath(std::string path) {
  int index = _skeleton->jointIndexFromPath(path);
  _jointindices.push_back(index);
}
void IkChain::bindToJointID(std::string path) {
  int index = _skeleton->jointIndexFromID(path);
  _jointindices.push_back(index);
}

/////////////////////////////////////////////////////////////////////////////

void IkChain::prepare() {

  int numbones = _skeleton->numBones();
  for (int bi = 0; bi < numbones; bi++) {
    int child           = _skeleton->bone(bi)._childIndex;
    _bonelengths[child] = _skeleton->boneLength(bi);
  }
}

/////////////////////////////////////////////////////////////////////////////

void IkChain::compute(
    xgmlocalpose_ptr_t localpose, //
    const fvec3& target) {

  ///////////////////////////////////////////////////////////
  // fill in pose
  ///////////////////////////////////////////////////////////

  int count = _jointindices.size();
  int last  = count - 1;

  _jointtemp.resize(count);

  for (int i = 0; i < count; i++) {
    int ji        = _jointindices[i];
    _jointtemp[i] = localpose->_concat_matrices[ji];
  }

  ///////////////////////////////////////////////////

  auto pivot = [](const fquat& Q, const fvec3& pivot_pos, fmtx4& dest_matrix) {
    fmtx4 P;
    P.setTranslation(pivot_pos);
    fmtx4 R     = P * fmtx4(Q) * P.inverse();
    dest_matrix = R * dest_matrix;
  };

  ///////////////////////////////////////////////////

  auto do_end = [&]() {
    auto& end_joint = _jointtemp[last];

    auto head   = end_joint.translation();
    auto tail   = fvec3(0, 1, 0).transform(end_joint).xyz();
    auto h2tdir = (tail - head).normalized();
    auto del    = (target - head).normalized();
    auto axis   = h2tdir.crossWith(del).normalized();

    float angle = glm::orientedAngle(
        h2tdir.asGlmVec3(), //
        del.asGlmVec3(),    //
        axis.asGlmVec3());

    auto Q = fquat(axis, angle * _C2);

    pivot(Q, head, end_joint);
  };

  ///////////////////////////////////////////////////

  int maxiters = int(_C1 * 512);

  for (int outer_loop = 0; outer_loop < maxiters; outer_loop++) {

    for (int i = last; i >= 0; i--) {

      bool end_link = (i == last);

      if (end_link) {
        do_end();
      } else { // inner link

        auto head = _jointtemp[i].translation();
        auto end  = _jointtemp[last].translation();

        auto del_tgt = (target - head).normalized();
        auto del_end = (end - head).normalized();

        auto axis = del_end.crossWith(del_tgt).normalized();

        float angle = glm::orientedAngle(
            del_end.asGlmVec3(), //
            del_tgt.asGlmVec3(), //
            axis.asGlmVec3());

        auto Q = fquat(axis, angle * _C2);

        for (int j = i; j < count; j++) {
          pivot(Q, head, _jointtemp[j]);
        }
      }
    }
  }

  for (int i = 0; i < count; i++) {
    int ji                         = _jointindices[i];
    localpose->_concat_matrices[ji] = _jointtemp[i];
  }
}

} // namespace ork::lev2
