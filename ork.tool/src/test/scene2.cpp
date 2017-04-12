#include <ork/pch.h>

#include <cstdio>
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/editor/editor.h>
//#include <ork/kernel/core_interface.h>
//#include <ork/platform_lev2/gfx/gfxenv.h>
//#include <ork/platform_lev2/manip/manip.h>
#include <unittest++/UnitTest++.h>

using namespace ork;
using namespace ork::ent;

namespace ork { namespace tool { void InitBuiltinInterfaces(); } }

namespace SceneTest {

TEST(SceneTortureTest)
{
	SceneEditorBase the_editor;
	NewSceneReq nsr;

	the_editor.QueueOpSync(nsr);

	SceneData* pscene = the_editor.GetSceneData();

	{
		Future new_ent;
		NewEntityReq ner(new_ent);
		the_editor.QueueOpASync(ner);
		auto pent = new_ent.GetResult().Get<EntData*>();
	}
	///////////////////////////////////////////////////////////////////
	// randomly generate archetypes
	///////////////////////////////////////////////////////////////////

	static const int kmaxarch = 10;
	Archetype* parchs[kmaxarch];
	FixedString<256> fstr;
	for( int i=0; i<kmaxarch; i++ )
	{
		Archetype* parch = nullptr;
		switch(i%2)
		{
			case 0:
				parch = the_editor.EditorNewArchetype("ModelArchetype","blah");
				fstr.format("ModelArchetype_%d", int(i));
				break;
			case 1:
				parch = the_editor.EditorNewArchetype("BulletObjectArchetype","blah");
				fstr.format("BulletObjectArchetype_%d", int(i));
				break;
		}
		assert(parch!=nullptr);
		parchs[i] = parch;
		bool bOK = the_editor.EditorRenameSceneObject( parch, fstr.c_str() );
		assert(bOK);
	}


	static const int kmaxiter = 300;

	///////////////////////////////////////////////////////////////////
	// randomly generate entities
	///////////////////////////////////////////////////////////////////

	for( int i=0; i<=kmaxiter; i++ )
	{

		if( 0 == (i%100) )
		{
			// ANTI-SPAM!
			//orkprintf( "SceneTortureUnitTest [iter %03d/%03d] [nument %d] [numsceneobj %d] [numuniq %d]\n", i, kmaxiter, inument, inumobj, inumuni );
		}

		int iarch = rand()%kmaxarch;
		Archetype* parch = parchs[iarch];
		{
			Future new_ent;
			NewEntityReq ner(new_ent);
			ner.mArchetype = parch;
			the_editor.QueueOpASync(ner);
			auto pent = new_ent.GetResult().Get<EntData*>();
			fstr.format("Ent_%d", int(i));
			bool bOK = the_editor.EditorRenameSceneObject( pent, fstr.c_str() );
			assert(bOK);
		}
		//pent2->SetName( pscene->TryObjectName(  ).c_str() );
		//pscene->AddSceneObject( pent2 );

		/////////////////////
	}

#if 0
		ork::lev2::CManipManager::GetRef().AttachObject( pent2 );
		ork::lev2::CManipManager::GetRef().ReleaseObject();

		const orkset<CObject*> & SelSet = ork::tool::SelectManager::GetRef().GetActiveSelection();
		/*
		DagSceneObject * DagObj = cobject_downcast<DagSceneObject>( pent2 );

		if( DagObj )
		{
			CVector3 Pos = DagObj->GetTransformNode().GetWorldTransform()->GetPosition();
		}
		switch( rand()%10 )
		{
			case 0:
			case 1:
			case 2:
			{	ork::tool::SelectManager::GetRef().ToggleSelection( pent2 );
				break;
			}
			case 9:
			{
				ork::tool::SelectManager::GetRef().ClearSelection();
				break;
			}
			default:
			{
				ork::tool::SelectManager::GetRef().AddObjectToSelection( pent2 );
				break;
			}
		}*/

		switch( rand()%10 )
		{
			case 0:
			case 1:
			case 2:
			case 3:
			{	
				if( SelSet.size() )
				{	Editor.EditorDupe();
				}
				for( orkmap<std::string, SceneObject *>::const_iterator it=SceneMap.begin(); it!= SceneMap.end(); it++ )
				{
					SceneObject * pobj = it->second;
					OrkAssert( strlen( pobj->GetStringTableIndexName().c_str() ) > 0 );
				}
				break;
			}
			case 4:
				Editor.EditorGroup();
				break;
			case 5:
				Editor.EditorUnGroup();
				break;
			default:
			{	for( orkmap<std::string, SceneObject *>::const_iterator it=SceneMap.begin(); it!= SceneMap.end(); it++ )
				{
					SceneObject * pobj = it->second;
					OrkAssert( strlen( pobj->GetStringTableIndexName().c_str() ) > 0 );
				}
				pscene->RecomposeEntities();
				pscene->BuildEntityVect();
				break;
			}
		}
		if( SelSet.size() )
		{
			CObject *pobj = *SelSet.begin();

			OrkAssert( pobj->GetClass()->IsSubclassOf( SceneObject::GetClassStatic() ) );

			Entity *pent = cobject_downcast<Entity>( pobj );

			//SceneGroup *group = cobject_downcast<SceneGroup>( pobj );
			
			////////////////////////////////////////////////////////////
			// if its an entity, randomly rename it
			////////////////////////////////////////////////////////////

			if( pent )
			{
				switch( rand()%4 )
				{
					case 0: // RENAME / Group TEST
					case 1: // RENAME / Group TEST
					case 2: // RENAME / Group TEST
					case 3: // RENAME / Group TEST
					{
						const std::string & OldName = pent->GetName();
						int i = rand()%5;
						const std::string NewNameAttempt = create_fstring( "EntityRenameTest%d", i );
						pent->SetName( NewNameAttempt.c_str() );
						for( orkmap<std::string, SceneObject *>::const_iterator it=SceneMap.begin(); it!= SceneMap.end(); it++ )
						{
							SceneObject * pobj = it->second;
							OrkAssert( strlen( pobj->GetStringTableIndexName().c_str() ) > 0 );
						}

						ork::lev2::IEditorInterface *editorinterface = pent2->GetClass()->QueryInterface<ork::lev2::IEditorInterface>();
						OrkAssert( 0 != editorinterface );
						editorinterface->PropertyChanged( pent, pent->GetClass()->GetProperty( "Name" ) );
						break;
					}

				}
			}

			////////////////////////////////////////////////////////////
			// move it whatever it is
			////////////////////////////////////////////////////////////
			
			ork::lev2::CManipManager::GetRef().AttachObject( pobj );
			ork::TransformNode3D xfnode;
			CReal fx(CReal(rand()%65535)/CReal(65536.0f));
			CReal fy(CReal(rand()%65535)/CReal(65536.0f));
			CReal fz(CReal(rand()%65535)/CReal(65536.0f));
			ork::CVector3 Translation( fx,fy,fz );
			xfnode.Translate( ork::TransformNode3D::EMODE_ABSOLUTE, Translation );
			ork::lev2::CManipManager::GetRef().ApplyTransform( xfnode );
			ork::lev2::CManipManager::GetRef().ReleaseObject();

			/*if( group )
			{
				switch( rand()%4 )
				{
					case 0:
					case 1:
						Editor.EditorUnGroup();
						break;
					case 2:
						Editor.EditorGroup();
						break;
				}
			}*/

		}

		while( SceneMap.size() > 300 )
		{
			Entity *penttodelete = 0;

			for( orkmap<std::string, SceneObject *>::const_iterator it=SceneMap.begin(); (penttodelete==0); it++ )
			{
				SceneObject * pobj = it->second;
				penttodelete = cobject_downcast<Entity>(pobj);
			}
			
			ork::tool::SelectManager::GetRef().ClearSelection();
			ork::tool::SelectManager::GetRef().AddObjectToSelection( penttodelete );

			Editor.EditorDelete();
		}
		
	}
	pscene->CleanUp();


	///////////////////////////////////////////////////////////////////
	// save the scene
	///////////////////////////////////////////////////////////////////

	CMiniorkApplication::GetRef().GetCurrentContext()->SaveScene( "SceneTortureTest.xml" );
	CMiniorkApplication::GetRef().GetCurrentContext()->SaveScene( "SceneTortureTest.bin" );

	const orkmap<std::string, SceneObject *> & DeSerSceneMap = pscene->GetSceneObjects();

	int inumsceneobj = DeSerSceneMap.size();

	///////////////////////////////////////////////////////////////////
	// cache scene data for later comparison
	///////////////////////////////////////////////////////////////////
	
	orkvector<std::string> ArchetypeNameVect;
	orkvector<std::string> ArchetypeClassVect;
	orkvector<std::string> EntityArchetypeVect;
	orkvector<std::string> EntityNameVect;
	orkvector<std::string> EntityTransforms;

	const orkmap<std::string, SceneObject *> & SerSceneMap = pscene->GetSceneObjects();

	for( orkmap<std::string, SceneObject *>::const_iterator it=SerSceneMap.begin(); it!=SerSceneMap.end(); it++ )
	{
		const std::string ObjName = it->first;
		SceneObject *pobj = it->second;

		if( pobj->GetClass()->IsSubclassOf( Entity::GetClassStatic() ) )
		{
			Entity *pent = safe_cobject_downcast<Entity>( pobj );

			EntityNameVect.push_back( pent->GetName() );
			EntityArchetypeVect.push_back( pent->GetArchetype()->GetName() );
			const TransformNode3D& Node = GetEntityTransformNode(pent);
			PropTypeString tstr;
			CPropType<TransformNode3D>::ToString(Node,tstr);
			EntityTransforms.push_back( tstr.c_str() );
		}
		else if( pobj->GetClass()->IsSubclassOf( EntityArchetype::GetClassStatic() ) )
		{
			EntityArchetype *parch = safe_cobject_downcast<EntityArchetype>( pobj );
			ArchetypeNameVect.push_back( parch->GetName() );
			ArchetypeClassVect.push_back( parch->GetClass()->GetName() );
		}	
	}

	///////////////////////////////////////////////////////////////////
	// Load Scene we just saved
	///////////////////////////////////////////////////////////////////

	Editor.EditorNewScene(CTestScene::GetClassNameStatic());
	CMiniorkApplication::GetRef().GetCurrentContext()->LoadScene( "SceneTortureTest.xml" );
	pscene = CMiniorkApplication::GetRef().GetCurrentContext()->GetScene();

	///////////////////////////////////////////////////////////////////
	// make sure loaded data lines up with original
	///////////////////////////////////////////////////////////////////

	const orkmap<std::string, SceneObject *> & NewDeSerSceneMap = pscene->GetSceneObjects();

	int inewnumsceneobj = NewDeSerSceneMap.size();

	CHECK( inumsceneobj == inewnumsceneobj );

	int ientidx = 0 ;
	int iarchidx = 0 ;

	for( orkmap<std::string, SceneObject *>::const_iterator it=NewDeSerSceneMap.begin(); it!=NewDeSerSceneMap.end(); it++ )
	{
		const std::string ObjName = it->first;
		SceneObject *pobj = it->second;

		if( pobj->GetClass()->IsSubclassOf( Entity::GetClassStatic() ) )
		{
			Entity *pent = safe_cobject_downcast<Entity>( pobj );

			CHECK( pent->GetName() == EntityNameVect[ ientidx ] );
			OrkAssert( pent->GetArchetype() );
			CHECK( pent->GetArchetype()->GetName() == EntityArchetypeVect[ ientidx ] );
			const TransformNode3D& Node = GetEntityTransformNode(pent);
			PropTypeString tstr;
			CPropType<TransformNode3D>::ToString(Node,tstr);
			CHECK( 0 == strcmp( tstr.c_str(), EntityTransforms[ientidx].c_str() ) );

			ientidx++;
		}
		else if( pobj->GetClass()->IsSubclassOf( EntityArchetype::GetClassStatic() ) )
		{
			EntityArchetype *parch = safe_cobject_downcast<EntityArchetype>( pobj );
			
			CHECK( parch->GetName() == ArchetypeNameVect[ iarchidx ] );
			CHECK( parch->GetClass()->GetName() == ArchetypeClassVect[ iarchidx ] );
			iarchidx++;
		}	
	}
		
	///////////////////////////////////////////////////////////////////

	Editor.EditorNewScene(CTestScene::GetClassNameStatic());
	#endif
}


} // namespace SceneTest
