////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/ecs/componenttable.h>
#include <ork/ecs/component.h>
#include <ork/kernel/orklut.hpp>

///////////////////////////////////////////////////////////////////////////////

template class ork::orklut< ork::PoolString, ork::ecs::Component* >;
template class ork::orklut< ork::PoolString, ork::ecs::componentdata_constptr_t >;
//template<> class orklut<const rtti::Class*,ent::Component*>;

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ecs {
///////////////////////////////////////////////////////////////////////////////

ComponentTable::ComponentTable(LutType&comps)
	: _components(comps)
{

}
ComponentTable::~ComponentTable()
{
}

void ComponentTable::AddComponent(Component* component)
{
	_components.AddSorted(component->GetFamily(), component);

}

void ComponentTable::RemoveComponent(Component* component)
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

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

ComponentDataTable::ComponentDataTable(LutType&comps)
	: _components(comps)
{
}
ComponentDataTable::~ComponentDataTable()
{
}

void ComponentDataTable::addComponent(componentdata_constptr_t component)
{
	auto clazz = component->GetClass();
	auto name = clazz->Name();

	_components.AddSorted(clazz, component);
}

void ComponentDataTable::Clear()
{
	_components.clear();
}

void ComponentDataTable::removeComponent(componentdata_constptr_t component)
{
	auto clazz = component->GetClass();
	auto name = clazz->Name();
	LutType::iterator it = _components.find(clazz);
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

} }
