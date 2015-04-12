////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if 0

#include <pkg/ent3d/ScriptComponent.h>

#include <ork/kernel/orklut.hpp>

#include <pkg/ent3d/entity.h>

#define DEBUG_SCRIPT_COMPONENT_VERBOSE	0

#if DEBUG_SCRIPT_COMPONENT_VERBOSE
# define DEBUG_SCRIPT_COMPONENT		1
#else
# define DEBUG_SCRIPT_COMPONENT		0
#endif

static const Char4 ScriptComponentName = "cscr";

INSTANTIATE_RTTI(jelly::ScriptComponentFactory, "ScriptComponentFactory")
INSTANTIATE_TRANSPARENT_CASTABLE(jelly::ScriptComponent)
INSTANTIATE_RTTI(jelly::StartNamedScripts, "StartNamedScripts")
INSTANTIATE_RTTI(jelly::StartScript, "StartScript")

template class ork::orklut<ork::PoolString, jelly::EventScript::Player>;

namespace ork { namespace ent3d {

void ScriptComponentFactory::ClassInit()
{
	GetClassStatic();

	AddProperty(OrkStackNew ork::CLutContainerProp<ork::PoolString, ork::PoolString>("NamedScripts", ork::CProp::EFLAG_NONE, PROP_OFFSET(ScriptComponentFactory, mNamedScripts)));
	EditorAnnotateProperty("NamedScripts", "Editor.ChoiceList", "Scene.Scripts");

	AddProperty(OrkStackNew ork::CVectorContainerProp<ork::PoolString>("StartupScriptNames", ork::CProp::EFLAG_NONE, PROP_OFFSET(ScriptComponentFactory, mStartupScriptNames)));
}

void ScriptComponentFactory::Describe()
{
	ork::reflect::RegisterMapProperty("NamedScripts", &ScriptComponentFactory::mNamedScripts);
	ork::reflect::RegisterArrayProperty("StartupScriptNames", &ScriptComponentFactory::mStartupScriptNames);
}

ScriptComponentFactory::ScriptComponentFactory() : mNamedScripts(ork::EKEYPOLICY_MULTILUT)
{
}

const ork::orklut<ork::PoolString, ork::PoolString> &ScriptComponentFactory::GetNamedScripts() const
{
	return mNamedScripts;
}

ork::orklut<ork::PoolString, ork::PoolString> &ScriptComponentFactory::GetNamedScripts()
{
	return mNamedScripts;
}

const orkvector<ork::PoolString> &ScriptComponentFactory::GetStartupScriptNames() const
{
	return mStartupScriptNames;
}

orkvector<ork::PoolString> &ScriptComponentFactory::GetStartupScriptNames()
{
	return mStartupScriptNames;
}

const ork::ComponentFamily& ScriptComponentFactory::GetFamily() const
{
	static ork::ComponentFamily family(ScriptComponentName);
	return family;
}

ork::EntityComponent* ScriptComponentFactory::CreateComponent(ork::Entity* pent, const ork::PoolString& name) const
{
	return OrkNew ScriptComponent(pent, name, this);
}

void ScriptComponent::Describe()
{
	ork::reflect::RegisterFunctor("StartScripts", &ScriptComponent::StartScripts);
	ork::reflect::RegisterFunctor("StopScripts", &ScriptComponent::StopScripts);
}

ScriptComponent::ScriptComponent(ork::Entity* entity, const ork::PoolString& name, const ScriptComponentFactory *factory)
	: ork::EntityComponent(name, factory)
	, mFactory(factory)
{
	OrkAssert(mFactory);
	SetEntity(entity);
}

ScriptComponent::~ScriptComponent()
{
}

void ScriptComponent::DoResolveDependencies(ork::EntityComponentTable::ComponentLut& components)
{
	ork::EntityComponentTable::ComponentBounds bounds = GetEntity()->GetComponentsByFamily(ork::EnvironmentCollisionInfoComponent::GetFamilyStatic());
	for(ork::EntityComponentTable::ComponentLut::const_iterator it = bounds.first; it != bounds.second; ++it)
	{
		if(const ork::EnvironmentCollisionInfoComponent *collision = ork::rtti::downcast<const ork::EnvironmentCollisionInfoComponent *>(it->second))
		{
			for(int em = 0; em < collision->GetData()->GetNumEdgeMeshes(); em++)
			{
				ork::ecic_edgemesh *edgemesh = collision->GetData()->GetEdgeMesh(em);
				for(int e = 0; e < edgemesh->GetNumEdges(); e++)
				{
					ork::ecic_edge *edge = edgemesh->GetEdge(e);
					CompileScriptPlayer(edge->GetScript());
				}
			}

			for(int am = 0; am < collision->GetData()->GetNumAreaMeshes(); am++)
			{
				ork::ecic_areamesh *areamesh = collision->GetData()->GetAreaMesh(am);
				CompileScriptPlayer(areamesh->GetScript());
			}
		}
	}

	const ork::orklut<ork::PoolString, ork::PoolString> &named_scripts = mFactory->GetNamedScripts();
	for(ork::orklut<ork::PoolString, ork::PoolString>::const_iterator it = named_scripts.begin(); it != named_scripts.end(); ++it)
		CompileScriptPlayer(it->second);

	const orkvector<ork::PoolString> &startup_scripts = mFactory->GetStartupScriptNames();
	for(orkvector<ork::PoolString>::const_iterator it = startup_scripts.begin(); it != startup_scripts.end(); ++it)
		StartScripts(*it);
}

const ork::ComponentFamily& ScriptComponent::GetFamily() const
{
	static ork::ComponentFamily family(ScriptComponentName);
	return family;
}

void ScriptComponent::Update()
{
#if DEBUG_SCRIPT_COMPONENT_VERBOSE
	ork::orkprintf("Entity %s : ScriptComponent::Update()\n", GetEntity()->GetName());
#endif

	for(ork::orklut<ork::PoolString, EventScript::Player>::iterator it = mScriptPlayers.begin(); it != mScriptPlayers.end(); it++)
	{
		EventScript::Player &player = it->second;

#if DEBUG_SCRIPT_COMPONENT_VERBOSE
		if(player.mTime >= ork::CReal::Zero())
			ork::orkprintf("  Script %s : player.Advance()\n", it->first.c_str());
#endif

		player.Advance();

		// needed to allow access to player for script intrinsics
		EventScript::PlayerExecuteContext context(&player);

		const CompiledScript *script;
		while(player.GetNext(script))
		{
#if DEBUG_SCRIPT_COMPONENT_VERBOSE
			ork::orkprintf("    Time %g : script->Evaluate()\n", player.mTime.FloatCast());
#endif

			script->Evaluate();
		}
	}
}

void ScriptComponent::Notify(const ork::EntityEvent* event)
{
	if(const StartNamedScripts *start = ork::rtti::downcast<const StartNamedScripts *>(event))
		StartScripts(start->GetName());
	else if(const StartScript *start = ork::rtti::downcast<const StartScript *>(event))
	{
#if DEBUG_SCRIPT_COMPONENT
	ork::orkprintf("Entity %s : ScriptComponent::Notify(StartScript)\n", GetEntity()->GetName());
#endif

		ork::orklut<ork::PoolString, EventScript::Player>::iterator spit = mScriptPlayers.find(start->GetName());
		if(spit != mScriptPlayers.end())
		{
#if DEBUG_SCRIPT_COMPONENT
			ork::orkprintf("  Script %s : EventScript::Player.Start()\n", spit->first.c_str());
#endif

			spit->second.Start();
		}
	}
}

void ScriptComponent::CompileScriptPlayer(ork::PoolString script)
{
#if DEBUG_SCRIPT_COMPONENT
	ork::orkprintf("Entity %s : ScriptComponent::CompileScriptPlayer(%s)\n", GetEntity()->GetName(), script.c_str());
#endif

	ork::orklut<ork::PoolString, EventScript::Player>::iterator spit = mScriptPlayers.find(script);
	if(spit == mScriptPlayers.end())
	{
		spit = mScriptPlayers.AddSorted(script, EventScript::Player());

		ork::ResizableString scriptPath;
		scriptPath.format("data://scripts/%s.txt", script.c_str());

		ScriptSelfContext context(GetEntity());

		EventScript* script = LoadScript(scriptPath);

		if(script)
			spit->second.Init(*script);
		else
			ork::orkprintf("WARNING: Script %s not found!\n", scriptPath.c_str());
	}
}

void ScriptComponent::StartScripts(ork::PoolString name)
{
#if DEBUG_SCRIPT_COMPONENT
	ork::orkprintf("Entity %s : ScriptComponent::StartScripts(%s)\n", GetEntity()->GetName(), name.c_str());
#endif

	const ork::orklut<ork::PoolString, ork::PoolString> &named_scripts = mFactory->GetNamedScripts();
	for(ork::orklut<ork::PoolString, ork::PoolString>::const_iterator it = named_scripts.find(name);
			it != named_scripts.end() && it->first == name; ++it)
	{
		ork::orklut<ork::PoolString, EventScript::Player>::iterator spit = mScriptPlayers.find(it->second);
		if(spit != mScriptPlayers.end())
		{
#if DEBUG_SCRIPT_COMPONENT
			ork::orkprintf("  Script %s : EventScript::Player.Start()\n", spit->first.c_str());
#endif

			spit->second.Start();
		}
	}
}

void ScriptComponent::StopScripts(ork::PoolString name)
{
#if DEBUG_SCRIPT_COMPONENT
	ork::orkprintf("Entity %s : ScriptComponent::StopScripts(%s)\n", GetEntity()->GetName(), name.c_str());
#endif

	const ork::orklut<ork::PoolString, ork::PoolString> &named_scripts = mFactory->GetNamedScripts();
	for(ork::orklut<ork::PoolString, ork::PoolString>::const_iterator it = named_scripts.find(name);
			it != named_scripts.end() && it->first == name; ++it)
	{
		ork::orklut<ork::PoolString, EventScript::Player>::iterator spit = mScriptPlayers.find(it->second);
		if(spit != mScriptPlayers.end())
		{
#if DEBUG_SCRIPT_COMPONENT
			ork::orkprintf("  Script %s : EventScript::Player.Stop()\n", spit->first.c_str());
#endif

			spit->second.Stop();
		}
	}
}

StartNamedScripts::StartNamedScripts(ork::PieceString name)
{
	SetName(name);
}

StartNamedScripts::StartNamedScripts(ork::PoolString name) : mName(name)
{
}

void StartNamedScripts::SetName(ork::PieceString name)
{
	mName = ork::AddPooledString(name);
}

void StartNamedScripts::SetName(ork::PoolString name)
{
	mName = name;
}

ork::PoolString StartNamedScripts::GetName() const
{
	return mName;
}

StartScript::StartScript(ork::PieceString name)
{
	SetName(name);
}

StartScript::StartScript(ork::PoolString name) : mName(name)
{
}

void StartScript::SetName(ork::PieceString name)
{
	mName = ork::AddPooledString(name);
}

void StartScript::SetName(ork::PoolString name)
{
	mName = name;
}

ork::PoolString StartScript::GetName() const
{
	return mName;
}

} }  // ork::ent3d

#endif
