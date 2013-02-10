////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace tool {

class OrkQtApp : public QApplication
{	//DeclareMoc(OrkQtApp,QApplication);
	Q_OBJECT
public:
	///////////////////////////////////
	OrkQtApp( int argc, char** argv );
	///////////////////////////////////
	QTimer				mIdleTimer;
	QMainWindow*		mpMainWindow;

public Q_SLOTS:
	void OnTimer();
};

}}
