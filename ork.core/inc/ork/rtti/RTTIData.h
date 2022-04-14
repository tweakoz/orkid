////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/kernel/string/ConstString.h>

namespace ork {
namespace rtti {

struct Class;

struct RTTIData
{
public:
	RTTIData(Class *parent, void (*initializer)())
		: _parentClass(parent)
		, mClassInitializer(initializer)
	{}

	Class *ParentClass() const { return _parentClass; }
	void (*ClassInitializer() const)() { return mClassInitializer; }
private:
	Class *_parentClass;
	void (*mClassInitializer)();
};

} }
