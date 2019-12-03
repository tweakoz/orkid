////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/application/application.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/scene.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/reflect/AccessorObjectPropertyType.hpp>
#include <ork/rtti/downcast.h>
#include <ork/reflect/DirectObjectVectorPropertyType.h>
#include <ork/reflect/DirectObjectVectorPropertyType.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.h>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneObjectClass, "SceneObjectClass")
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::SceneObject,"Ent3dSceneObject");
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::DagNode,"Ent3dDagNode");
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SceneGroup, "Ent3dSceneGroup" );
INSTANTIATE_TRANSPARENT_RTTI( ork::ent::SceneDagObject, "Ent3dSceneDagObject" );
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SceneObject::Describe()
{
	ArrayString<64> arrstr;
	MutableString mutstr(arrstr);
	mutstr.format("SceneObject");
	GetClassStatic()->SetPreferredName(arrstr);

	// Name must be registered for the case that a SceneObject does not live inside a Scene and exists only by Reference from a Spawner
	reflect::RegisterProperty("Name", &SceneObject::mName);
	reflect::RegisterFunctor("GetName", &SceneObject::GetName);
	reflect::annotatePropertyForEditor<SceneObject>( "Name", "editor.visible", "false" );
}

SceneObject::SceneObject()
{
}

void SceneObject::SetName( PoolString name )
{
	mName = name;
}
void SceneObject::SetName( const char* name )
{
	mName = AddPooledString( name );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SceneDagObject::Describe()
{
	reflect::annotateClassForEditor<SceneDagObject>( "editor.3dpickable", ConstString("true") );
	reflect::annotateClassForEditor<SceneDagObject>( "editor.3dxfable", true );
	reflect::annotateClassForEditor<SceneDagObject>( "editor.3dxfinterface", ConstString("SceneDagObjectManipInterface") );
	reflect::RegisterProperty( "DagNode", & SceneDagObject::AccessDagNode );
	reflect::RegisterProperty( "Parent", & SceneDagObject::mParentName );

	reflect::annotatePropertyForEditor<SceneDagObject>( "DagNode", "editor.visible", "false" );
	reflect::annotatePropertyForEditor<SceneDagObject>( "Parent", "editor.visible", "false" );

}
SceneDagObject::SceneDagObject()
	: mDagNode( this )
	, mParentName( AddPooledString( "scene" ) )
{
}
SceneDagObject::~SceneDagObject()
{
}
void SceneDagObject::SetParentName( const PoolString& pname )
{
	mParentName = pname;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void SceneGroup::Describe()
{
	reflect::annotateClassForEditor<SceneGroup>( "editor.3dpickable", ConstString("true") );
	reflect::annotateClassForEditor<SceneGroup>( "editor.3dxfable", true );
	reflect::annotateClassForEditor<SceneGroup>( "editor.3dxfinterface", ConstString("SceneDagObjectManipInterface") );
}
///////////////////////////////////////////////////////////////////////////////
SceneGroup::SceneGroup()
{
}
///////////////////////////////////////////////////////////////////////////////
SceneGroup::~SceneGroup()
{
}
///////////////////////////////////////////////////////////////////////////////
void SceneGroup::AddChild( SceneDagObject* pobj )
{	mChildren.push_back(pobj);
	DagNode& cnode = pobj->GetDagNode();
	DagNode& pnode = GetDagNode();
	pnode.AddChild( & cnode );
}
///////////////////////////////////////////////////////////////////////////////
void SceneGroup::RemoveChild( SceneDagObject* pobj )
{	orkvector<SceneDagObject*>::iterator it = std::find( mChildren.begin(), mChildren.end(), pobj );
	OrkAssert( it != mChildren.end() );
	mChildren.erase( it );
	DagNode& cnode = pobj->GetDagNode();
	DagNode& pnode = GetDagNode();
	pnode.RemoveChild( & cnode );
	pobj->SetParentName( AddPooledString( "scene" ) );
}
///////////////////////////////////////////////////////////////////////////////
void SceneGroup::UnGroupAll()
{
	const PoolString& parname = GetParentName();

	while( mChildren.empty() == false )
	{
		SceneDagObject* child = *mChildren.begin();
		RemoveChild( child );
	
		if( 0 == strcmp( parname.c_str(), "scene" ) )
		{
			
		}
		else
		{
			
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void DagNode::Describe()
{
	reflect::RegisterProperty( "TransformNode", & DagNode::mTransformNode3D );
}
///////////////////////////////////////////////////////////////////////////////
DagNode::DagNode( const ork::rtti::ICastable* powner )
	: mpOwner( powner )
{
}
///////////////////////////////////////////////////////////////////////////////
void DagNode::AddChild( DagNode* pobj )
{	mChildren.push_back(pobj);
	const auto& this_xfn = GetTransformNode();
	pobj->GetTransformNode().SetParent( & this_xfn );
}
///////////////////////////////////////////////////////////////////////////////
void DagNode::RemoveChild( DagNode* pobj )
{	orkvector<DagNode*>::iterator it = std::find( mChildren.begin(), mChildren.end(), pobj );
	OrkAssert( it != mChildren.end() );
	mChildren.erase( it );
	pobj->GetTransformNode().UnParent();
}
///////////////////////////////////////////////////////////////////////////////
void DagNode::CopyTransformMatrixFrom( const DagNode& other )
{
	const auto& other_mtx = other.GetTransformNode().GetTransform().GetMatrix();
	GetTransformNode().GetTransform().SetMatrix( other_mtx );
}
void DagNode::SetTransformMatrix( const fmtx4& mtx )
{
	GetTransformNode().GetTransform().SetMatrix( mtx );
}

}}
///////////////////////////////////////////////////////////////////////////////
