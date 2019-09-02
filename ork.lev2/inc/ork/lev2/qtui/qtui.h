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
#include <QtWidgets/QApplication>
#include <QtCore/QMetaObject>
#include <QtCore/qmetatype.h>
#include <QtCore/qdatastream.h>
#include <QtCore/QMetaMethod>
#include <QtCore/QSize>
#include <QtCore/QTimer>
#include <QtGui/QResizeEvent>
#include <QtGui/QShowEvent>
#include <QtGui/QPaintEvent>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QWidget>
#include <QtWidgets/QSplashScreen>
#include <QtWidgets/QInputDialog>
#include <QtWidgets/QFileDialog>
#include <QtWidgets/QOpenGLWidget>

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

	fvec2 MapCoordToGlobal( const fvec2& v ) const override;

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

} // namespace tool
} // namespace ork

///////////////////////////////////////////////////////////////////////////////

#endif // ORK_CONFIG_QT
