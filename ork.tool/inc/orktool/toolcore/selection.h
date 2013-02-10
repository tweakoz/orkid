////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef _ORK_TOOLCORE_SELECT_H
#define _ORK_TOOLCORE_SELECT_H

///////////////////////////////////////////////////////////////////////////////

#if defined( ORK_CONFIG_EDITORBUILD )

#include <ork/kernel/core/singleton.h>
#include <ork/math/TransformNode.h>
#include <ork/object/Object.h>
#include <ork/object/AutoConnector.h>

///////////////////////////////////////////////////////////////////////////////

namespace ork {
namespace tool {

///////////////////////////////////////////////////////////////////////////////
/// The SelectManager tracks selected objects in an editor application

class SelectManager : public ork::AutoConnector
{
	RttiDeclareAbstract( SelectManager, ork::AutoConnector );

	////////////////////////////////////////////////////////////
	DeclarePublicSignal( ObjectSelected );
	DeclarePublicSignal( ObjectDeSelected );
	DeclarePublicSignal( ClearSelection );
	////////////////////////////////////////////////////////////
	DeclarePublicAutoSlot( ObjectDeleted );
	////////////////////////////////////////////////////////////

public:
	
	void ToggleSelection( ork::Object *pobj );					/// if an object is selected, deselect it, otherwise select it
	void AddObjectToSelection( ork::Object *pobj );
	void RemoveObjectFromSelection( ork::Object *pobj );
	bool IsObjectSelected( const ork::Object *pobj ) const ;
	void ClearSelection();

	const orkset<ork::Object*> & GetActiveSelection() const;

	SelectManager();

private:

	void SlotObjectDeleted( ork::Object* pobj );

	void SigObjectSelected(ork::Object *pobj);
	void SigObjectDeSelected(ork::Object *pobj);
	void SigClearSelection();

	void InternalClearSelection();
	void InternalAddObjectToSelection( ork::Object *pobj );
	void InternalRemoveObjectFromSelection( ork::Object *pobj );
	
	orkset<ork::Object*>	mActiveSelectionSet;

};

///////////////////////////////////////////////////////////////////////////////

}
}

#endif // ORK_CONFIG_EDITORBUILD
#endif // _ORK_TOOLCORE_SELECT_H
