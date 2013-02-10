////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _JELLY_SCRIPTCOMPONENT_H_
#define _JELLY_SCRIPTCOMPONENT_H_

#include <ork/rtti/RTTI.h>

#include <pkg/ent/entity.h>

#include <pkg/script/Script.h>

namespace ork { namespace ent {

typedef ork::rtti::RTTI<ScriptComponentFactory, ork::EntityComponentFactory> ScriptComponentFactoryBase;
    
class ScriptComponentFactory : public ScriptComponentFactoryBase
{
    DECLARE_TRANSPARENT_CASTABLE( ScriptComponentFactory, ScriptComponentFactoryBase );
    
public:
	//static void ClassInit();
	//static void Describe();
	ScriptComponentFactory();

	const ork::orklut<ork::PoolString, ork::PoolString> &GetNamedScripts() const;
	ork::orklut<ork::PoolString, ork::PoolString> &GetNamedScripts();

	const orkvector<ork::PoolString> &GetStartupScriptNames() const;
	orkvector<ork::PoolString> &GetStartupScriptNames();

private:
	virtual ork::EntityComponent* CreateComponent(ork::Entity* pent, const ork::PoolString& name) const;

	ork::orklut<ork::PoolString, ork::PoolString> mNamedScripts;
	orkvector<ork::PoolString> mStartupScriptNames;
};

class ScriptComponent : public ork::EntityComponent
{
	DECLARE_TRANSPARENT_CASTABLE(ScriptComponent, ork::EntityComponent)
public:
	static void Describe();

	ScriptComponent(ork::Entity* entity, const ork::PoolString& name, const ScriptComponentFactory *factory);
	/*virtual*/ ~ScriptComponent();

	/*virtual*/ void DoResolveDependencies(ork::EntityComponentTable::ComponentLut& components);

private:
	/*virtual*/ void Update();
	/*virtual*/ void Notify(const ork::EntityEvent* event);

	void CompileScriptPlayer(ork::PoolString script);

	void StartScripts(ork::PoolString name);
	void StopScripts(ork::PoolString name);

	const ScriptComponentFactory *mFactory;

	ork::orklut<ork::PoolString, EventScript::Player> mScriptPlayers;
};

class StartNamedScripts : public ork::EntityEvent
{
	DECLARE_TRANSPARENT_CASTABLE(ScriptComponent, ork::EntityEvent)
public:

	StartNamedScripts(ork::PieceString name = ork::PieceString());
	StartNamedScripts(ork::PoolString name);

	void SetName(ork::PieceString name);
	void SetName(ork::PoolString name);
	ork::PoolString GetName() const;

private:

	ork::PoolString mName;
};
    
class StartScript : public ork::EntityEvent
{
	DECLARE_TRANSPARENT_CASTABLE(StartScript, ork::EntityEvent)
public:

	StartScript(ork::PieceString name = ork::PieceString());
	StartScript(ork::PoolString name);

	void SetName(ork::PieceString name);
	void SetName(ork::PoolString name);
	ork::PoolString GetName() const;

private:

	ork::PoolString mName;
};

} // namespace jelly

#endif // !_JELLY_SCRIPTCOMPONENT_H_
