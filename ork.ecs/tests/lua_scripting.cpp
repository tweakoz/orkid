////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if 1
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/register.h>
#include <ork/util/Context.h>
#include <utpp/UnitTest++.h>
#include <ork/ecs/lua/LuaComponent.h>
#include <ork/ecs/controller.h>

#include "ecstest.inl"
#include <ork/ecs/entity.inl>
#include <ork/ecs/archetype.inl>
#include <ork/ecs/scene.inl>

using namespace ork;
using namespace ork::ecs;

///////////////////////////////////////////////////////////////////////////////

TEST(LuaScripting1)
{
  atomic<int> counter;
  counter = 0;

///////////////////////////////////////////////////////////////////////////////


  for( int i=0; i<2; i++ )
  {
    counter++;

    auto opl1 = [&counter]()
    {
        auto app = StringPoolStack::top();

        auto scenedata = std::make_shared<SceneData>();

        auto arch = scenedata->createSceneObject<Archetype>("a1"_pool);

        auto sd1 = scenedata->getTypedSystemData<TestSystemData1>();
        auto sd2 = scenedata->getTypedSystemData<TestSystemData2>();
        auto scr_sys = scenedata->getTypedSystemData<LuaSystemData>();

        auto c1 = arch->addComponent<TestComponentData1>();
        auto c2 = arch->addComponent<TestComponentData2>();
        auto sc = arch->addComponent<LuaComponentData>();
        sc->SetPath( "src://scripts/arch/yo.lua");

        auto spawner1 = scenedata->createSceneObject<SpawnData>("e1"_pool);
        spawner1->SetArchetype(arch);

        auto spawner2 = scenedata->createSceneObject<SpawnData>("e2"_pool);
        spawner2->SetArchetype(arch);

        auto controller = std::make_shared<ecs::Controller>(app);
        controller->bindScene(scenedata);

        controller->createSimulation();

        for(int i=0; i<10; i++){
          controller->update();
          ork::usleep(16000);
        }

        counter--;
    };

    opq::updateSerialQueue()->enqueue(opl1);
  }
  while( counter != 0 )
  {
    printf( "waiting for counter<%d>\n", counter.load() );
    ork::usleep(1<<20);
  }

  printf( "SceneManip1 DONE counter<%d>\n", int(counter));
  //WaitForOpqExit();
}


///////////////////////////////////////////////////////////////////////////////
#endif
