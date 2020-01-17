////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/scene.h>
#include <ork/lev2/gfx/renderer/drawable.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/dataflow.h>
#include <pkg/ent/PerfController.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/kernel/orklut.hpp>
#include <ork/application/application.h>

namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static bool gbusepreview = false;

bool DataflowRecieverComponentData::DoNotify(const event::Event *event)
{	if( const ObjectGedVisitEvent* pev = rtti::autocast( event ) )
	{	gbusepreview = true;
		return true;
	}
	return false;
}

void DataflowRecieverComponentData::Describe()
{
	ork::reflect::RegisterMapProperty("FloatRecievers", & DataflowRecieverComponentData::mFloatValues );
	ork::reflect::RegisterMapProperty("Vect3Recievers", & DataflowRecieverComponentData::mVect3Values );
}

///////////////////////////////////////////////////////////////////////////////

DataflowRecieverComponentData::DataflowRecieverComponentData()
{

}

///////////////////////////////////////////////////////////////////////////////

ComponentInst *DataflowRecieverComponentData::createComponent(Entity *pent) const
{
	return OrkNew DataflowRecieverComponentInst( *this, pent );
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void DataflowRecieverComponentInst::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////

DataflowRecieverComponentInst::DataflowRecieverComponentInst(const DataflowRecieverComponentData &data, Entity *pent)
	: ComponentInst(&data,pent)
	, mData(data)
{
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	const orklut<PoolString,float>& invals_float = mData.GetFloatValues();
	orklut<PoolString,ork::dataflow::dyn_external::FloatBinding>& float_bindings = mExternal.GetFloatBindings();
	float_bindings.clear();
	mMutableFloatValues = invals_float;
	for( orklut<PoolString,float>::const_iterator
			it=invals_float.begin();
			it!=invals_float.end();
			it++ )
	{
		const ork::PoolString& name = it->first;
		const float& ConstReferencedValue = it->second;
		ork::dataflow::dyn_external::FloatBinding binding;
		binding.mpSource = & ConstReferencedValue;
		float_bindings.AddSorted( name, binding );
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
	const orklut<PoolString,fvec3>& invals_vect3 = mData.GetVect3Values();
	orklut<PoolString,ork::dataflow::dyn_external::Vect3Binding>& vect3_bindings = mExternal.GetVect3Bindings();
	vect3_bindings.clear();
	mMutableVect3Values = invals_vect3;

	for( orklut<PoolString,fvec3>::const_iterator
			it=invals_vect3.begin();
			it!=invals_vect3.end();
			it++ )
	{
		const ork::PoolString& name = it->first;
		const fvec3& ConstReferencedValue = it->second;
		ork::dataflow::dyn_external::Vect3Binding binding;
		binding.mpSource = & ConstReferencedValue;
		vect3_bindings.AddSorted( name, binding );
	}
	//////////////////////////////////////////////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void DataflowRecieverComponentInst::BindExternalValue( PoolString name, const float* psource )
{
	if( false == gbusepreview )
	{
		orklut<PoolString,ork::dataflow::dyn_external::FloatBinding>::iterator it = mExternal.GetFloatBindings().find(name);
		if( it != mExternal.GetFloatBindings().end() )
		{
			ork::dataflow::dyn_external::FloatBinding& binding = it->second;
			binding.mpSource = psource;
		}
	}
}

void DataflowRecieverComponentInst::BindExternalValue( PoolString name, const fvec3* psource )
{
	if( false == gbusepreview )
	{
		orklut<PoolString,ork::dataflow::dyn_external::Vect3Binding>::iterator it = mExternal.GetVect3Bindings().find(name);
		if( it != mExternal.GetVect3Bindings().end() )
		{
			ork::dataflow::dyn_external::Vect3Binding& binding = it->second;
			binding.mpSource = psource;
		}
	}
}

bool DataflowRecieverComponentInst::DoNotify(const event::Event *event)
{
	if( const PerfControlEvent* pce = rtti::autocast(event) )
	{
//		const FixedString<256>& tgt = pce->mTarget;
//		const FixedString<32>& val = pce->mValue;

		printf( "DataflowRecieverComponentInst<%p> PerfControlEvent<%p> key<%s>\n", this, pce, pce->mTarget.c_str() );
		PoolString k = AddPooledString(pce->mTarget.c_str());

			//orklut<PoolString,float>				mMutableFloatValues;
		orklut<PoolString,float>& invals_float = const_cast< DataflowRecieverComponentData& >(mData).GetFloatValues();
		//orklut<PoolString,float>::iterator it=mMutableFloatValues.find(k);
		orklut<PoolString,float>::iterator it=invals_float.find(k);
		if( it!=invals_float.end() )
		{
			float fres = float(atof(pce->mValue.c_str()));
			it->second  = fres;
			printf( "  key<%s> newval<%f>\n", k.c_str(), fres );
		}

	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////

void DataflowRecieverComponentInst::DoUpdate( ork::ent::Simulation* psi ){}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

}} // namespace ork { namespace ent {

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::DataflowRecieverComponentData, "DataflowRecieverComponentData");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::DataflowRecieverComponentInst, "DataflowRecieverComponentInst");


template const ork::ent::DataflowRecieverComponentData* ork::ent::EntData::GetTypedComponent<ork::ent::DataflowRecieverComponentData>() const;
template ork::ent::DataflowRecieverComponentInst* ork::ent::Entity::GetTypedComponent<ork::ent::DataflowRecieverComponentInst>(bool);
