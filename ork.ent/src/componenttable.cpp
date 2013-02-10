////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <pkg/ent/componenttable.h>
#include <pkg/ent/component.h>
#include <ork/kernel/orklut.hpp>

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut< ork::PoolString, ork::ent::ComponentInst* >;
template class ork::orklut< ork::PoolString, ork::ent::ComponentData* >;
//template<> class orklut<const rtti::Class*,ent::ComponentInst*>;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

ComponentTable::ComponentTable(LutType&comps)
	: mComponents(comps)
{

}
ComponentTable::~ComponentTable()
{
}

void ComponentTable::AddComponent(ComponentInst* component)
{
	//orkprintf("ComponentTable::AddComponent(%s)\n", component->GetFamily().c_str());
	mComponents.AddSorted(component->GetFamily(), component);

}

void ComponentTable::RemoveComponent(ComponentInst* component)
{
	LutType::iterator it = mComponents.find(component->GetFamily());
	mComponents.RemoveItem(it);

}

const ComponentTable::LutType& ComponentTable::GetComponents() const
{
	return mComponents;
}

ComponentTable::LutType& ComponentTable::GetComponents()
{
	return mComponents;
}

ComponentTable::ComponentBounds ComponentTable::GetComponentsByFamily(PoolString family)
{
	return std::make_pair(mComponents.LowerBound(family), mComponents.UpperBound(family));
}

ComponentTable::ComponentBoundsConst ComponentTable::GetComponentsByFamily(PoolString family) const
{
	return std::make_pair(mComponents.LowerBound(family), mComponents.UpperBound(family));
}

ComponentDataTable::ComponentDataTable(LutType&comps)
	: mComponents(comps)
{
}
ComponentDataTable::~ComponentDataTable()
{
}

void ComponentDataTable::AddComponent(ComponentData* component)
{
	mComponents.AddSorted(component->GetFamily(), component);
}

void ComponentDataTable::Clear()
{
	mComponents.clear();
}

void ComponentDataTable::RemoveComponent(ComponentData* component)
{
	LutType::iterator it = mComponents.find(component->GetFamily());
	mComponents.RemoveItem(it);

}

const ComponentDataTable::LutType& ComponentDataTable::GetComponents() const
{
	return mComponents;
}

ComponentDataTable::LutType& ComponentDataTable::GetComponents()
{
	return mComponents;
}

ComponentDataTable::ComponentBounds ComponentDataTable::GetComponentsByFamily(PoolString family)
{
	return std::make_pair(mComponents.LowerBound(family), mComponents.UpperBound(family));
}

ComponentDataTable::ComponentBoundsConst ComponentDataTable::GetComponentsByFamily(PoolString family) const
{
	return std::make_pair(mComponents.LowerBound(family), mComponents.UpperBound(family));
}

} }
