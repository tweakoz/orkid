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
#include <unittest++/UnitTest++.h>
#include <pkg/ent/ScriptComponent.h>
#include <pkg/ent/SimpleCharacterArchetype.h>

using namespace ork;
using namespace ork::ent;

///////////////////////////////////////////////////////////////////////////////

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
				    sceneinst->SetSceneInstMode(ESCENEMODE_EDIT);
				    //printf( "sceneinst<%p>\n", sceneinst);
				    //printf( "scenedata<%p>\n", scenedata);
				    delete sceneinst;
				    delete scenedata;
					counter --;
				    //printf( "DONE counter<%d>\n", int(counter));
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

TEST(ScriptCompTest)
{
	auto opl1 = []()
	{
	    SceneData* scenedata = new SceneData;

	    EntData *entdata1 = new EntData;
	    entdata1->SetName("entity1");
	    scenedata->AddSceneObject(entdata1);

	    auto arch = new SimpleCharacterArchetype;
	    entdata1->SetArchetype(arch);


	    auto app = ApplicationStack::Top();
	    SceneInst *sceneinst = new SceneInst(scenedata,app);
		sceneinst->SetSceneInstMode(ESCENEMODE_EDIT);
	    scenedata->EnterEditState();

	    auto sc = arch->GetTypedComponent<ScriptComponentData>();
	    OrkAssert(sc!=nullptr);
	    sc->SetPath( "src://scripts/yo.lua");

		printf( "ScriptCompTest: starting up test scene\n");
	    sceneinst->SetSceneInstMode(ESCENEMODE_RUN);

	    for( int i=0; i<10; i++ )
	    {
			sceneinst->Update();
			usleep(100);
	    }

		printf( "ScriptCompTest: shutting down up test scene\n");
		sceneinst->SetSceneInstMode(ESCENEMODE_EDIT);
	    delete sceneinst;
	    delete scenedata;

	};
	UpdateSerialOpQ().push(Op(opl1));

	UpdateSerialOpQ().drain();
}

///////////////////////////////////////////////////////////////////////////////

