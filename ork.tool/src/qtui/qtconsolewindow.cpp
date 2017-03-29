////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/Array.hpp>
#if 0
#include <dispatch/dispatch.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtconsole.h>
#include <QtGui/QScrollBar>
#include <ork/lev2/qtui/qtui.hpp>
#include <fcntl.h>
///////////////////////////////////////////////////////////////////////////////
dispatch_queue_t PYQ();
extern char slave_out_name[256];
extern char slave_err_name[256];
extern char slave_inp_name[256];
static int fd_tty_out_slave = -1;
static int fd_tty_err_slave = -1;
static int fd_tty_inp_slave = -1;
///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace tool {
///////////////////////////////////////////////////////////////////////////////
void console_handler();
///////////////////////////////////////////////////////////////////////////////
dispatch_queue_t CONQ()
{	
	static dispatch_queue_t gQ=0;
	static dispatch_once_t ginit_once;
	auto once_blk = ^ void (void)
	{
		gQ = dispatch_get_main_queue(); //_create( "com.tweakoz.pyq", NULL );
	};
	dispatch_once(&ginit_once, once_blk );
	return gQ;
}
///////////////////////////////////////////////////////////////////////////////
static vp_cons* gPCON = nullptr;
void vp_cons::Register()
{
	gPCON = this;
	
	auto handler_blk = ^ void (void)
	{
		//const char* inpname = slave_inp_name;
		const char* outname = slave_out_name;
		const char* errname = slave_err_name;

		//fd_tty_inp_slave = open( inpname, O_WRONLY|O_NONBLOCK );
		fd_tty_out_slave = open( outname, O_RDONLY|O_NONBLOCK );
		fd_tty_err_slave = open( errname, O_RDONLY|O_NONBLOCK );

		//assert( fd_tty_inp_slave>=0 );
		assert( fd_tty_out_slave>=0 );
		assert( fd_tty_err_slave>=0 );

		usleep(1<<18);

		console_handler();
	};
	dispatch_time_t at = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC/2 );
	dispatch_after( at, CONQ(), handler_blk );
}
///////////////////////////////////////////////////////////////////////////////
void vp_cons::AppendOutput( const std::string & data )
{
	std::vector<std::string> strs;
	boost::split(strs, data, boost::is_any_of("\n"));

	//ConsoleLine* cline = mLinePool.allocate();
	//mLineList.push_back(cline);

	int inumstrs = strs.size();
	
	//mOutputText.clear();
	
	for( int i=0; i<inumstrs; i++ )
	{
		std::string line = strs[i];
		std::string::size_type pos = line.find("\n");
		if(std::string::npos == pos)
		{
			line += ("\n" );
		}
		mLines.push_back(line);
	}
}
///////////////////////////////////////////////////////////////////////////////
void console_handler()
{
	/////////////////////
	char buf[256];
	int nread = read(fd_tty_out_slave, buf, 254);
	switch( nread )
	{
		case -1:
		{
			if (errno == EAGAIN)
			{
				break;
			}
			perror("read");
			break;
		}
		default:
		{	buf[nread] = 0;
			std::string outstr(&buf[0]);
			if( gPCON )
				gPCON->AppendOutput( outstr );
			break;
		}
	}
	////////////////////////////
	/*nread = read(0, buf, 254);
	switch( nread )
	{
		case -1:
		{
			if (errno == EAGAIN)
			{
				break;
			}
			perror("read");
			break;
		}
		default:
			for( int i = 0; i < nread; i++)
			{
			  write(fd_pty_master, buf+i, 1);
			  //fputc(buf[i],fslave);
			}
			break;
	}*/
	
	//PyGILState_STATE gstate = PyGILState_Ensure();
	//int iret = PyRun_InteractiveOne(fp_pty_master,"???");
	//PyGILState_Release(gstate);
	//fprintf( fp_pty_master, ">>>" );
	//fflush( fp_pty_master );
	
	/////////////////////

	auto repeat_blk = ^ void (void)
	{
		console_handler();
	};
	
	
	dispatch_time_t at = dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC/10 );
	dispatch_after( at, CONQ(), repeat_blk );

}
///////////////////////////////////////////////////////////////////////////////
QtConsoleWindow * QtConsoleWindow::gpWindow = 0;
///////////////////////////////////////////////////////////////////////////////

QtConsoleWindow::QtConsoleWindow( bool bfloat, QWidget *pparent )
	: QWidget(pparent)
	//: MocImp< QtConsoleWindow, QObject >( (QObject*)0 )
	, mbEcho( true )
{
	static int viewnum = 0;
	std::string viewname = CreateFormattedString( "Console:%d", viewnum+1 );
	viewnum++;

	//mpDockWidget = new QDockWidget(pparent->tr(viewname.c_str()), pparent);
	//mpDockWidget->setFloating( bfloat );
	//mpDockWidget->setAllowedAreas(Qt::AllDockWidgetAreas );
	//mpDockWidget->setAutoFillBackground(false);

	mpGROUPBOX = new QGroupBox;
	QVBoxLayout *playout = new QVBoxLayout( mpGROUPBOX );

	mpConsoleOutputTextEdit = new QTextEdit( mpGROUPBOX );
	mpConsoleInputTextEdit = new QLineEdit( mpGROUPBOX );

	QPalette p=mpConsoleOutputTextEdit->palette();
    p.setColor(QPalette::Active, QPalette::Base , QColor(0,0,128,255) );
    p.setColor(QPalette::Active, QPalette::Text , QColor(0,255,0,255) );
    mpConsoleOutputTextEdit->setPalette(p);	
	mpConsoleOutputTextEdit->setBackgroundRole ( QPalette::Dark );
	mpConsoleOutputTextEdit->setForegroundRole ( QPalette::BrightText );
	mpConsoleOutputTextEdit->setAutoFillBackground(true);
	QFont* consolefont = new QFont("Inconsolata",18);

	mpConsoleOutputTextEdit->setCurrentFont( *consolefont );
	
    p.setColor(QPalette::Active, QPalette::Base , QColor(128,0,0,255) );
    p.setColor(QPalette::Active, QPalette::Text , QColor(255,255,0,255) );
	mpConsoleInputTextEdit->setPalette(p);


	mpConsoleOutputTextEdit->setReadOnly(true);

	QGroupBox *pwH = new QGroupBox;
	playout->addWidget( mpConsoleOutputTextEdit );
	playout->addWidget( mpConsoleInputTextEdit );
	//QHBoxLayout *playoutH = new QHBoxLayout( pwH );

	//QCheckBox *but = new QCheckBox("echo");
	//playoutH->addWidget( but );
	//playoutH->addWidget( mpConsoleInputTextEdit );
	
	pwH->resize( 156, 16 );
	mpGROUPBOX->resize( 156, 32 );

	//mpDockWidget->setWidget( pw );
	//mpDockWidget->setMinimumSize( 100, 48 );

	connect( mpConsoleInputTextEdit, SIGNAL(editingFinished()), SLOT(InputDone()) );
	//connect( but, SIGNAL(toggled(bool)), SLOT(EchoToggle()) );

	this->resize( 160, 160 );
	//mpDockWidget->show();

	gpWindow = this;
}
///////////////////////////////////////////////////////////////////////////////
QtConsoleWindow::~QtConsoleWindow()
{
	gpWindow = 0;
}
///////////////////////////////////////////////////////////////////////////////
void QtConsoleWindow::EchoToggle( void )
{
	mbEcho = mbEcho ? false : true;
	PropTypeString tstr;
	CPropType<bool>::ToString(mbEcho,tstr);

	std::string tglline = CreateFormattedString( "echo = %s", tstr.c_str() );
	AppendOutput(tglline);
}
///////////////////////////////////////////////////////////////////////////////
void QtConsoleWindow::InputDone( void )
{
	QString qstr = mpConsoleInputTextEdit->text();
	std::string sstr = qstr.toStdString();
	if( mbEcho )
	{
		AppendOutput(sstr);
	}
	//printf( "INPUT<%s>\n", sstr.c_str() );
	if( sstr.length() )
	{
		auto Pyblock = ^ void()
		{
			Py::Ctx().Call(sstr);
		};
		dispatch_async(PYQ(),Pyblock);
	}
	mpConsoleInputTextEdit->clear();
	//Py::Ctx().Call("print dir(sys)");
	//OrkAssertNotImpl();
}
///////////////////////////////////////////////////////////////////////////////
void QtConsoleWindow::MocInit( void )
{
	QtConsoleWindow::Moc.AddSlot0( "InputDone()", & QtConsoleWindow::InputDone );
	QtConsoleWindow::Moc.AddSlot0( "EchoToggle()", & QtConsoleWindow::EchoToggle );
}
///////////////////////////////////////////////////////////////////////////////
QtConsoleWindow *QtConsoleWindow::GetCurrentOutputConsole( void )
{
	return gpWindow;
}*/
///////////////////////////////////////////////////////////////////////////////
} } // namespace ork::tool

#endif
