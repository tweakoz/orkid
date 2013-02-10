////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once 

#include <ork/reflect/IObjectMapPropertyType.h>
#include <ork/kernel/core_interface.h>
#include <orktool/toolcore/choiceman.h>

namespace ork { namespace dataflow { class outplugbase; } }
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
class SliderBase 
{
public:

	virtual void resize( int ix, int iy, int iw, int ih ) = 0;
	virtual void mouseMoveEvent ( QMouseEvent * pEV ) = 0;
	virtual void mouseReleaseEvent ( QMouseEvent * pEV ) = 0;
	virtual void mouseDoubleClickEvent ( QMouseEvent * pEV ) =0;

	void SetLogMode( bool bv ) { mlogmode=bv; }
	float GetIndicPos() const { return mfIndicPos; }
	float GetTextPos() const { return mfTextPos; }
	void SetIndicPos( float fi ) { mfIndicPos=fi; }
	void SetTextPos( float ti ) { mfTextPos=ti; }
	PropTypeString& ValString() { return mValStr; }
	void SetLabelH(int ilabh) { miLabelH=ilabh; }

protected:

	float					mfx;
	float					mfw;
	float					mfh;
	int						miLabelH;
	bool					mlogmode;
	float					mfTextPos;
	float					mfIndicPos;
	PropTypeString			mValStr;
	SliderBase();
};
///////////////////////////////////////////////////////////////////////////////
template <typename T> 
class Slider : public SliderBase
{
public:

	typedef typename T::datatype		datatype;

	Slider( T& ParentW, datatype min, datatype max, datatype def );

	/*virtual*/ void mouseMoveEvent ( QMouseEvent * event );
	/*virtual*/ void mouseReleaseEvent ( QMouseEvent * event ) ;
	/*virtual*/ void mouseDoubleClickEvent ( QMouseEvent * event );

	/*virtual*/ void resize(int ix, int iy, int iw, int ih);
	void SetVal( datatype val );
	void Refresh();

	void SetMinMax( datatype min, datatype max )
	{
		mmin = min;
		mmax = max;
	}

private:

	T&							mParent;
	datatype					mval;
	datatype					mmin;
	datatype					mmax;
	bool						mbUpdateOnDrag;

	float LogToVal( float logval ) const;
	float ValToLog( float val ) const;
	float LinToVal( float linval ) const;
	float ValToLin( float val ) const;
};

///////////////////////////////////////////////////////////////////////////////
template <typename Setter> class GedBoolNode : public GedItemNode
{
public:
	typedef bool datatype;
	Setter		mSetter;
	GedBoolNode(ObjModel &mdl, const char *name, const reflect::IObjectProperty *prop, ork::Object *obj);
	/*virtual*/ void mouseReleaseEvent(QMouseEvent *pEV);
	/*virtual*/ void mouseDoubleClickEvent(QMouseEvent *pEV);
	/*virtual*/ void DoDraw(lev2::GfxTarget *pTARG);
};
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> class GedFloatNode : public GedItemNode
{
public:
	bool		mLogMode;
	GedFloatNode(ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	/*virtual*/ void mouseMoveEvent( QMouseEvent * pEV );
	/*virtual*/ void mouseDoubleClickEvent ( QMouseEvent * event );
	/*virtual*/ void mouseReleaseEvent ( QMouseEvent * event );
	/*virtual*/ void DoDraw( lev2::GfxTarget* pTARG );
	typedef float datatype;
	void ReSync(); // virtual 
	Slider<GedFloatNode>* GetSlider() { return slider; }
	IODriver& RefIODriver() { return mIoDriver; }
private:
	Slider<GedFloatNode>* slider;
	IODriver	mIoDriver;
};
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> class GedIntNode : public GedItemNode
{
public:
	bool		mLogMode;
	GedIntNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	/*virtual*/ void mouseMoveEvent( QMouseEvent * pEV );
	/*virtual*/ void mouseDoubleClickEvent ( QMouseEvent * event );
	/*virtual*/ void mouseReleaseEvent ( QMouseEvent * event );
	/*virtual*/ void DoDraw( lev2::GfxTarget* pTARG );
	typedef int datatype;
	void ReSync(); // virtual
	IODriver& RefIODriver() { return mIoDriver; }
private:
	SliderBase* slider;
	IODriver	mIoDriver;
};
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver,typename T> class GedSimpleNode : public GedItemNode
{
public:
	GedSimpleNode(ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	/*virtual*/ void mouseDoubleClickEvent ( QMouseEvent * event );
	/*virtual*/ void DoDraw( lev2::GfxTarget* pTARG );
	IODriver& RefIODriver() { return mIoDriver; }
private:
	IODriver	mIoDriver;
};
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
template <typename Setter> 
class GedObjNode : public GedItemNode
{
public:
	GedObjNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	void OnCreateObject();
	void mouseDoubleClickEvent ( QMouseEvent * event ); // virtual
	void DoDraw( lev2::GfxTarget* pTARG ); // virtual
private:
	Setter		mSetter;
	bool		mbInteractive;
	bool		mbCollapse;
};
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
struct KeyDecoName
{
	PropTypeString			mActualKey;
	int						miMultiIndex;

	PropTypeString			DecoratedName() const;

	KeyDecoName( const char* pkey, int imulti ); // decomposed name/index
	KeyDecoName( const char* pkey ); // precomposed name/index
};

///////////////////////////////////////////////////////////////////////////////
class GedMapNode : public GedItemNode
{
	RttiDeclareAbstract(GedMapNode,GedItemNode);

public:
	GedMapNode(	ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	const orkmap<PropTypeString,KeyDecoName>& GetKeys() const { return mMapKeys; }
	void FocusItem( const PropTypeString& key );

	bool IsKeyPresent( const KeyDecoName& pkey ) const;
	void AddKey( const KeyDecoName& pkey );

	bool IsMultiMap() const { return mbIsMultiMap; }

private:

	const reflect::IObjectMapProperty*  mMapProp;
	orkmap<PropTypeString,KeyDecoName>	mMapKeys;
	GedItemNode*						mKeyNode;
	PropTypeString						mCurrentKey;
	int									mItemIndex;
	bool								mbSingle;
	bool								mbConst;
	bool								mbIsMultiMap;
	bool								mbImpExp;

	virtual void mouseDoubleClickEvent ( QMouseEvent * pEV );
	void CheckVis();
	virtual void DoDraw( lev2::GfxTarget* pTARG );

	void AddItem(QMouseEvent * pEV);
	void RemoveItem(QMouseEvent * pEV);
	void MoveItem(QMouseEvent * pEV);
	void DuplicateItem(QMouseEvent * pEV);
	void ImportItem(QMouseEvent * pEV);
	void ExportItem(QMouseEvent * pEV);

	bool DoDrawDefault() const { return false; } // virtual 
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class GedFactoryEnum : public GedFactory
{
	RttiDeclareConcrete(GedFactoryEnum,GedFactory);
public:
		virtual GedItemNode* CreateItemNode(	ObjModel&mdl,
												const ConstString& Name,
												const reflect::IObjectProperty *prop,
												Object* obj) const;
};
///////////////////////////////////////////////////////////////////////////////
class GedFactory_PlugFloat : public GedFactory
{
	RttiDeclareConcrete(GedFactory_PlugFloat,GedFactory);
public:
	GedItemNode* CreateItemNode(ObjModel&mdl,const ConstString& Name,const reflect::IObjectProperty *prop,Object* obj) const;
	virtual void Recurse(ObjModel& mdl, const reflect::IObjectProperty *prop,ork::Object*pobj) const;
	GedFactory_PlugFloat();
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryOutliner : public GedFactory
{
	RttiDeclareConcrete(GedFactoryOutliner,GedFactory);
public:
	GedItemNode* CreateItemNode(	ObjModel&mdl,
									const ConstString& Name,
									const reflect::IObjectProperty *prop,
									Object* obj ) const;
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryGradient : public GedFactory
{
	RttiDeclareConcrete(GedFactoryGradient,GedFactory);
public:
	GedItemNode* CreateItemNode(	ObjModel&mdl,
									const ConstString& Name,
									const reflect::IObjectProperty *prop,
									Object* obj ) const;
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryCurve : public GedFactory
{
	RttiDeclareConcrete(GedFactoryCurve,GedFactory);
public:
	GedItemNode* CreateItemNode(	ObjModel&mdl,
									const ConstString& Name,
									const reflect::IObjectProperty *prop,
									Object* obj ) const;
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryAssetList : public GedFactory
{
	RttiDeclareConcrete(GedFactoryAssetList,GedFactory);
public:
	GedItemNode* CreateItemNode(	ObjModel&mdl,
										const ConstString& Name,
										const reflect::IObjectProperty *prop,
										Object* obj ) const;
};
///////////////////////////////////////////////////////////////////////////////
class GedFactoryFileList : public GedFactory
{
	RttiDeclareConcrete(GedFactoryFileList,GedFactory);
public:
	GedItemNode* CreateItemNode(	ObjModel&mdl,
									const ConstString& Name,
									const reflect::IObjectProperty *prop,
									Object* obj ) const;
};
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> 
class GedAssetNode : public GedItemNode
{
	DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(GedAssetNode<IODriver>,GedItemNode);

public:
	GedAssetNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	void OnCreateObject();
	void SetLabel();
	void DoDraw( lev2::GfxTarget* pTARG ); // virtual
	IODriver& RefIODriver() { return mIoDriver; }

private:

	IODriver	mIoDriver;
	/*virtual*/ void mouseDoubleClickEvent ( QMouseEvent * event );
	virtual bool DoDrawDefault() const { return false; }
};
///////////////////////////////////////////////////////////////////////////////
template <typename IODriver> 
class GedFileNode : public GedItemNode
{
	DECLARE_TRANSPARENT_TEMPLATE_ABSTRACT_RTTI(GedFileNode<IODriver>,GedItemNode);

public:
	GedFileNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	void OnCreateObject();
	void SetLabel();
	void DoDraw( lev2::GfxTarget* pTARG ); // virtual
	IODriver& RefIODriver() { return mIoDriver; }

private:

	IODriver		mIoDriver;
	/*virtual*/ void mouseDoubleClickEvent( QMouseEvent * event );
	virtual bool DoDrawDefault() const { return false; }
};
///////////////////////////////////////////////////////////////////////////////
class UserChoices : public tool::CChoiceList
{
	IUserChoiceDelegate&								mucd;
	orkmap<PoolString,IUserChoiceDelegate::ValueType>	mUserChoices;

public:

	virtual void EnumerateChoices( bool bforcenocache=false );
	UserChoices( IUserChoiceDelegate& ucd , ork::Object* pobj, ork::Object* puserobj );
};
///////////////////////////////////////////////////////////////////////////////

class IPlugChoiceDelegate : public Object
{
	RttiDeclareAbstract( IPlugChoiceDelegate, Object );

public:

	typedef orkmap<std::string,ork::dataflow::outplugbase*> OutPlugMapType;
	
	virtual void EnumerateChoices( GedItemNode* pnode, OutPlugMapType& Choices ) = 0;

};

class IOpsDelegate;

struct OpsTask
{
	IOpsDelegate*	mpDelegate;
	ork::Object*	mpTarget;

	OpsTask() : mpDelegate(0), mpTarget(0) {}
};

class IOpsDelegate : public Object
{
	RttiDeclareAbstract( IOpsDelegate, Object );

	typedef orkvector<OpsTask*> TaskList;
	static LockedResource<TaskList> gCurrentTasks;

	float	mfProgress;

public:
	virtual void Execute( ork::Object* ptarget ) = 0;
	float GetProgress() const { return mfProgress; }
	void SetProgress( float fv ) { mfProgress=fv; }

	static void AddTask			( ork::object::ObjectClass* pdelegclass, ork::Object* ptarget );
	static void RemoveTask		( ork::object::ObjectClass* pdelegclass, ork::Object* ptarget );
	static OpsTask* GetTask		( ork::object::ObjectClass* pdelegclass, ork::Object* ptarget );

protected:
	IOpsDelegate() : mfProgress(0.0f) {}
};
class OpsNode : public GedItemNode
{
	void DoDraw( lev2::GfxTarget* pTARG ); // virtual
	void mouseDoubleClickEvent ( QMouseEvent * pEV );
	orkvector< std::pair< std::string,ork::object::ObjectClass*> > mOps;	

	//IOpsDelegate* mpCurrentDelegate;
public:
	OpsNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj );
	virtual bool DoDrawDefault() const { return false; }
};
class ObjectImportDelegate : public IOpsDelegate
{
	RttiDeclareConcrete( ObjectImportDelegate, tool::ged::IOpsDelegate );
	void Execute( ork::Object* ptarget ); // virtual
};
class ObjectExportDelegate : public IOpsDelegate
{
	RttiDeclareConcrete( ObjectExportDelegate, tool::ged::IOpsDelegate );
	void Execute( ork::Object* ptarget ); // virtual
};
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class GedTextEdit : public QLineEdit
{
	//DeclareMoc( GedTextEdit, QLineEdit );

	Q_OBJECT

public:

	GedTextEdit( QWidget* parent );
	void focusOutEvent( QFocusEvent* pev ); // virtual
	void keyPressEvent ( QKeyEvent * pev ); // virtual
	void SigEditFinished();
	void SigCanceled();
	void SetText( const char* ptext );

};
class GedInputDialog : public QDialog
{
	Q_OBJECT
	//DeclareMoc( GedInputDialog, QDialog );

	GedTextEdit	mTextEdit;
	QString		mResult;
	bool		mbChanged;

	GedInputDialog();
	void done( );
	void canceled( );
	void SlotTextChanged(QString str);
	QString GetResult();
	void clear() { mTextEdit.clear(); }

public:

	static QString getText( QMouseEvent* pev, GedItemNode* pnode, const char* defstr, int ix, int iy, int iw, int ih );

};
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void EnumerateFactories( const ork::Object* pdestobj, const reflect::IObjectProperty* prop,  orkset<object::ObjectClass*>& FactoryClassVect );
void EnumerateFactories( object::ObjectClass* pbaseclass, orkset<object::ObjectClass*>& FactoryClassVect );
object::ObjectClass* FactoryMenu( orkset<object::ObjectClass*>& FactoryClasses );
bool DeserializeInPlace(reflect::IDeserializer &deserializer, rtti::ICastable *value);
QMenu *CreateFactoryMenu( const orkset<object::ObjectClass*>& FactoryClassVect );

} } }
