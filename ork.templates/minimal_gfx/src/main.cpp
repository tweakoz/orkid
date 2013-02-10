#include <ork/pch.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/lev2/qtui/qtui.hpp>
#include <ork/kernel/opq.h>
#include <ork/file/file.h>

using namespace ork;

////////////////////////////////////////////////////////////////////

class TestApplication : public ork::Application
{ 
	RttiDeclareConcrete(TestApplication, ork::Application );
};
void TestApplication::Describe()
{
}

INSTANTIATE_TRANSPARENT_RTTI(TestApplication, "TestApplication");

///////////////////////////////////////////////////////////////////////////////

class TestVP : public lev2::CUIViewport
{
public:
	TestVP( const std::string & name )
		: CUIViewport( name, 0, 0, 0, 0, CColor3(0.5f,0.1f,0.1f), 0.0f )
		, mPhase(0.0f)
	{
	}
private:

	float mPhase;

	void DoDraw(  ) // virtual 
	{

		CColor4 clr;
		float fr = 0.5f+sinf(mPhase*0.1f)*0.5f;
		float fg = 0.5f+sinf(mPhase*0.17f)*0.5f;
		float fb = 0.5f+sinf(mPhase*0.31f)*0.5f;
		clr.Set(fr,fg,fb,1.0f);

		GetClearColorRef()=clr.GetXYZ();

		auto targ = GetTarget();
		auto fbi = targ->FBI();
		fbi->SetAutoClear(true);
		fbi->SetClearColor(clr);
		fbi->SetViewport( 0,0, targ->GetW(), targ->GetH() );
		fbi->SetScissor( 0,0, targ->GetW(), targ->GetH() );
		targ->BeginFrame();
		targ->EndFrame();

		mPhase += 0.1f;


		//printf("redraw phase<%f>\n", mPhase );
	}
};

///////////////////////////////////////////////////////////////////////////////
namespace ork {

namespace lev2 {
void OpenGlGfxTargetInit();	
}

class QtApp : public QApplication
{	DeclareMoc(QtApp,QApplication);
public:
	///////////////////////////////////
	QtApp( int argc, char** argv );
	void OnTimer();
	///////////////////////////////////
	QTimer				mIdleTimer;
	TestVP*				mVP;
	lev2::CQtGfxWindow* mWindow;
	lev2::CTQT*			mCTQT;

};

QtApp::QtApp( int argc, char** argv )
	: QApplication( argc, argv )
{
	bool bcon = mIdleTimer.connect( & mIdleTimer, SIGNAL(timeout()), this, SLOT(OnTimer()));

	mIdleTimer.setInterval(10);
	mIdleTimer.setSingleShot(false);
	mIdleTimer.start();

	mVP = new TestVP("yo");
	mWindow = new lev2::CQtGfxWindow( mVP );
	mCTQT = new lev2::CTQT( mWindow, nullptr );
	mCTQT->Show();
	mCTQT->GetQWidget()->Enable();

}

///////////////////////////////////////////////////////////////////////////////

void QtApp::OnTimer()
{
	while(MainThreadOpQ().Process());
	mCTQT->GetQWidget()->repaint(0,0,0,0);
}

///////////////////////////////////////////////////////////////////////////////

void QtApp::MocInit( void )
{
	QtApp::Moc.AddSlot0( "OnTimer()", & QtApp::OnTimer );
}

///////////////////////////////////////////////////////////////////////////////

ImplementMoc( QtApp, QApplication );
}
////////////////////////////////////////////////////////////////////

int main(int argc, char** argv)
{
	ork::Opq& mainthreadopq = ork::MainThreadOpQ();
	ork::OpqTest ot( &mainthreadopq );

	static SFileDevContext LocPlatformLevel2FileContext;
	LocPlatformLevel2FileContext.SetFilesystemBaseAbs( "data/platform_lev2/" );
	LocPlatformLevel2FileContext.SetPrependFilesystemBase( true );
	CFileEnv::RegisterUrlBase( "lev2://", LocPlatformLevel2FileContext );

	static SFileDevContext DataDirContext;
	DataDirContext.SetFilesystemBaseAbs( "data/pc" );
	DataDirContext.SetPrependFilesystemBase( true );
	CFileEnv::RegisterUrlBase( "data://", DataDirContext );

	TestApplication the_app;
    ApplicationStack::Push(&the_app);

	//auto test_op = []()
	//{
	///	usleep(2000*10);
	//	printf( "yo\n" );
	//};
	//Op(test_op).QueueASync(mainthreadopq);

	ork::lev2::CQNoMocBase::MocInitAll();
    ork::rtti::Class::InitializeClasses();
	ork::lev2::GfxTargetCreationParams CreationParams;
	CreationParams.miNumSharedVerts = 512<<10;
	ork::lev2::GfxEnv::GetRef().PushCreationParams(CreationParams);

	ork::lev2::OpenGlGfxTargetInit();
	ork::QtApp qtapp(argc,argv);




	qtapp.exec();


    ApplicationStack::Pop();

	return 0;
}
