////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/Array.hpp>
#include <ork/kernel/opq.h>
#if 1
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtconsole.h>
#include <QtWidgets/QScrollBar>
#include <QtWidgets/QVBoxLayout>
#include <ork/lev2/qtui/qtui.hpp>
#include <fcntl.h>
#include <boost/algorithm/string.hpp>
///////////////////////////////////////////////////////////////////////////////
namespace ork {
namespace tool {
static int fd_tty_out_slave = -1;
static int fd_tty_err_slave = -1;
static int fd_tty_inp_slave = -1;
///////////////////////////////////////////////////////////////////////////////
static void console_handler();
///////////////////////////////////////////////////////////////////////////////
static QtConsoleWindow* gPCON = nullptr;
void QtConsoleWindow::Register()
{
    gPCON = this;

    MainThreadOpQ().push([&]() {
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
    });
}
///////////////////////////////////////////////////////////////////////////////
void QtConsoleWindow::AppendOutput( const std::string & data )
{
    /////////////////////////////////
    // alloc line from linepool
    /////////////////////////////////

    auto allocLine = [this]() -> ConsoleLine* {
        static const int kmaxlines = 8;
        auto& used = mLinePool.used();
        if( used.size()>=kmaxlines )
        {
            auto oldine = *used.rbegin();
            assert(oldine);
            mLinePool.deallocate(oldine);
        }
        ConsoleLine* cline = mLinePool.allocate();
        return cline;
    };

    /////////////////////////////////

    std::vector<std::string> strs;
    boost::split(strs, data, boost::is_any_of("\n"));

    int inumstrs = strs.size();

    for( int i=0; i<inumstrs; i++ )
    {
        std::string line = strs[i];
        std::string::size_type pos = line.find("\n");
        if(std::string::npos == pos)
        {
            line += ("\n" );
        }
        auto cline = allocLine();

        cline->Set(line.c_str());

    }

    mOutputText.clear();
    const auto& used_lines = mLinePool.used();
    for( const auto& l : used_lines )
        mOutputText += l->mBuffer;

    auto tout = mpConsoleOutputTextEdit;
    auto tdoc = tout->document();
    tdoc->setPlainText(QString(mOutputText.c_str()));
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
        {   buf[nread] = 0;
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

    MainThreadOpQ().push([&]() {
        console_handler();
    });
}
///////////////////////////////////////////////////////////////////////////////

QtConsoleWindow::QtConsoleWindow( bool bfloat, QWidget *pparent )
	: QWidget(pparent)
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
	QFont* consolefont = new QFont("Inconsolata",16);

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

    Register();

}
///////////////////////////////////////////////////////////////////////////////
QtConsoleWindow::~QtConsoleWindow()
{
}
///////////////////////////////////////////////////////////////////////////////
void QtConsoleWindow::EchoToggle( void )
{
	mbEcho = mbEcho ? false : true;
	PropTypeString tstr;
	PropType<bool>::ToString(mbEcho,tstr);

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
    MainThreadOpQ().push([&]() {
			  Py::Ctx().Call(sstr);
		});
	}
	mpConsoleInputTextEdit->clear();
	//Py::Ctx().Call("print dir(sys)");
	//OrkAssertNotImpl();
}
///////////////////////////////////////////////////////////////////////////////
} } // namespace ork::tool

#endif
