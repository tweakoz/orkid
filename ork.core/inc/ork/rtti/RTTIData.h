////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
