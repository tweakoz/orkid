////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_TOOL_QTAPP_H 
#define _ORK_TOOL_QTAPP_H

namespace ork { namespace tool {

class OrkQtApp : public QApplication
{	DeclareMoc(OrkQtApp,QApplication);
public:
	///////////////////////////////////
	OrkQtApp( int argc, char** argv );
	void OnTimer();
	///////////////////////////////////
	QTimer				mIdleTimer;
	QMainWindow*		mpMainWindow;

};

}}

#endif
