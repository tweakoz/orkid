////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_GFX_SCENE_HPP_
#define _ORK_GFX_SCENE_HPP_

#include <ork/rtti/downcast.h>
#include <ork/application/application.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {

class CCameraData;

namespace lev2 { class XgmModel; }
namespace lev2 { class XgmModelInst; }
namespace lev2 { class Renderer; }
namespace lev2 { class LightManager; }

namespace ent {

///////////////////////////////////////////////////////////////////////////////
/// SceneData is the "model" of the scene that is serialized and edited, and thats it....
/// this should never get subclassed
///////////////////////////////////////////////////////////////////////////////

template <typename T> T* SceneData::FindTypedObject( const PoolString& pstr )
{
	SceneObject* cd = FindSceneObjectByName( pstr );
	T* pret = (cd==0) ? 0 : rtti::safe_downcast<T*>( cd );
	return pret;
}
template <typename T> const T* SceneData::FindTypedObject( const PoolString& pstr ) const
{
	const SceneObject* cd = FindSceneObjectByName( pstr );
	T* pret = (cd==0) ? 0 : rtti::safe_downcast<T*>( cd );
	return pret;
}
template <typename T > 
T* SceneData::GetTypedSceneComponent() const
{
	const ork::object::ObjectClass* pclass = T::GetClassStatic();
	SceneComponentLut::const_iterator it = mSceneComponents.find( pclass );
	return (it==mSceneComponents.end()) ? 0 : rtti::autocast(it->second);
}


///////////////////////////////////////////////////////////////////////////////

template <typename T> T* SceneComposer::Register()
{
	ork::object::ObjectClass* pclass = T::GetClassStatic();
	orklut<const object::ObjectClass*,SceneComponentData*>::iterator it = mComponents.find(pclass);
	SceneComponentData* pobj = (it==mComponents.end()) ? 0 : it->second;
	if( 0 == pobj )
	{
		pobj = rtti::autocast( pclass->CreateObject() );
		mComponents.AddSorted( pclass, pobj );
	}
	T* rval = rtti::autocast( pobj );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
/// SceneInst is all the work data associated with running a scene
/// this might be subclassed
///////////////////////////////////////////////////////////////////////////////

template <typename T> T* SceneInst::FindTypedEntityComponent( const PoolString& entname ) const
{	T* pret = 0;
	Entity* pent = FindEntity( entname );
	if( pent )
	{
		pret = pent->GetTypedComponent<T>();
	}
	return pret;
}
template <typename T > 
T* SceneInst::FindTypedSceneComponent() const
{
	const ork::object::ObjectClass* pclass = T::GetClassStatic();
	SceneComponentLut::const_iterator it = mSceneComponents.find( pclass );
	return (it==mSceneComponents.end()) ? 0 : rtti::autocast(it->second);
}

///////////////////////////////////////////////////
template <typename T> T* SceneInst::FindTypedEntityComponent( const char* entname ) const
{
	return FindTypedEntityComponent<T>( AddPooledString( entname ) );
}

///////////////////////////////////////////////////////////////////////////////

} }

#endif // !_ORK_GFX_SCENE_H_
