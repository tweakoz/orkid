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
#include "LuaBindings.h"

using namespace LuaIntf;


namespace ork { namespace ent {

///////////////////////////////////////////////////////////////////////////////

struct ScriptComponentData : public ent::ComponentData
{
	ScriptComponentData();

	const file::Path& GetPath() const { return mScriptPath; }
	void SetPath(const file::Path& pth) { mScriptPath=pth; }

private:
	RttiDeclareConcrete( ScriptComponentData, ent::ComponentData );
	ent::ComponentInst* createComponent(ent::Entity* pent) const final;
	void DoRegisterWithScene( ork::ent::SceneComposer& sc ) final;

	file::Path mScriptPath;
};


typedef ork::FixedString<65536> script_text_t;
typedef ork::FixedString<256> script_funcname_t;

struct ScriptObject
{
	ScriptObject();

	script_text_t mScriptText;
	std::string mMD5Digest;
	int mOnEntLink;
	int mOnEntStart;
	int mOnEntStop;
	int mOnEntUpdate;
    int mModTabRef;
	int mScriptRef;
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
	bool DoStart(SceneInst *psi, const fmtx4 &world) final;
	void DoStop(SceneInst *psi) final;
	const ScriptComponentData&		mCD;
	std::string mScriptText;
	ScriptObject* mScriptObject;

	any<64> mLuaData;
    LuaRef mEntTable;
};

///////////////////////////////////////////////////////////////////////////////

class ScriptSystemData : public ork::ent::SystemData
{
	RttiDeclareConcrete(ScriptSystemData, ork::ent::SystemData);

public:
	///////////////////////////////////////////////////////
	ScriptSystemData();
	///////////////////////////////////////////////////////

private:
    ork::ent::System* createSystem(ork::ent::SceneInst *pinst) const final;

};

///////////////////////////////////////////////////////////////////////////////

class ScriptSystem : public ork::ent::System {

public:

  static constexpr systemkey_t SystemType = "ScriptSystem";
	systemkey_t systemTypeDynamic() final { return SystemType; }

	ScriptSystem( const ScriptSystemData &data, ork::ent::SceneInst *pinst );

	anyp GetLuaManager() { return mLuaManager; }

	ScriptObject* FlyweightScriptObject( const ork::file::Path& key );

private:

  ~ScriptSystem() final;

	bool DoLink(SceneInst *psi) final;
	void DoUnLink(SceneInst *psi) final;
	void DoUpdate(SceneInst *inst) final;
	void DoStart(SceneInst *psi) final;
	void DoStop(SceneInst *inst) final;

	anyp mLuaManager;
	std::string mScriptText;
	std::map<ork::file::Path,ScriptObject*> mScriptObjects;
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
