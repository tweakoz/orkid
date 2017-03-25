////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/orkconfig.h>

#pragma once 

#if defined( ORK_CONFIG_QT )

///////////////////////////////////////////////////////////////////////////////
#include <ork/lev2/gfx/gfxenv.h>
///////////////////////////////////////////////////////////////////////////////

extern int QtTest( int argc, char **argv );
#define register 

///////////////////////////////////////////////////////////////////////////////
#include <QtCore/QMetaObject>
#include <QtCore/qmetatype.h>
#include <QtCore/qdatastream.h>
#include <QtCore/QMetaMethod>
#include <QtCore/QSize>
#include <QtCore/QTimer>
#include <QtGui/qapplication.h>
#include <QtGui/qpushbutton.h>
#include <QtGui/qfont.h>
#include <QtGui/qmainwindow.h>
#include <QtGui/qmenu.h>
#include <QtGui/qmenubar.h>
#include <QtGui/qdockwidget.h>
#include <QtGui/qlistwidget.h>
#include <QtGui/qtoolbar.h>
#include <QtGui/qgroupbox.h>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtGui/QStyleFactory>
#include <QtGui/QCheckBox>
#include <QtGui/QWidget>
#include <QtGui/qshortcut.h>
#include <QtGui/QSplashScreen>
#include <QtGui/QInputDialog>

#if defined( IX )
#include <QtGui/QTreeView>
#include <QtGui/QListView>
#include <QtGui/QDirModel>
#include <Qt3Support/Q3HBox>
#include <QtCore/QAbstractItemModel>
#include <QtGui/QStandardItemModel>
#include <QtGui/QItemDelegate>
#include <QtGui/QItemEditorFactory>
#include <QtGui/QFileDialog>
#include <QtGui/QSpinBox>
#include <QtGui/QComboBox>
#include <QtGui/QHBoxLayout>
#include <QtGui/QTextEdit>
#include <QtGui/QLineEdit>
#else
#include <QtGui/QTreeView.h>
#include <QtGui/QListView.h>
#include <QtGui/QDirModel.h>
//#include <Qt/Q3HBox.h>
#include <QtCore/QAbstractItemModel.h>
#include <QtGui/QStandardItemModel.h>
#include <QtGui/QItemDelegate.h>
#include <QtGui/QItemEditorFactory.h>
#include <QtGui/QFileDialog.h>
#include <QtGui/QSpinBox.h>
#include <QtGui/QComboBox.h>
#include <QtGui/QHBoxLayout>
#include <QtGui/QTextEdit.h>
#include <QtGui/QTextBrowser.h>
#include <QtGui/QLabel.h>
#include <QtGui/QLineEdit.h>
#endif

#undef register 

#include <ork/lev2/ui/event.h>

#include <ork/lev2/gfx/ctxbase.h>

///////////////////////////////////////////////////////////////////////////////

struct SmtFinger;
typedef struct SmtFinger MtFinger;

namespace ork { 
	
std::string TypeIdNameStrip( const char * name );
std::string TypeIdName(const std::type_info*ti);

namespace lev2
{

///////////////////////////////////////////////////////////////////////////////

class CTQT;

typedef void* TouchRecieverIMPL;

class QCtxWidget : public QWidget
{
	friend class CTQT;

protected:

	QTimer	mQtTimer;
	bool	mbSignalConnected;
	CTQT*	mpCtxBase;
	bool	mbEnabled;
	int		miWidth;
	int		miHeight;
	TouchRecieverIMPL	mTouchReciver;

public:
	
	void Enable() { mbEnabled=true; }
	bool IsEnabled() const { return mbEnabled; }

    virtual bool event(QEvent* event);
	
	void OnTouchBegin( const MtFinger* pfinger );
	void OnTouchUpdate( const MtFinger* pfinger );
	void OnTouchEnd( const MtFinger* pfinger );

	virtual void focusInEvent ( QFocusEvent * event );
	virtual void focusOutEvent ( QFocusEvent * event );
	virtual void showEvent ( QShowEvent * event );

	virtual void mouseMoveEvent ( QMouseEvent * event );
	virtual void mousePressEvent ( QMouseEvent * event );
	virtual void mouseReleaseEvent ( QMouseEvent * event );
	virtual void mouseDoubleClickEvent( QMouseEvent * event );

	virtual void keyPressEvent ( QKeyEvent * event );
	virtual void keyReleaseEvent ( QKeyEvent * event );
	virtual void wheelEvent ( QWheelEvent * event );
	virtual void resizeEvent ( QResizeEvent * event );
	virtual void paintEvent ( QPaintEvent * event );

	void MouseEventCommon( QMouseEvent * event );

	const ui::Event& UIEvent() const;
	ui::Event& UIEvent();
	GfxTarget*	Target() const;
	GfxWindow* GetGfxWindow() const;
	bool AlwaysRun() const;

	QPaintEngine * paintEngine () const { return 0; } // virtual

	QCtxWidget( CTQT* pctxbase, QWidget* parent );
	~QCtxWidget();
    
	ui::MultiTouchPoint mMultiTouchTrackPoints[ui::Event::kmaxmtpoints];


private:

	void SendOrkUiEvent();
	
};

class CTQT : public CTXBASE 
{
	friend class QCtxWidget;

	bool			mbAlwaysRun;
	int				miUserMillis;
	int				miQtMillis;
	int				miLastMillis;
	QCtxWidget*		mpQtWidget;
	int				mix, miy, miw, mih;
	QWidget*		mParent;
	int				mDrawLock;

    void SlotRepaint() final; 

public:

    void Show() final;
    void Hide() final;
    void SetRefreshPolicy( ERefreshPolicy epolicy ) final;
    void SetRefreshRate( int ihz ) final;

	QTimer& Timer() const;
	CTQT( GfxWindow* pwin, QWidget* parent=0 );
	~CTQT();


    void Resize(int X, int Y, int W, int H);
	void SetParent( QWidget* pw );
	void SetAlwaysRun( bool brun ) {  mbAlwaysRun=brun; }
	void* winId() const { return (void*)mpQtWidget->winId(); }
	QCtxWidget* GetQWidget() const { return mpQtWidget; }
	QWidget* GetParent() const { return mParent; }

	CVector2 MapCoordToGlobal( const CVector2& v ) const override;

};

///////////////////////////////////////////////////////////////////////////////

class CQtGfxWindow : public ork::lev2::GfxWindow
{
	public:

	bool			mbinit;
	ui::Widget*	    mRootWidget;
	
	CQtGfxWindow( ui::Widget* root_widget );
	~CQtGfxWindow();

	virtual void Draw( void );
	//virtual void Show( void );
	virtual void GotFocus( void );
	virtual void LostFocus( void );
	virtual void Hide( void ) {	}
	/*virtual*/ void OnShow();
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
// we no like the MOC so fake the MOC with templates

struct MocFunctorBase
{
	std::string	mMethodName;
	std::string	mMethodType;

	MocFunctorBase( const std::string & name, const std::string & type )
		: mMethodName( name )
		, mMethodType( type )
	{

	}

	virtual void Invoke( void *pthis, int _id, void **_a ) = 0;
	virtual const char *Params( void ) = 0;

};

class CQNoMocBase
{
	///////////////////////////
	// these may need updating with new versions of QT (from qmetaobject.cpp)
	///////////////////////////

	struct QMetaObjectPrivate
	{
		int revision;
		int className;
		int classInfoCount, classInfoData;
		int methodCount, methodData;
		int propertyCount, propertyData;
		int enumeratorCount, enumeratorData;
	};

	enum MethodFlags  {
		AccessPrivate = 0x00,
		AccessProtected = 0x01,
		AccessPublic = 0x02,
		AccessMask = 0x03, //mask

		MethodMethod = 0x00,
		MethodSignal = 0x04,
		MethodSlot = 0x08,
		MethodTypeMask = 0x0c,

		MethodCompatibility = 0x10,
		MethodCloned = 0x20,
		MethodScriptable = 0x40
	};

	CQNoMocBase*						mPrevMocBase;
	
	static CQNoMocBase*& GetPreMainMocBaseIter();

	void MocInit( void ); 
	void Compile( void );
	void Link( void );

public: //

	typedef void(*MocInitFunc)(void);

	QMetaObject*						staticMetaObject;
	const uint*							mpQtMetaData;
	const std::string					mClassName;
	//CStringTable						mStringTable;
	const char*							mStringBlock;
	int									mStringBlockLen;
	int									mClassVersion;
	std::string							mMocString;
	orkvector<uint>						mMocUInt;
	orkvector<MocFunctorBase*>			mSlots;
	orkvector<MocFunctorBase*>			mSignals;
	QMetaObjectPrivate					mMocPrivate;
	int									miNumParentMethods;
	MocInitFunc							mInitFunc;

	static void MocInitAll();

	CQNoMocBase( const std::string &classname, MocInitFunc mfunc );

	CQNoMocBase* GetPreviousMocBase() const { return mPrevMocBase; }

	virtual int Invoke( void *pthis, int _id, void **_a ) = 0;

	virtual const QMetaObject *GetParentMeta( void ) = 0;
	virtual const QMetaObject *GetThisMeta( void ) const = 0;
	virtual       QMetaObject *GetThisMeta( void ) = 0;

};

template<typename T, typename P> class CQNoMoc : public CQNoMocBase
{
public:

	///////////////////////

	CQNoMoc( MocInitFunc initfunc )
		: CQNoMocBase( TypeIdNameStrip( typeid(T).name() ), initfunc )
	{
	}

	//typedef void(T::*MethodType)(R);

	template<typename S> void AddSlot0( const char *pname, S method );
	template<typename S> void AddSignal0( const char *pname, S method );
	template<typename R> void AddSlot1( const char *pname, void(T::*method)(R) );
	template<typename R> void AddSignal1( const char *pname, void(T::*method)(R) );
	template<typename R0, typename R1> void AddSlot2( const char *pname, const char* psig, void(T::*method)(R0,R1) );
	template<typename R0, typename R1> void AddSignal2( const char *pname, const char* psig, void(T::*method)(R0,R1) );

	virtual const QMetaObject * GetParentMeta( void )
	{
		return & P::staticMetaObject;
	}
	virtual const QMetaObject *GetThisMeta( void ) const
	{
		return & T::staticMetaObject;
	}
	virtual QMetaObject *GetThisMeta( void )
	{
		return & T::staticMetaObject;
	}
	virtual int Invoke( void *pthis, int _id, void **_a )
	{
		if( _id < int(mSignals.size()) )
		{
			MocFunctorBase* psig = mSignals[ _id ];
			psig->Invoke( pthis, _id, _a );
			_id = -1;
		}
		else
		{
			size_t slotid = _id - mSignals.size();
			//OrkAssert(  );
			if( size_t(slotid) < mSlots.size() )
			{
				MocFunctorBase* pslot = mSlots[ slotid ];
				pslot->Invoke( pthis, _id, _a );
				_id = -1;
			}
			else
			{
				_id = int(slotid-mSlots.size());
			}
		}
		return _id;
	}
};

///////////////////////////////////////////////////////////////////////////////

template <typename Subclass, typename Baseclass> class MocImp //: public Baseclass
{
public:
	//static void *qt_metacast(Subclass*pobj,const char *_clname);
	//static const QMetaObject *metaObject(Subclass*pobj);
	//static int qt_metacall( Subclass*pobj,QMetaObject::Call _c, int _id, void **_a );

	CQNoMoc<Subclass,Baseclass> mMoc;

	MocImp( CQNoMocBase::MocInitFunc initf ) : mMoc( initf ) {}

	//template <typename Subclass, typename Baseclass>
	void* qt_metacast(Subclass*pobj,const char *_clname)
	{	//return pobj->Baseclass::qt_metacast(_clname);
		if (!_clname) return 0;
		if (!strcmp(_clname, mMoc.mClassName.c_str()))
			return static_cast<void*>(const_cast<Subclass*>(pobj));
		return pobj->QObject::qt_metacast(_clname);
	}
	//template <typename Subclass, typename Baseclass>
	const QMetaObject *metaObject()
	{
		return mMoc.staticMetaObject;
	}
	//template <typename Subclass, typename Baseclass>
	int qt_metacall( Subclass*pobj,QMetaObject::Call _c, int _id, void **_a )
	{	//return pobj->Baseclass::qt_metacall(_c,_id,_a);
		_id = pobj->Baseclass::qt_metacall(_c, _id, _a);
		if (_id < 0)
		{	return _id;
		}
		if (_c == QMetaObject::InvokeMetaMethod)
		{	//printf( "qt_metacall (obj %08x) (_id %d)\n", this, _id );
			_id = mMoc.Invoke( (void *) pobj, _id, _a );
		}
		return _id;
	}
	//////////////////////////////////////
	template<typename S> void AddSlot0( const char *pname, S method )
	{
		mMoc.AddSlot0(pname,method);
	}
	template<typename S> void AddSignal0( const char *pname, S method )
	{
		mMoc.AddSignal0(pname,method);
	}
	//////////////////////////////////////
	template<typename R> void AddSlot1( const char *pname, void(Subclass::*method)(R) )
	{
		mMoc.AddSlot1(pname,method);
	}
	template<typename R> void AddSignal1( const char *pname, void(Subclass::*method)(R) )
	{
		mMoc.AddSignal1(pname,method);
	}
	//////////////////////////////////////
	template<typename R0,typename R1> void AddSlot2( const char *pname, const char* psig, void(Subclass::*method)(R0,R1) )
	{
		mMoc.AddSlot2(pname,psig,method);
	}
	template<typename R0,typename R1> void AddSignal2( const char *pname, const char* psig, void(Subclass::*method)(R0,R1) )
	{
		mMoc.AddSignal2(pname,psig,method);
	}
	//////////////////////////////////////

public:
	template<typename R> void Emit( Subclass*pobj, const char *pname, const R& val );
	void Emit( Subclass*pobj, const char *pname );
	int GetSlotIndex( const char *pname );
	int GetSignalIndex( const char *pname );
};

//template<typename x, typename y> CQNoMoc<x,y> MocImp< x, y >::Moc;

#define MOC_2_ARG__(X,Y) X,Y

#define DeclareMoc(Sub,Base)\
protected:\
friend class lev2::MocImp<Sub, Base>;\
static lev2::MocImp<Sub, Base> Moc;\
void *qt_metacast(const char *_clname) override { return Moc.qt_metacast(this,_clname); }\
const QMetaObject *metaObject() const override { return Moc.metaObject(); }\
int qt_metacall( QMetaObject::Call _c, int _id, void **_a ) override { return Moc.qt_metacall(this,_c,_id,_a); }\
public:\
static QMetaObject	staticMetaObject;\
static void MocInit();

#define ImplementMoc(Sub,Base)\
lev2::MocImp<Sub, Base> Sub::Moc( Sub::MocInit );\
QMetaObject Sub::staticMetaObject;

///////////////////////////////////////////////////////////////////////////////

} // namespace tool
} // namespace ork

///////////////////////////////////////////////////////////////////////////////

#endif // ORK_CONFIG_QT
