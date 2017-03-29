////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <orktool/filter/gfx/meshutil/meshutil.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/qtui/qtui.h>

#if !defined(ORK_OSX)
#include <X11/Xlib.h>
#endif

#include <ork/kernel/opq.h>
#include <ork/kernel/environment.h>

using namespace ork;
using namespace ork::ent;
using namespace ork::MeshUtil;
using namespace ork::tool;

namespace ork { namespace tool { tokenlist Init(int argc, char **argv); }}
namespace ork { namespace tool { int main(int& argc, char **argv); }}
namespace ork { namespace lev2 { void Init(); }}

class TestApplication : public ork::Application
{
public:

	TestApplication();
	~TestApplication();
};

TestApplication::TestApplication()
{
}

TestApplication::~TestApplication()
{
}

int main(int argc, char** argv, char** argp)
{
	genviron.init_from_envp(argp);

	SetCurrentThreadName( "MainThread" );

#if defined(IX) && ! defined(ORK_OSX)
//	XInitThreads();
#endif
	TestApplication the_app;
    ApplicationStack::Push(&the_app);

    ork::rtti::Class::InitializeClasses();
	ork::lev2::GfxTargetCreationParams CreationParams;
	CreationParams.miNumSharedVerts = 64<<10;
	ork::lev2::GfxEnv::GetRef().PushCreationParams(CreationParams);

	ork::Opq& mainthreadopq = ork::MainThreadOpQ();
	ork::OpqTest ot( &mainthreadopq );

    return ork::tool::main(argc,argv);
}

//extern "C"
//{
//	void* myalloc(size_t siz) { return malloc(siz); }
//	void myfree(void* mem) { free(mem); }
//	void* myrealloc(void* mem,size_t siz) { return realloc(mem,siz); }
//};
