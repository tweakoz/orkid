////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <pkg/ent/component.h>
#include <pkg/ent/componenttable.h>
#include <ork/math/TransformNode.h>
#include <ork/lev2/gfx/renderer.h>
#include <ork/lev2/lev2_asset.h>
#include <ork/gfx/camera.h>

namespace ork { namespace ent {
    
///////////////////////////////////////////////////////////////////////////////

struct ScriptComponentData : public ent::ComponentData
{
	ScriptComponentData();

	const file::Path& GetPath() const { return mScriptPath; }

private:
	RttiDeclareConcrete( ScriptComponentData, ent::ComponentData );
	ent::ComponentInst* CreateComponent(ent::Entity* pent) const final;
	void DoRegisterWithScene( ork::ent::SceneComposer& sc ) final;

	file::Path mScriptPath;

};

///////////////////////////////////////////////////////////////////////////////

struct ScriptComponentInst : public ent::ComponentInst
{
	ScriptComponentInst( const ScriptComponentData& cd, ork::ent::Entity* pent );
	const ScriptComponentData&	GetCD() const { return mCD; }

private:

	RttiDeclareAbstract( ScriptComponentInst, ent::ComponentInst );
	void DoUpdate(ent::SceneInst* sinst) final;
	bool DoLink(SceneInst *psi) final;
	void DoUnLink(SceneInst *psi) final;
	bool DoStart(SceneInst *psi, const CMatrix4 &world) final;
	void DoStop(SceneInst *psi) final;
	const ScriptComponentData&		mCD;
	std::string mScriptText;

};

///////////////////////////////////////////////////////////////////////////////

class ScriptManagerComponentData : public ork::ent::SceneComponentData
{
	RttiDeclareConcrete(ScriptManagerComponentData, ork::ent::SceneComponentData);

public:
	///////////////////////////////////////////////////////
	ScriptManagerComponentData();
	ork::ent::SceneComponentInst* CreateComponentInst(ork::ent::SceneInst *pinst) const; // virtual 
	///////////////////////////////////////////////////////

private:

};

///////////////////////////////////////////////////////////////////////////////

class ScriptManagerComponentInst : public ork::ent::SceneComponentInst
{
	RttiDeclareAbstract(ScriptManagerComponentInst, ork::ent::ComponentInst);

public:

	ScriptManagerComponentInst( const ScriptManagerComponentData &data, ork::ent::SceneInst *pinst );
	~ScriptManagerComponentInst();

	void DoUpdate(SceneInst *inst) final;

	anyp GetLuaManager() { return mLuaManager; }

private:

	anyp mLuaManager;
	std::string mScriptText;
	int mScriptRef;
};

///////////////////////////////////////////////////////////////////////////////

/*class StartNamedScripts : public ork::EntityEvent
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
};*/

}} // namespace ork/ent

