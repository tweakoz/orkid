////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
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
	: _components(comps)
{

}
ComponentTable::~ComponentTable()
{
}

void ComponentTable::AddComponent(ComponentInst* component)
{
	//orkprintf("ComponentTable::AddComponent(%s)\n", component->GetFamily().c_str());
	_components.AddSorted(component->GetFamily(), component);

}

void ComponentTable::RemoveComponent(ComponentInst* component)
{
	LutType::iterator it = _components.find(component->GetFamily());
	_components.RemoveItem(it);

}

const ComponentTable::LutType& ComponentTable::GetComponents() const
{
	return _components;
}

ComponentTable::LutType& ComponentTable::GetComponents()
{
	return _components;
}

ComponentTable::ComponentBounds ComponentTable::GetComponentsByFamily(PoolString family)
{
	return std::make_pair(_components.LowerBound(family), _components.UpperBound(family));
}

ComponentTable::ComponentBoundsConst ComponentTable::GetComponentsByFamily(PoolString family) const
{
	return std::make_pair(_components.LowerBound(family), _components.UpperBound(family));
}

ComponentDataTable::ComponentDataTable(LutType&comps)
	: _components(comps)
{
}
ComponentDataTable::~ComponentDataTable()
{
}

void ComponentDataTable::AddComponent(ComponentData* component)
{
	_components.AddSorted(component->GetFamily(), component);
}

void ComponentDataTable::Clear()
{
	_components.clear();
}

void ComponentDataTable::RemoveComponent(ComponentData* component)
{
	LutType::iterator it = _components.find(component->GetFamily());
	_components.RemoveItem(it);

}

const ComponentDataTable::LutType& ComponentDataTable::GetComponents() const
{
	return _components;
}

ComponentDataTable::LutType& ComponentDataTable::GetComponents()
{
	return _components;
}

ComponentDataTable::ComponentBounds ComponentDataTable::GetComponentsByFamily(PoolString family)
{
	return std::make_pair(_components.LowerBound(family), _components.UpperBound(family));
}

ComponentDataTable::ComponentBoundsConst ComponentDataTable::GetComponentsByFamily(PoolString family) const
{
	return std::make_pair(_components.LowerBound(family), _components.UpperBound(family));
}

} }
