////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/kernel/string/string.h>
#include <ork/reflect/RegisterProperty.h>

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::SelectManager, "SelectionManager");

namespace ork { namespace tool {

void SelectManager::Describe() 
{
	RegisterAutoSignal( SelectManager, ObjectSelected );
	RegisterAutoSignal( SelectManager, ObjectDeSelected );
	RegisterAutoSignal( SelectManager, ClearSelection );

	RegisterAutoSlot( SelectManager, ObjectDeleted );
}

SelectManager::SelectManager()
	: ConstructAutoSlot(ObjectDeleted)
{
	SetupSignalsAndSlots();
}

///////////////////////////////////////////////////////////////////////////////

const orkset<ork::Object*> & SelectManager::GetActiveSelection( void ) const
{
	return mActiveSelectionSet;
}

///////////////////////////////////////////////////////////////////////////////

void SelectManager::SigObjectSelected(ork::Object *pobj)
{
	mSignalObjectSelected(&SelectManager::SigObjectSelected, pobj);
}

///////////////////////////////////////////////////////////////////////////////

void SelectManager::SigObjectDeSelected(ork::Object *pobj)
{
	mSignalObjectDeSelected(&SelectManager::SigObjectDeSelected, pobj);
}

void SelectManager::SigClearSelection()
{
	mSignalClearSelection(&SelectManager::SigClearSelection);
}

///////////////////////////////////////////////////////////////////////////////
// object has been deleted externally,
// lets make sure it is unselected

void SelectManager::SlotObjectDeleted(ork::Object *pobj)
{
	//RemoveObjectFromSelection( pobj );
	InternalRemoveObjectFromSelection( pobj );
}

///////////////////////////////////////////////////////////////////////////////

void SelectManager::ToggleSelection( ork::Object *pobj )
{
	if( IsObjectSelected( pobj ) )
	{
		RemoveObjectFromSelection( pobj );
	}
	else
	{
		AddObjectToSelection( pobj );
	}
}

///////////////////////////////////////////////////////////////////////////////

void SelectManager::ClearSelection()
{
	InternalClearSelection();
	SigClearSelection();
}

///////////////////////////////////////////////////////////////////////////////

void SelectManager::AddObjectToSelection( ork::Object *pobj )
{
	printf( "SelectManager::AddObjectToSelection<%p>\n", pobj );
	InternalAddObjectToSelection(pobj);
}

void SelectManager::InternalAddObjectToSelection( ork::Object *pobj )
{
	if( pobj )
	{
		bool badded = OrkSTXSetInsert( mActiveSelectionSet, pobj );
		if( badded )
		{
			SigObjectSelected( pobj );
		}
	}	
}

///////////////////////////////////////////////////////////////////////////////

void SelectManager::RemoveObjectFromSelection( ork::Object *pobj )
{
	InternalRemoveObjectFromSelection(pobj);
}

void SelectManager::InternalRemoveObjectFromSelection( ork::Object *pobj )
{
	bool bremoved = OrkSTXRemoveFromSet( mActiveSelectionSet, pobj );
	if( bremoved )
		SigObjectDeSelected( pobj );
}

///////////////////////////////////////////////////////////////////////////////

bool SelectManager::IsObjectSelected( const ork::Object *pobj ) const 
{
	return OrkSTXIsInSet( mActiveSelectionSet, const_cast<ork::Object*>( pobj ) );
}

///////////////////////////////////////////////////////////////////////////////

void SelectManager::InternalClearSelection()
{
	for( orkset<ork::Object *>::iterator it=mActiveSelectionSet.begin(); it!=mActiveSelectionSet.end(); it++ )
	{
		SigObjectDeSelected( *it );
	}
	mActiveSelectionSet.clear();
}

///////////////////////////////////////////////////////////////////////////////

} }
