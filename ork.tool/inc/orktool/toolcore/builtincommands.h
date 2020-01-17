////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_TOOLCORE_BUILTINCOMMANDS_H
#define _ORK_TOOLCORE_BUILTINCOMMANDS_H

///////////////////////////////////////////////////////////////////////////////

#include <ork/kernel/core/singleton.h>
#include <ork/kernel/command/command.h>

namespace ork {
namespace tool {

///////////////////////////////////////////////////////////////////////////////

class ExportCommand : public Command
{
public:
	static const char* GetNameStatic() { return "export"; }
	static Command* Factory( void ) { return OrkNew ExportCommand; }

	void DoExecute( bool bdo );

	virtual void Execute();
	virtual void Undo();
	virtual void Redo();
	virtual std::string Format() const;
	virtual void Parse(const tokenlist& tokens);

	ExportCommand();
	std::string mFilename;
};


///////////////////////////////////////////////////////////////////////////////

class RecomposeEntitiesCommand : public Command
{
public:
	static const char* GetNameStatic() { return "recompose_entities"; }
	static Command* Factory( void ) { return OrkNew RecomposeEntitiesCommand; }

	void DoExecute( bool bdo );

	virtual void Execute();
	virtual void Undo();
	virtual void Redo();
	virtual std::string Format() const;
	virtual void Parse(const tokenlist& tokens);

	RecomposeEntitiesCommand();

};

///////////////////////////////////////////////////////////////////////////////

class DeleteCommand : public Command
{
public:

	static const char* GetNameStatic() { return "delete"; }
	static Command* Factory( void ) { return OrkNew DeleteCommand; }

	void DoExecute( bool bdo );

	virtual void Execute();
	virtual void Undo();
	virtual void Redo();
	virtual std::string Format() const;
	virtual void Parse(const tokenlist& tokens);
	virtual std::string GetName() const { return "delete"; }
	virtual orkvector<std::string> GetOpts() const { return orkvector<std::string>(); }

	DeleteCommand();

	//orkmap<CObject*,bool> mObjects;
};

///////////////////////////////////////////////////////////////////////////////
/*
class RenameCommand : public Command
{

public:

	static const char* GetNameStatic() { return "rename"; }
	static Command* Factory( void ) { return OrkNew RenameCommand; }

	void DoExecute( bool bdo );

	virtual void Execute();
	virtual void Undo();
	virtual void Redo();
	virtual std::string Format() const;
	virtual void Parse(const tokenlist& tokens);

	RenameCommand();

	orkmap<std::string,SceneObject*>	mObjectMap;
	std::string							mBaseName;

};

///////////////////////////////////////////////////////////////////////////////

class SignalCommand : public Command
{
public:

	static const char* GetNameStatic() { return "signal"; }
	static Command* Factory( void );

	void DoExecute( bool bdo );

	virtual void Execute();
	virtual void Undo();
	virtual void Redo();
	virtual std::string Format() const;
	virtual void Parse(const tokenlist& tokens);

	SignalCommand();

	std::string mSignalType;

};
*/
///////////////////////////////////////////////////////////////////////////////
/// PropChangeCommand (changes property for all selected Objects or a particular Object)
///   prop <PropertyName> <NewValue>
///   prop <PropertyName> <NewValue> <SceneObjectName> ...

class PropChangeCommand : public Command
{
public:

	static const char* GetNameStatic() { return "prop"; }
	static Command* Factory( void ) { return OrkNew PropChangeCommand; }

	//static const std::string sPropChangeCommandName;

	/// Constructor - Not yet parsed
	PropChangeCommand();
	/// Constructor - Parsed as form 1
	PropChangeCommand(const std::string& propName, const PropTypeString& after);
	/// Constructor - Parsed as form 2
	PropChangeCommand(const std::string& propName, const PropTypeString& after, const orkvector<std::string>& names);
	/// Constructor - Executed on a single Object
	//PropChangeCommand(const CProp* pProp, Object* pObject, const PropTypeString& before, const PropTypeString& after, bool selected);
	/// Constructor - Executed on multiple Objects
	//PropChangeCommand(orkvector<const CProp*>& props, orkvector<CObject*>& objects, orkvector<PropTypeString>& befores, const PropTypeString& after, bool selected);

	virtual void Execute();

	virtual void Undo();
	virtual void Redo();
	virtual std::string Format() const;
	virtual void Parse(const tokenlist& tokens);

protected:

	/// Parsed Command Data
	std::string mPropName;
	orkvector<std::string> mObjectNames;
	PropTypeString mAfterValue;

	/// Executed Command Data
	//orkvector<CObject*> mObjects;
	//orkvector<const CProp*> mProps;
	orkvector<PropTypeString> mBeforeValues;

};

///////////////////////////////////////////////////////////////////////////////
/// ObjectCreateCommand (creates a new Object and adds it to the ObjectManager)
///   new <ClassName>
///   new <ClassName> <SceneObjectName>
/// Note: second form only applies if Object is a SceneObject
/*
class ObjectCreateCommand : public Command
{
public:

	static const char* GetNameStatic() { return "create"; }
	static Command* Factory( void ) { return OrkNew ObjectCreateCommand; }

	/// Constructor - Not yet parsed
	ObjectCreateCommand() : Command(false, false), mObject(0) {}

	/// Constructor - Parsed as form 1
	ObjectCreateCommand(const std::string& className)
		: Command(GetNameStatic(),true, false), mClassName(className), mObject(0)
	{
		OrkAssert(mClassName.size() > 0);
	}

	/// Constructor - Parsed as form 1
	ObjectCreateCommand(const std::string& className, const std::string& sceneObjectName)
		: Command(GetNameStatic(),true, false), mClassName(className), mSceneObjectName(sceneObjectName), mObject(0)
	{
		OrkAssert(mClassName.size() > 0);
	}

	/// Constructor - Executed
	ObjectCreateCommand(CObject* pObject)
		: Command(GetNameStatic(),true, true), mObject(pObject)
	{
		OrkAssert(pObject);

		CClass* pClass = pObject->GetClass();
		mClassName = pClass->GetName();
		if(pClass->IsSubclassOf(SceneObject::GetClassStatic()))
			mSceneObjectName = static_cast<SceneObject*>(pObject)->GetName();
	}

	virtual void Execute();

	virtual void Undo();
	virtual void Redo();
	virtual std::string Format() const;
	virtual void Parse(const tokenlist& tokens);

protected:

	/// Parsed Command Data
	std::string mClassName;
	std::string mSceneObjectName;

	/// Executed Command Data
	CObject* mObject;

};
*/
///////////////////////////////////////////////////////////////////////////////

/*class ObjectCloneCommand : public Command
{
public:

	static const char* GetNameStatic() { return "clone"; }
	static Command* Factory( void ) { return OrkNew ObjectCloneCommand; }

	ObjectCloneCommand() : Command(GetNameStatic(),false, false), mObject(0), mCloneObject(0) {}

	ObjectCloneCommand(CObject* pObject, CObject* pCloneObject)
		: Command(GetNameStatic(),true, true), mObject(pObject), mCloneObject(pCloneObject) { OrkAssert(pObject && pCloneObject); }

	inline CObject* GetObject() const { return mObject; }
	inline CObject* GetCloneObject() const { return mCloneObject; }

	virtual void Execute();

	virtual void Undo();
	virtual void Redo();
	virtual std::string Format() const;
	virtual void Parse(const tokenlist& tokens);

protected:

	CObject* mObject;
	CObject* mCloneObject;

};
*/
///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////

#endif
