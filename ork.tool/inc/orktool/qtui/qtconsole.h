////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <QtCore/qobject.h>
#include <QtCore/qstringlist.h>
#include <QtGui/QMouseEvent>
#include <QtGui/QKeyEvent>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxvtxbuf.h>
#include <ork/lev2/gfx/gfxmaterial_test.h>
#include <ork/lev2/ui/viewport.h>
#include <ork/lev2/qtui/qtui.h>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QDockWidget>

class QTextStream;
class QSocketNotifier;

namespace ork {
namespace tool {

///////////////////////////////////////////////////////////////////////////////

void ConsoleOutput( const std::string & outstr );

///////////////////////////////////////////////////////////////////////////////

struct ConsoleLine
{
	static const int kmaxlinew = 4096;
	char mBuffer[kmaxlinew];
	int mSize;
	void Clear() { mBuffer[0] = 0; mSize=0; }
	ConsoleLine() : mSize(0) { Clear(); }
	void Set(const char*pdata)
	{
		mSize = strlen(pdata);
		strncpy( mBuffer, pdata, kmaxlinew );
	}
};

///////////////////////////////////////////////////////////////////////////////

class vp_cons : public ui::Viewport
{
public:

	vp_cons( const std::string& vname );

	void BindCTQT(lev2::CTQT*ct);

	void AppendOutput( const std::string & outputline );

	static const int kmaxlines = 256;
	typedef fixed_pool<ConsoleLine,kmaxlines> LinePool;

private:

	ui::HandlerResult DoOnUiEvent( const ui::Event& EV ) final;
	void DoDraw(ui::DrawEvent& drwev); // virtual
	lev2::CTQT*							mCTQT;
	ork::lev2::GfxMaterial3DSolid		mBaseMaterial;

	void Register();
	LinePool	mLinePool;
	LinePool	mHistPool;
	int mHistIndex;
	float mPhase0, mPhase1, mPhase2;

	std::list<ConsoleLine*> mLineList;
	std::list<ConsoleLine*> mHistList;
	std::string mInputLine;

};

///////////////////////////////////////////////////////////////////////////////

class QtConsoleWindow : public QWidget
{
    Q_OBJECT

	QTextEdit*		mpConsoleOutputTextEdit;
	QLineEdit *		mpConsoleInputTextEdit;
	QDockWidget *	mpDockWidget;
	QGroupBox*		mpGROUPBOX;

	std::string		mOutputText;
	bool			mbEcho;
	static QtConsoleWindow *gpWindow;

    static const int kmaxlines = 256;
    typedef fixed_pool<ConsoleLine,kmaxlines> LinePool;
    LinePool  mLinePool;
    std::list<ConsoleLine*> mLineList;

public:

	bool EchoOn( void ) const { return mbEcho; }

	QtConsoleWindow( bool bfloat, QWidget *pparent );
	~QtConsoleWindow();

	QDockWidget *GetDockWidget( void );
	QGroupBox*	GroupBox() const { return mpGROUPBOX; }
    void AppendOutput( const std::string & outputline );
    void Register();


public slots:

	void InputDone( void );
	void EchoToggle( void );

};

///////////////////////////////////////////////////////////////////////////////

inline QDockWidget *QtConsoleWindow::GetDockWidget( void )
{
	return mpDockWidget;
}
} }	// namespace ork::tool
