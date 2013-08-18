////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
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
//#include <Qt3Support/q3textedit.h>
//#include <Qt3Support/Q3PopupMenu>

class QTextStream;
class QSocketNotifier;

namespace ork {
namespace tool {

void InitPython();

class Py
{
public:
	static Py& Ctx();
	void Call(const std::string& cmdstr);
private:
	Py();
	~Py();

};
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

private:
	virtual ui::HandlerResult DoOnUiEvent( ui::Event *pEV );
	void DoDraw(ui::DrawEvent& drwev); // virtual
	lev2::CTQT*							mCTQT;
	ork::lev2::GfxMaterial3DSolid		mBaseMaterial;
#if defined(IX)
	void Register();
	//static const int kmaxlines = 256;
	//typedef fixed_pool<ConsoleLine,kmaxlines> LinePool;
	//LinePool	mLinePool;
#endif

	//std::list<ConsoleLine*> mLineList;
	std::vector<std::string> mLines;
};

///////////////////////////////////////////////////////////////////////////////

/*class QtConsoleWindow : public QWidget
{
	DeclareMoc(QtConsoleWindow,QWidget);

	QTextEdit*		mpConsoleOutputTextEdit;
	QLineEdit *		mpConsoleInputTextEdit;
	//QDockWidget *	mpDockWidget;
	QGroupBox*		mpGROUPBOX;
	
	std::string		mOutputText;
	bool			mbEcho;
	static QtConsoleWindow *gpWindow;


public:

	bool EchoOn( void ) const { return mbEcho; }

	QtConsoleWindow( bool bfloat, QWidget *pparent );
	~QtConsoleWindow();

	//QDockWidget *GetDockWidget( void );
	QGroupBox*	GroupBox() const { return mpGROUPBOX; }


	void InputDone( void );
	void EchoToggle( void );

	static QtConsoleWindow *GetCurrentOutputConsole( void );
};

///////////////////////////////////////////////////////////////////////////////

inline QDockWidget *QtConsoleWindow::GetDockWidget( void )
{
	return mpDockWidget;
}*/

} }	// namespace ork::tool
