////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once  

///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtui_tool.h>
#include <ork/object/AutoConnector.h>
#include <ork/lev2/ui/surface.h>

namespace ork { namespace lev2{ class CFont; } }
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
	bool AreEntsEnabled() const { return mShowEnts; }
	bool AreArchsEnabled() const { return mShowArchs; }
	bool AreCompsEnabled() const { return mShowComps; }
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
	int mLastSelection;
	bool mShowEnts;
	bool mShowArchs;
	bool mShowComps;
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
	
	void DoRePaintSurface(ui::DrawEvent& drwev) override;
	void DoInit( lev2::GfxTarget* pt ) override;
	ui::HandlerResult DoOnUiEvent( const ui::Event& EV ) override;
	void SetNameOfSelectedItem();
	
	bool mBlockUser;
	bool mInSlotFromSelectionManager;
	ork::lev2::CFont* mFont;
	int mCharW, mCharH;
	int miScrollY;
	int mContentH;
	bool mDark;
	ork::lev2::CTXBASE* mCtxBase;

};
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
