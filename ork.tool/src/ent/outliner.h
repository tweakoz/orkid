////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once  

///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtui_tool.h>
#include <ork/object/AutoConnector.h>
#include <QtCore/QAbstractItemModel>
#include <QtCore/QVariant>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace ent {
///////////////////////////////////////////////////////////////////////////////

class SceneEditorBase;
class SceneObject;

///////////////////////////////////////////////////////////////////////////////
class OutlinerModel : public QAbstractItemModel, public AutoConnector
{
	RttiDeclareAbstract(OutlinerModel, AutoConnector)

public:

	OutlinerModel(SceneEditorBase&ed);

	/*virtual*/ ~OutlinerModel();
	/*virtual*/ QVariant data(const QModelIndex& index, int role) const;
	/*virtual*/ Qt::ItemFlags flags(const QModelIndex& index) const;
	/*virtual*/ QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	/*virtual*/ QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
	/*virtual*/ QModelIndex parent(const QModelIndex& index) const;
	/*virtual*/ int rowCount(const QModelIndex& parent = QModelIndex()) const;
	/*virtual*/ int columnCount(const QModelIndex& parent = QModelIndex()) const;
	/*virtual*/ bool setData(const QModelIndex& index, const QVariant& value, int role = Qt::EditRole);
	////////////////////////////////////////////////////////////////////
	//int NumChildren( const SceneGroup* par ) const;
	//SceneDagObject* DagChild( int idx, const SceneGroup* par ) const;
	////////////////////////////////////////////////////////////////////
	SceneObject* SceneChild( int idx ) const;
	int ChildIndex( const SceneObject* pobj ) const;
	QModelIndex FindIndex( const SceneObject* pobj ) const;
	////////////////////////////////////////////////////////////////////
	void SlotSceneTopoChanged();
	////////////////////////////////////////////////////////////////////

	SceneEditorBase& Editor() const { return mEditor; }

private:

	bool IsScene( const QModelIndex& idx ) const;
	bool IsDag( const QModelIndex& idx ) const;
	//bool IsGroup( const QModelIndex& idx ) const;
	bool IsOther( const QModelIndex& idx ) const;

	void ChangeNodeName(const QModelIndex& idx, std::string name );

	object::Signal						mChangeNodeNameSignal;
	SceneEditorBase&					mEditor;
	const QModelIndex					mSceneIndex;
};
///////////////////////////////////////////////////////////////////////////////
class OutlinerView : public QTreeView, public AutoConnector
{
	Q_OBJECT
	RttiDeclareAbstract(OutlinerView, AutoConnector)
	//DeclareMoc(OutlinerView,QTreeView);

	OutlinerModel*					mOutlinerModel;

	void contextMenuEvent(QContextMenuEvent * ev);

	void SlotObjectSelected( ork::Object* pobj );			// notification of externally changed selection
	void SlotObjectDeSelected( ork::Object* pobj );			// notification of externally changed selection

public:

	OutlinerView(QWidget* parent);
	void OutlinerNodeSelected( const QModelIndex& index);
	void OutlinerNodeDeselected( const QModelIndex& index );
	////////////////////////////////////////////////////////////////////
	////////////////////////////////////////////////////////////////////
	void setModel ( QAbstractItemModel * model ); /*virtual*/

	public Q_SLOTS:

	void UserSelectionChanged(QItemSelection selected, QItemSelection deselected);

private:

	void UserNodeSelected(const QModelIndex& node);
	void UserNodeDeselected(const QModelIndex& node);
	bool mBlockUser;
	bool mInSlotFromSelectionManager;

};
///////////////////////////////////////////////////////////////////////////////
class SceneData;
class QtOutlinerWindow : public QWidget
{
	Q_OBJECT
	//DeclareMoc(QtOutlinerWindow,QWidget);

	static QtOutlinerWindow*	gpWindow;

	SceneData*					mCurrentScene;
	OutlinerModel				mOutlinerModel;
	OutlinerView				mOutlinerView;

	/*virtual*/ void resizeEvent ( QResizeEvent * event ) ;

public:

	QtOutlinerWindow( bool bfloat, QWidget *pparent, SceneEditorBase& ed );
	~QtOutlinerWindow();

	static QtOutlinerWindow *GetCurrentOutlinerWindow( void );

	OutlinerModel& Model() { return mOutlinerModel; }
	OutlinerView& View() { return mOutlinerView; }

	void AttachToSceneData( SceneData* psd );

};
///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
