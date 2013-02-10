////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
//  The PerfController (short for Performance Controller)
//   manages meta programs and collections of programs
//   
// A 'program' is defined as one or more key/value pairs that will be applied
//  to running entity components.
//  An entity component must respond to the 'PerfControlEvent' and 'PerfSnapShotEvent' events
//
///////////////////////////////////////////////////////////////////////////////

#ifndef ORK_ENT_PERFCONTROLLER_H
#define ORK_ENT_PERFCONTROLLER_H

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/entity.h>
#include <ork/math/cvector3.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace dataflow { class morph_event; } }
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

class PerfProgramTarget : public ork::Object
{
	RttiDeclareConcrete(PerfProgramTarget, ork::Object);
	
public:

	PerfProgramTarget();
	PerfProgramTarget(const char* tname, const char* tval);

	const PoolString& GetTargetName() const { return mTargetName; }
	const PoolString& GetValue() const { return mValue; }

private:
	
	PoolString mTargetName;
	PoolString mValue;
};

///////////////////////////////////////////////////////////////////////////////

class PerfProgramData : public ork::Object
{
	RttiDeclareConcrete(PerfProgramData, ork::Object);
	
public:

	PerfProgramData();
	const orklut<PoolString,ork::Object*>& GetTargets() const { return mTargets; }

	void AddTarget( const char* name, PerfProgramTarget* ptarget );
	void Clear();
	
private:
	
	orklut<PoolString,ork::Object*> mTargets;
};

///////////////////////////////////////////////////////////////////////////////

class PerfControllerComponentData : public ork::ent::ComponentData
{
	RttiDeclareConcrete(PerfControllerComponentData, ork::ent::ComponentData);

public:
	///////////////////////////////////////////////////////
	PerfControllerComponentData();
	~PerfControllerComponentData();
	virtual ork::ent::ComponentInst *CreateComponent(ork::ent::Entity *pent) const;
	const orklut<PoolString,ork::Object*>& GetPrograms() const { return mPrograms; }
	const PoolString& GetCurrentProgram() const { return mCurrentProgram; }
	void AddProgram( PoolString name, PerfProgramData* program );
	void WriteProgram();
	void AdvanceProgram();
	void MorphEvent(const ork::dataflow::morph_event* me);
	
private:

	orklut<PoolString,ork::Object*> mPrograms;
	PoolString						mCurrentProgram;
	PoolString						mAutoTargets;
	PoolString						mMorphGroup;
	const PoolString				mkGroupAString;
	const PoolString				mkGroupBString;
	const PoolString				mkGroupCString;
	const PoolString				mkGroupDString;
	float							mfMorphA;
	float							mfMorphB;
	float							mfMorphC;
	float							mfMorphD;

	void SetMorphA( const float& fv );
	void SetMorphB( const float& fv );
	void SetMorphC( const float& fv );
	void SetMorphD( const float& fv );
	void GetMorphA( float& outf ) const;
	void GetMorphB( float& outf ) const;
	void GetMorphC( float& outf ) const;
	void GetMorphD( float& outf ) const;
};

///////////////////////////////////////////////////////////////////////////////

class PerfControllerComponentInst : public ork::ent::ComponentInst
{
	RttiDeclareAbstract(PerfControllerComponentInst, ork::ent::ComponentInst);

public:

	PerfControllerComponentInst( const PerfControllerComponentData &data, ork::ent::Entity *pent );
	~PerfControllerComponentInst();
	const PerfControllerComponentData& GetPCCD() const { return mPCCD; }
	void ChangeProgram( ent::SceneInst* psi, const PoolString& progname );
	
private:

	virtual void DoUpdate(ork::ent::SceneInst *inst);
	bool DoLink(ork::ent::SceneInst *psi); // virtual

	const PerfControllerComponentData&	mPCCD;
	PoolString							mCurrentProgram;

};

///////////////////////////////////////////////////////////////////////////////

class PerfControllerArchetype : public Archetype
{
	RttiDeclareConcrete(PerfControllerArchetype, Archetype);
public:
	PerfControllerArchetype();
private:
	void DoCompose(ArchComposer& composer); // virtual
	void DoStartEntity(SceneInst*, const CMatrix4& mtx, Entity* pent ) const {}
};

///////////////////////////////////////////////////////////////////////////////
// PerfControlEvent - send performance data change event to an entity component
///////////////////////////////////////////////////////////////////////////////

class PerfControlEvent : public ork::event::Event
{
	RttiDeclareConcrete(PerfControlEvent, ork::event::Event);
public:

	PerfControlEvent();

	FixedString<256> mTarget;
	FixedString<32> mValue;
	std::string PopTargetNode();
	
	PoolString ValueAsPoolString() const;
	
private:

};

///////////////////////////////////////////////////////////////////////////////
// PerfSnapShotEvent- query performance data change event from an entity component
//   this data will be stored in a program for later recall
///////////////////////////////////////////////////////////////////////////////

class PerfSnapShotEvent : public ork::event::Event
{
	RttiDeclareConcrete(PerfSnapShotEvent, ork::event::Event);

public:

	PerfSnapShotEvent();

	typedef FixedString<256> str_type;
	void SetProgram( PerfProgramData* pprog ) { mSnapShotProgram=pprog; }
	PerfProgramData* GetProgram() const { return mSnapShotProgram; }
	void PushNode( str_type str ) const; 
	void PopNode() const; 
	
	str_type GenNodeName() const;
	void AddTarget( const char* tgtval ) const;

private:

	mutable orkvector<str_type> mNodeStack;
	PerfProgramData*	mSnapShotProgram;

};

///////////////////////////////////////////////////////////////////////////////

}}

#endif
