////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

#include <QtCore/QProcess>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QDockWidget>

namespace ork { namespace tool {

class ProcessView : public QObject
{	//DeclareMoc(ProcessView,QObject);
public:
	QProcess	mProcess;
	QTextEdit* mTextEdit;
	QDockWidget* gfxdock;
	QTimer mTimer;
	volatile int	miLastReturnCode;
	volatile bool	mbDone;
	volatile bool	mbStarted;
	std::string	mTitle;
	std::string mCommandLine;
	///////////////////////////////////////////////////////////////////////////////
	ProcessView(const char* title, QMainWindow* mainwin = 0);
	void updateError();
	void updateText();
	void updateExit(int icode);
	void CreatePanel( const char* title, QMainWindow* mainwin = 0 );
	void OnStart();
	void OnError(QProcess::ProcessError error);
	void OnState(QProcess::ProcessState newState);
	void AppendString( QString str );
	///////////////////////////////////////////////////////////////////////////////
	void start( const char* pstr );
	int wait();
};

}} // namespace ork::tool

