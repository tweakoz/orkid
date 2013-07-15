////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
#include <ork/kernel/prop.h>
#include <ork/kernel/opq.h>
#include <ork/lev2/qtui/qtui.hpp>
#include <ork/reflect/Functor.h>
#include <ork/reflect/RegisterProperty.h>
///////////////////////////////////////////////////////////////////////////////
#include <QtGui/QScrollBar>
///////////////////////////////////////////////////////////////////////////////
#include <pkg/ent/scene.h>
#include <pkg/ent/entity.h>
#include <pkg/ent/editor/outliner.h>
#include <pkg/ent/editor/editor.h>
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI(ork::ent::OutlinerView, "OutlinerView")
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////
ImplementMoc( QtOutlinerWindow, QWidget );
ImplementMoc(OutlinerView,QTreeView);

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
QtOutlinerWindow * QtOutlinerWindow::gpWindow = 0;
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
QtOutlinerWindow::QtOutlinerWindow( bool bfloat, QWidget *pparent, SceneEditorBase& ed )
	: QWidget(pparent)
	, mCurrentScene( 0 )
	, mOutlinerModel( ed )
	, mOutlinerView( this )
{
	mOutlinerView.setModel( & mOutlinerModel );

	QObject* selmodel = mOutlinerView.selectionModel();

	bool bconOK = mOutlinerView.QObject::connect( selmodel,
									SIGNAL(selectionChanged(const QItemSelection&, const QItemSelection&)),
									SLOT(UserSelectionChanged(const QItemSelection&, const QItemSelection&)) );

	OrkAssert( bconOK );

}
///////////////////////////////////////////////////////////////////////////////
QtOutlinerWindow::~QtOutlinerWindow()
{
	gpWindow = 0;
}
///////////////////////////////////////////////////////////////////////////////
void QtOutlinerWindow::MocInit( void )
{
}
///////////////////////////////////////////////////////////////////////////////
void QtOutlinerWindow::AttachToSceneData( SceneData* pscene )
{
	mCurrentScene = pscene;
}
void QtOutlinerWindow::resizeEvent ( QResizeEvent * event )
{
	int iw = event->size().width();
	int ih = event->size().height();
	mOutlinerView.resize( iw, ih );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::Describe()
{
	reflect::RegisterFunctor("SlotObjectSelected", & OutlinerView::SlotObjectSelected );
	reflect::RegisterFunctor("SlotObjectDeSelected", & OutlinerView::SlotObjectDeSelected );
}
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::MocInit()
{	Moc.AddSlot2( "UserSelectionChanged(QItemSelection,QItemSelection)", "const QItemSelection&, const QItemSelection&", & OutlinerView::UserSelectionChanged ); 
}
///////////////////////////////////////////////////////////////////////////////
OutlinerView::OutlinerView(QWidget* parent)
	: QTreeView(parent)
	, mBlockUser(false)
	, mOutlinerModel( 0 )
	, mInSlotFromSelectionManager( false )
{
	setSelectionMode(QAbstractItemView::SingleSelection);
	setSelectionBehavior(QAbstractItemView::SelectRows);
}
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::setModel(QAbstractItemModel *model)
{
	QTreeView::setModel(model);
	OutlinerModel*pm = static_cast<OutlinerModel*>( model );
	ork::Object* pobj = static_cast<ork::Object*>( pm );
	OrkAssert( pobj->GetClass() == OutlinerModel::GetClassStatic() );
	mOutlinerModel = pm;
}
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::UserNodeSelected(const QModelIndex& index)
{
	if( false == mInSlotFromSelectionManager ) // was selection changed INTERNALLY ?
	{	// yes changed INTERNALLY, go thru the SelectionManager
		ork::Object* pobj = static_cast<ork::Object*>( index.internalPointer() );
		if( pobj )
		{
			SceneObject* sobj = rtti::autocast( pobj );
			OrkAssert( sobj );
			mOutlinerModel->Editor().SelectionManager().AddObjectToSelection( pobj );
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::UserNodeDeselected(const QModelIndex& index)
{
	if( false == mInSlotFromSelectionManager ) // was selection changed INTERNALLY ?
	{	// yes changed INTERNALLY, go thru the SelectionManager
		ork::Object* pobj = static_cast<ork::Object*>( index.internalPointer() );
		if( pobj )
		{
			SceneObject* sobj = rtti::autocast( pobj );
			OrkAssert( sobj );
			mOutlinerModel->Editor().SelectionManager().RemoveObjectFromSelection( pobj );
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::UserSelectionChanged(QItemSelection selected, QItemSelection deselected)
{
	if(!mBlockUser)
	{
		for(QItemSelection::const_iterator it = deselected.begin(); it != deselected.end(); it++)
		{
			QItemSelectionRange range = *it;
			QModelIndex parent = range.parent();
			QModelIndex tl = range.topLeft();
			QModelIndex br = range.bottomRight();
			for(int row = tl.row(); row <= br.row(); row++)
			{
				QModelIndex index = model()->index(row, 0, parent);
				UserNodeDeselected(index);
			}
		}

		for(QItemSelection::const_iterator it = selected.begin(); it != selected.end(); it++)
		{
			QItemSelectionRange range = *it;
			QModelIndex parent = range.parent();
			QModelIndex tl = range.topLeft();
			QModelIndex br = range.bottomRight();
			for(int row = tl.row(); row <= br.row(); row++)
			{
				QModelIndex index = model()->index(row, 0, parent);
				UserNodeSelected(index);
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
// selection has changed EXTERNALLY
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::SlotObjectSelected( ork::Object* pobj )
{
	mInSlotFromSelectionManager = true;
	{
		SceneObject* psobj = rtti::autocast( pobj );

		if( mOutlinerModel && (psobj!=0) )
		{
			QModelIndex idx = mOutlinerModel->FindIndex( psobj );
			if( idx.isValid() )
			{
				selectionModel()->select(idx, QItemSelectionModel::Current|QItemSelectionModel::Select);
				scrollTo( idx );
			}
		}
	}
	mInSlotFromSelectionManager = false;
}
///////////////////////////////////////////////////////////////////////////////
// selection has changed EXTERNALLY
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::SlotObjectDeSelected( ork::Object* pobj )
{
	mInSlotFromSelectionManager = true;
	{
		SceneObject* psobj = rtti::autocast( pobj );

		if( mOutlinerModel && (psobj!=0) )
		{
			QModelIndex idx = mOutlinerModel->FindIndex( psobj );
			selectionModel()->select(idx, QItemSelectionModel::Current|QItemSelectionModel::Deselect);
		}

	}
	mInSlotFromSelectionManager = false;
}
///////////////////////////////////////////////////////////////////////////////
void OutlinerView::contextMenuEvent(QContextMenuEvent * ev)
{
	QModelIndex index = indexAt( ev->pos() );

	if( index.isValid() )
	{
		ork::Object* pobj = static_cast<ork::Object*>( index.internalPointer() );
		if( pobj )
		{
			SceneObject* sobj = rtti::autocast( pobj );
			EntData* pentdata = rtti::autocast( pobj );
			OrkAssert( sobj );

			QMenu menu(this);
			
			if( sobj )
			{
				menu.addAction("Delete");
			}
			if( pentdata )
			{
				menu.addAction("RenameFromArch");
			}

			QAction* pact = menu.exec(mapToGlobal(ev->pos()));

			if( pact )
			{
				if( pact->text() == "Delete" )
				{	//orkprintf( "deleting<%08x>\n", sobj );
					auto lamb1 = [=]()
					{	auto lamb2 = [=]()
						{
							this->mOutlinerModel->Editor().EditorDeleteObject( sobj );
							this->mOutlinerModel->SlotSceneTopoChanged();
						};
						Op(lamb2).QueueSync(MainThreadOpQ()); 
					};
					UpdateSerialOpQ().push(Op(lamb1)); 
					//mOutlinerModel->reset();
					
				}
				else if( pact->text() == "RenameFromArch" )
					if(pentdata && pentdata->GetArchetype())
					{
						ArrayString<64> arrstr;
						MutableString mutstr(arrstr);
						if(ConstString(pentdata->GetArchetype()->GetName()).find("/arch/") != ConstString::npos)
							mutstr = ConstString(pentdata->GetArchetype()->GetName()).substr(6);
						else
							mutstr = pentdata->GetArchetype()->GetName().c_str();

						int i = int(mutstr.size()) - 1;
						for(; i >= 0; i--)
							if(!isdigit(mutstr.c_str()[i]))
								break;
						mutstr = mutstr.substr(0, i + 1);
						mutstr += "1";
						mOutlinerModel->Editor().EditorRenameSceneObject(pentdata, arrstr.c_str());
					}
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
