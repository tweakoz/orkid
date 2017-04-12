////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>

#include <ork/rtti/Class.h>
#include <ork/kernel/string/StringPool.h>
#include <ork/rtti/RTTI.h>
#include <ork/rtti/Category.h>

#include <ork/kernel/orklut.hpp>

#include <ork/kernel/csystem.h>

#include <ork/application/application.h>

namespace ork {
	template class orklut<PoolString, rtti::Class *>;
}

namespace ork { namespace rtti {

Class::Class(const RTTIData &rtti)
	: mParentClass(rtti.ParentClass())
	, mClassInitializer(rtti.ClassInitializer())
	, mFactory(0)

	, mNextClass(sLastClass)
	, mNextSiblingClass(this)
	, mPrevSiblingClass(this)
	, mChildClass(NULL)
{
	sLastClass = this;
}


void Class::Initialize()
{
	if(mParentClass)
		mParentClass->AddChild(this);

	if(mClassInitializer != NULL)
	{
		(*mClassInitializer)();
	}
}

void Class::InitializeClasses()
{
	//CSystem::Log(CSystem::LOG_SEVERITY_INFO, "System", "InitializeClasses");

	//for(Class *clazz = sLastClass; clazz != NULL; clazz = clazz->mNextClass)
	//{
	//	orkprintf( "InitClass class<%08x>\n", clazz );
	//}
	for(Class *clazz = sLastClass; clazz != NULL; clazz = clazz->mNextClass)
	{
		//orkprintf( "InitClass class<%08x> next<%08x>\n", clazz, clazz->mNextClass );
		clazz->Initialize();
		//orkprintf( "InitClass class<%08x><%s>\n", clazz, clazz->Name().c_str() );
	}

	sLastClass = NULL;
}

void Class::SetName(ConstString name,bool badd2map)
{
	if(name.length())
	{
		mClassName = AddPooledString(name.c_str());

		if( badd2map )
		{
			ClassMapType::iterator it = mClassMap.find(mClassName);
			if(it != mClassMap.end())
			{
				if(it->second != this)
				{
					Class *previous = it->second;

					orkprintf("ERROR: Duplicate class name %s! previous class %p\n", mClassName.c_str(), previous);
					OrkAssert(false);
				}
			}
			else
			{
				mClassMap.AddSorted(mClassName, this);
			}
		}
	}
}

void Class::SetFactory(rtti::ICastable *(*factory)())
{
	mFactory = factory;
}

Class *Class::Parent()
{
	return mParentClass;
}

const Class *Class::Parent() const
{
	return mParentClass;
}

Class *Class::FirstChild()
{
	return mChildClass;
}

Class *Class::NextSibling()
{
	return mNextSiblingClass;
}

Class *Class::PrevSibling()
{
	return mPrevSiblingClass;
}

void Class::AddChild(Class *pClass)
{
	if(mChildClass)
	{
		pClass->mNextSiblingClass = mChildClass->mNextSiblingClass;
		pClass->mPrevSiblingClass = mChildClass;
		pClass->FixSiblingLinks();
	}

	mChildClass = pClass;
}

void Class::FixSiblingLinks()
{
	mNextSiblingClass->mPrevSiblingClass = this;
	mPrevSiblingClass->mNextSiblingClass = this;
}

void Class::RemoveFromHierarchy()
{
	mNextSiblingClass->mPrevSiblingClass = mPrevSiblingClass;
	mPrevSiblingClass->mNextSiblingClass = mNextSiblingClass;

	if(mParentClass->mChildClass == this)
		mParentClass->mChildClass = mNextSiblingClass;

	if(mParentClass->mChildClass == this)
		mParentClass->mChildClass = NULL;

	mNextSiblingClass = mPrevSiblingClass = this;
}

const PoolString &Class::Name() const
{
	return mClassName;
}

Class *Class::FindClass(const ConstString &name)
{
	return OrkSTXFindValFromKey(mClassMap, FindPooledString(name.c_str()), NULL);
}

Class *Class::FindClassNoCase(const ConstString &name)
{
	std::string nocasename = name.c_str();
	std::transform(nocasename.begin(), nocasename.end(), nocasename.begin(), ::tolower);
	for( const auto& it : mClassMap )
	{
		if( 0 == strcasecmp(it.first.c_str(),nocasename.c_str() ) )
			return it.second;
	}
    return nullptr;
}

rtti::ICastable *Class::CreateObject() const
{
	return (*mFactory)();
}

bool Class::IsSubclassOf(const Class *other) const
{
	const Class *this_class = this;

	for(;;)
	{
		if(this_class == other) return true;
		if(this_class == NULL) return false;
		this_class = this_class->Parent();
	}
}

const ICastable *Class::Cast(const ICastable *other) const
{
	if(NULL == other || other->GetClass()->IsSubclassOf(this)) return other;
	return NULL;
}

ICastable *Class::Cast(ICastable *other) const
{
	if(NULL == other || other->GetClass()->IsSubclassOf(this)) return other;
	return NULL;
}

void Class::CreateClassAlias( ConstString name , Class *pclass )
{
	PoolString ClassName = AddPooledString(name.c_str());

	ClassMapType::iterator it = mClassMap.find(ClassName);
	OrkAssert( it == mClassMap.end() );
	mClassMap.AddSorted( ClassName, pclass );
}

Class *Class::sLastClass = NULL;
Class::ClassMapType Class::mClassMap;

static Category sClassClass(RTTI<Class, ICastable, NamePolicy, Category>::ClassRTTI());




Category *Class::GetClassStatic() { return &sClassClass; }
Class *Class::GetClass() const { return Class::GetClassStatic(); }
ConstString Class::DesignNameStatic() { return "Class"; }
} }
