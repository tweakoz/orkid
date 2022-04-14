////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/kernel/orklut.h>

#include <ork/kernel/string/PoolString.h>
#include <ork/ecs/types.h>

namespace ork { namespace ecs {

struct ComponentDataTable {
public:
  typedef orklut<object::ObjectClass*, componentdata_constptr_t> LutType;

  typedef LutType::iterator Iterator;
  typedef LutType::const_iterator ConstIterator;

  typedef std::pair<Iterator, Iterator> ComponentBounds;
  typedef std::pair<ConstIterator, ConstIterator> ComponentBoundsConst;

  ComponentDataTable(LutType& comps);
  ~ComponentDataTable();

  void addComponent(componentdata_constptr_t component);
  void removeComponent(componentdata_constptr_t component);

  const LutType& GetComponents() const;
  LutType& GetComponents();

  void Clear();

private:
  LutType& _components;
};

struct ComponentTable {
public:
  typedef orklut<PoolString, Component*> LutType;

  typedef LutType::iterator Iterator;
  typedef LutType::const_iterator ConstIterator;

  typedef std::pair<Iterator, Iterator> ComponentBounds;
  typedef std::pair<ConstIterator, ConstIterator> ComponentBoundsConst;

  ComponentTable(LutType& comps);
  ~ComponentTable();

  void AddComponent(Component* component);
  void RemoveComponent(Component* component);

  const LutType& GetComponents() const;
  LutType& GetComponents();

  ComponentBounds GetComponentsByFamily(PoolString family);
  ComponentBoundsConst GetComponentsByFamily(PoolString family) const;

private:
  LutType& _components;
};

}} // namespace ork::ecs
