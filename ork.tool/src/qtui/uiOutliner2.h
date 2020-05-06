////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtui_tool.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/ui/surface.h>

namespace ork { namespace lev2{ class Font; } }
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class SceneEditorBase;
class SceneObject;
struct Outliner2View;

///////////////////////////////////////////////////////////////////////////////

struct Outliner2Item
{
	Outliner2Item() : mObject(nullptr), mIndent(0), mSelected(false) {}
	std::string mName;
	ork::Object* mObject;
	int mIndent;
	bool mSelected;
};

struct Outliner2Model : public AutoConnector
{
public:

	RttiDeclareAbstract(Outliner2Model, AutoConnector)

	DeclarePublicAutoSlot( SceneTopoChanged );
	DeclarePublicAutoSlot( ClearSelection );
	DeclarePublicAutoSlot( ObjectSelected );
	DeclarePublicAutoSlot( ObjectDeSelected );
	DeclarePublicSignal( ModelChanged );

	Outliner2Model(SceneEditorBase&ed,Outliner2View&v);
	~Outliner2Model();
	////////////////////////////////////////////////////////////////////
	void ToggleEnts();
	void ToggleArchs();
	void ToggleComps();
	void ToggleSystems();
	void ToggleGlobals();
	bool AreEntsEnabled() const { return mShowEnts; }
	bool AreArchsEnabled() const { return mShowArchs; }
	bool AreCompsEnabled() const { return mShowComps; }
	bool AreSystemsEnabled() const { return mShowSystems; }
	bool AreGlobalsEnabled() const { return mShowGlobals; }
	////////////////////////////////////////////////////////////////////
	void SigModelChanged();
	void SlotSceneTopoChanged();
	void SlotClearSelection();
	void SlotObjectSelected(ork::Object*pobj);
	void SlotObjectDeSelected(ork::Object*pobj);
	////////////////////////////////////////////////////////////////////
	SceneEditorBase& Editor() const { return mEditor; }
	const std::vector<Outliner2Item>& Items() const { return mItems; }

	void IncSel();
	void DecSel();

	int GetLastSelection() const { return mLastSelection; }

private:

	void UpdateModel();

	//object::Signal						mChangeNodeNameSignal;
	SceneEditorBase&					mEditor;
	Outliner2View&						mVP;
	std::vector<Outliner2Item>			mItems;
	std::set<ork::Object*>				mSelected;
	int mLastSelection = 0;
	bool mShowEnts = true;
	bool mShowArchs = true;
	bool mShowComps = true;
	bool mShowSystems = true;
	bool mShowGlobals = true;
};
///////////////////////////////////////////////////////////////////////////////
struct Outliner2View : public ui::Surface
{
	Outliner2View(SceneEditorBase&ed);

	////////////////////////////////////////////////////////////////////

	DeclarePublicAutoSlot( ModelChanged );

	void SlotModelChanged();
	void SlotObjectSelected( ork::Object* pobj );			// notification of externally changed selection
	void SlotObjectDeSelected( ork::Object* pobj );			// notification of externally changed selection

	Outliner2Model mOutlinerModel;

private:

	int kitemh() const;

	void DoRePaintSurface(ui::drawevent_ptr_t drwev) override;
	void DoInit( lev2::Context* pt ) override;
	ui::HandlerResult DoOnUiEvent( ui::event_constptr_t EV ) override;
	void SetNameOfSelectedItem();

	bool mBlockUser = true;
	bool mInSlotFromselectionManager;
	ork::lev2::Font* mFont = nullptr;
	int mCharW = 0, mCharH = 0;
	int miScrollY = 0;
	int mContentH = 0;
	bool mDark = true;
	ork::lev2::CTXBASE* mCtxBase = nullptr;

};
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
