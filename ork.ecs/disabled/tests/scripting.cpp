#if 1
#include <ork/pch.h>
#include <ork/kernel/opq.h>
#include <ork/application/application.h>
#include <ork/object/Object.h>
#include <ork/rtti/downcast.h>
#include <ork/reflect/properties/register.h>
#include <ork/util/Context.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/ecs/entity.h>
#include <ork/ecs/entity.hpp>
#include <ork/ecs/scene.h>
#include <ork/ecs/scene.hpp>
#include <utpp/UnitTest++.h>
//#include <ork/ent/LuaComponent.h>

using namespace ork;
using namespace ork::ecs;

///////////////////////////////////////////////////////////////////////////////

class ScriptOnlyArchetype : public Archetype
{
  void DoStartEntity(Simulation* psi, const fmtx4 &world, Entity *pent ) const final
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
  composer.Register<ork::ent::LuaComponentData>();
}

TEST(ScriptCompTest)
{
  auto opl1 = []
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

      auto app = StringPoolStack::Top();
      Simulation *simulation = new Simulation(scenedata,app);
    simulation->SetSimulationMode(ESCENEMODE_EDIT);
      scenedata->EnterEditState();

      auto sc = arch->GetTypedComponent<LuaComponentData>();
      OrkAssert(sc!=nullptr);
      sc->SetPath( "src://scripts/yo.lua");

    printf( "%s", ANSI_COLOR_GREEN );
    //printf( "%s", ANSI_COLOR_RESET );
    printf( "ScriptCompTest: starting up test scene\n");
      simulation->SetSimulationMode(ESCENEMODE_RUN);

      ork::Timer tmr;
      tmr.Start();
      static const int knumframes = 2000;
      for( int i=0; i<knumframes; i++ )
      {
      simulation->Update();
      }
      float ftime = tmr.SecsSinceStart();
      size_t numentupd = simulation->GetEntityUpdateCount();

    printf( "ScriptCompTest: shutting down up test scene\n");
    printf( "FPS<%f>\n", float(knumframes)/ftime);
    printf( "LuaEntUpdates<%zu>\n", numentupd );
    printf( "LuaEntUpdatesPS<%f>\n", float(numentupd)/ftime );
    //printf( "%s", ANSI_COLOR_RESET );
    printf( "%s", ANSI_COLOR_GREEN );
      delete simulation;
      delete scenedata;

  };
  opq::updateSerialQueue()->enqueue(opl1);

  opq::updateSerialQueue()->drain();
}
