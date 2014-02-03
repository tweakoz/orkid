////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
///////////////////////////////////////////////////////////////////////////////
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/dataflow/dataflow.h>
#include <ork/file/file.h>
#include <ork/stream/FileInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/kernel/fixedlut.hpp>
#include <ork/kernel/Array.hpp>
///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::GedFactory,"GedFactory");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::IPlugChoiceDelegate,"IPlugChoiceDelegate");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::ged::IOpsDelegate,"IOpsDelegate");

///////////////////////////////////////////////////////////////////////////////

template class ork::fixedvector<
	ork::tool::ged::GedSkin::GedPrim,
	ork::tool::ged::GedSkin::PrimContainer::kmaxprims
>;
template class ork::fixedvector<
	ork::tool::ged::GedSkin::GedPrim*,
	ork::tool::ged::GedSkin::PrimContainer::kmaxprims
>;

template class ork::fixedvector<
	ork::tool::ged::GedSkin::GedPrim*,
	ork::tool::ged::GedSkin::PrimContainer::kmaxprimsper
>;

template class ork::fixedvector<
	ork::tool::ged::GedSkin::PrimContainer,
	32
>;
template class ork::fixedvector<
	ork::tool::ged::GedSkin::PrimContainer*,
	32
>;

/*template class ork::fixedvector<
	ork::tool::ged::GedSkin::GedPrim,
	ork::tool::ged::GedSkin::PrimContainer::kmaxprims
>;*/

template class ork::fixedvector
<
	std::pair
	<	int,
		ork::tool::ged::GedSkin::PrimContainer*
	>,
	ork::tool::ged::GedSkin::kMaxPrimContainers
>;

template class ork::fixedlut<
	int,
	ork::tool::ged::GedSkin::PrimContainer*
	,ork::tool::ged::GedSkin::kMaxPrimContainers
>;

///////////////////////////////////////////////////////////////////////////////

namespace ork { namespace dataflow {
	extern bool gbGRAPHLIVE;
}}

namespace ork { namespace tool { namespace ged {

void GedFactory::Describe()
{
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

GedItemNode* GedFactory::CreateItemNode(ObjModel&mdl,const ConstString& Name,const reflect::IObjectProperty *prop,Object* obj) const
{
	GedItemNode* PropContainerW = new GedLabelNode( 
		mdl, 
		Name.c_str(),
		prop,
		obj
		);
	return PropContainerW;
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void EnumerateFactories( const ork::Object* pdestobj, const reflect::IObjectProperty* prop, orkset<object::ObjectClass*>& FactoryClassSet )
{
	/////////////////////////////////////////////////////////

	ConstString anno = prop->GetAnnotation( "editor.factorylistbase" );

	orkvector<std::string> Classes;
	SplitString( anno.c_str(), Classes, " " );
	int inumclasses = int(Classes.size());

	if( inumclasses )
	{
		for( int i=0; i<inumclasses; i++ )
		{
			const std::string& ps = Classes[i];

			rtti::Class *pclass = rtti::Class::FindClass(ps.c_str());

			object::ObjectClass* pobjclass = rtti::downcast<object::ObjectClass *>( pclass );

			//////////////////////////////////////////////

			OrkAssert( pobjclass );

			orkstack<object::ObjectClass*> FactoryStack;

			FactoryStack.push( pobjclass );

			while( FactoryStack.empty() == false )
			{
				object::ObjectClass* pclass = FactoryStack.top();
				FactoryStack.pop();

				if( pclass->HasFactory() )
				{
					//////////////////////////////////////////////
					// check if class marked uninstantiable by editor
					//////////////////////////////////////////////

					any16 instanno = pclass->Description().GetClassAnnotation( "editor.instantiable" );

					bool bok2add = instanno.IsA<bool>() ? instanno.Get<bool>() : true;

					if( pdestobj ) // check if also OK with the destination object
					{
						ObjectFactoryFilter factfilterev;
						factfilterev.mpClass = pclass;
						pdestobj->Query(&factfilterev);
						bok2add &= factfilterev.mbFactoryOK;
					}

					if( bok2add )
					{
						//pact = qm.addAction( pclass->Name().c_str() );
						//pact->setData( QVariant( pclass->Name().c_str() ) );

						FactoryClassSet.insert( pclass );
					}
				}

				rtti::Class * const pfirstchild = pclass->FirstChild();
				rtti::Class *pchild = pfirstchild;

				while( pchild )
				{
					object::ObjectClass* pobjchildclass = rtti::downcast<object::ObjectClass *>( pchild );
					OrkAssert( pobjchildclass );
					FactoryStack.push( pobjchildclass );
					pchild = (pchild->NextSibling()==pfirstchild) ? 0 : pchild->NextSibling();
				}

			}
		}
	}
}

void EnumerateFactories( object::ObjectClass* pobjclass, orkset<object::ObjectClass*>& FactoryClassVect )
{
	OrkAssert( pobjclass );
	orkstack<object::ObjectClass*> FactoryStack;
	FactoryStack.push( pobjclass );
	while( FactoryStack.empty() == false )
	{	object::ObjectClass* pclass = FactoryStack.top();
		FactoryStack.pop();
		if( pclass->HasFactory() )
		{	//////////////////////////////////////////////
			// check if class marked uninstantiable by editor
			//////////////////////////////////////////////

			any16 instanno = pclass->Description().GetClassAnnotation( "editor.instantiable" );

			bool bok2add = instanno.IsA<bool>() ? instanno.Get<bool>() : true;

			/*ConstString instanno = pclass->Description().GetClassAnnotation( "editor.instantiable" );
			bool bok2add = true;
			if( instanno.length() )
			{	if( instanno == "false" )
				{
					bok2add = false;
				}
			}*/
			if( bok2add )
			{
				if( pclass->HasFactory() )
				{
					FactoryClassVect.insert( pclass );
				}
			}
		}
		rtti::Class * const pfirstchild = pclass->FirstChild();
		rtti::Class *pchild = pfirstchild;
		while( pchild )
		{
			object::ObjectClass* pobjchildclass = rtti::downcast<object::ObjectClass *>( pchild );
			OrkAssert( pobjchildclass );
			FactoryStack.push( pobjchildclass );
			pchild = (pchild->NextSibling()==pfirstchild) ? 0 : pchild->NextSibling();
		}
	}
}

QMenu *CreateFactoryMenu( const orkset<object::ObjectClass*>& FactoryClassVect )
{
	QMenu *pMenu = new QMenu(0);

	orkmap<ork::PoolString,object::ObjectClass*> ClassMap;

	for( orkset<object::ObjectClass*>::const_iterator it=FactoryClassVect.begin(); it!=FactoryClassVect.end(); it++ )
	{
		object::ObjectClass* objclass = (*it);

		bool badd = true;

		if( badd )
		{
			const ork::PoolString& pstr = objclass->Name();
			ClassMap[pstr] = objclass;
		}
	}

	for( orkmap<ork::PoolString,object::ObjectClass*>::const_iterator it=ClassMap.begin(); it!=ClassMap.end(); it++ )
	{
		object::ObjectClass* objclass = it->second;
		ork::PoolString name = it->first;

		QAction *pchildact = pMenu->addAction( name.c_str() );

		QVariant UserData( QString( name.c_str() ) );
		pchildact->setData(UserData);
	}

	return pMenu;
}

///////////////////////////////////////////////////////////////////////////////

void UserChoices::EnumerateChoices( bool bforcenocache )
{
	mucd.EnumerateChoices( mUserChoices );

	for( orkmap<PoolString,IUserChoiceDelegate::ValueType>::const_iterator it=mUserChoices.begin(); it!=mUserChoices.end(); it++ )
	{
		const char* item = it->first.c_str();
		CAttrChoiceValue myval( item, item );
		myval.SetCustomData( it->second );
		add( myval );
	}
}

UserChoices::UserChoices( IUserChoiceDelegate& ucd , ork::Object* pobj, ork::Object* puserobj )
	: mucd( ucd )
{
	mucd.SetObject( pobj,puserobj );
	EnumerateChoices();
}

//////////////////////////////////////////////////////////////////////////////
void IPlugChoiceDelegate::Describe() {}
void IOpsDelegate::Describe()
{
	ObjectImportDelegate::GetClassStatic();
	ObjectExportDelegate::GetClassStatic();
}
//////////////////////////////////////////////////////////////////////////////

static const int ops_ioff = 2;
LockedResource<IOpsDelegate::TaskList> IOpsDelegate::gCurrentTasks;

void IOpsDelegate::AddTask( ork::object::ObjectClass* pdelegclass, ork::Object* ptarget )
{
	if( 0 == GetTask( pdelegclass, ptarget ) )
	{
		IOpsDelegate* deleg = rtti::autocast(pdelegclass->CreateObject());
		if( deleg )
		{
			OpsTask* ptask = new OpsTask;
			ptask->mpDelegate = deleg;
			ptask->mpTarget = ptarget;

			TaskList& tsklist = gCurrentTasks.LockForWrite();
			{
				tsklist.push_back( ptask );
			}
			gCurrentTasks.UnLock();

			deleg->Execute(ptarget);
		}
	}
}
void IOpsDelegate::RemoveTask( ork::object::ObjectClass* pdelegclass, ork::Object* ptarget )
{
	OpsTask* ptask = GetTask( pdelegclass, ptarget );

	if( 0 != ptask )
	{
		TaskList& tsklist = gCurrentTasks.LockForWrite();
		{
			TaskList::iterator iterase = tsklist.end();

			for( TaskList::iterator it = tsklist.begin(); it!=tsklist.end(); it++ )
			{
				OpsTask* ttask = (*it);

				if( ttask == ptask )
				{
					iterase = it;
				}
			}

			if( iterase != tsklist.end() )
			{
				tsklist.erase( iterase );
			}
			
		}
		gCurrentTasks.UnLock();
	}
}
OpsTask* IOpsDelegate::GetTask( ork::object::ObjectClass* pdelegclass, ork::Object* ptarget )
{
	OpsTask* pret = 0;
	const TaskList& tsklist = gCurrentTasks.LockForRead();
	{
		for( TaskList::const_iterator it = tsklist.begin(); it!=tsklist.end(); it++ )
		{
			OpsTask* ptask = (*it);

			if( ptask->mpDelegate->GetClass() == pdelegclass )
			{
				if( ptask->mpTarget == ptarget )
				{
					pret = ptask;
				}
			}
		}		
	}
	gCurrentTasks.UnLock();
	return pret;
}

void OpsNode::DoDraw( lev2::GfxTarget* pTARG ) // virtual
{
	const int ops_size = 12*get_charw();

	bool bispick = pTARG->FBI()->IsPickState();

	GetSkin()->DrawOutlineBox( this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE );
	for( int i=0; i<int(mOps.size()); i++ )
	{	int ix = miX+ops_ioff+(i*ops_size);
		
		ork::object::ObjectClass* pclass = mOps[i].second;
		ork::Object* ptarget = GetOrkObj();

		OpsTask* ptask = IOpsDelegate::GetTask( pclass, ptarget );

		bool bactive = (ptask!=0);

		GedSkin::ESTYLE estyle = bactive 
									? GedSkin::ESTYLE_DEFAULT_OUTLINE
									: GedSkin::ESTYLE_BACKGROUND_OPS ;

		GetSkin()->DrawBgBox( this, ix, miY+2, (ops_size-ops_ioff), miH-4, estyle );

		if( ptask )
		{
			static int ispinner = 0;

			float fprogress = ptask->mpDelegate->GetProgress();
			int inw = int ( float((ops_size-ops_ioff)-1) * fprogress );
			GetSkin()->DrawBgBox( this, ix+1, miY+3, inw, miH-6, GedSkin::ESTYLE_BACKGROUND_2 );
	
			std::string Label = CreateFormattedString( "%s(%d)", mOps[i].first.c_str(), int(fprogress*100.0f) );
			GetSkin()->DrawText( this, ix, miY+4, Label.c_str() );

			float fspinner = float(ispinner)/100.0f;
			float fxo = ork::sinf( fspinner )*float(miH/3);
			float fyo = ork::cosf( fspinner )*float(miH/3);

			float fxc = ix+(ops_size-ops_ioff)-(miH/3);
			float fyc = miY+(miH/2);

			int ix0 = int(fxc-fxo);
			int ix1 = int(fxc+fxo);
			int iy0 = int(fyc-fyo);
			int iy1 = int(fyc+fyo);

			GetSkin()->DrawLine( this, ix0, iy0, ix1, iy1, GedSkin::ESTYLE_BACKGROUND_2 );

			ispinner++;
		}
		else
		{
			GetSkin()->DrawText( this, ix+6, miY+4, mOps[i].first.c_str() );
		}

	}
}

void OpsNode::mouseDoubleClickEvent ( QMouseEvent * pEV )
{
	const int ops_size = 12*get_charw();

	Qt::MouseButton button = pEV->button();
	int ix = pEV->x() - this->miX;
	int iy = pEV->y() - this->miY;
	int index = (ix-ops_ioff)/ops_size;
	orkprintf( "op<%s>\n", mOps[index].first.c_str() );
	ork::object::ObjectClass *pclass = mOps[index].second;
	if( pclass )
	{		
		mModel.SigRepaint();
		IOpsDelegate::AddTask( pclass, GetOrkObj() );
		//mModel.SigRepaint();
	}
}

OpsNode::OpsNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj )
	: GedItemNode( mdl, name, prop, obj)
{	object::ObjectClass* objclass = rtti::downcast<object::ObjectClass*>( obj->GetClass() );
	any16 obj_ops = objclass->Description().GetClassAnnotation( "editor.object.ops" );
	orkvector<std::string> opstrings;
	ConstString obj_ops_str = obj_ops.Get<ConstString>();
	SplitString( obj_ops_str.c_str(), opstrings, " " );
	for( int i=0; i<int(opstrings.size()); i++ )
	{	const std::string opstr = opstrings[i];
		orkvector<std::string> opbreak;
		SplitString( opstr.c_str(), opbreak, ":" );
		rtti::Class *the_class = rtti::Class::FindClass(opbreak[1].c_str());
		if( the_class )
		{
			OrkAssert( the_class->IsSubclassOf( IOpsDelegate::GetClassStatic() ) );
			ork::object::ObjectClass* pclass = rtti::autocast(the_class);
			mOps.push_back( std::pair<std::string,ork::object::ObjectClass*>(opbreak[0],pclass) );
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

static const int koff = 1;
//static const int kdim = GedMapNode::klabelsize-2;

void GedGroupNode::DoDraw( lev2::GfxTarget* pTARG )
{
	int inumitems = GetNumItems();

	/////////////////
	// drop down box
	/////////////////

	int ioff = koff;
	int idim = get_charh();

	int dbx1 = miX+ioff;
	int dbx2 = dbx1+idim;
	int dby1 = miY+ioff;
	int dby2 = dby1+idim;

	int labw = this->GetNameWidth();
	int labx = miX+(miW>>1)-(labw>>1);
	if( labx<dbx2+3 ) labx = dbx2+3;

	GetSkin()->DrawBgBox( this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1 );
	GetSkin()->DrawOutlineBox( this, miX+ioff, miY+ioff, idim, idim, GedSkin::ESTYLE_DEFAULT_CHECKBOX );
	GetSkin()->DrawText( this, labx, miY+4, mName.c_str() );

	GetSkin()->DrawBgBox( this, miX, miY, miW, get_charh(), GedSkin::ESTYLE_BACKGROUND_GROUP_LABEL );

	if( inumitems )
	{
		if( mbCollapsed )
		{
			GetSkin()->DrawRightArrow( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_DEFAULT_CHECKBOX );
			GetSkin()->DrawLine( this, dbx1+1, dby1, dbx1+1, dby2, GedSkin::ESTYLE_DEFAULT_CHECKBOX );
		}
		else
		{
			GetSkin()->DrawDownArrow( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_DEFAULT_CHECKBOX );
			GetSkin()->DrawLine( this, dbx1, dby1+1, dbx2, dby1+1, GedSkin::ESTYLE_DEFAULT_CHECKBOX );
		}
	}
}

GedGroupNode::GedGroupNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj )
	: GedItemNode( mdl, name, prop, obj )
	, mbCollapsed( true )
{

	std::string fixname = name;
	/////////////////////////////////////////////////////////////////
	// localize collapse states to instances of properties underneath other properties
	GedItemNode* parent = mdl.GetGedWidget()->ParentItemNode();
	if( parent )
	{
		const char* parname = parent->mName.c_str();
		if( parname )
		{
			fixname += CreateFormattedString( "_%s_", parname );
		}
	}
	/////////////////////////////////////////////////////////////////
	
	int ilen = (int) fixname.length();
	for( int i=0; i<ilen; i++ )
	{
		switch( fixname[i] )
		{
			case ':':
			case '<':
			case '>':
			case ' ':
				fixname[i] = ' ';
				break;
		}
	}

	mPersistID.format( "%s_group_collapse", fixname.c_str() );

	///////////////////////////////////////////
	PersistHashContext HashCtx;
	PersistantMap* pmap = mdl.GetPersistMap( HashCtx );
	///////////////////////////////////////////

	const std::string& str_collapse = pmap->GetValue( mPersistID.c_str() );

	if( str_collapse == "false"  )
	{
		mbCollapsed = false;
	}

	CheckVis();
}
///////////////////////////////////////////////////////////////////////////////
void GedGroupNode::mouseDoubleClickEvent ( QMouseEvent * pEV )
{
	printf( "GedGroupNode<%p>::mouseDoubleClickEvent\n", this);
	int inumitems = GetNumItems();

	Qt::MouseButtons Buttons = pEV->buttons();
	Qt::KeyboardModifiers modifiers = pEV->modifiers();

	bool isCTRL = (modifiers&Qt::ControlModifier);

	const int kdim = get_charh();

	if( inumitems )
	{
		int ix = pEV->x() - this->miX;
		int iy = pEV->y() - this->miY;

		if( ix >= koff && ix <= kdim && iy >= koff && iy <= kdim ) // drop down
		{
			mbCollapsed = ! mbCollapsed;

			///////////////////////////////////////////
			PersistHashContext HashCtx;
			PersistantMap* pmap = mModel.GetPersistMap( HashCtx );
			///////////////////////////////////////////
		
			pmap->SetValue( mPersistID.c_str(), mbCollapsed ? "true" : "false" );

			if( isCTRL ) // also do siblings
			{
				GedItemNode* par = GetParent();
				if( par )
				{
					int inumc = par->GetNumItems();
					for( int i=0; i<inumc; i++ )
					{
						GedItemNode* item = par->GetItem( i );
						GedGroupNode* pgroup = rtti::autocast(item);
						if( pgroup )
						{
							pgroup->mbCollapsed = mbCollapsed;
							pgroup->CheckVis();
							pmap->SetValue( pgroup->mPersistID.c_str(), mbCollapsed ? "true" : "false" );
						}
					}
				}
			}


			CheckVis();
			return;
		}
		///////////////////////////////////////////////////
		///////////////////////////////////////////////////
		else if( isCTRL )
		{
			if( GetOrkObj() )
			{
				ork::Object* top = mModel.BrowseStackTop();
				if( top == GetOrkObj() )
				{
					mModel.PopBrowseStack();
					top = mModel.BrowseStackTop();
					if( top )
					{
						mModel.Attach(top,false);
					}
				}
				else
				{
					mModel.PushBrowseStack(GetOrkObj());
					mModel.Attach( GetOrkObj(), false );
				}
			}
		}
		///////////////////////////////////////////////////
		// SPAWN a new GED
		///////////////////////////////////////////////////
		else if( modifiers&Qt::AltModifier )
		{
			if( GetOrkObj() )
			{
				// spawn new window here	
				mModel.SigSpawnNewGed( GetOrkObj() );
			}
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void GedGroupNode::CheckVis()
{
	int inumitems = GetNumItems();

	if( inumitems )
	{
		if( mbCollapsed )
		{
			for( int it=0; it<inumitems; it++ )
			{
				GetItem(it)->SetVisible( false );
			}
		}
		else
		{
			for( int it=0; it<inumitems; it++ )
			{
				GetItem(it)->SetVisible( true );
			}
		}
	}
	mModel.GetGedWidget()->DoResize();
}
//////////////////////////////////////////////////////////////////////////////

class GraphImportDelegate : public IOpsDelegate
{
	RttiDeclareConcrete( GraphImportDelegate, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget )
	{
		ork::dataflow::graph_inst* pgraph = rtti::autocast( ptarget );

		if( pgraph )
		{
			lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
			QString FileName = QFileDialog::getOpenFileName( 0, "Import Dataflow Graph", 0, "DataflowGraph (*.dfg)");
			file::Path::NameType fname = FileName.toAscii().data();
			if( fname.length() )
			{
				//SetRecentSceneFile(FileName.toAscii().data(),SCENEFILE_DIR);
				if( ork::CFileEnv::filespec_to_extension( fname ).length() == 0 ) fname += ".dfg";
				stream::FileInputStream istream(fname.c_str());
				reflect::serialize::XMLDeserializer iser(istream);
				//ork::stream::FileOutputStream ostream(fname.c_str());
				//ork::reflect::serialize::XMLSerializer oser(ostream);
				//oser.Serialize(ptex);
				pgraph->DeserializeInPlace(iser);
			}
			lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
		}
		tool::ged::IOpsDelegate::RemoveTask( GraphImportDelegate::GetClassStatic(), ptarget );
	}
};
class GraphExportDelegate : public IOpsDelegate
{
	RttiDeclareConcrete( GraphExportDelegate, tool::ged::IOpsDelegate );
	virtual void Execute( ork::Object* ptarget )
	{
		ork::dataflow::graph_inst* pgraph = rtti::autocast( ptarget );

		if( pgraph )
		{
			lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
			QString FileName = QFileDialog::getSaveFileName( 0, "Export Dataflow Graph", 0, "DataflowGraph (*.dfg)");
			file::Path::NameType fname = FileName.toAscii().data();
			if( fname.length() )
			{
				//SetRecentSceneFile(FileName.toAscii().data(),SCENEFILE_DIR);
				if( ork::CFileEnv::filespec_to_extension( fname ).length() == 0 ) fname += ".dfg";
				ork::stream::FileOutputStream ostream(fname.c_str());
				ork::reflect::serialize::XMLSerializer oser(ostream);
				//oser.Serialize(ptex);
				pgraph->SerializeInPlace(oser);
			}
			lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
		}
		tool::ged::IOpsDelegate::RemoveTask( GraphExportDelegate::GetClassStatic(), ptarget );
	}
};
	void ObjectImportDelegate::Execute( ork::Object* ptarget )
	{
		if( ptarget )
		{
			lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
			QString FileName = QFileDialog::getOpenFileName( 0, "Import Object (Be careful!)", 0, "Orkid Object (*.mox)");
			file::Path::NameType fname = FileName.toAscii().data();
			if( fname.length() )
			{
				//SetRecentSceneFile(FileName.toAscii().data(),SCENEFILE_DIR);
				if( ork::CFileEnv::filespec_to_extension( fname ).length() == 0 ) fname += ".mox";
				stream::FileInputStream istream(fname.c_str());
				reflect::serialize::XMLDeserializer iser(istream);
				//ork::stream::FileOutputStream ostream(fname.c_str());
				//ork::reflect::serialize::XMLSerializer oser(ostream);
				//oser.Serialize(ptex);
				ptarget->DeserializeInPlace(iser);
			}
			lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
		}
		tool::ged::IOpsDelegate::RemoveTask( ObjectImportDelegate::GetClassStatic(), ptarget );
	}
	void ObjectExportDelegate::Execute( ork::Object* ptarget )
	{
		ork::Object* pobj = rtti::autocast( ptarget );

		if( pobj )
		{
			lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
			QString FileName = QFileDialog::getSaveFileName( 0, "Export Object ", 0, "Orkid Object (*.mox)");
			file::Path::NameType fname = FileName.toAscii().data();
			if( fname.length() )
			{
				if( ork::CFileEnv::filespec_to_extension( fname ).length() == 0 ) fname += ".mox";
				ork::stream::FileOutputStream ostream(fname.c_str());
				ork::reflect::serialize::XMLSerializer oser(ostream);
				//oser.Serialize(ptex);
				pobj->SerializeInPlace(oser);
			}
			lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
		}
		tool::ged::IOpsDelegate::RemoveTask( ObjectExportDelegate::GetClassStatic(), ptarget );
	}
void GraphImportDelegate::Describe(){}
void GraphExportDelegate::Describe(){}
void ObjectImportDelegate::Describe(){}
void ObjectExportDelegate::Describe(){}
} } }

INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::GraphImportDelegate, "dflowgraphimport");
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::GraphExportDelegate, "dflowgraphexport");
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::ObjectImportDelegate, "objectimport");
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::ObjectExportDelegate, "objectexport");
