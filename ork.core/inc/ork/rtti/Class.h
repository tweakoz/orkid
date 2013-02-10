////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/rtti/ICastable.h>

#include <ork/kernel/string/ConstString.h>
#include <ork/kernel/string/PoolString.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/reflect/Description.h>

#include <ork/config/config.h>

namespace ork { namespace rtti {

class RTTIData;

class Category;

class  Class : public ICastable
{
public:
    Class(const RTTIData &);

    static void InitializeClasses();

	Class *Parent();
    Class *FirstChild();
    Class *NextSibling();
    Class *PrevSibling();
	const Class *Parent() const;
	const PoolString &Name() const;
	void SetName(ConstString name,bool badd2map=true);

	rtti::ICastable *CreateObject() const;
	void SetFactory(rtti::ICastable *(*factory)());

	bool HasFactory() const { return (mFactory!=0); }

	virtual void Initialize();

	static Class *FindClass(const ConstString &name);

	static ConstString DesignNameStatic();
	static Category *GetClassStatic();
	/*virtual*/ Class *GetClass() const;

	template<typename ClassType>
	static void InitializeType() {}

	bool IsSubclassOf(const Class *other) const;
	const ICastable *Cast(const ICastable *other) const;
	ICastable *Cast(ICastable *other) const;

	static void CreateClassAlias( ConstString name , Class * );

private:
	void AddChild(Class *pClass);
	void FixSiblingLinks();
	void RemoveFromHierarchy();

    void (*mClassInitializer)();

	Class *mParentClass;
    Class *mChildClass;
    Class *mNextSiblingClass;
    Class *mPrevSiblingClass;
    
	PoolString mClassName;
	rtti::ICastable *(*mFactory)();
	Class *mNextClass;

    static Class *sLastClass;

	typedef orklut<PoolString, Class *> ClassMapType;
	static ClassMapType mClassMap;
};

} }
