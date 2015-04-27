////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#include <pkg/ent/component.h>

#include <pkg/ent/entity.h>
#include <pkg/ent/componenttable.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>
#include <ork/reflect/DirectObjectVectorPropertyType.hpp>
#include <ork/reflect/RegisterProperty.h>
#include <ork/application/application.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ComponentDataClass, "ComponentDataClass")
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ComponentData, "ComponentData")
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::ComponentInst, "ComponentInst")
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneComponentData, "SceneComponentData")
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneComponentInst, "SceneComponentInst")
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EditorPropMapData, "EditorPropMapData")
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::EditorPropMapInst, "EditorPropMapInst")

template class ork::orklut< ork::PoolString, orklist< ork::ent::ComponentInst* > >;
template class ork::orklut< ork::PoolString, orklist< ork::ent::ComponentData* > >;
template class ork::reflect::DirectObjectVectorPropertyType< orkvector<ork::PoolString> >;

namespace ork { namespace ent {

void SceneComponentData::Describe()
{
}
void SceneComponentInst::Describe()
{
}
void SceneComponentInst::Link( SceneInst* psi )
{
	DoLink(psi);
}
void SceneComponentInst::UnLink( SceneInst* psi )
{
	DoUnLink(psi);
}
void SceneComponentInst::Start( SceneInst* psi )
{
	DoStart(psi);
}
void SceneComponentInst::Stop( SceneInst* psi )
{
	DoStop(psi);
}
void SceneComponentInst::Update( SceneInst* psi )
{
	DoUpdate( psi );
}
ComponentDataClass::ComponentDataClass(const rtti::RTTIData &data) : object::ObjectClass(data)
{
}

void ComponentData::Describe()
{
}

ComponentData::ComponentData()
{
}

PoolString ComponentData::GetFamily() const
{
	const ComponentDataClass *clazz = rtti::autocast(GetClass());
	OrkAssert(clazz);
	return clazz->GetFamily();
}

void ComponentInst::Describe()
{
}

PoolString ComponentInst::GetFamily() const
{
	return mComponentData ? mComponentData->GetFamily() : PoolString();
}


void ComponentInst::Update(SceneInst *inst)
{
	if( mbValid )
	{
		MCheckPointContext( "ComponentInst::Update" );
		DoUpdate(inst);
	}
}

void EditorPropMapInst::Describe()
{
}
void EditorPropMapData::Describe()
{
	RegisterFamily<EditorPropMapData>(ork::AddPooledLiteral("")); // no update
	ork::reflect::RegisterMapProperty( "Properties", & EditorPropMapData::mProperties );
}

EditorPropMapData::EditorPropMapData()
{
}

void EditorPropMapData::SetProperty(const ConstString &key, const ConstString &val)
{
	mProperties.AddSorted(key, val);
}

ConstString EditorPropMapData::GetProperty(const ConstString &key) const
{
	ConstString rval("");
	orklut<ConstString, ConstString>::const_iterator it = mProperties.find(key);
	if(it != mProperties.end())
		rval = (*it).second;
	return rval;
}

ComponentInst::ComponentInst( const ComponentData* data, Entity* pent )
	: mComponentData(data)
	, mEntity(pent)
	, mbStarted(false)
	, mbValid(false)
{
	//printf( "ComponentInst::ComponentInst<%p> ent<%p>\n", this, pent );
}

void ComponentInst::Link(SceneInst *psi)
{
	mbValid = DoLink(psi);

	/*if(!mbValid) orkprintf("WARNING: Failed to Link component %s of entity %s with archetype %s\n"
		, GetClass()->Name().c_str(), mEntity->GetEntData().GetName().c_str()
		, mEntity->GetEntData().GetArchetype() ? mEntity->GetEntData().GetArchetype()->GetName().c_str() : "null");*/
}
void ComponentInst::UnLink(SceneInst *psi)
{
	DoUnLink(psi);
	mbValid = false;
}

void ComponentInst::Start(SceneInst *psi, const CMatrix4 &world)
{
	if( mbValid && (false == mbStarted) )
	{
		mbValid = DoStart(psi, world);
		if( mbValid )
		{
			mbStarted=true;
		}
	}
}

void ComponentInst::Stop(SceneInst *psi)
{
	if( mbStarted )
	{
		DoStop(psi);
		mbStarted=false;
	}
}

const char* ComponentInst::GetEntityName() const
{
	return mEntity->GetEntData().GetName().c_str();
}


} }
