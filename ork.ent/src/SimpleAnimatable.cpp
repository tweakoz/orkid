////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>

#include <ork/event/EventListener.h>

#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>

#include <pkg/ent/SimpleAnimatable.h>

#include <pkg/ent/event/PlayAnimationEvent.h>
#include <pkg/ent/event/AnimFinishEvent.h>
#include <pkg/ent/event/ChangeAnimationSpeedEvent.h>
#include <pkg/ent/event/AnimationPriorityEvent.h>
#include <pkg/ent/event/MaskPriorityEvent.h>
#include <pkg/ent/event/PriorityEvent.h>

#include <ork/lev2/gfx/gfxmodel.h>

#define ANIMATE_VERBOSE					(0)
#define PRINT_CONDITION_NAME(__name)	(ork::PieceString(__name).find("ship1") != ork::PieceString::npos)
#define PRINT_CONDITION					(PRINT_CONDITION_NAME(GetEntity()->GetEntData().GetName()))
#define PRINT_CONDITION_AD				(entity && PRINT_CONDITION_NAME(entity->GetEntData().GetName()))
#if ANIMATE_VERBOSE
#	define DEBUG_ANIMATE_PRINT	if(PRINT_CONDITION) orkprintf
#	define DEBUG_ANIMATE_PRINT_AD	if(PRINT_CONDITION_AD) orkprintf
//#   define DEBUG_ANIMATE_PRINT_AD(...)
#else
#	define DEBUG_ANIMATE_PRINT(...)
#	define DEBUG_ANIMATE_PRINT_AD(...)
#endif

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SimpleAnimatableData, "SimpleAnimatableData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SimpleAnimatableInst, "SimpleAnimatableInst");

template class ork::reflect::DirectObjectMapPropertyType<orkmap<ork::PoolString, ork::ent::AnimSeqTable *> >;

namespace ork { namespace ent {

static ork::PoolString sAsteriskString;

void SimpleAnimatableData::Describe()
{
	ork::ent::RegisterFamily<SimpleAnimatableData>(ork::AddPooledLiteral("animate"));

	ork::reflect::RegisterMapProperty("AnimationMap", &SimpleAnimatableData::mAnimationMap);
	ork::reflect::AnnotatePropertyForEditor<SimpleAnimatableData>("AnimationMap", "editor.assettype", "xganim");
	ork::reflect::AnnotatePropertyForEditor<SimpleAnimatableData>("AnimationMap", "editor.assetclass", "xganim");

	ork::reflect::RegisterMapProperty("AnimSeqTableMap", &SimpleAnimatableData::mAnimSeqTableMap);
	ork::reflect::AnnotatePropertyForEditor<SimpleAnimatableData>("AnimSeqTableMap", "editor.factorylistbase", "AnimSeqTable");

	ork::reflect::RegisterMapProperty("AnimMaskMap", &SimpleAnimatableData::mAnimMaskMap);

	sAsteriskString = ork::Application::AddPooledLiteral("*");
}

SimpleAnimatableData::SimpleAnimatableData()  //mAnimationMap(ork::EKEYPOLICY_MULTILUT), mAnimSeqTableMap(ork::EKEYPOLICY_MULTILUT)
{
}
SimpleAnimatableData::~SimpleAnimatableData()
{
	for( ork::orklut<ork::PoolString, AnimSeqTable*>::const_iterator
			it=mAnimSeqTableMap.begin();
			it!=mAnimSeqTableMap.end();
			it++ )
	{
		AnimSeqTable* ptab = it->second;
		delete ptab;
	}
}

bool SimpleAnimatableData::HasAnimation(ork::PoolString name) const
{
	return mAnimationMap.find(name) != mAnimationMap.end();
}

ork::ent::ComponentInst *SimpleAnimatableData::CreateComponent(ork::ent::Entity *pent) const
{
	return OrkNew SimpleAnimatableInst(*this, pent);
}

static ork::PoolString sEmptyString;

void SimpleAnimatableInst::Describe()
{
	ork::reflect::RegisterFunctor("FlushQueue", &SimpleAnimatableInst::FlushQueue);
	ork::reflect::RegisterFunctor("QueueAnimation", &SimpleAnimatableInst::QueueAnimation);
	ork::reflect::RegisterFunctor("PlayAnimation", &SimpleAnimatableInst::PlayAnimation);
	ork::reflect::RegisterFunctor("PlayAnimationEx", &SimpleAnimatableInst::PlayAnimationEx);
	ork::reflect::RegisterFunctor("ChangeAnimationSpeed", &SimpleAnimatableInst::ChangeAnimationSpeed);
	ork::reflect::RegisterFunctor("ChangeAnimationSpeedEx", &SimpleAnimatableInst::ChangeAnimationSpeedEx);
	ork::reflect::RegisterFunctor("ChangeAnimationFrameToFirst", &SimpleAnimatableInst::ChangeAnimationFrameToFirst);
	ork::reflect::RegisterFunctor("ChangeAnimationFrameToFirstEx", &SimpleAnimatableInst::ChangeAnimationFrameToFirstEx);
	ork::reflect::RegisterFunctor("ChangeAnimationFrameToLast", &SimpleAnimatableInst::ChangeAnimationFrameToLast);
	ork::reflect::RegisterFunctor("ChangeAnimationFrameToLastEx", &SimpleAnimatableInst::ChangeAnimationFrameToLastEx);
	ork::reflect::RegisterFunctor("ChangeAnimationFrameToMiddle", &SimpleAnimatableInst::ChangeAnimationFrameToMiddle);
	ork::reflect::RegisterFunctor("ChangeAnimationFrameToMiddleEx", &SimpleAnimatableInst::ChangeAnimationFrameToMiddleEx);
	ork::reflect::RegisterFunctor("ChangeAnimationPriority", &SimpleAnimatableInst::ChangeAnimationPriority);
	ork::reflect::RegisterFunctor("ChangeMaskPriority", &SimpleAnimatableInst::ChangeMaskPriority);
	ork::reflect::RegisterFunctor("ChangePriority", &SimpleAnimatableInst::ChangePriority);

	sEmptyString = ork::AddPooledLiteral("");
}

static void SetMaskFromJoints(ork::lev2::XgmAnimMask &mask, const orkset<int> &joints)
{
	mask.DisableAll();
	for(orkset<int>::const_iterator it = joints.begin(); it != joints.end(); it++)
		mask.Enable(*it, ork::lev2::XFORM_COMPONENT_ALL);
}

static void SetJointsFromExpression(orkset<int> &joints, const ork::lev2::XgmSkeleton &skeleton, const SimpleAnimatableData *sad, ork::PieceString expression)
{
	ork::PieceString::size_type space = expression.find(" ");
	if(space != ork::PieceString::npos)
	{
		ork::PieceString element = expression.substr(0, space);
		SetJointsFromExpression(joints, skeleton, sad, element);

		ork::PieceString remaining = expression.substr(space + 1);
		SetJointsFromExpression(joints, skeleton, sad, remaining);
	}
	else
	{
		if(expression == "*")
			for(int i = 0; i < skeleton.GetNumJoints(); i++)
				joints.insert(i);
		else if(expression.find("~") == 0)
		{
			orkset<int> children;
			SetJointsFromExpression(children, skeleton, sad, expression.substr(1));
			for(int i = 0; i < skeleton.GetNumJoints(); i++)
				if(children.find(i) == children.end())
					joints.insert(i);
		}
		else if(expression.find("tree(") == 0)
		{
			ork::PieceString::size_type end = expression.find(")");
			ork::PieceString param = expression.substr(5, end - 5);

			orkvector<int> parents;
			parents.push_back(skeleton.GetJointIndex(ork::AddPooledString(param)));
			bool changed = true;
			while(changed)
			{
				changed = false;
				for(int i = 0; i < skeleton.GetNumJoints(); i++)
					if(std::find(parents.begin(), parents.end(), skeleton.GetJointParent(i)) != parents.end()
							&& std::find(parents.begin(), parents.end(), i) == parents.end())
					{
						parents.push_back(i);
						changed = true;
					}
			}
			for(orkvector<int>::size_type i = 0; i < parents.size(); i++)
				joints.insert(parents[i]);
		}
		else
		{
			ork::PoolString exprStr = ork::AddPooledString(expression);
			SimpleAnimatableData::AnimMaskMap::const_iterator it = sad->GetAnimMaskMap().find(exprStr);
			if(it != sad->GetAnimMaskMap().end())
				SetJointsFromExpression(joints, skeleton, sad, it->second);
			else
				joints.insert(skeleton.GetJointIndex(exprStr));
		}
	}
}

SimpleAnimatableInst::SimpleAnimatableInst(const SimpleAnimatableData &data, ork::ent::Entity *pent)
	: ork::ent::ComponentInst(&data, pent)
	, mData(data)
	, mModelInst(NULL)
{
	if(mData.GetAnimMaskMap().size() > 0)
		for(SimpleAnimatableData::AnimMaskMap::const_iterator it = mData.GetAnimMaskMap().begin();
				it != mData.GetAnimMaskMap().end(); it++)
			mBodyPartMap.AddSorted(it->first, new AnimBodyPart(*this));
	else
		mBodyPartMap.AddSorted(ork::AddPooledLiteral(""), new AnimBodyPart(*this));

	for(SimpleAnimatableData::AnimationMap::const_iterator it = mData.GetAnimationMap().begin();
			it != mData.GetAnimationMap().end(); it++)
		mAnimToMaskMap.AddSorted(it->first, orklist<BodyPartMap::iterator>());
}
SimpleAnimatableInst::~SimpleAnimatableInst()
{
	for( BodyPartMap::const_iterator it=mBodyPartMap.begin(); it!=mBodyPartMap.end(); it++ )
	{
		AnimBodyPart* part = it->second;
		delete part;
	}
}

bool SimpleAnimatableInst::DoStart(ork::ent::SceneInst *psi, const ork::CMatrix4 &world)
{
	return true;
}

bool SimpleAnimatableInst::DoLink(ork::ent::SceneInst *psi)
{
	ork::ent::ModelComponentInst *modelcinst = GetEntity()->GetTypedComponent<ork::ent::ModelComponentInst>();
	
	if( 0 == modelcinst) return false;

	ork::ent::ModelDrawable &mdraw = modelcinst->GetModelDrawable();
	mModelInst = mdraw.GetModelInst();
	if(mModelInst)
	{
		// Cache joints from masks
		if(mData.GetAnimMaskMap().size() > 0)
			for(SimpleAnimatableData::AnimMaskMap::const_iterator it = mData.GetAnimMaskMap().begin();
					it != mData.GetAnimMaskMap().end(); it++)
				SetJointsFromExpression(mBodyPartMap.find(it->first)->second->mCachedJoints
					, mModelInst->GetXgmModel()->RefSkel(), &mData, it->second);
		else
		{
			SimpleAnimatableInst::BodyPartMap::const_iterator it = mBodyPartMap.begin();
			SetJointsFromExpression(it->second->mCachedJoints, mModelInst->GetXgmModel()->RefSkel()
				, &mData, sAsteriskString);
		}
	}

	return true;
}

void SimpleAnimatableInst::PlayAnimation(ork::PoolString name, float speed, float interp_duration, bool loop)
{
	SimpleAnimatableData::AnimationMap::const_iterator itanim = mData.GetAnimationMap().find(name);
	if(itanim == mData.GetAnimationMap().end())
	{
		// nasa - not really a WARNING any more
		//orkprintf("WARNING: Anim is unknown: %s\n", name.c_str());

		return;
	}

	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		PlayAnimationOnMask(itmask, itanim, 0, speed, interp_duration, loop);
}

void SimpleAnimatableInst::FlushQueue()
{
	mAnimationQueue.clear();
	ChangePriority(0);
}

void SimpleAnimatableInst::QueueAnimation(ork::PoolString name, float speed, float interp_duration, bool loop)
{
//	if(prodigy::ent::GroundControllableInst *groundInst = GetEntity()->GetTypedComponent<prodigy::ent::GroundControllableInst>())
//		groundInst->Disable();

	ChangePriority(0);
	if(mAnimationQueue.size() == 0)
		PlayAnimationEx(sEmptyString, name, 0, speed, interp_duration, loop);
	mAnimationQueue.push_back(AnimationQueueItem(name, speed, interp_duration, loop));
}

void SimpleAnimatableInst::PlayAnimationEx(ork::PoolString maskname, ork::PoolString name, int priority, float speed, float interp_duration, bool loop)
{
	BodyPartMap::iterator itmask = mBodyPartMap.find(maskname);
	if(itmask == mBodyPartMap.end())
	{
		//orkprintf("WARNING: Mask %s is unknown on entity %s (%s) with model %s\n", maskname.c_str(), GetEntity()->GetEntData().GetName().c_str(), GetEntity()->GetEntData().GetArchetype()->GetName().c_str(), mModelInst->GetXgmModel()->GetAssetName().c_str());
		//return;

		maskname = sEmptyString;
		itmask = mBodyPartMap.find(maskname);
	}

	SimpleAnimatableData::AnimationMap::const_iterator itanim = mData.GetAnimationMap().find(name);
	if(itanim == mData.GetAnimationMap().end())
	{
		// nasa - not really a WARNING any more
		//orkprintf("WARNING: Anim %s is unknown on entity %s (%s) with model %s\n", name.c_str(), GetEntity()->GetEntData().GetName().c_str(), GetEntity()->GetEntData().GetArchetype()->GetName().c_str(), mModelInst->GetXgmModel()->GetAssetName().c_str());
		return;
	}

	if((itmask == mBodyPartMap.end()) && (maskname == sEmptyString)) //e do it full body
	{
		for(BodyPartMap::iterator itmask2 = mBodyPartMap.begin(); itmask2 != mBodyPartMap.end(); itmask2++)
			PlayAnimationOnMask(itmask2, itanim, priority, speed, interp_duration, loop);
	}
	else
		PlayAnimationOnMask(itmask, itanim, priority, speed, interp_duration, loop);
}

void SimpleAnimatableInst::ChangeAnimationSpeed(float speed)
{
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		itmask->second->mCurrentAnimData.mSpeed = speed;
}

void SimpleAnimatableInst::ChangeAnimationSpeedEx(ork::PoolString name, float speed)
{
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		if(itmask->second->mCurrentAnimData.mName == name)
			itmask->second->mCurrentAnimData.mSpeed = speed;
}

void SimpleAnimatableInst::ChangeAnimationFrameToFirst()
{
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		itmask->second->mCurrentAnimData.SetFrame(ork::CFloat::Zero());
}

void SimpleAnimatableInst::ChangeAnimationFrameToFirstEx(ork::PoolString name)
{
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		if(itmask->second->mCurrentAnimData.mName == name)
			itmask->second->mCurrentAnimData.SetFrame(ork::CFloat::Zero());
}

void SimpleAnimatableInst::ChangeAnimationFrameToLast()
{
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		itmask->second->mCurrentAnimData.SetFrame(itmask->second->mCurrentAnimData.AnimInst().GetNumFrames() - ork::CFloat::One());
}

void SimpleAnimatableInst::ChangeAnimationFrameToLastEx(ork::PoolString name)
{
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		if(itmask->second->mCurrentAnimData.mName == name)
			itmask->second->mCurrentAnimData.SetFrame(itmask->second->mCurrentAnimData.AnimInst().GetNumFrames() - ork::CFloat::One());
}

void SimpleAnimatableInst::ChangeAnimationFrameToMiddle(float scale)
{
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
	{
		float frame = scale * (itmask->second->mCurrentAnimData.AnimInst().GetNumFrames() - ork::CFloat::One());
		if(frame >= itmask->second->mCurrentAnimData.AnimInst().GetNumFrames() - ork::CFloat::One())
			itmask->second->mCurrentAnimData.SetFrame(itmask->second->mCurrentAnimData.AnimInst().GetNumFrames() - ork::CFloat::One());
		else if(frame <= ork::CFloat::Zero())
			itmask->second->mCurrentAnimData.SetFrame(ork::CFloat::Zero());
		else
			itmask->second->mCurrentAnimData.SetFrame(frame);
	}
}

void SimpleAnimatableInst::ChangeAnimationFrameToMiddleEx(ork::PoolString name, float scale)
{
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
	{
		if(itmask->second->mCurrentAnimData.mName == name)
		{
			float frame = scale * (itmask->second->mCurrentAnimData.AnimInst().GetNumFrames() - ork::CFloat::One());
			if(frame >= itmask->second->mCurrentAnimData.AnimInst().GetNumFrames() - ork::CFloat::One())
				itmask->second->mCurrentAnimData.SetFrame(itmask->second->mCurrentAnimData.AnimInst().GetNumFrames() - ork::CFloat::One());
			else if(frame <= ork::CFloat::Zero())
				itmask->second->mCurrentAnimData.SetFrame(ork::CFloat::Zero());
			else
				itmask->second->mCurrentAnimData.SetFrame(frame);
		}
	}
}

void SimpleAnimatableInst::ChangeAnimationPriority(ork::PoolString name, int priority)
{
	DEBUG_ANIMATE_PRINT("ChangeAnimationPriority anim %s with priority %d\n",name.c_str(), priority);
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		if(itmask->second->mCurrentAnimData.mName == name)
			itmask->second->mPriority = priority;
}

void SimpleAnimatableInst::ChangeMaskPriority(ork::PoolString maskname, int priority)
{
	DEBUG_ANIMATE_PRINT("ChangeMaskPriority on  mask %s with priority %d\n", strlen(maskname.c_str()) ? maskname.c_str() : "<fullbody>", priority);

	BodyPartMap::iterator itmask = mBodyPartMap.find(maskname);
	if(itmask != mBodyPartMap.end())
		itmask->second->mPriority = priority;
}

void SimpleAnimatableInst::ChangePriority(int priority)
{
	DEBUG_ANIMATE_PRINT("ChangePriority with priority %d\n",priority);
	for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		itmask->second->mPriority = priority;
}

bool SimpleAnimatableInst::IsAnimPlaying(ork::PoolString name) const
{
	for(BodyPartMap::const_iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		if(itmask->second->mCurrentAnimData.mName == name)
			return true;
	return false;
}

void SimpleAnimatableInst::MarkAnimOnMask(BodyPartMap::iterator &itmask, ork::PoolString name)
{
	AnimToMaskMap::iterator itanimtomask = mAnimToMaskMap.find(name);
	itanimtomask->second.push_back(itmask);

	SimpleAnimatableData::AnimSeqTableMap::const_iterator ittable = mData.GetAnimSeqTableMap().find(name);
	if(ittable != mData.GetAnimSeqTableMap().end())
	{
		bool first = true;
		for(orklist<BodyPartMap::iterator>::const_iterator it = itanimtomask->second.begin(); it != itanimtomask->second.end(); it++)
			if(first)
				(*it)->second->mCurrentAnimData.SetAnimSeqTable(ittable->second);
			else
				(*it)->second->mCurrentAnimData.SetAnimSeqTable(NULL);
	}
}

void SimpleAnimatableInst::UnmarkAnimOnMask(BodyPartMap::iterator &itmask, ork::PoolString name)
{
	AnimToMaskMap::iterator itanimtomask = mAnimToMaskMap.find(name);
	for(orklist<BodyPartMap::iterator>::iterator it = itanimtomask->second.begin(); it != itanimtomask->second.end(); it++)
		if((*it) == itmask)
		{
			itanimtomask->second.erase(it);
			return;
		}
}

void SimpleAnimatableInst::PlayAnimationOnMask(BodyPartMap::iterator &itmask, SimpleAnimatableData::AnimationMap::const_iterator &itanim,
											   int priority, float speed, float interp_duration, bool loop)
{
	if( 0 == mModelInst ) return;

	if(!itanim->second)
	{
		itmask->second->mPreviousAnimData.BindAnim(NULL);
		itmask->second->mCurrentAnimData.BindAnim(NULL);

		//orkprintf("WARNING: Anim is not loaded: %s\n", itanim->first.c_str());

		return;
	}

	if(itmask->second->mCurrentAnimData.AnimInst().GetAnim())
	{
		if(itmask->second->mCurrentAnimData.mName == itanim->first)
		{
			DEBUG_ANIMATE_PRINT("Already Playing Anim %s on mask %s with priority %d\n", itanim->first.c_str(), strlen(itmask->first.c_str()) ? itmask->first.c_str() : "<fullbody>", priority);
			//e even though we are playing change priority
			itmask->second->mPriority = priority;

			return; /// ALREADY PLAYING ANIM
		}

		if(priority < itmask->second->mPriority)
		{
			DEBUG_ANIMATE_PRINT("Priority is not high enough for Anim %s on mask %s with priority %d\n", itanim->first.c_str(), strlen(itmask->first.c_str()) ? itmask->first.c_str() : "<fullbody>", priority);

			return; /// PRIORITY IS NOT HIGH ENOUGH
		}

		DEBUG_ANIMATE_PRINT("Play Anim %s on mask %s with priority %d on entity %s\n",
			itanim->first.c_str(), strlen(itmask->first.c_str()) ? itmask->first.c_str() : "<fullbody>",
			priority, GetEntity()->GetEntData().GetName().c_str());

		UnmarkAnimOnMask(itmask, itmask->second->mCurrentAnimData.mName);

		if(interp_duration > 0.0f)
		{
			/// INTERP
			if(!itmask->second->mPreviousAnimData.AnimInst().GetAnim() || itmask->second->mPreviousAnimData.AnimInst().GetWeight() < 0.5f)
			{
				/// COPY CURRENT ANIM TO PREVIOUS ANIM
				itmask->second->mPreviousAnimData = itmask->second->mCurrentAnimData;
			}

			itmask->second->mInterpSpeed = 1.0f / interp_duration;

			itmask->second->mCurrentAnimData.SetFrame(0.0f);
			itmask->second->mCurrentAnimData.SetWeight(0.0f);
			itmask->second->mCurrentAnimData.mSpeed = speed;
			itmask->second->mCurrentAnimData.mLoop = loop;
			itmask->second->mCurrentAnimData.mName = itanim->first;
			itmask->second->mPriority = priority;
			itmask->second->mCurrentAnimData.SetAnimSeqTable(NULL);

			if(!itmask->first.empty())
				SetMaskFromJoints(itmask->second->mCurrentAnimData.RefMask(), itmask->second->mCachedJoints);

#if ANIMATE_VERBOSE
			/*if(PRINT_CONDITION)
			{
				orkprintf("Mask for %s\n", itmask->first.c_str());
				for(int i = 0; i < mModelInst->GetXgmModel()->RefSkel().GetNumJoints(); i++)
					if(itmask->second->mCurrentAnimData.RefMask().Check(i))
						orkprintf(" %s", mModelInst->GetXgmModel()->RefSkel().GetJointName(i));
				orkprintf("\n");
			}*/
#endif

			itmask->second->mPreviousAnimData.SetWeight(1.0f);
			itmask->second->mPreviousAnimData.SetAnimSeqTable(NULL);

			itmask->second->mCurrentAnimData.BindAnim(GetAnim(itanim->second));
		}
		else
		{
			itmask->second->mCurrentAnimData.SetFrame(0.0f);
			itmask->second->mCurrentAnimData.SetWeight(1.0f);
			itmask->second->mCurrentAnimData.mSpeed = speed;
			itmask->second->mCurrentAnimData.mLoop = loop;
			itmask->second->mCurrentAnimData.mName = itanim->first;
			itmask->second->mPriority = priority;
			itmask->second->mCurrentAnimData.SetAnimSeqTable(NULL);

			if(!itmask->first.empty())
				SetMaskFromJoints(itmask->second->mCurrentAnimData.RefMask(), itmask->second->mCachedJoints);

#if ANIMATE_VERBOSE
			/*if(PRINT_CONDITION)
			{
				orkprintf("Mask for %s\n", itmask->first.c_str());
				for(int i = 0; i < mModelInst->GetXgmModel()->RefSkel().GetNumJoints(); i++)
					if(itmask->second->mCurrentAnimData.RefMask().Check(i))
						orkprintf(" %s", mModelInst->GetXgmModel()->RefSkel().GetJointName(i));
				orkprintf("\n");
			}*/
#endif

			itmask->second->mPreviousAnimData.SetWeight(0.0f);
			itmask->second->mPreviousAnimData.SetAnimSeqTable(NULL);

			/// NO INTERP
			itmask->second->mCurrentAnimData.BindAnim(GetAnim(itanim->second));
		}
	}
	else
	{
		DEBUG_ANIMATE_PRINT("Play First Anim %s on mask %s with priority %d on entity %s\n",
			itanim->first.c_str(), strlen(itmask->first.c_str()) ? itmask->first.c_str() : "<fullbody>",
			priority, GetEntity()->GetEntData().GetName().c_str());

		itmask->second->mCurrentAnimData.SetFrame(0.0f);
		itmask->second->mCurrentAnimData.SetWeight(1.0f);
		itmask->second->mCurrentAnimData.mSpeed = speed;
		itmask->second->mCurrentAnimData.mLoop = loop;
		itmask->second->mCurrentAnimData.mName = itanim->first;
		itmask->second->mPriority = priority;
		itmask->second->mCurrentAnimData.SetAnimSeqTable(NULL);
		if(!itmask->first.empty())
			SetMaskFromJoints(itmask->second->mCurrentAnimData.RefMask(), itmask->second->mCachedJoints);

#if ANIMATE_VERBOSE
		/*if(PRINT_CONDITION)
		{
			mModelInst->GetXgmModel()->RefSkel().dump();
			orkprintf("Mask for %s\n", itmask->first.c_str());
			for(int i = 0; i < mModelInst->GetXgmModel()->RefSkel().GetNumJoints(); i++)
				if(itmask->second->mCurrentAnimData.RefMask().Check(i))
					orkprintf(" %s", mModelInst->GetXgmModel()->RefSkel().GetJointName(i));
			orkprintf("\n");
		}*/
#endif

		itmask->second->mPreviousAnimData.SetWeight(0.0f);
		itmask->second->mPreviousAnimData.SetAnimSeqTable(NULL);

		/// FIRST ANIM
		itmask->second->mCurrentAnimData.BindAnim(GetAnim((itanim->second)));
	}

	MarkAnimOnMask(itmask, itanim->first);
}

bool SimpleAnimatableInst::AnimDataUpdate(AnimData &data, float delta, ork::lev2::XgmModelInst *modelInst, ork::ent::Entity *entity)
{
	OrkAssert(modelInst);
	OrkAssert(data.AnimInst().GetAnim());
	OrkAssert(data.AnimInst().GetNumFrames());

	bool result = false;

	float oldframe = data.AnimInst().GetCurrentFrame();
	float frame = oldframe + delta * data.mSpeed * data.AnimInst().GetSampleRate();
	if(data.mLoop)
	{
		bool loop_foward = frame >= data.AnimInst().GetNumFrames();
		bool loop_backward = frame < 0.0f;
		// Loop if beyond last frame
		while(frame >= data.AnimInst().GetNumFrames())
		{
			frame -= data.AnimInst().GetNumFrames();
			result = true;
		}
		// Loop if before first frame
		while(frame < 0.0f)
		{
			frame += data.AnimInst().GetNumFrames();
			result = true;
		}
		// Notify AnimSeq events IF I loop I need 2 events
		if(data.mAnimSeqTable && entity)
		{
			if(loop_foward)
			{
				data.mAnimSeqTable->NotifyListener(oldframe, data.AnimInst().GetNumFrames(), entity);
				data.mAnimSeqTable->NotifyListener(0, frame, entity);
			}
			else if(loop_backward)
			{
				data.mAnimSeqTable->NotifyListener(0, oldframe, entity);
				data.mAnimSeqTable->NotifyListener(frame, data.AnimInst().GetNumFrames(), entity);
			}
			else
				data.mAnimSeqTable->NotifyListener(oldframe, frame, entity);
		}
	}
	else
	{
		// Clamp if beyond last frame
		if(frame >= data.AnimInst().GetNumFrames())
		{
			frame = data.AnimInst().GetNumFrames() - 1.0f;
			result = true;
		}
		// Clamp if before first frame
		if(frame < 0.0f)
		{
			frame = 0.0f;
			result = true;
		}
		// Notify AnimSeq events
		if(data.mAnimSeqTable && entity)
			data.mAnimSeqTable->NotifyListener(oldframe, frame, entity);
	}

	DEBUG_ANIMATE_PRINT_AD("frame %f\n", frame);
	data.SetFrame(frame);

	// Apply the previous anim to the model using its weight
	modelInst->RefLocalPose().ApplyAnimInst(data.AnimInst());
	modelInst->RefMaterialInst().ApplyAnimInst(data.AnimInst());

	return result;
}
float SimpleAnimatableInst::GetFrameNumOnAnimationOnFirstMask()
{
	if(mModelInst)
	{
		BodyPartMap::iterator itmask = mBodyPartMap.begin();
		return(itmask->second->mCurrentAnimData.GetFrame());
	}
	return(-1.0f);
}

void SimpleAnimatableInst::DoUpdate(ork::ent::SceneInst *inst)
{
#if 1
	float dt = inst->GetDeltaTime();

	if(mModelInst)
	{
		// Put the model in the model pose
		mModelInst->RefLocalPose().BindPose();

		bool service_queue = false;

		for(BodyPartMap::iterator itmask = mBodyPartMap.begin(); itmask != mBodyPartMap.end(); itmask++)
		{
			bool finish = false;

			if(itmask->second->mCurrentAnimData.AnimInst().GetAnim() && itmask->second->mCurrentAnimData.AnimInst().GetNumFrames())
			{
				// if current anim is valid, animate the weight
				if(itmask->second->mCurrentAnimData.AnimInst().GetWeight() < 1.0f)
				{
					float weight = itmask->second->mCurrentAnimData.AnimInst().GetWeight() + itmask->second->mInterpSpeed * dt;
					if(weight >= 1.0f)
						itmask->second->mCurrentAnimData.SetWeight(1.0f);
					else
						itmask->second->mCurrentAnimData.SetWeight(weight);
				}

				// update current anim (weight is always greater than zero for current anim)
				finish = AnimDataUpdate(itmask->second->mCurrentAnimData, dt, mModelInst, GetEntity());

				DEBUG_ANIMATE_PRINT("Current Anim: %s anim is on mask %s at frame %g with weight %g on entity %s\n",
					itmask->second->mCurrentAnimData.mName.c_str(),
					strlen(itmask->first.c_str()) ? itmask->first.c_str() : "<fullbody>",
					itmask->second->mCurrentAnimData.GetFrame(), itmask->second->mCurrentAnimData.GetWeight(),
					GetEntity()->GetEntData().GetName().c_str());
			}

			if(itmask->second->mPreviousAnimData.AnimInst().GetAnim() && itmask->second->mPreviousAnimData.AnimInst().GetNumFrames())
			{
				// if previous anim is valid, animate the weight
				if(itmask->second->mPreviousAnimData.AnimInst().GetWeight() > 0.0f)
				{
					float weight = itmask->second->mPreviousAnimData.AnimInst().GetWeight() - itmask->second->mInterpSpeed * dt;
					if(weight <= 0.0f)
						itmask->second->mPreviousAnimData.SetWeight(0.0f);
					else
						itmask->second->mPreviousAnimData.SetWeight(weight);
				}

				// if weight is greater than zero, update previous anim
				if(itmask->second->mPreviousAnimData.AnimInst().GetWeight() > 0.0f)
				{
					AnimDataUpdate(itmask->second->mPreviousAnimData, dt, mModelInst);

					DEBUG_ANIMATE_PRINT("Previous Anim: %s anim is on mask %s at frame %g with weight %g\n", itmask->second->mPreviousAnimData.mName.c_str(), strlen(itmask->first.c_str()) ? itmask->first.c_str() : "<fullbody>", itmask->second->mPreviousAnimData.GetFrame(), itmask->second->mPreviousAnimData.GetWeight());
				}
			}

			if(finish)
			{
				service_queue = true;

				event::AnimFinishEvent event(itmask->second->mCurrentAnimData.mName);
				GetEntity()->Notify(&event);
			}
		}

		mModelInst->RefLocalPose().BuildPose();

		if(service_queue)
		{
			if(mAnimationQueue.size())
			{
				AnimationQueueItem lastitem = mAnimationQueue.front();

				mAnimationQueue.pop_front();
				if(mAnimationQueue.size())
				{
					AnimationQueueItem &item = mAnimationQueue.front();
					DEBUG_ANIMATE_PRINT("Anim Queue:Play anim %s with priority 0 on entity %s\n", item.mName, GetEntity()->GetEntData().GetName().c_str());
					PlayAnimationEx(sEmptyString, item.mName, 0, item.mSpeed, item.mInterpDuration, item.mLoop);
				}
				else if(!lastitem.mLoop) {}
					//if(prodigy::ent::GroundControllableInst *groundInst = GetEntity()->GetTypedComponent<prodigy::ent::GroundControllableInst>())
					//	groundInst->Enable();
			}
		}
	}
#endif
}

void SimpleAnimatableInst::AnimData::BindAnim(const ork::lev2::XgmAnim *anim)
{
	if(mSai.mModelInst)
	{
		mSai.mModelInst->RefLocalPose().UnBindAnimInst(mAnimInst);
		mSai.mModelInst->RefMaterialInst().UnBindAnimInst(mAnimInst);
		mAnimInst.BindAnim(anim);
		mSai.mModelInst->RefMaterialInst().BindAnimInst(mAnimInst);
		mSai.mModelInst->RefLocalPose().BindAnimInst(mAnimInst);
	}
}

bool SimpleAnimatableInst::DoNotify(const ork::event::Event *event)
{
	if(const event::PlayAnimationEvent *play = ork::rtti::autocast(event))
	{
		if(play->GetMaskName().empty())
			PlayAnimationEx(sEmptyString, play->GetName(), play->GetPriority(), play->GetSpeed(), play->GetInterpDuration(), play->IsLoop());
		else
			PlayAnimationEx(play->GetMaskName(), play->GetName(), play->GetPriority(), play->GetSpeed(), play->GetInterpDuration(), play->IsLoop());
		return true;
	}
	else if(const event::ChangeAnimationSpeedEvent *change_speed = ork::rtti::autocast(event))
	{
		ChangeAnimationSpeed(change_speed->GetSpeed());
		return true;
	}
	else if(const event::AnimationPriority *anim_priority = ork::rtti::autocast(event))
	{
		ChangeAnimationPriority(anim_priority->GetName(), anim_priority->GetPriority());
		return true;
	}
	else if(const event::MaskPriority *mask_priority = ork::rtti::autocast(event))
	{
		ChangeMaskPriority(mask_priority->GetMaskName(), mask_priority->GetPriority());
		return true;
	}
	else if(const event::PriorityEvent *priority = ork::rtti::autocast(event))
	{
		ChangePriority(priority->GetPriority());
		return true;
	}
	return false;
}

} }
