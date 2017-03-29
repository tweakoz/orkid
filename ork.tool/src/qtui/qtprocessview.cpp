////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <orktool/qtui/qtprocessview.h>
#include <orktool/qtui/qtapp.h>
#include <ork/lev2/qtui/qtui.hpp>

namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

extern OrkQtApp* gpQtApplication;

ProcessView::ProcessView(const char* title, QMainWindow* mainwin)
	: mTextEdit(0)
	, mTitle(title)
	, mbStarted(false)
{
	////////////////////////////////////////////////////////////

	CreatePanel(title,mainwin);

	bool bcon0 = mProcess.connect(&mProcess, SIGNAL(readyReadStandardError()), this, SLOT(updateError()));
	bool bcon1 = mProcess.connect(&mProcess, SIGNAL(readyReadStandardOutput()), this, SLOT(updateText()));
	bool bcon2 = mProcess.connect(&mProcess, SIGNAL(finished(int)), this, SLOT(updateExit(int)));
	bool bcon3 = mProcess.connect(&mProcess, SIGNAL(started()), this, SLOT(OnStart()));
	bool bcon4 = mProcess.connect(&mProcess, SIGNAL(error(QProcess::ProcessError)), this, SLOT(OnError(QProcess::ProcessError)));
	bool bcon5 = mProcess.connect(&mProcess, SIGNAL(stateChanged(QProcess::ProcessState)), this, SLOT(OnState(QProcess::ProcessState)));

	////////////////////////////////////////////////////////////

}

void ProcessView::start(const char* pstr)
{
	OrkAssert( mbStarted==false );
	ork::msleep(100);
	mCommandLine = pstr;
	orkprintf( "starting cmd<%s>\n", pstr );
	mProcess.start( pstr, QProcess::ReadWrite );
	ork::msleep(100);
	while(false==mbStarted)
	{
		ork::msleep(100);
	}
}

int ProcessView::wait()
{
	mbDone = false;
	miLastReturnCode = -999;
	while( false == mbDone )
	{
		ork::msleep(100);
	}	
	ork::msleep(100);
	mbStarted = false;
	return miLastReturnCode;
}

///////////////////////////////////////////////////////////////////////////////
void ProcessView::AppendString( QString str )
{
	QString newtext = mTextEdit->toPlainText() + str;
	mTextEdit->setText(newtext);
	QTextCursor c = mTextEdit->textCursor();
	c.movePosition(QTextCursor::End);
	mTextEdit->setTextCursor(c);
}

void ProcessView::updateError()
{
	QByteArray data = mProcess.readAllStandardError();
	AppendString( QString(data) );
}
void ProcessView::updateText()
{
	QByteArray data = mProcess.readAllStandardOutput();
	AppendString( QString(data) );
}
void ProcessView::updateExit(int icode)
{
	miLastReturnCode = icode;
	std::string retstr = CreateFormattedString("cmd<%s>\nreturned<%d>\n", mCommandLine.c_str(), icode );
	AppendString( QString("\n/////////////////////////////////////////////////////////////////////\n") );
	AppendString( QString(retstr.c_str()) );
	AppendString( QString("/////////////////////////////////////////////////////////////////////\n\n") );
	mbDone = true;
}
void ProcessView::OnStart()
{
	AppendString( QString("\n/////////////////////////////////////////////////////////////////////\n") );
	std::string retstr = CreateFormattedString("cmd<%s>\nbegin\n", mCommandLine.c_str() );
	AppendString( QString(retstr.c_str()) );
	AppendString( QString("/////////////////////////////////////////////////////////////////////\n\n") );
	mbStarted = true;
}

void ProcessView::OnError(QProcess::ProcessError error)
{
	AppendString( QString("\n/////////////////////////////////////////////////////////////////////\n") );
	std::string errstr;
	switch( error )
	{
		case QProcess::FailedToStart:
			errstr = "FailedToStart";
			break;
		case QProcess::Crashed:
			errstr = "Crashed";
			break;
		case QProcess::Timedout:
			errstr = "Timedout";
			break;
		case QProcess::ReadError:
			errstr = "ReadError";
			break;
		case QProcess::WriteError:
			errstr = "WriteError";
			break;
		case QProcess::UnknownError:
		default:
			errstr = "UnknownError";
			break;
	}

	std::string retstr = CreateFormattedString("cmd<s>\nerror<%s>\n", mCommandLine.c_str(),errstr.c_str() );
	AppendString( QString(retstr.c_str()) );
	AppendString( QString("/////////////////////////////////////////////////////////////////////\n\n") );
}

void ProcessView::OnState(QProcess::ProcessState newState)
{
	AppendString( QString("\n/////////////////////////////////////////////////////////////////////\n") );
	std::string statestr;
	switch( newState )
	{
		case QProcess::NotRunning:
			statestr = "NotRunning";
			break;
		case QProcess::Starting:
			statestr = "Starting";
			break;
		case QProcess::Running:
			statestr = "Running";
			break;
		default:
			statestr = "UnknownState";
			break;
	}
	std::string retstr = CreateFormattedString("cmd<%s>\nstate<%s>\n", mCommandLine.c_str(), statestr.c_str() );
	AppendString( QString(retstr.c_str()) );
	AppendString( QString("/////////////////////////////////////////////////////////////////////\n\n") );
}

///////////////////////////////////////////////////////////////////////////////
void ProcessView::CreatePanel( const char* title, QMainWindow* mainwin )
{
	gfxdock = 0;

	if( mainwin )
	{
		gfxdock = new QDockWidget(mainwin->tr(title), mainwin );
		gfxdock->setFloating( false );
		gfxdock->setAllowedAreas(Qt::AllDockWidgetAreas );
		gfxdock->setAutoFillBackground(false); 

		mTextEdit = new QTextEdit( gfxdock );
		gfxdock->setWidget( mTextEdit );

		mainwin->addDockWidget(Qt::RightDockWidgetArea, gfxdock);
	}
	else
	{
		mTextEdit = new QTextEdit( 0 );
	}

	mTextEdit->setStyleSheet(	"color: #00FF00; background-color: #101020;"
								"selection-color: white; "
								"selection-background-color: blue;" );


	if( gfxdock )
	{
		gfxdock->show();
	}
}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
