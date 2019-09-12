////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <pkg/ent/scene.h>
#include <pkg/ent/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <ork/dataflow/dataflow.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace ent {

class DataflowRecieverComponentData : public ComponentData
{
	RttiDeclareConcrete(DataflowRecieverComponentData, ComponentData)

public:

	DataflowRecieverComponentData();

	ComponentInst *createComponent(Entity *pent) const final;

	ork::orklut<ork::PoolString,float>& GetFloatValues() { return mFloatValues; }
	const ork::orklut<ork::PoolString,float>& GetFloatValues() const { return mFloatValues; }
	ork::orklut<ork::PoolString,fvec3>& GetVect3Values() { return mVect3Values; }
	const ork::orklut<ork::PoolString,fvec3>& GetVect3Values() const { return mVect3Values; }

private:

	ork::orklut<ork::PoolString,float>		mFloatValues;
	ork::orklut<ork::PoolString,fvec3>	mVect3Values;

	bool DoNotify(const event::Event *event) final;

	const char* GetShortSelector() const final { return "dfr"; }

};

///////////////////////////////////////////////////////////////////////////////

class DataflowRecieverComponentInst : public ComponentInst
{
	RttiDeclareAbstract(DataflowRecieverComponentInst, ComponentInst)

public:

	DataflowRecieverComponentInst(const DataflowRecieverComponentData &data, Entity *pent);

	//////////////////////////////////////////////////////////////
	// call BindExternalValue in external components dolink
	//  for binding to data in the external component
	//////////////////////////////////////////////////////////////

	void BindExternalValue( PoolString name, const float* psource );
	void BindExternalValue( PoolString name, const fvec3* psource );

	dataflow::dyn_external&					RefExternal() { return mExternal; }
	const dataflow::dyn_external&			RefExternal() const { return mExternal; }

private:

	const DataflowRecieverComponentData&	mData;
	dataflow::dyn_external					mExternal;
	orklut<PoolString,float>				mMutableFloatValues;
	orklut<PoolString,fvec3>				mMutableVect3Values;

	void DoUpdate( ork::ent::Simulation* psi ) final;
    bool DoNotify(const event::Event *event) final;

};

///////////////////////////////////////////////////////////////////////////////

}} // namespace ork { namespace ent {
