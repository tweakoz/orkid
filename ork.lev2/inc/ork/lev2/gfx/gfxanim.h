////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/math/cmatrix4.h>
#include <ork/math/box.h>
#include <ork/file/path.h>
#include <ork/kernel/varmap.inl>
#include <ork/rtti/RTTIX.inl>

namespace ork { namespace lev2 {

class XgmAnim;
class XgmSkeleton;
class XgmAnimInst;
class XgmLocalPose;
class XgmWorldPose;
class XgmAnimChannel;
class XgmModelInst;
class XgmModel;
class GfxMaterial;
class MaterialInstItem;
typedef orkmap<int, int> TXGMBoneRegMap;
typedef ork::lev2::XgmAnimChannel AnimChannelType;
typedef XgmAnim AnimType;
typedef orkmap<PoolString, ork::lev2::AnimChannelType*> AnimChannelsMap;

using animchannel_ptr_t = std::shared_ptr<XgmAnimChannel>;

/// ///////////////////////////////////////////////////////////////////////////
/// Animation Channel
/// there can be multiple animation channels per anim
/// each channel has a name (target)
/// each channel has a type (float,vector,matrix,etc..)
/// each channel also has a usage semantic (such as "Joint" or "Effect")
/// ///////////////////////////////////////////////////////////////////////////

class XgmAnimChannel : public ork::Object {
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

  XgmAnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& UsageSemantic, EChannelType etype);
  XgmAnimChannel(EChannelType etype);

  const PoolString& GetChannelName() const {
    return mChannelName;
  }
  const PoolString& GetObjectName() const {
    return mObjectName;
  }
  EChannelType GetChannelType() const {
    return meChannelType;
  }
  const PoolString& GetUsageSemantic() const {
    return mUsageSemantic;
  }
  virtual int GetNumFrames() const = 0;

  void SetChannelName(const PoolString& name) {
    mChannelName = name;
  }
  void SetObjectName(const PoolString& name) {
    mObjectName = name;
  }
  void SetChannelUsage(const PoolString& usage) {
    mUsageSemantic = usage;
  }

protected:
  PoolString mChannelName;
  PoolString mObjectName;
  PoolString mUsageSemantic;
  int miNumFrames;
  EChannelType meChannelType;
};

///////////////////////////////////////////////////////////////////////////////

class XgmFloatAnimChannel : public XgmAnimChannel {
  DeclareConcreteX(XgmFloatAnimChannel, XgmAnimChannel);

  orkvector<float> mSampledFrames;

public:
  XgmFloatAnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage);
  XgmFloatAnimChannel();

  void AddFrame(float v) {
    mSampledFrames.push_back(v);
  }
  float GetFrame(int index) const {
    return mSampledFrames[index];
  }
  void ReserveFrames(int iv) {
    mSampledFrames.reserve(iv);
  }

private:
  int GetNumFrames() const final {
    return int(mSampledFrames.size());
  }
};

///////////////////////////////////////////////////////////////////////////////

class XgmVect3AnimChannel : public XgmAnimChannel {
  DeclareConcreteX(XgmVect3AnimChannel, XgmAnimChannel);

  orkvector<fvec3> mSampledFrames;

public:
  XgmVect3AnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage);
  XgmVect3AnimChannel();

  void AddFrame(const fvec3& v) {
    mSampledFrames.push_back(v);
  }
  const fvec3& GetFrame(int index) const {
    return mSampledFrames[index];
  }
  void ReserveFrames(int iv) {
    mSampledFrames.reserve(iv);
  }

private:
  int GetNumFrames() const final {
    return int(mSampledFrames.size());
  }
};

///////////////////////////////////////////////////////////////////////////////

class XgmVect4AnimChannel : public XgmAnimChannel {
  DeclareConcreteX(XgmVect4AnimChannel, XgmAnimChannel);

  orkvector<fvec4> mSampledFrames;

public:
  XgmVect4AnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage);
  XgmVect4AnimChannel();

  void AddFrame(const fvec4& v) {
    mSampledFrames.push_back(v);
  }
  const fvec4& GetFrame(int index) const {
    return mSampledFrames[index];
  }
  void ReserveFrames(int iv) {
    mSampledFrames.reserve(iv);
  }

private:
  int GetNumFrames() const final {
    return int(mSampledFrames.size());
  }
};

///////////////////////////////////////////////////////////////////////////////
enum EXFORM_COMPONENT {
  // NONE always results in no change for Enable/Disable and returns false for Check
  XFORM_COMPONENT_NONE = 0,

  XFORM_COMPONENT_TRANS  = (1 << 0),
  XFORM_COMPONENT_ORIENT = (1 << 1),
  XFORM_COMPONENT_SCALE  = (1 << 2),

  // TRANSORIENT always results in the SCALE being unchanged for Enable/Disable and SCALE is ignored for Check
  XFORM_COMPONENT_TRANSORIENT = XFORM_COMPONENT_TRANS | XFORM_COMPONENT_ORIENT,

  // ALL always results in the whole bone being changed for Enable/Disable and returns true for Check, if *any* component is enabled
  XFORM_COMPONENT_ALL = XFORM_COMPONENT_TRANS | XFORM_COMPONENT_ORIENT | XFORM_COMPONENT_SCALE
};

struct DecompMtx44 {
  fquat mRot;
  fvec3 mTrans;
  float mScale;

  void Compose(fmtx4& mtx, EXFORM_COMPONENT components) const;
};

///////////////////////////////////////////////////////////////////////////////

class XgmDecompAnimChannel : public XgmAnimChannel {

  DeclareConcreteX(XgmDecompAnimChannel, XgmAnimChannel);

  int GetNumFrames() const final;

protected:
  DecompMtx44* mSampledFrames;
  int miNumFrames;
  int miAddIndex;

public:
  XgmDecompAnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage);
  XgmDecompAnimChannel();

  void AddFrame(const DecompMtx44& v);
  const DecompMtx44& GetFrame(int index) const;
  void ReserveFrames(int iv);
};

///////////////////////////////////////////////////////////////////////////////

class XgmMatrixAnimChannel : public XgmAnimChannel {

  DeclareConcreteX(XgmMatrixAnimChannel, XgmAnimChannel);

  int GetNumFrames() const final;

protected:
  fmtx4* mSampledFrames;
  int miNumFrames;
  int miAddIndex;

public:
  XgmMatrixAnimChannel(const PoolString& ObjName, const PoolString& ChanName, const PoolString& Usage);
  XgmMatrixAnimChannel();

  void AddFrame(const fmtx4& v);
  const fmtx4& GetFrame(int index) const;
  void ReserveFrames(int iv);
};

/// ///////////////////////////////////////////////////////////////////////////
/// Animation
/// mAnimationChannels:	collection of animation channels
/// mPose:				static state of joints (when there is no animated data)
/// ///////////////////////////////////////////////////////////////////////////

class XgmAnim {
public:
  typedef orklut<PoolString, ork::lev2::XgmDecompAnimChannel*> JointChannelsMap;
  typedef orklut<PoolString, animchannel_ptr_t> MaterialChannelsMap;

  void AddChannel(const PoolString& Name, animchannel_ptr_t pchan);
  void SetNumFrames(int ifr) {
    miNumFrames = ifr;
  }

  //////////////////////////

  static bool LoadUnManaged(XgmAnim* mdl, const AssetPath& fname);
  static bool Save(const file::Path& AnimFile, const XgmAnim* panm);

  static bool unloadUnManaged(XgmAnim* mdl);

  //////////////////////////

  XgmAnim();

  int GetNumFrames(void) const {
    return miNumFrames;
  }
  size_t GetNumJointChannels(void) const {
    return mJointAnimationChannels.size();
  }
  size_t GetNumMaterialChannels(void) const {
    return mMaterialAnimationChannels.size();
  }
  const JointChannelsMap& RefJointChannels(void) const {
    return mJointAnimationChannels;
  }
  const MaterialChannelsMap& RefMaterialChannels(void) const {
    return mMaterialAnimationChannels;
  }

  orklut<PoolString, DecompMtx44>& GetStaticPose() {
    return mPose;
  }
  const orklut<PoolString, DecompMtx44>& GetStaticPose() const {
    return mPose;
  }

private:
  int miNumFrames;
  JointChannelsMap mJointAnimationChannels;
  MaterialChannelsMap mMaterialAnimationChannels;
  orklut<PoolString, DecompMtx44> mPose;
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
  void Enable(const XgmSkeleton& Skeleton, const PoolString& BoneName, EXFORM_COMPONENT component);
  void Enable(int iboneindex, EXFORM_COMPONENT component);

  void DisableAll();
  void Disable(const XgmSkeleton& Skeleton, const PoolString& BoneName, EXFORM_COMPONENT component);
  void Disable(int iboneindex, EXFORM_COMPONENT component);

  bool Check(const XgmSkeleton& Skeleton, const PoolString& BoneName, EXFORM_COMPONENT component) const;
  bool Check(int iboneindex, EXFORM_COMPONENT component) const;

  EXFORM_COMPONENT GetComponents(const XgmSkeleton& Skeleton, const PoolString& BoneName) const;
  EXFORM_COMPONENT GetComponents(int iboneindex) const;
};

/// ///////////////////////////////////////////////////////////////////////////
/// Animation Instance
/// A running instance of an animation
/// mMask:		Animation Mask for this instance
/// mWeight:	Matrix Weighting for this instance
/// ///////////////////////////////////////////////////////////////////////////

class XgmAnimInst {
public:
  static const int kmaxbones = 64;

  struct Binding {
    unsigned int mSkelIndex : 16;
    unsigned int mChanIndex : 16;
    Binding(int is = -1, int ic = -1)
        : mSkelIndex(is)
        , mChanIndex(ic) {
    }
  };

private:
  const XgmAnim* mAnim;
  XgmAnimMask mMask;
  float mFrame;
  float mWeight;

  Binding mPoseBindings[kmaxbones];
  Binding mAnimBindings[kmaxbones];
  static const Binding gBadBinding;

public:
  XgmAnimInst();

  void BindAnim(const XgmAnim* anim);
  const XgmAnim* GetAnim() const {
    return mAnim;
  }

  float GetSampleRate() const {
    return 30.0f;
  }
  float GetCurrentFrame() const {
    return mFrame;
  }
  float GetNumFrames() const {
    return (mAnim != 0) ? mAnim->GetNumFrames() : 0.0f;
  }
  float GetWeight() const {
    return mWeight;
  }

  void SetCurrentFrame(float fr) {
    mFrame = fr;
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

  const Binding& GetPoseBinding(int i) const {
    return (i < kmaxbones) ? mPoseBindings[i] : gBadBinding;
  }
  const Binding& GetAnimBinding(int i) const {
    return (i < kmaxbones) ? mAnimBindings[i] : gBadBinding;
  }
  void SetPoseBinding(int i, const Binding& inp) {
    if (i < kmaxbones)
      mPoseBindings[i] = inp;
  }
  void SetAnimBinding(int i, const Binding& inp) {
    if (i < kmaxbones)
      mAnimBindings[i] = inp;
  }
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

  typedef std::function<void(XgmSkelNode* node)> nodevisitfn_t;

  ///////////////////////////

  fmtx4 bindMatrix() const;
  fmtx4 concatenated() const;
  fmtx4 concatenated2() const;
  fmtx4 concatenatednode() const;
  fmtx4 concatenatednode2() const;
  NodeType nodetype() const;
  void visitHierarchy(nodevisitfn_t visitfn);
  void visitHierarchyUp(nodevisitfn_t visitfn);
  XgmSkelNode* findCentimeterToMeterNode();
  bool applyCentimeterToMeterScale();
  bool isParentOf(XgmSkelNode* testnode);     // is this node a parent of testnode
  bool isDescendantOf(XgmSkelNode* testnode); // is this node a descendant of testnode

  ///////////////////////////

  std::string _name;
  fmtx4 _bindMatrixInverse;
  fmtx4 _bindMatrix;
  fmtx4 _jointMatrix;
  fmtx4 _nodeMatrix;
  int _numBoundVertices;
  fmtx4 _assimpOffsetMatrix;

  XgmSkelNode* _parent = nullptr;
  orkvector<XgmSkelNode*> mChildren;
  int miSkelIndex = -1;
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
  virtual void PostBlendPreConcat(DecompMtx44& decomposed_local) = 0;
  virtual void PostBlendPostConcat(fmtx4& composed_object)       = 0;
};

struct XgmBlendPoseInfo {
public:
  static const int kmaxblendanims = 2;

  XgmBlendPoseInfo();

  void InitBlendPose();
  void AddPose(const DecompMtx44& mat, float weight, EXFORM_COMPONENT components);

  void ComputeMatrix(fmtx4& mtx) const;

  int GetNumAnims() const {
    return miNumAnims;
  }

  void SetPoseCallback(PoseCallback* callback) {
    mPoseCallback = callback;
  }
  PoseCallback* GetPoseCallback() const {
    return mPoseCallback;
  }

private:
  int miNumAnims;

  DecompMtx44 AnimMat[kmaxblendanims];
  float AnimWeight[kmaxblendanims];
  EXFORM_COMPONENT Ani_components[kmaxblendanims];

  PoseCallback* mPoseCallback;
};

/// ///////////////////////////////////////////////////////////////////////////
/// Local Pose
///  a pose of a skeleton in local(object) space
///  may have multiple matrices per joint
///  computes bounding volumes in Concatenate()
/// ///////////////////////////////////////////////////////////////////////////

class XgmLocalPose {
  const XgmSkeleton& mSkeleton;
  orkvector<fmtx4> mLocalMatrices;
  orkvector<XgmBlendPoseInfo> mBlendPoseInfos;
  fvec4 mObjSpaceBoundingSphere;
  AABox mObjSpaceAABoundingBox;

public:
  void BindAnimInst(XgmAnimInst& AnimInst);
  void UnBindAnimInst(XgmAnimInst& AnimInst);

  XgmLocalPose(const XgmSkeleton& Skeleton);
  void BindPose(void);  /// set pose to the skeletons bind pose
  void BuildPose(void); /// Blend Poses
  void Concatenate(void);
  int NumJoints() const;
  std::string dumpc(fvec3 color) const;
  std::string invdumpc(fvec3 color) const;
  std::string dump() const;

  fmtx4& RefLocalMatrix(int idx) {
    return mLocalMatrices[idx];
  }
  XgmBlendPoseInfo& RefBlendPoseInfo(int idx) {
    return mBlendPoseInfos[idx];
  }
  fvec4& RefObjSpaceBoundingSphere() {
    return mObjSpaceBoundingSphere;
  }
  AABox& RefObjSpaceAABoundingBox() {
    return mObjSpaceAABoundingBox;
  }

  const fmtx4& RefLocalMatrix(int idx) const {
    return mLocalMatrices[idx];
  }
  const XgmBlendPoseInfo& RefBlendPoseInfo(int idx) const {
    return mBlendPoseInfos[idx];
  }
  const fvec4& RefObjSpaceBoundingSphere() const {
    return mObjSpaceBoundingSphere;
  }
  const AABox& RefObjSpaceAABoundingBox() const {
    return mObjSpaceAABoundingBox;
  }

  ////////////////////////////////////////////////////////////////
  // Application Methods (from anim, ik, physics, etc....)

  void ApplyAnimInst(const XgmAnimInst& AnimInst); /// Apply an Animation Instance (weighted) to this pose

  ////////////////////////////////////////////////////////////////
};

/// ///////////////////////////////////////////////////////////////////////////
/// World Pose
///  a world space joint buffer for rendering or other purposes
/// ///////////////////////////////////////////////////////////////////////////

class XgmWorldPose {
  const XgmSkeleton& mSkeleton;
  orkvector<fmtx4> mWorldMatrices;

public:
  XgmWorldPose(const XgmSkeleton& Skeleton);
  orkvector<fmtx4>& GetMatrices() {
    return mWorldMatrices;
  }
  const orkvector<fmtx4>& GetMatrices() const {
    return mWorldMatrices;
  }
  void apply(const fmtx4& worldmtx, const XgmLocalPose& LocalPose);
  std::string dumpc(fvec3 color) const;
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

  void ApplyAnimInst(const XgmAnimInst& AnimInst); /// Apply an Animation Instance (weighted) to this pose

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

  const PoolString& GetJointName(int idx) const {
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
  int jointIndex(const PoolString& Named) const;

  /////////////////////////////////////

  void resize(int inumjoints); // set number of joints
  void AddJoint(int iskelindex, int iparindex, const PoolString& name);
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
  fmtx4& RefInverseBindMatrix(int idx) {
    return _inverseBindMatrices[idx];
  }
  const fmtx4& RefInverseBindMatrix(int idx) const {
    return _inverseBindMatrices[idx];
  }

  fmtx4 concatenated(PoolString named) const;

  /////////////////////////////////////

  std::string dump(fvec3 color) const;
  std::string dumpInvBind(fvec3 color) const;
  std::string dumpBind(fvec3 color) const;

  /////////////////////////////////////

  orkvector<fmtx4> _inverseBindMatrices;
  orkvector<fmtx4> _jointMatrices;
  orkvector<fmtx4> _nodeMatrices;
  int miNumJoints;
  orkvector<PoolString> mvJointNameVect;
  orkvector<XgmBone> _bones;
  orkvector<int> maJointParents;
  PoolString msSkelName;

  int miRootNode;
  XgmSkelNode* mpRootNode;

  orklut<PoolString, int> mmJointNameMap;

  fvec4 mBoundMin;
  fvec4 mBoundMax;
  void* mpUserData;

  fmtx4 mBindShapeMatrix;
  fmtx4 mTopNodesMatrix;
};
}} // namespace ork::lev2
