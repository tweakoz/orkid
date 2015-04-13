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

class ScriptComponentData : public ent::ComponentData
{
	RttiDeclareConcrete( ScriptComponentData, ent::ComponentData );

public:

	virtual ent::ComponentInst* CreateComponent(ent::Entity* pent) const;

	ScriptComponentData();

	void DoRegisterWithScene( ork::ent::SceneComposer& sc ) final;

};

///////////////////////////////////////////////////////////////////////////////

class ScriptComponentInst : public ent::ComponentInst
{
	RttiDeclareAbstract( ScriptComponentInst, ent::ComponentInst );

	const ScriptComponentData&		mCD;

	void DoUpdate(ent::SceneInst* sinst) final;
	bool DoLink(ork::ent::SceneInst *psi) final;

public:
	const ScriptComponentData&	GetCD() const { return mCD; }

	ScriptComponentInst( const ScriptComponentData& cd, ork::ent::Entity* pent );
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

