////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/lev2/gfx/gfxanim.h>
#include <ork/lev2/gfx/gfxmodel.h>

#include <ork/application/application.h>

#include <pkg/ent/entity.h>
#include <pkg/ent/ModelComponent.h>

#include <pkg/ent/AnimSeqTable.h>
#include <ork/lev2/lev2_asset.h>

namespace ork { namespace ent {

// Class: SimpleAnimatableData
// SimpleAnimatable is meant to be shared by many different archetypes. Therefore, game- and character-
// specific code should not appear in here.
//
// See <ShipArchetype> and <ShipData> for examples of where game- and character-specific animation
// code belongs.
//
// Animation Features:
//   - masking and blending
//   - looping, reversing, and variable speed playback
//   - Animation sequence events
class SimpleAnimatableData : public ork::ent::ComponentData {
  RttiDeclareConcrete(SimpleAnimatableData, ork::ent::ComponentData)

      public : typedef ork::orklut<ork::PoolString, ork::lev2::XgmAnimAsset*> AnimationMap;
  typedef ork::orklut<ork::PoolString, AnimSeqTable*> AnimSeqTableMap;
  typedef ork::orklut<ork::PoolString, ork::PoolString> AnimMaskMap;

  ork::lev2::XgmAnim* GetAnim(ork::lev2::XgmAnimAsset* passet) {
    return (passet == 0) ? 0 : passet->GetAnim();
  }

  SimpleAnimatableData();

  const AnimationMap& GetAnimationMap() const {
    return mAnimationMap;
  }
  AnimationMap& GetAnimationMap() {
    return mAnimationMap;
  }

  const AnimSeqTableMap& GetAnimSeqTableMap() const {
    return mAnimSeqTableMap;
  }
  AnimSeqTableMap& GetAnimSeqTableMap() {
    return mAnimSeqTableMap;
  }

  const AnimMaskMap& GetAnimMaskMap() const {
    return mAnimMaskMap;
  }
  AnimMaskMap& GetAnimMaskMap() {
    return mAnimMaskMap;
  }

  bool HasAnimation(ork::PoolString name) const;

private:
  ~SimpleAnimatableData() final;
  ork::ent::ComponentInst* createComponent(ork::ent::Entity* pent) const final;

  AnimationMap mAnimationMap;
  AnimSeqTableMap mAnimSeqTableMap;
  AnimMaskMap mAnimMaskMap;
};

class SimpleAnimatableInst : public ork::ent::ComponentInst {
  RttiDeclareAbstract(SimpleAnimatableInst, ork::ent::ComponentInst) public :

      SimpleAnimatableInst(const SimpleAnimatableData& data, ork::ent::Entity* pent);

  ork::lev2::XgmAnim* GetAnim(ork::lev2::XgmAnimAsset* passet) {
    return (passet == 0) ? 0 : passet->GetAnim();
  }

  void FlushQueue();
  void QueueAnimation(ork::PoolString name, float speed = 1.0f, float interp_duration = 0.0f, bool loop = true);
  float GetFrameNumOnAnimationOnFirstMask();
  void PlayAnimation(ork::PoolString name, float speed = 1.0f, float interp_duration = 0.0f, bool loop = true);
  void PlayAnimationEx(
      ork::PoolString maskname,
      ork::PoolString name,
      int priority          = 0,
      float speed           = 1.0f,
      float interp_duration = 0.0f,
      bool loop             = true);
  void ChangeAnimationSpeed(float speed);
  void ChangeAnimationSpeedEx(ork::PoolString name, float speed);
  void ChangeAnimationFrameToFirst();
  void ChangeAnimationFrameToFirstEx(ork::PoolString name);
  void ChangeAnimationFrameToLast();
  void ChangeAnimationFrameToLastEx(ork::PoolString name);
  void ChangeAnimationFrameToMiddle(float scale);
  void ChangeAnimationFrameToMiddleEx(ork::PoolString name, float scale);

  void ChangeAnimationPriority(ork::PoolString name, int priority);
  void ChangeMaskPriority(ork::PoolString name, int priority);
  void ChangePriority(int priority);

  bool IsAnimPlaying(ork::PoolString name) const;

  void SetModelInst(ork::lev2::xgmmodelinst_ptr_t inst) {
    _modelinst = inst;
  }
  const SimpleAnimatableData& GetData() {
    return mData;
  }

protected:
  const SimpleAnimatableData& mData;

private:
  ~SimpleAnimatableInst();

  ork::lev2::xgmmodelinst_ptr_t GetModelInst() const {
    return _modelinst;
  }

  struct AnimationQueueItem {
    AnimationQueueItem(
        ork::PoolString name  = ork::AddPooledLiteral(""),
        float speed           = 1.0f,
        float interp_duration = 0.0f,
        bool loop             = true)
        : mName(name)
        , mSpeed(speed)
        , mInterpDuration(interp_duration)
        , mLoop(loop) {
    }
    ork::PoolString mName;
    float mSpeed;
    float mInterpDuration;
    bool mLoop;
  };
  orklist<AnimationQueueItem> mAnimationQueue;

  class AnimData {
    ork::lev2::XgmAnimInst mAnimInst;
    const SimpleAnimatableInst& mSai;

  public:
    AnimData(const SimpleAnimatableInst& sai)
        : mSai(sai)
        , mSpeed(1.0f)
        , mAnimSeqTable(NULL) {
    }
    AnimData& operator=(const AnimData& data) {
      mSpeed        = data.mSpeed;
      mLoop         = data.mLoop;
      mName         = data.mName;
      mAnimSeqTable = data.mAnimSeqTable;
      mAnimInst     = data.mAnimInst;
      return *this;
    }

    float mSpeed;
    bool mLoop;
    ork::PoolString mName;
    AnimSeqTable* mAnimSeqTable;

    const ork::lev2::XgmAnimInst& AnimInst() const {
      return mAnimInst;
    }

    void BindAnim(const ork::lev2::XgmAnim* anim);
    void SetFrame(float f) {
      mAnimInst.SetCurrentFrame(f);
    }
    float GetFrame() {
      return mAnimInst.GetCurrentFrame();
    }
    void SetWeight(float w) {
      mAnimInst.SetWeight(w);
    }
    float GetWeight() {
      return mAnimInst.GetWeight();
    }
    void SetAnimSeqTable(AnimSeqTable* table) {
      mAnimSeqTable = table;
    }
    ork::lev2::XgmAnimMask& RefMask() {
      return mAnimInst.RefMask();
    }
  };

  struct AnimBodyPart {
    AnimBodyPart(const SimpleAnimatableInst& sai)
        : mCurrentAnimData(sai)
        , mPreviousAnimData(sai)
        , mInterpSpeed(0.0f)
        , mPriority(0) {
    }
    AnimData mCurrentAnimData;
    AnimData mPreviousAnimData;
    float mInterpSpeed;
    int mPriority;
    orkset<int> mCachedJoints;
  };

  typedef ork::orklut<ork::PoolString, AnimBodyPart*> BodyPartMap;

  void MarkAnimOnMask(BodyPartMap::iterator& itmask, ork::PoolString name);
  void UnmarkAnimOnMask(BodyPartMap::iterator& itmask, ork::PoolString name);

  void PlayAnimationOnMask(
      BodyPartMap::iterator& itmask,
      SimpleAnimatableData::AnimationMap::const_iterator& itanim,
      int priority,
      float speed,
      float interp_duration,
      bool loop);

  /// Helper routine for incrementing the anim frame, applying optional loop, and notifying the entity of AnimSeq and AnimFinish
  /// events
  static bool AnimDataUpdate(AnimData& data, float delta, ork::lev2::xgmmodelinst_ptr_t modelInst, ork::ent::Entity* entity = NULL);

  void DoUpdate(ork::ent::Simulation* inst) final;
  bool DoStart(ork::ent::Simulation* psi, const ork::fmtx4& world) final;
  bool DoLink(ork::ent::Simulation* psi) final;
  bool DoNotify(const ork::event::Event* event) final;

  BodyPartMap mBodyPartMap;
  ork::lev2::xgmmodelinst_ptr_t _modelinst;

  typedef ork::orklut<ork::PoolString, orklist<BodyPartMap::iterator>> AnimToMaskMap;
  AnimToMaskMap mAnimToMaskMap;
};

}} // namespace ork::ent
