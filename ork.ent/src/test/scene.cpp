#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/util/Context.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/entity.hpp>
#include <pkg/ent/scene.h>
#include <pkg/ent/scene.hpp>
#include <unittest++/UnitTest++.h>
#include <pkg/ent/ScriptComponent.h>

using namespace ork;
using namespace ork::ent;

///////////////////////////////////////////////////////////////////////////////
#define ANSI_COLOR_RESET     "\x1b[30m"
#define ANSI_COLOR_GREEN     "\x1b[32m"

TEST(SceneManip1)
{
	atomic<int> counter;
	counter = 0;

	for( int i=0; i<100; i++ )
	{
		counter++;

		auto opl1 = [&counter]()
		{
		    SceneData* scenedata = new SceneData;

		    EntData *entdata1 = new EntData;
		    entdata1->SetName("entity1");
		    scenedata->AddSceneObject(entdata1);

		    EntData *entdata2 = new EntData;
		    entdata2->SetName("entity2");
		    scenedata->AddSceneObject(entdata2);

		    auto app = ApplicationStack::Top();
		    SceneInst *sceneinst = new SceneInst(scenedata,app);
			sceneinst->SetSceneInstMode(ESCENEMODE_EDIT);
		    sceneinst->SetSceneInstMode(ESCENEMODE_RUN);

		    Entity *entity = sceneinst->FindEntity(AddPooledLiteral("entity1"));

		    //printf( "entity<%p>\n", entity );
		    entity = sceneinst->FindEntity(AddPooledLiteral("entity2"));
		    //printf( "entity2<%p>\n", entity );

			auto opl2 = [=,&counter]()
			{

				auto opl3 = [=,&counter]()
				{
				    delete sceneinst;
				    delete scenedata;
					counter --;
				};
				UpdateSerialOpQ().push(Op(opl3));
			};
			MainThreadOpQ().push(Op(opl2));



		};

		UpdateSerialOpQ().push(Op(opl1));
	}
	while( counter != 0 )
	{
		usleep(100);
	}

	printf( "SceneManip1 DONE counter<%d>\n", int(counter));
	//WaitForOpqExit();
}

///////////////////////////////////////////////////////////////////////////////

class ScriptOnlyArchetype : public Archetype
{
	void DoStartEntity(SceneInst* psi, const fmtx4 &world, Entity *pent ) const final
	{
		//printf( "ScriptOnlyArchetype::DoStartEntity(%p)\n", pent );

	}
	void DoCompose(ork::ent::ArchComposer& composer) final;

public:

	ScriptOnlyArchetype() {}

};


//////////////////////////////////////////////////////////

void ScriptOnlyArchetype::DoCompose(ork::ent::ArchComposer& composer)
{
	composer.Register<ork::ent::ScriptComponentData>();
}

TEST(ScriptCompTest)
{
	auto opl1 = []()
	{
	    SceneData* scenedata = new SceneData;




	    auto arch = new ScriptOnlyArchetype;
	    arch->SetName("SceneObject1");
	    scenedata->AddSceneObject(arch);
	    static const int knument = 3;
	    for( int i=0; i<knument; i++ )
	    {
	    	ork::fxstring<256> ename;
	    	ename.format("ent%d", i );
	    	EntData *entdata1 = new EntData;
		    entdata1->SetName(ename.c_str());
		    scenedata->AddSceneObject(entdata1);
	    	entdata1->SetArchetype(arch);

	    	int ix = rand()%100;
	    	int iy = rand()%100;
	    	int iz = rand()%100;

	    	auto fx = float(ix)*0.001f;
	    	auto fy = float(iy)*0.001f;
	    	auto fz = float(iz)*0.001f;

			auto& dn = entdata1->GetDagNode();
			auto& xn = dn.GetTransformNode();
			auto& xf = xn.GetTransform();

			xf.SetPosition(fvec3(fx,fy,fz));
		}

	    auto app = ApplicationStack::Top();
	    SceneInst *sceneinst = new SceneInst(scenedata,app);
		sceneinst->SetSceneInstMode(ESCENEMODE_EDIT);
	    scenedata->EnterEditState();

	    auto sc = arch->GetTypedComponent<ScriptComponentData>();
	    OrkAssert(sc!=nullptr);
	    sc->SetPath( "src://scripts/yo.lua");

		printf( "%s", ANSI_COLOR_GREEN );
		//printf( "%s", ANSI_COLOR_RESET );
		printf( "ScriptCompTest: starting up test scene\n");
	    sceneinst->SetSceneInstMode(ESCENEMODE_RUN);

	    ork::Timer tmr;
	    tmr.Start();
	    static const int knumframes = 2000;
	    for( int i=0; i<knumframes; i++ )
	    {
			sceneinst->Update();
	    }
	    float ftime = tmr.SecsSinceStart();
	    size_t numentupd = sceneinst->GetEntityUpdateCount();

		printf( "ScriptCompTest: shutting down up test scene\n");
		printf( "FPS<%f>\n", float(knumframes)/ftime);
		printf( "LuaEntUpdates<%zu>\n", numentupd );
		printf( "LuaEntUpdatesPS<%f>\n", float(numentupd)/ftime );
		//printf( "%s", ANSI_COLOR_RESET );
		printf( "%s", ANSI_COLOR_GREEN );
	    delete sceneinst;
	    delete scenedata;

	};
	UpdateSerialOpQ().push(Op(opl1));

	UpdateSerialOpQ().drain();
}

///////////////////////////////////////////////////////////////////////////////
