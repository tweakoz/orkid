////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include <ork/kernel/orklut.h>

#include <ork/kernel/string/PoolString.h>

namespace ork { namespace ent {

class ComponentInst;
class ComponentData;

class ComponentDataTable 
{
public:
	typedef orklut<PoolString, ComponentData*>	LutType;

	typedef LutType::iterator					Iterator;
	typedef LutType::const_iterator				ConstIterator;

	typedef std::pair<Iterator, Iterator> ComponentBounds;
	typedef std::pair<ConstIterator, ConstIterator> ComponentBoundsConst;

	ComponentDataTable(LutType &comps);
	~ComponentDataTable();

	void AddComponent(ComponentData* component);
	void RemoveComponent(ComponentData* component);

	const LutType& GetComponents() const;
	LutType& GetComponents();

	ComponentBounds GetComponentsByFamily(PoolString family);
	ComponentBoundsConst GetComponentsByFamily(PoolString family) const;

	void Clear();

private:
	 LutType& _components;
};

class ComponentTable 
{
public:
	typedef orklut<PoolString, ComponentInst*>	LutType;

	typedef LutType::iterator					Iterator;
	typedef LutType::const_iterator				ConstIterator;

	typedef std::pair<Iterator, Iterator> ComponentBounds;
	typedef std::pair<ConstIterator, ConstIterator> ComponentBoundsConst;

	ComponentTable(LutType &comps);
	~ComponentTable();

	void AddComponent(ComponentInst* component);
	void RemoveComponent(ComponentInst* component);

	const LutType& GetComponents() const;
	LutType& GetComponents();

	ComponentBounds GetComponentsByFamily(PoolString family);
	ComponentBoundsConst GetComponentsByFamily(PoolString family) const;

private:
	 LutType& _components;
};

} } // namespace ork::ent

