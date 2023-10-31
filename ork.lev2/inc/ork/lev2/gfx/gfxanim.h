////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/lev2_types.h>
#include <ork/math/cmatrix4.h>
#include <ork/math/box.h>
#include <ork/file/path.h>
#include <ork/kernel/varmap.inl>
#include <ork/kernel/orklut.h>
#include <ork/rtti/RTTIX.inl>

namespace ork::lev2 {

static const int kmaxbones = 64;

/// ///////////////////////////////////////////////////////////////////////////
/// Animation Channel
/// there can be multiple animation channels per anim
/// each channel has a name (target)
/// each channel has a type (float,vector,matrix,etc..)
/// each channel also has a usage semantic (such as "Joint" or "Effect")
/// ///////////////////////////////////////////////////////////////////////////

struct DecompMatrix{

  fmtx4 compose() const;
  void decompose(fmtx4) ;
  fvec3 _position;
  fquat _orientation;
  fvec3 _scale;
};

struct XgmAnimChannel : public ork::Object {
  DeclareAbstractX(XgmAnimChannel, ork::Object);
public:

  enum EChannelType {
    EXGMAC_FLOAT = 0,
    EXGMAC_VECT2, // fvec2
    EXGMAC_VECT3, // fvec3
    EXGMAC_VECT4, // fvec4
    // EXGMAC_MTX43,		// 4x3 Matrix
    EXGMAC_MTX44, // 4x4 Matrix
    EXGMAC_DCMTX, // Decomposed 4x4 Matrix (quat, scale and pos)
  };

  XgmAnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& UsageSemantic, EChannelType etype);
  XgmAnimChannel(EChannelType etype);
  virtual ~XgmAnimChannel() {}

  const std::string& GetChannelName() const {
    return mChannelName;
  }
  const std::string& GetObjectName() const {
    return mObjectName;
  }
  EChannelType GetChannelType() const {
    return meChannelType;
  }
  const std::string& GetUsageSemantic() const {
    return mUsageSemantic;
  }
  virtual size_t numFrames() const = 0;

  void SetChannelName(const std::string& name) {
    mChannelName = name;
  }
  void SetObjectName(const std::string& name) {
    mObjectName = name;
  }
  void SetChannelUsage(const std::string& usage) {
    mUsageSemantic = usage;
  }

  std::string mChannelName;
  std::string mObjectName;
  std::string mUsageSemantic;
  int miNumFrames;
  EChannelType meChannelType;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmFloatAnimChannel : public XgmAnimChannel {
  DeclareConcreteX(XgmFloatAnimChannel, XgmAnimChannel);
public:


  XgmFloatAnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage);
  XgmFloatAnimChannel();

  void setFrame(size_t i, float v) {
    _sampledFrames[i] = v;
  }
  float GetFrame(int index) const {
    return _sampledFrames[index];
  }
  void reserveFrames(size_t iv) {
    _sampledFrames.reserve(iv);
  }

  size_t numFrames() const final {
    return int(_sampledFrames.size());
  }
  orkvector<float> _sampledFrames;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmVect3AnimChannel : public XgmAnimChannel {
  DeclareConcreteX(XgmVect3AnimChannel, XgmAnimChannel);
public:

  XgmVect3AnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage);
  XgmVect3AnimChannel();

  void setFrame(size_t i, const fvec3& v) {
    _sampledFrames[i] = v;
  }
  fvec3 GetFrame(int index) const {
    return _sampledFrames[index];
  }
  void reserveFrames(size_t iv) {
    _sampledFrames.reserve(iv);
  }

  size_t numFrames() const final {
    return int(_sampledFrames.size());
  }
  orkvector<fvec3> _sampledFrames;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmVect4AnimChannel : public XgmAnimChannel {
  DeclareConcreteX(XgmVect4AnimChannel, XgmAnimChannel);

public:

  XgmVect4AnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage);
  XgmVect4AnimChannel();

  void setFrame(size_t i, const fvec4& v) {
    _sampledFrames[i] = v;
  }
  fvec4 GetFrame(int index) const {
    return _sampledFrames[index];
  }
  void reserveFrames(size_t iv) {
    _sampledFrames.reserve(iv);
  }

  size_t numFrames() const final {
    return int(_sampledFrames.size());
  }

  orkvector<fvec4> _sampledFrames;
};

///////////////////////////////////////////////////////////////////////////////

struct XgmDecompMatrixAnimChannel : public XgmAnimChannel  {
  DeclareConcreteX(XgmDecompMatrixAnimChannel, XgmAnimChannel);

public:

  size_t numFrames() const override;

  XgmDecompMatrixAnimChannel(const std::string& ObjName, const std::string& ChanName, const std::string& Usage);
  XgmDecompMatrixAnimChannel();
  ~XgmDecompMatrixAnimChannel() override;

  void setFrame(size_t i, const DecompMatrix& v);
  DecompMatrix GetFrame(int index) const;
  void reserveFrames(size_t iv);


  std::vector<DecompMatrix> _sampledFrames;
};

/// ///////////////////////////////////////////////////////////////////////////
/// Animation
/// mAnimationChannels:	collection of animation channels
/// mPose:				static state of joints (when there is no animated data)
/// ///////////////////////////////////////////////////////////////////////////

struct XgmAnim {

  using joint_channels_lut_t = ork::orklut<std::string, animdecompmatrixchannel_ptr_t>;
  using material_channels_lut_t = ork::orklut<std::string, animchannel_ptr_t>;
  using matrix_lut_t = ork::orklut<std::string,DecompMatrix>;

  void AddChannel(const std::string& Name, animchannel_ptr_t pchan);

  //////////////////////////

  static bool LoadUnManaged(XgmAnim* mdl, const AssetPath& fname);
  static datablock_ptr_t Save(const XgmAnim* panm);

  static bool unloadUnManaged(XgmAnim* mdl);

  //////////////////////////

  static bool _loaderSelect(XgmAnim* mdl, datablock_ptr_t dblock);
  static bool _loadXGA(XgmAnim* mdl, datablock_ptr_t dblock);
  static bool _loadAssimp(XgmAnim* mdl, datablock_ptr_t dblock);

  //////////////////////////

  XgmAnim();

  size_t GetNumMaterialChannels(void) const {
    return mMaterialAnimationChannels.size();
  }
  const material_channels_lut_t& RefMaterialChannels(void) const {
    return mMaterialAnimationChannels;
  }

  size_t _numframes = 0;
  joint_channels_lut_t _jointanimationchannels;
  material_channels_lut_t mMaterialAnimationChannels;
  matrix_lut_t _static_pose;
};

/// ///////////////////////////////////////////////////////////////////////////
/// Animation Mask
/// Collection of Enable Bits for an AnimInst (1 bit per animation channel)
/// ///////////////////////////////////////////////////////////////////////////

struct XgmAnimMask {
  static const int knummaskbytes = (100 / 8) * 4; // 100 bones, 8 bits per byte, times 4 bit mask
  U8 mMaskBits[knummaskbytes];

  XgmAnimMask();
  XgmAnimMask(const XgmAnimMask& mask);
  XgmAnimMask& operator=(const XgmAnimMask& mask);

  void EnableAll();
  void Enable(xgmskeleton_constptr_t Skeleton, const std::string& BoneName);
  void Enable(int iboneindex);

  void DisableAll();
  void Disable(xgmskeleton_constptr_t Skeleton, const std::string& BoneName);
  void Disable(int iboneindex);

  bool isEnabled(xgmskeleton_constptr_t Skeleton, const std::string& BoneName) const;
  bool isEnabled(int iboneindex) const;

};

struct XgmSkeletonBinding {
  unsigned int mSkelIndex : 16;
  unsigned int mChanIndex : 16;
  XgmSkeletonBinding(int is = -1, int ic = -1)
      : mSkelIndex(is)
      , mChanIndex(ic) {
  }
};

struct XgmPoser {

  const XgmSkeletonBinding& getPoseBinding(int i) const;
  const XgmSkeletonBinding& getAnimBinding(int i) const;
  void setPoseBinding(int i, const XgmSkeletonBinding& inp);
  void setAnimBinding(int i, const XgmSkeletonBinding& inp);


  XgmSkeletonBinding _poseBindings[kmaxbones];
  XgmSkeletonBinding _animBindings[kmaxbones];

};

struct XgmSkelApplicator {

  using fn_t = std::function<void(int i)>;

  XgmSkelApplicator(xgmskeleton_constptr_t skeleton);

  xgmskelapplicator_ptr_t clone() const;

  void bindToBone(const std::string& a);

  void apply(fn_t the_fn) const;

  xgmskeleton_constptr_t _skeleton;

  std::vector<int> _bones2apply;

};

/// ///////////////////////////////////////////////////////////////////////////
/// Animation Instance
/// A running instance of an animation
/// mMask:		Animation Mask for this instance
/// mWeight:	Matrix Weighting for this instance
/// ///////////////////////////////////////////////////////////////////////////

struct XgmAnimInst {

  XgmAnimInst();

  void bindToSkeleton(xgmskeleton_constptr_t skeleton);
  void applyToPose(XgmLocalPose& localpose) const;

  void bindAnim(const XgmAnim* anim);

  float GetSampleRate() const {
    return 30.0f;
  }
  size_t numFrames() const {
    return (_animation != nullptr) ? _animation->_numframes : 0;
  }
  float GetWeight() const {
    return mWeight;
  }

  void SetWeight(float fw) {
    mWeight = fw;
  }

  XgmAnimMask& RefMask() {
    return mMask;
  }
  const XgmAnimMask& RefMask() const {
    return mMask;
  }

  const XgmAnim* _animation = nullptr;
  XgmAnimMask mMask;
  float _current_frame;
  float mWeight;
  bool _use_temporal_lerp = false;

  XgmPoser _poser;

};

/// ///////////////////////////////////////////////////////////////////////////
/// Heirarchal skelton node (only used in collada exporter, will be moved
/// ///////////////////////////////////////////////////////////////////////////

struct XgmSkelNode {

  XgmSkelNode(const std::string& Name);

  ///////////////////////////

  enum NodeType {
    ENODE_ROOT = 0,
    ENODE_NONLEAF,
    ENODE_LEAF,
  };

  typedef std::function<void(xgmskelnode_ptr_t node)> nodevisitfn_t;

  ///////////////////////////

  fmtx4 concatenated_joint() const;
  fmtx4 concatenated_node() const;
  NodeType nodetype() const;
  static void visitHierarchy(xgmskelnode_ptr_t node,nodevisitfn_t visitfn);
  static void visitHierarchyUp(xgmskelnode_ptr_t node,nodevisitfn_t visitfn);
  static xgmskelnode_ptr_t findCentimeterToMeterNode(xgmskelnode_ptr_t root);
  static bool applyCentimeterToMeterScale(xgmskelnode_ptr_t root);
  static bool isParentOf(xgmskelnode_ptr_t parnode, xgmskelnode_ptr_t childnode);     // is this node a parent of testnode
  static bool isDescendantOf(xgmskelnode_ptr_t childnode, xgmskelnode_ptr_t parnode); // is this node a descendant of testnode

  ///////////////////////////

  xgmskelnode_ptr_t _parent  = nullptr;
  int miSkelIndex       = -1;
  int _numBoundVertices = 0;

  orkvector<xgmskelnode_ptr_t> _children;

  std::string _name;
  fmtx4 _bindMatrixInverse;
  fmtx4 _bindMatrix;
  fmtx4 _jointMatrix;
  fmtx4 _nodeMatrix;
  fmtx4 _assimpOffsetMatrix;
  ork::varmap::VarMap _varmap;
};

/// ///////////////////////////////////////////////////////////////////////////
/// Runtime skeleton (flattened hierarchy : linearized tree)
/// ///////////////////////////////////////////////////////////////////////////

struct XgmBone {
  int _parentIndex = -1;
  int _childIndex  = -1;
};

/// ///////////////////////////////////////////////////////////////////////////
/// Blend Pose Info (one per joint)
///  record all weighted matrices for a given joint
///  combines weighted matrices for a given joint to a single matrix
///  will eventually use pre-decomposed transforms
/// ///////////////////////////////////////////////////////////////////////////

struct PoseCallback {
  virtual void PostBlendPreConcat(fmtx4& decomposed_local) = 0;
  virtual void PostBlendPostConcat(fmtx4& composed_object)       = 0;
};

struct XgmBlendPoseInfo {

  static const int kmaxblendanims = 2;

  XgmBlendPoseInfo();

  void initBlendPose();
  void addPose(const DecompMatrix& mat, float weight);
  void computeMatrix(fmtx4& mtx) const;

  int _numanims = 0;
  PoseCallback* _posecallback = nullptr;
  DecompMatrix _matrices[kmaxblendanims];
  int _operations[kmaxblendanims];
  float _weights[kmaxblendanims];
};

/// ///////////////////////////////////////////////////////////////////////////
/// Local Pose
///  a pose of a skeleton in local(object) space
///  may have multiple matrices per joint
///  computes bounding volumes in Concatenate()
/// ///////////////////////////////////////////////////////////////////////////

struct XgmLocalPose {

  XgmLocalPose(xgmskeleton_constptr_t Skeleton);
  void identityPose();
  void bindPose();  /// set pose to the skeletons bind pose
  void blendPoses(); /// Blend Poses
  void concatenate();
  int NumJoints() const;
  std::string dumpc(fvec3 color) const;
  std::string invdumpc(fvec3 color) const;
  std::string dump() const;

  fvec4& RefObjSpaceBoundingSphere() {
    return mObjSpaceBoundingSphere;
  }
  AABox& RefObjSpaceAABoundingBox() {
    return mObjSpaceAABoundingBox;
  }

  const fvec4& RefObjSpaceBoundingSphere() const {
    return mObjSpaceBoundingSphere;
  }
  const AABox& RefObjSpaceAABoundingBox() const {
    return mObjSpaceAABoundingBox;
  }

  ////////////////////////////////////////////////////////////////

  xgmskeleton_constptr_t _skeleton;
  orkvector<fmtx4> _local_matrices;
  orkvector<fmtx4> _concat_matrices;
  orkvector<fmtx4> _bindrela_matrices;
  orkvector<XgmBlendPoseInfo> _blendposeinfos;
  orkvector<int> _boneprops;
  fvec4 mObjSpaceBoundingSphere;
  AABox mObjSpaceAABoundingBox;

};

/// ///////////////////////////////////////////////////////////////////////////
/// World Pose
///  a world space joint buffer for rendering or other purposes
/// ///////////////////////////////////////////////////////////////////////////

struct XgmWorldPose {

  XgmWorldPose(xgmskeleton_constptr_t Skeleton);

  void apply(const fmtx4& worldmtx, const XgmLocalPose& LocalPose);
  std::string dumpc(fvec3 color) const;

  xgmskeleton_constptr_t _skeleton;
  orkvector<fmtx4> _world_bindrela_matrices;
  orkvector<fmtx4> _world_concat_matrices;
  orkvector<int> _boneprops;

};

using xgmworldpose_ptr_t = std::shared_ptr<XgmWorldPose>;

/// ////////////////////////////////////////////////////////////////////////////
/// material state instance (analogous to XgmLocalPose for materials)
/// ////////////////////////////////////////////////////////////////////////////

class XgmMaterialStateInst {
  const XgmModelInst& mModelInst;
  const XgmModel* mModel;
  orklut<const XgmAnimInst*, MaterialInstItem*> mVarMap;

public:
  int GetNumItems() const {
    return int(mVarMap.size());
  }
  MaterialInstItem* GetItem(int idx) const {
    return mVarMap.GetItemAtIndex(idx).second;
  }

  void BindAnimInst(const XgmAnimInst& AnimInst);
  void UnBindAnimInst(const XgmAnimInst& AnimInst);

  void applyAnimInst(const XgmAnimInst& AnimInst); /// Apply an Animation Instance (weighted) to this pose

  XgmMaterialStateInst(const XgmModelInst& minst);
};

/// ///////////////////////////////////////////////////////////////////////////
/// Skeleton
///  transformation hierarchy for skinned characters
///	 _bones:	flattened hierarchy (runtime)
///	 mpRootNode:		tree hierarchy (export) (move to collada land)
/// ///////////////////////////////////////////////////////////////////////////

struct XgmSkeleton {

  /////////////////////////////////////

  XgmSkeleton();
  virtual ~XgmSkeleton();

  /////////////////////////////////////

  int numJoints(void) const {
    return miNumJoints;
  }
  int numBones(void) const {
    return int(_bones.size());
  }

  float boneLength(int ibone) const;

  const std::string& GetJointName(int idx) const {
    return mvJointNameVect[idx];
  }
  int GetJointParent(int idx) const {
    return maJointParents[idx];
  }
  void* GetUserData(void) {
    return mpUserData;
  }
  const XgmBone& bone(int idx) const {
    return _bones[idx];
  }
  int jointIndex(const std::string& Named) const;

  /////////////////////////////////////

  void resize(int inumjoints); // set number of joints
  void AddJoint(int iskelindex, int iparindex, const std::string& name);
  void addBone(const XgmBone& bone);

  /////////////////////////////////////

  fmtx4& RefNodeMatrix(int idx) {
    return _nodeMatrices[idx];
  }
  const fmtx4& RefNodeMatrix(int idx) const {
    return _nodeMatrices[idx];
  }
  fmtx4& RefJointMatrix(int idx) {
    return _jointMatrices[idx];
  }
  const fmtx4& RefJointMatrix(int idx) const {
    return _jointMatrices[idx];
  }

  fmtx4 concatenated(const std::string& named) const;

  /////////////////////////////////////

  std::string dump(fvec3 color) const;
  std::string dumpInvBind(fvec3 color) const;
  std::string dumpBind(fvec3 color) const;

  /////////////////////////////////////

  fmtx4 bindMatrixByName(const std::string& named) const;
  fmtx4 invBindMatrixByName(const std::string& named) const;

  /////////////////////////////////////

  XgmSkelNode* mpRootNode = nullptr;
  void* mpUserData        = nullptr;

  int miNumJoints = 0;
  int miRootNode  = -1;

  std::string msSkelName;

  fvec4 mBoundMin;
  fvec4 mBoundMax;
  fmtx4 mBindShapeMatrix;
  fmtx4 mTopNodesMatrix;

  orkvector<fmtx4> _inverseBindMatrices;
  orkvector<fmtx4> _bindMatrices;
  orkvector<DecompMatrix> _bindDecomps;
  orkvector<fmtx4> _jointMatrices;
  orkvector<fmtx4> _nodeMatrices;
  orkvector<std::string> mvJointNameVect;
  orkvector<XgmBone> _bones;
  orkvector<int> maJointParents;
  orklut<std::string, int> mmJointNameMap;
};

/////////////////////////////////////////////////////////////////////////////
// BoneTransformer 
//  transforms selected bones of a skeleton
//   by a transform
/////////////////////////////////////////////////////////////////////////////

struct BoneTransformer{

  BoneTransformer( xgmskeleton_constptr_t skel);
  void bindToBone(std::string named);
  void compute(XgmLocalPose& localpose, //
               const fmtx4& xf);

  /////////////////////////////////////////////////////////////////////////////

  std::vector<int> _jointindices;
  xgmskeleton_constptr_t _skeleton;

};

using bone_transformer_ptr_t = std::shared_ptr<BoneTransformer>;

} // namespace ork::lev2
