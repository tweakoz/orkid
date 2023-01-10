#pragma once

struct Transformer{

  /////////////////////////////////////////////////////////////////////////////

  Transformer( const XgmSkeleton& skel)
    : _skeleton(skel) {

  }

  /////////////////////////////////////////////////////////////////////////////

  void bindToBone(std::string named){
    int index = _skeleton.jointIndex(named);
    _jointindices.push_back(index);
  }

  /////////////////////////////////////////////////////////////////////////////

  void compute(XgmLocalPose& localpose, //
               const fmtx4& xf){

    int count = _jointindices.size();
    for( int i=0; i<count; i++ ){
      int ji = _jointindices[i];
      auto& J = localpose._concat_matrices[ji];
      J = xf*J;
    }
  }

  /////////////////////////////////////////////////////////////////////////////

  std::vector<int> _jointindices;
  const XgmSkeleton& _skeleton;

};

using transformer_ptr_t = std::shared_ptr<Transformer>;