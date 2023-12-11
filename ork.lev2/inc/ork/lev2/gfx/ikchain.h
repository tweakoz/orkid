////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cmatrix4.h>
#include <ork/math/cvector3.h>
#include <ork/lev2/lev2_types.h>

namespace ork::lev2 {
struct IkChain{

  /////////////////////////////////////////////////////////////////////////////

  IkChain( xgmskeleton_constptr_t skel);
  void bindToJointNamed(std::string named);
  void bindToJointPath(std::string path);
  void bindToJointID(std::string idstr);
  void prepare();
  void compute(xgmlocalpose_ptr_t localpose, //
               const fvec3& target);

  /////////////////////////////////////////////////////////////////////////////

  std::vector<int> _jointindices;
  std::vector<fmtx4> _jointtemp;
  std::map<int,float> _bonelengths; // length from parent to child

  xgmskeleton_constptr_t _skeleton;
  xgmlocalpose_ptr_t _bindpose;

  float _C1 = 0.222f;
  float _C2 = 0.007f;
  float _C3 = 0.341f;
  float _C4 = 1.000f;
};

using ikchain_ptr_t = std::shared_ptr<IkChain>;
} //namespace ork::lev2 {
