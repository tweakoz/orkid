////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/rtti/RTTI.h>
#include <ork/object/ObjectCategory.h>

#include <ork/config/config.h>

namespace ork { namespace object {

class ObjectCategory;

class  ObjectClass : public rtti::Class
{
	RttiDeclareExplicit(ObjectClass, rtti::Class, rtti::NamePolicy, ObjectCategory)
public:
	ObjectClass(const rtti::RTTIData &);

	reflect::Description &Description();
	const reflect::Description &Description() const;

	template<typename ClassType>
	static void InitializeType() { ClassType::Describe(); }
private:
	reflect::Description mDescription;
	void Initialize() override;
};

} }
