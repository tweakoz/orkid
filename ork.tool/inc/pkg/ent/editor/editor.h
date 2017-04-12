////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <orktool/toolcore/choiceman.h>
#include <orktool/toolcore/builtinchoices.h>
#include <orktool/toolcore/selection.h>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <ork/lev2/gfx/lighting/gfx_lighting.h>
#include <orktool/manip/manip.h>
#include <ork/reflect/Functor.h>
#include <ork/reflect/Command.h>
#include <ork/object/AutoConnector.h>
#include <ork/kernel/opq.h>
#include <ork/kernel/any.h>

namespace ork { namespace dataflow { struct scheduler; } }

namespace ork { namespace ent {

class SceneEditorBase;

const ork::PoolString& EditorChanName();

class ArchetypeChoices : public ork::tool::CChoiceList
{
	SceneEditorBase& mSceneEditor;

public:

	virtual void EnumerateChoices( bool bforcenocache=false );

	ArchetypeChoices(SceneEditorBase& SceneEditor);
};

///////////////////////////////////////////////////////////////////////////

class RefArchetypeChoices : public ork::tool::CChoiceList
{
public:
	virtual void EnumerateChoices( bool bforcenocache );
	RefArchetypeChoices();
};

///////////////////////////////////////////////////////////////////////////

class SceneEditorBase : public ork::AutoConnector
{
	RttiDeclareAbstract( SceneEditorBase, ork::AutoConnector );

	///////////////////////////////////////////////
	// private signals and slots
	///////////////////////////////////////////////

	DeclarePublicSignal( SceneTopoChanged );
	DeclarePublicSignal( ObjectDeleted );
	DeclarePublicSignal( NewScene );

	DeclarePublicAutoSlot( ModelInvalidated );
	DeclarePublicAutoSlot( PreNewObject );
	DeclarePublicAutoSlot( NewObject );

	void SigSceneTopoChanged();
	void SigNewScene();
	void SigObjectDeleted(ork::Object* pobj);

	void SlotModelInvalidated();
	void SlotPreNewObject();
	void SlotNewObject( ork::Object* pobj );

	//////////////////////////////////////////////////

	void NewSceneInst();

public:

	SceneEditorBase();
	~SceneEditorBase();

	typedef any128 var_t;

	ork::Application*				mApplication;
	bool							mbInit;
	ork::tool::CChoiceManager		mChoiceMan;

	tool::CModelChoices*			mpMdlChoices;
	tool::ChsmChoices*				mpChsmChoices;
	tool::CAnimChoices*				mpAnmChoices;
	tool::CTextureChoices*			mpTexChoices;
	tool::ScriptChoices*			mpScriptChoices;
	tool::AudioStreamChoices*		mpAudStreamChoices;
	tool::AudioBankChoices*			mpAudBankChoices;
	tool::FxShaderChoices*			mpFxShaderChoices;
	ArchetypeChoices*				mpArchChoices;
	RefArchetypeChoices*			mpRefArchChoices;

	ent::SceneData*					mpScene;

	
	SceneInst* GetActiveSceneInst() const;
	SceneInst* GetEditSceneInst() const;
	SceneInst* GetExecSceneInst() const;
	SceneData* GetSceneData() const { return mpScene; }

	///////////////////////////////////////////////

	//void SetCursor( const ork::CVector3& v3 ) { mCursor=v3; }
	void SetSpawnMatrix( const ork::CMatrix4& mtx ) { mSpawnMatrix=mtx; }

	///////////////////////////////////////////////

	const tool::SelectManager&		SelectionManager() const { return mSelectionManager; }
	tool::SelectManager&			SelectionManager() { return mSelectionManager; }
	lev2::CManipManager&			ManipManager() { return mManipManager; }

	///////////////////////////////////////////////

	bool EditorRenameSceneObject( SceneObject* pobj, const char* pname );
	SceneObject* FindSceneObject( const char* pname );
	const SceneObject* FindSceneObject( const char* pname ) const;

	///////////////////////////////////////////////
	
	ReferenceArchetype* NewReferenceArchetype( const std::string& archassetname );
	//Archetype* NewArchetype( const std::string& classname );
	Archetype* EditorNewArchetype(const std::string& classname, const std::string& name);

	///////////////////////////////////////////////

	void EditorLocateEntity(const CMatrix4 &matrix);
	bool EditorGetEntityLocation(CMatrix4 &matrix);

	void EditorNewEntities(int count);
	ent::EntData* EditorReplicateEntity();
	void EditorPlaceEntity();
	void EditorGroup();
	void EditorArchExport();
	void EditorArchImport();
	void EditorArchMakeReferenced();
	void EditorArchMakeLocal();
	void EditorDupe();
	void EditorRefreshModels();
	void EditorRefreshAnims();

	void EditorRefreshTextures();
	void RegisterChoices();

	///////////////////////////////////////////////
	
	void ClearSelection();
	void ToggleSelection( ork::Object* pobj );
	void AddObjectToSelection( ork::Object* pobj );
	void EditorUnGroup( SceneGroup* pgrp );

	void GetSelected( orkset<ork::Object*>& SelSet );

	ork::CColor4 GetModColor( const ork::Object* pobj ) const;

	void QueueOpASync( const var_t& op );
	void QueueOpSync( const var_t& op );
	void QueueSync();

	ent::EntData* EditorNewEntity(const ent::Archetype* parchetype = NULL);
	void EditorDeleteObject(ork::Object* pobj);

private:

	void DisableViews();
	void EnableViews();
	void DisableUpdates();
	void EnableUpdates();

	////////////////////////////
	// impl functions must be serialized on the runloop
	////////////////////////////

	SceneData* ImplNewScene();
	SceneData* ImplGetScene();
	void ImplDeleteObject(ork::Object* pobj);
	EntData* ImplNewEntity(const ent::Archetype* parchetype = NULL);
	Archetype* ImplNewArchetype(const std::string& classname, const std::string& name);
	void ImplLoadScene( std::string filename );
	void ImplEnterRunLocalState();
	void ImplEnterPauseState();
	void ImplEnterEditState();

	void RunLoop();

	int 							mRunStatus; 							

	tool::SelectManager				mSelectionManager;
	lev2::CManipManager				mManipManager;

	ork::MpMcBoundedQueue<var_t>	mSerialQ;
	//ork::CVector3					mCursor;
	ork::CMatrix4					mSpawnMatrix;

	SceneInst*						mpExecSceneInst;
	SceneInst*						mpEditSceneInst;
	
};

///////////////////////////////////////////////////////////////////////////////

struct DeleteObjectReq
{
	DeleteObjectReq() : mObject(nullptr) {}
	ork::Object* mObject;
};
struct NewEntityReq
{
	NewEntityReq(Future&f=gnilfut) : mArchetype(nullptr), mResult(f) {}
	const ent::Archetype* mArchetype;
	EntData* GetEntity();
	void SetEntity(EntData*pent);
private:

	Future& mResult;
	static Future gnilfut;
};
struct NewArchReq
{
	NewArchReq(Future&f=gnilfut) : mResult(f) {}
	std::string mClassName;
	std::string mName;
	Archetype* GetArchetype();
	void SetArchetype(Archetype*parch);
private:

	Future& mResult;
	static Future gnilfut;
};
struct LoadSceneReq
{
	void SetOnLoadedOp( const void_lambda_t& l ) { mOnLoaded.Set<void_lambda_t>(l); }
	std::string mFileName;
	const any64& GetOnLoaded() const { return mOnLoaded; }
private:
	any64 mOnLoaded;
};
struct NewSceneReq
{
	NewSceneReq(Future&f=gnilfut) : mResult(f) {}
	SceneData* GetScene();
	void SetScene(SceneData*parch);
	Future& mResult;
	static Future gnilfut;
};
struct GetSceneReq
{
	GetSceneReq(Future&f=gnilfut) : mResult(f) {}
	SceneData* GetScene();
	void SetScene(SceneData*parch);
	Future& mResult;
	static Future gnilfut;
};
struct RunLocalReq
{
};
struct StopLocalReq
{
};

///////////////////////////////////////////////////////////////////////////////

}

}
