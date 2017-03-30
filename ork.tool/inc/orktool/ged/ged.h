////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <ork/object/AutoConnector.h>
#include <ork/kernel/string/ArrayString.h>
#include <ork/lev2/gfx/pickbuffer.h>
#include <ork/kernel/fixedlut.h>
#include <ork/kernel/orkpool.h>
#include <ork/kernel/any.h>
#include <ork/lev2/ui/viewport.h>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QDialog>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
class Object;
namespace lev2 { class CFont; }
namespace ui { struct Event; }
namespace tool {
class CChoiceManager;
template <typename T> class CPickBuffer;
namespace ged {
///////////////////////////////////////////////////////////////////////////////
class GedWidget;
class GedItemNode;
class GedSerializer;
class GedWidget;
class GedItemNode;
static const int kmaxgedstring = 256;

void ClearObjSet();
void AddToObjSet(void*pobj);
bool IsObjInSet(void*pobj);

///////////////////////////////////////////////////////////////////////////////

struct PersistHashContext
{
	ork::Object*						mObject;
	const reflect::IObjectProperty*		mProperty;
	const char*							mString;

	PersistHashContext();
	int GenerateHash() const;
};

class PersistantMap : public ork::Object
{
	RttiDeclareConcrete(PersistantMap,ork::Object);

	orklut<std::string,std::string>	mProperties;
	
public:

	const std::string& GetValue( const std::string& key );
	void SetValue( const std::string& key, const std::string& val );

	PersistantMap();
	~PersistantMap();
};

///////////////////////////////////////////////////////////////////////////////

class PersistMapContainer : public ork::Object
{
	RttiDeclareConcrete(PersistMapContainer,ork::Object);

	orklut<int, PersistantMap* >									mPropPersistMap;

public:

	PersistMapContainer();
	~PersistMapContainer();

	orklut<int, PersistantMap* >& GetMap() { return mPropPersistMap; }
	const orklut<int, PersistantMap* >& GetMap() const { return mPropPersistMap; }

	void CloneFrom( const PersistMapContainer& oth );
};

///////////////////////////////////////////////////////////////////////////////

class ObjModel : public ork::AutoConnector
{
	RttiDeclareConcrete(ObjModel,ork::AutoConnector);

public:

	static orkset<ObjModel*>	gAllObjectModels;
	static void FlushAllQueues();

	void SetGedWidget( GedWidget* Gedw ) { mpGedWidget=Gedw; }
	void Attach( ork::Object* obj, bool bclearstack=true, GedItemNode* rootw = 0 );
	GedItemNode* Recurse( ork::Object* obj, const char* pname = 0, bool binline=false );
	void Detach( );
	GedWidget* GetGedWidget() const { return mpGedWidget; }
	ObjModel();
    /*virtual*/ ~ObjModel();
	
	void SetChoiceManager( CChoiceManager* chcman ) { mChoiceManager=chcman; }
	CChoiceManager* GetChoiceManager( void ) const { return mChoiceManager; }
	
	void Dump(const char* header) const;

	void QueueObject( ork::Object* obj ) { mQueueObject=obj; }

	void ProcessQueue();

	ork::Object* CurrentObject() const { return mCurrentObject; }

	PersistantMap* GetPersistMap( const PersistHashContext& ctx );

	void QueueUpdateAll();
	void QueueUpdate();

	void FlushQueue();

	void PushBrowseStack( ork::Object* pobj );
	void PopBrowseStack();
	ork::Object* BrowseStackTop() const;
	int StackSize() const;

	PersistMapContainer& GetPersistMapContainer() { return mPersistMapContainer; }
	const PersistMapContainer& GetPersistMapContainer() const { return mPersistMapContainer; }

	void EnablePaint() { mbEnablePaint=true; }

private:
	GedWidget*														mpGedWidget;
	ork::Object*													mCurrentObject;
	ork::Object*													mRootObject;
	ork::Object*													mQueueObject;
	CChoiceManager*													mChoiceManager;
	orkstack<ork::Object*>											mBrowseStack;
	PersistMapContainer												mPersistMapContainer;
	bool															mbEnablePaint;

	//////////////////////////////////////////////////////////

	DeclarePublicSignal( Repaint );
	DeclarePublicSignal( PreNewObject );
	DeclarePublicSignal( ModelInvalidated );
	DeclarePublicSignal( PropertyInvalidated );
	DeclarePublicSignal( NewObject );
	DeclarePublicSignal( SpawnNewGed );

	DeclarePublicSignal( PostNewObject );

	DeclarePublicAutoSlot( NewObject );
	DeclarePublicAutoSlot( RelayModelInvalidated );
	DeclarePublicAutoSlot( RelayPropertyInvalidated );
	DeclarePublicAutoSlot( ObjectDeleted );
	DeclarePublicAutoSlot( ObjectSelected );
	DeclarePublicAutoSlot( ObjectDeSelected );
	DeclarePublicAutoSlot( Repaint );

	reflect::IInvokation*											mModelInvalidatedInvoker;
	
public:
	void SigModelInvalidated();
	void SigPreNewObject();
	void SigPropertyInvalidated( ork::Object* pobj, const reflect::IObjectProperty* prop );
	void SigRepaint();
	void SigSpawnNewGed( ork::Object* pobj );
	void SigNewObject(ork::Object*pobj);
	void SigPostNewObject(ork::Object*pobj);
private:

	//////////////////////////////////////////////////////////

	void SlotNewObject( ork::Object* pobj );
	void SlotRelayModelInvalidated();
	void SlotRelayPropertyInvalidated(ork::Object* pobj, const reflect::IObjectProperty* prop);
	void SlotObjectDeleted(ork::Object *pobj);
	void SlotObjectSelected( ork::Object* pobj );
	void SlotObjectDeSelected( ork::Object* pobj );
	void SlotRepaint();

	//////////////////////////////////////////////////////////

	struct sortnode
	{
		std::string															Name;
		orkvector< std::pair< std::string, reflect::IObjectProperty * > >	PropVect;
		orkvector< std::pair< std::string, sortnode* > >					GroupVect;
	};

	void EnumerateNodes( sortnode& in_node, object::ObjectClass* );

	//////////////////////////////////////////////////////////

	bool IsNodeVisible( const reflect::IObjectProperty *prop );
	GedItemNode* CreateNode( const std::string& Name, const reflect::IObjectProperty *prop, Object* pobject );

};
///////////////////////////////////////////////////////////////////////////////
class GedFactory : public ork::Object
{
	RttiDeclareAbstract(GedFactory,ork::Object);
public:
	virtual void Recurse(ObjModel& mdl, const reflect::IObjectProperty *prop,ork::Object*pobj) const {}
	virtual GedItemNode* CreateItemNode(ObjModel&mdl,const ConstString& Name,const reflect::IObjectProperty *prop,Object* obj) const;
};
///////////////////////////////////////////////////////////////////////////////

class GedObject;
class GedVP;

class GedSkin 
{

public:

	typedef void (*DrawCB)(GedSkin*pskin,GedObject*pnode,ork::lev2::GfxTarget* pTARG);

	struct GedPrim 
	{
		DrawCB						mDrawCB;
		GedObject*					mpNode;
		int							ix1, iy1, ix2, iy2;
		U32							ucolor;
		ork::lev2::EPrimitiveType	meType;
		int							miSortKey;

		GedPrim() : mDrawCB(0), mpNode(0), ix1(0), ix2(0), iy1(0), iy2(0), ucolor(0), miSortKey(0), meType(ork::lev2::EPRIM_END) {}
	};

	struct PrimContainer
	{
		static const int kmaxprims = 8192;
		static const int kmaxprimsper = 4096;
		fixed_pool<GedPrim,kmaxprims>	mPrimPool;
		fixedvector<GedPrim*,kmaxprimsper>	mLinePrims;
		fixedvector<GedPrim*,kmaxprimsper>	mQuadPrims;
		fixedvector<GedPrim*,kmaxprimsper>	mCustomPrims;

		void clear();
	};

	GedSkin(ork::lev2::GfxTarget* ptarg);

	typedef enum
	{
		ESTYLE_BACKGROUND_1 = 0,
		ESTYLE_BACKGROUND_2,
		ESTYLE_BACKGROUND_3,
		ESTYLE_BACKGROUND_4,
		ESTYLE_BACKGROUND_5,
		ESTYLE_BACKGROUND_OPS,
		ESTYLE_BACKGROUND_GROUP_LABEL,
		ESTYLE_BACKGROUND_MAPNODE_LABEL,
		ESTYLE_BACKGROUND_OBJNODE_LABEL,
		ESTYLE_DEFAULT_HIGHLIGHT,
		ESTYLE_DEFAULT_OUTLINE,
		ESTYLE_DEFAULT_CAPTION,
		ESTYLE_DEFAULT_CHECKBOX,
		ESTYLE_BUTTON_OUTLINE,
	} ESTYLE;

	virtual void Begin( ork::lev2::GfxTarget* pTARG, GedVP* pgedvp ) = 0;
	virtual void DrawBgBox( GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort=0 ) = 0;
	virtual void DrawOutlineBox( GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic, int isort=0 ) = 0;
	virtual void DrawLine( GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic ) = 0;
	virtual void DrawCheckBox( GedObject* pnode, int ix, int iy, int iw, int ih ) = 0;
	virtual void DrawDownArrow( GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic ) = 0;
	virtual void DrawRightArrow( GedObject* pnode, int ix, int iy, int iw, int ih, ESTYLE ic ) = 0;
	virtual void DrawText( GedObject* pnode, int ix, int iy, const char* ptext ) = 0;
	virtual void End( ork::lev2::GfxTarget* pTARG ) = 0;

	void SetScrollY( int iscrolly ) { miScrollY=iscrolly; }

	void AddPrim(const GedPrim& cb );// { mPrims.AddSorted(calcsort(cb.miSortKey),cb); }

	int GetScrollY() const { return miScrollY; }

	int calcsort( int isort )
	{
		int ioutsort = (isort<<16);//+int(mPrims.size());
		return ioutsort;
	}

	static const int kMaxPrimContainers = 32;
	fixed_pool<PrimContainer,32>	mPrimContainerPool;
	typedef fixedlut<int,PrimContainer*,kMaxPrimContainers> PrimContainers;

	void clear();

	int char_w() const { return miCHARW; }
	int char_h() const { return miCHARH; }

protected:

	PrimContainers	mPrimContainers;

	int	miScrollY;
	int miRejected;
	int miAccepted;
    GedVP* mpCurrentGedVp;
	ork::lev2::CFont* mpFONT;
	int	miCHARW;
	int	miCHARH;

	bool IsVisible( ork::lev2::GfxTarget* pTARG, int iy1, int iy2 )
	{
		int iry1 = iy1+miScrollY;
		int iry2 = iy2+miScrollY;
		int ih = pTARG->GetH();
		
		if( iry2<0 )
		{
			miRejected++;
			return false;
		}
		else if( iry1>ih )
		{
			miRejected++;
			return false;
		}

		miAccepted++;
		return true;
	}

};

///////////////////////////////////////////////////////////////////////////////

class GedObject : public ork::Object
{

protected:

	int									miD;
	int									miDecoIndex;

	GedObject() 
		: miD(0)
		, miDecoIndex(0)
	{}

public:

	void SetDepth( int id ) { miD=id; }
	int  GetDepth() const { return miD; }
	int GetDecoIndex() const { return miDecoIndex; }
	void SetDecoIndex(int idx ) { miDecoIndex=idx; }

    virtual void OnMouseDragged(const ork::ui::Event& ev) {}
	virtual void OnMouseMoved(const ork::ui::Event& ev) {}
	virtual void OnMouseClicked(const ork::ui::Event& ev) {}
	virtual void OnMouseDoubleClicked(const ork::ui::Event& ev) {}
	virtual void OnMouseReleased(const ork::ui::Event& ev) {}

	virtual void OnUiEvent( const ork::ui::Event& ev );
};

///////////////////////////////////////////////////////////////////////////////

class GedItemNode : public GedObject
{
	RttiDeclareAbstract(GedItemNode,GedObject);
	const reflect::IObjectProperty*		mOrkProp;
	ork::Object*						mOrkObj;
	static GedSkin*						gpSkin0;
	static GedSkin*						gpSkin1;
	static int							giSkin;

	virtual void DoDraw( lev2::GfxTarget* pTARG ) = 0;

protected:

	int									miX, miY;
	int									miW, miH;
	bool								mbVisible;
	std::string							mLabel;
	bool								mbInvalid;

public:

	typedef ork::ArrayString<kmaxgedstring> NameType;

	int get_charh() const;
	int get_charw() const;
	int get_text_center_y() const;
	
	///////////////////////////////////////////////////

	GedItemNode(	ObjModel& mdl,
					const char* name,
					const reflect::IObjectProperty* prop,
					ork::Object* obj );

	///////////////////////////////////////////////////

	~GedItemNode() override;

	void SetVisible( bool bv ) { mbVisible=bv; }
	bool IsVisible() const { return mbVisible; }

	void SetXY( int ix, int iy ) { miX=ix; miY=iy; }
	void SetWH( int iw, int ih ) { miW=iw; miH=ih; }
	int GetX() const { return miX; }
	int GetY() const { return miY; }

	void DestroyChildren();

	///////////////////////////////////////////////////

	void SigInvalidateProperty();

	void Init();

	int height() const { return micalch; }
	int width() const { return miW; }
	
	virtual void Layout( int ix, int iy, int iw, int ih );
	virtual bool CanSideBySide() const { return false; }
	virtual bool DoDrawDefault() const; // { return true; }
	virtual void Invalidate() { mbInvalid=true; }
	virtual void ReSync() {}
	///////////////////////////////////////////////////
	void SetName( const char* name );
	///////////////////////////////////////////////////
	void Draw( lev2::GfxTarget* pTARG );
	///////////////////////////////////////////////////
	int GetLabelWidth() const;
	int GetNameWidth() const;
	int LabelCenterX() const { return miX+(miW>>1)-(GetLabelWidth()>>1); }
	int NameCenterX() const { return miX+(miW>>1)-(GetNameWidth()>>1); }
	///////////////////////////////////////////////////
	void AddItem( GedItemNode*w ); 
	GedItemNode* GetItem( int idx ) const;
	int GetNumItems() const;
	GedItemNode* GetParent() const { return mParent; }
	///////////////////////////////////////////////////
	virtual int CalcHeight(void);
	GedItemNode* GetChildContainer() { return this; }
	///////////////////////////////////////////////////
	void SetOrkProp( const reflect::IObjectProperty* prop ) { mOrkProp=prop; }
	const reflect::IObjectProperty* GetOrkProp() const { return mOrkProp; }
	///////////////////////////////////////////////////
	void SetOrkObj( ork::Object* obj ) { mOrkObj=obj; }
	ork::Object* GetOrkObj() const { return mOrkObj; }
	///////////////////////////////////////////////////
	GedWidget* GetGedWidget() const { return mRoot; }
	///////////////////////////////////////////////////
	bool IsObjectHilighted( const GedObject* pobj ) const;
	///////////////////////////////////////////////////
	GedSkin* GetSkin() const;
	bool							mbcollapsed;
	ork::ArrayString<kmaxgedstring>	mvalue;
	orkvector<GedItemNode*>			mItems;
	GedItemNode*					mParent;
	orkmap<std::string,std::string>	mTags;
	GedWidget*						mRoot;
	int								micalch;
	ArrayString<kmaxgedstring>		mName;
	///////////////////////////////////////////////////
	ObjModel&					mModel;

};

///////////////////////////////////////////////////////////////////////////////

class GedLabelNode : public GedItemNode
{
public:

	///////////////////////////////////////////////////

	GedLabelNode(	ObjModel& mdl,
					const char* name,
					const reflect::IObjectProperty* prop,
					ork::Object* obj ) : GedItemNode( mdl, name, prop, obj ) {}

private:

	virtual void DoDraw( lev2::GfxTarget* pTARG );

};

///////////////////////////////////////////////////////////////////////////////
class GedRootNode : public GedItemNode
{
	virtual void DoDraw( lev2::GfxTarget* pTARG );
	virtual void Layout( int ix, int iy, int iw, int ih );
	virtual int CalcHeight(void);

public:
	GedRootNode(	ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	bool DoDrawDefault() const { return false; } // virtual 
};
///////////////////////////////////////////////////////////////////////////////
class GedGroupNode : public GedItemNode
{
	virtual void DoDraw( lev2::GfxTarget* pTARG );
	bool	mbCollapsed;
	void OnMouseDoubleClicked(const ork::ui::Event& ev) final;
	ork::file::Path::NameType	mPersistID;

public:
	
	GedGroupNode(	ObjModel& mdl, 
					const char* name,
					const reflect::IObjectProperty* prop,
					ork::Object* obj,
					bool is_obj_node=false );

	void CheckVis();
	bool DoDrawDefault() const { return false; } // virtual 
	bool mIsObjNode;
};
///////////////////////////////////////////////////////////////////////////////
class GedVP;

class GedWidget : public ork::AutoConnector
{
	RttiDeclareAbstract(GedWidget,ork::AutoConnector);

	static const int kdim = 8;

	GedItemNode*				mRootItem;
	int							miW;
	int							miH;
	ork::Object*				mRootObject;
	ObjModel&					mModel;
	std::deque<GedItemNode*>	mItemStack;
	int							miRootH;
	GedVP*						mViewport;
	U64							mStackHash;
	orkvector<GedSkin*>			mSkins;
	int							miSkin;
	bool						mbDeleteModel;

	void ComputeStackHash(); 

	//////////////////////////////////////////////////////////////

	DeclarePublicSignal( Repaint );
	DeclarePublicAutoSlot( Repaint );
	DeclarePublicAutoSlot( ModelInvalidated );

	void						SlotRepaint();
	void						SlotModelInvalidated();

	//////////////////////////////////////////////////////////////

public:

	void SetDeleteModel( bool bv ) { mbDeleteModel=true; }

	void PropertyInvalidated( ork::Object* pobj, const reflect::IObjectProperty* prop );

	GedWidget( ObjModel& model );
	~GedWidget();
	void Attach( ork::Object* obj );

	void Draw( lev2::GfxTarget* pTARG, int iw, int ih, int iscrolly );

	ObjModel& GetModel() { return mModel; }

	GedItemNode* GetRootItem() const { return mRootItem; }

	void IncrementSkin();
	GedItemNode* ParentItemNode() const;
	void PushItemNode(GedItemNode*qw);
	void PopItemNode(GedItemNode*qw);
	void AddChild(GedItemNode*pw);
	void DoResize();
	void OnSelectionChanged();
	int GetStackDepth() const { return int( mItemStack.size() ); }
	int GetRootHeight() const { return miRootH; }

	void SetViewport( GedVP* pvp ) { mViewport=pvp; }
	U64 GetStackHash() const; 

	GedVP* GetViewport() const { return mViewport; }
	GedSkin* GetSkin();
	void AddSkin( GedSkin* psk);
	void SetDims( int iw, int ih );

};
///////////////////////////////////////////////////////////////////////////////
class GedVP : public ui::Surface
{
public:
	
	friend class lev2::CPickBuffer<GedVP>;

    uint32_t AssignPickId(GedObject*pobj);
	GedWidget& GetGedWidget() { return mWidget; }
	GedVP( const std::string & name, ObjModel& model );
	~GedVP();

	void ResetScroll() { miScrollY=0; }

	const GedObject* GetMouseOverNode() const { return mpMouseOverNode; }

	static orkset<GedVP*> gAllViewports;
	void SetDims( int iw, int ih );

private:

	void DoRePaintSurface(ui::DrawEvent& drwev) override;
	void DoSurfaceResize() override;
	ui::HandlerResult DoOnUiEvent( const ui::Event& EV ) override;
	void DoInit( lev2::GfxTarget* pt ) override;
	
	ObjModel&						mModel;
	GedWidget						mWidget;
	GedObject*						mpActiveNode;
	int								miScrollY;
	const GedObject*				mpMouseOverNode;

};
///////////////////////////////////////////////////////////////////////////////
class GedTextEdit : public QLineEdit
{
    Q_OBJECT

public:

	GedTextEdit( QWidget* parent );
	void focusOutEvent( QFocusEvent* pev ) final; // virtual
	void keyPressEvent ( QKeyEvent * pev ) final; // virtual
	void SetText( const char* ptext );

signals:
    void editFinished();
    void canceled();

};
class GedInputDialog : public QDialog
{
    Q_OBJECT
public:

	static QString getText( const ork::ui::Event& ev, GedItemNode* pnode, const char* defstr, int ix, int iy, int iw, int ih );
	bool WasChanged() const { return mbChanged; }
    GedInputDialog();
    QString GetResult();
    void clear() { mTextEdit.clear(); }
    GedTextEdit mTextEdit;
    QString     mResult;
    bool        mbChanged;
public slots:
    void done( );
    void canceled( );
    void textChanged(QString str);

};
///////////////////////////////////////////////////////////////////////////////
} } }
///////////////////////////////////////////////////////////////////////////////
