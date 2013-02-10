////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
#include <ork/lev2/qtui/qtui.hpp>
#include <ork/reflect/Functor.h>
#include <ork/reflect/RegisterProperty.h>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/editor/editor.h>
///////////////////////////////////////////////////////////////////////////////
#include "outliner.h"

INSTANTIATE_TRANSPARENT_RTTI(ork::ent::OutlinerModel, "OutlinerModel")
//INSTANTIATE_TRANSPARENT_RTTI(ork::ent::OutlinerNode, "OutlinerNode")

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

void OutlinerModel::Describe()
{
	reflect::RegisterFunctor("SlotSceneTopoChanged", &OutlinerModel::SlotSceneTopoChanged);
}

///////////////////////////////////////////////////////////////////////////////

OutlinerModel::OutlinerModel(SceneEditorBase&ed)
	: mEditor( ed )
	, mSceneIndex(createIndex(0,0,nullptr))

{
	SlotSceneTopoChanged();
}

///////////////////////////////////////////////////////////////////////////////

void OutlinerModel::SlotSceneTopoChanged()
{
	layoutChanged();
}

///////////////////////////////////////////////////////////////////////////////

OutlinerModel::~OutlinerModel()
{
}

///////////////////////////////////////////////////////////////////////////////

QVariant OutlinerModel::data(const QModelIndex& index, int role) const
{
	if(!index.isValid())
		return QVariant();

	if(role != Qt::DisplayRole && role != Qt::EditRole)
		return QVariant();

	SceneObject* pobj = static_cast<SceneObject *>(index.internalPointer());
	if(index.column() == 0)
	{
		//OutlinerNode *node = GetNodeForIndex( index ); //;
		const char* name = pobj ? pobj->GetName().c_str() : "SCENE";
		return QVariant(name);
	}
	else if(index.column() == 1)
	{
		if( pobj )
		{
			if( pobj->GetClass() == EntData::GetClassStatic() )
			{
				return QVariant( "entdata" );
			}
			else if( pobj->GetClass() == SceneGroup::GetClassStatic() )
			{
				return QVariant( "group" );
			}
			else if( pobj->GetClass()->IsSubclassOf(Archetype::GetClassStatic()) )
			{
				return QVariant( "archetype" );
			}
		}
		return QVariant();
	}
	else
		return QVariant();
}

///////////////////////////////////////////////////////////////////////////////

Qt::ItemFlags OutlinerModel::flags(const QModelIndex& index) const
{
	if(!index.isValid()) // we cant edit or select the scene root
		return Qt::ItemIsEnabled;

	return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;

	// unreachable
	/*if(index.column() == 0) // only allow editing of the scene objects name, not its type
		return Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable;
	return Qt::ItemIsEnabled | Qt::ItemIsSelectable;*/
}
///////////////////////////////////////////////////////////////////////////////

QVariant OutlinerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
	if(orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case 0: return QVariant("Name");
			case 1: return QVariant("Type");
		}
	}

	return QVariant();
}

///////////////////////////////////////////////////////////////////////////////

bool OutlinerModel::IsScene( const QModelIndex& idx ) const
{
	return idx.isValid() ? (idx.internalPointer()==0) : false;
}

///////////////////////////////////////////////////////////////////////////////

bool OutlinerModel::IsDag( const QModelIndex& idx ) const
{
	if( false == idx.isValid() ) return false;
	if( IsScene(idx) ) return false;
	ork::Object* pobj = static_cast<ork::Object*>( idx.internalPointer() );
	return rtti::downcast<SceneDagObject*>( pobj ) != 0;
}

///////////////////////////////////////////////////////////////////////////////

bool OutlinerModel::IsOther( const QModelIndex& idx ) const
{
	if( false == idx.isValid() ) return false;
	if( IsScene(idx) ) return false;
	if( IsDag(idx) ) return false;
	SceneObject* pobj = static_cast<SceneObject*>( idx.internalPointer() );
	return rtti::downcast<SceneObject*>( pobj ) != 0;
}

///////////////////////////////////////////////////////////////////////////////

QModelIndex OutlinerModel::FindIndex( const SceneObject* pobj ) const
{
	int icidx = ChildIndex( pobj );
	return createIndex(icidx,0,(void*)pobj);
}

///////////////////////////////////////////////////////////////////////////////

SceneObject* OutlinerModel::SceneChild( int idx ) const
{	
	int icnt = 0;
	SceneObject* pret = 0;
	if( mEditor.mpScene )
	{	orkmap<PoolString, SceneObject*>& objs = mEditor.mpScene->GetSceneObjects();
		for( orkmap<PoolString, SceneObject*>::const_iterator it=objs.begin(); it!=objs.end(); it++ )
		{	SceneObject* pobj = (*it).second;
			if( icnt == idx )
			{
				pret = pobj;
			}
			icnt++;
		}
	}
	return pret;
}

///////////////////////////////////////////////////////////////////////////////

int OutlinerModel::ChildIndex( const SceneObject* pobj ) const
{	OrkAssert( mEditor.mpScene );
	int icnt = 0;
	if( mEditor.mpScene )
	{	
		orkmap<PoolString, SceneObject*>& objs = mEditor.mpScene->GetSceneObjects();
		for( orkmap<PoolString, SceneObject*>::const_iterator it=objs.begin(); it!=objs.end(); it++ )
		{	SceneObject* ptst = (*it).second;
			if( ptst == pobj )
			{	return icnt;
			}
			icnt++;
		}
		return icnt;
	}
	OrkAssert( false );
	return -1;
}

///////////////////////////////////////////////////////////////////////////////

QModelIndex OutlinerModel::index(int row, int column, const QModelIndex& parent) const
{
	SceneObject* parentsobj = static_cast<SceneObject*>(parent.internalPointer());

	//orkprintf( "index(%d,%d):parent(%d,%d,%08x): ", row, column,  parent.row(), parent.column(), parent.internalPointer() );

	////////////////////////////
	if( parent.isValid() )
	////////////////////////////
	{
		OrkAssert( row >= 0 );
		OrkAssert( column >= 0 );
		
		if( IsScene(parent) && mEditor.mpScene )
		{
			OrkAssert( mEditor.mpScene );

			SceneObject* pobj = SceneChild(row);

			return createIndex(row,column,pobj); 

		}
		else
		{
//			OrkAssert(false);
			return QModelIndex();
		}
	}
	////////////////////////////
	else
	////////////////////////////
	{
		//orkprintf( "childNV<%08x:(%d,0,%08x)>\n", 0, 0, 0 );
		return mSceneIndex;
	}
}

///////////////////////////////////////////////////////////////////////////////

QModelIndex OutlinerModel::parent(const QModelIndex& index) const
{
	if( IsScene(index) )
	{
		return QModelIndex();
	}
	else
	{
		return mSceneIndex;
	}
}

///////////////////////////////////////////////////////////////////////////////

int OutlinerModel::rowCount(const QModelIndex& index) const
{
	int iret = 0;

	if( index.isValid() )
	{
		bool bscn = IsScene(index);

		if( bscn )
		{
			if( mEditor.mpScene )
			{	orkmap<PoolString, SceneObject*>& objs = mEditor.mpScene->GetSceneObjects();

				iret = (int) objs.size();
			}
		}
		else
		{
			iret = 0;
		}
	}
	else
	{
		iret = 1;
	}

	//orkprintf( "rowcount<%d,%d,%08x><%d>\n", index.row(), index.column(), index.internalPointer(), iret );

	return iret;
}

///////////////////////////////////////////////////////////////////////////////

int OutlinerModel::columnCount(const QModelIndex& parent) const
{
	return 2;
}

///////////////////////////////////////////////////////////////////////////////

bool OutlinerModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
	if(!index.isValid())
		return false;

	if(index.column() != 0)
		return false;

	if(!value.isValid())
		return false;

	ChangeNodeName( index, qvariant_cast<QString>(value).toUtf8().data());
	//emit dataChanged(index, index);

	return true;
}

///////////////////////////////////////////////////////////////////////////////

#if 0 
void OutlinerModel::BeginOutlinerNodeInsert(OutlinerNode *node, int child_index)
{
	/*QList<int> mRowIndices;
	OutlinerNode *parentnode = node->GetParent();
	while(parentnode)
	{
		int index = parentnode->GetIndex();
		mRowIndices.push_front(index);
		parentnode = parentnode->GetParent();
	}

	QModelIndex parentindex;
	for(QList<int>::const_iterator it = mRowIndices.begin(); it != mRowIndices.end(); it++)
		parentindex = index(*it, 0, parentindex);

	emit beginInsertRows(parentindex, child_index, child_index);

	if(!node->GetParent())
		mRootNode = node;
*/
}

///////////////////////////////////////////////////////////////////////////////

void OutlinerModel::OutlinerNodeInserted(OutlinerNode *node)
{
	emit endInsertRows();
}

///////////////////////////////////////////////////////////////////////////////

void OutlinerModel::BeginOutlinerNodeRemove(OutlinerNode *node)
{
/*	QList<int> mRowIndices;
	OutlinerNode *parentnode = node->GetParent();
	while(parentnode)
	{
		int index = parentnode->GetIndex();
		mRowIndices.push_front(index);
		parentnode = parentnode->GetParent();
	}

	QModelIndex parentindex;
	for(QList<int>::const_iterator it = mRowIndices.begin(); it != mRowIndices.end(); it++)
		parentindex = index(*it, 0, parentindex);

	emit beginRemoveRows(parentindex, node->GetIndex(), node->GetIndex());

	if(!node->GetParent())
		mRootNode = NULL;*/
}

///////////////////////////////////////////////////////////////////////////////

void OutlinerModel::OutlinerNodeRemoved(OutlinerNode *parent, OutlinerNode *node)
{
	emit endRemoveRows();
}
#endif

///////////////////////////////////////////////////////////////////////////////

//void OutlinerModel::OutlinerNodeNameChanged(OutlinerNode *node)
//{
//}

///////////////////////////////////////////////////////////////////////////////
// name changed FROM outliner

void OutlinerModel::ChangeNodeName(const QModelIndex& index, std::string name )
{
	ork::Object* pobj = static_cast<ork::Object*>( index.internalPointer() );

	if( pobj )
	{
		SceneObject* psobj = rtti::downcast<SceneObject*>(pobj);
		mEditor.EditorRenameSceneObject( psobj, name.c_str() );
	}
	this->SlotSceneTopoChanged();
	//this->Refresh();
	//mChangeNodeNameSignal( & OutlinerModel::ChangeNodeName, node, name );
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
