///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

#include <ork/kernel/opq.h>
///////////////////////////////////////////////////////////////////////////////

#include <queue>

#include <orktool/ged/ged.h>
#include <orktool/ged/ged_delegate.h>
#include <orktool/ged/ged_io.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectMapProperty.h>
#include <ork/reflect/IObjectArrayProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/DirectObjectPropertyType.h>
#include <ork/reflect/RegisterProperty.h>
#include <ork/rtti/downcast.h>

#include <ork/kernel/orklut.hpp>
#include <ork/reflect/DirectObjectMapPropertyType.hpp>

#include <ork/util/crc.h>

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::ObjModel, "ObjModel" );
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::GedWidget, "GedWidget" );
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::PersistantMap, "GedPersistantMap" );
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::PersistMapContainer, "GedPersistMapContainer" );

template class ork::orklut<int,ork::tool::ged::PersistantMap*>;

namespace ork { namespace tool { namespace ged {

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void ObjModel::Describe()
{	//////////////////////////////////////////////////////////////////
	RegisterAutoSignal( ObjModel, Repaint );
	RegisterAutoSignal( ObjModel, ModelInvalidated );
	RegisterAutoSignal( ObjModel, PreNewObject );
	RegisterAutoSignal( ObjModel, PropertyInvalidated );
	RegisterAutoSignal( ObjModel, NewObject );
	RegisterAutoSignal( ObjModel, SpawnNewGed );
	//////////////////////////////////////////////////////////////////
	RegisterAutoSlot( ObjModel, NewObject );
	RegisterAutoSlot( ObjModel, ObjectDeleted );
	RegisterAutoSlot( ObjModel, ObjectSelected );
	RegisterAutoSlot( ObjModel, ObjectDeSelected );
	RegisterAutoSlot( ObjModel, RelayModelInvalidated );
	RegisterAutoSlot( ObjModel, RelayPropertyInvalidated );
	//////////////////////////////////////////////////////////////////
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotRelayPropertyInvalidated( ork::Object* pobj, const reflect::IObjectProperty* prop )
{
	if( mpGedWidget )
	{
		mpGedWidget->PropertyInvalidated( pobj, prop );
	}
	//Attach( mCurrentObject );
	//SigModelInvalidated();
}

void ObjModel::SlotRelayModelInvalidated()
{
	Attach( mCurrentObject );
	//SigModelInvalidated();
}

orkset<ObjModel*> ObjModel::gAllObjectModels;

ObjModel::ObjModel()
	: mpGedWidget( 0 )
	, mChoiceManager( 0 )
	, mModelInvalidatedInvoker( mSignalModelInvalidated.CreateInvokation() )
	, mQueueObject( 0 )
	, mCurrentObject( 0 )
	, mRootObject( 0 )
	, mbQueueUpdateAll( false )
	, mbEnablePaint(false)
	, ConstructAutoSlot(NewObject)
	, ConstructAutoSlot(RelayModelInvalidated)
	, ConstructAutoSlot(RelayPropertyInvalidated)
	, ConstructAutoSlot(ObjectDeleted)
	, ConstructAutoSlot(ObjectSelected)
	, ConstructAutoSlot(ObjectDeSelected)
	, ConstructAutoSlot(Repaint)
{
	SetupSignalsAndSlots();
	///////////////////////////////////////////
	// A Touch Of Class

	GedFactoryEnum::GetClassStatic();
	GedFactoryGradient::GetClassStatic();
	GedFactoryCurve::GetClassStatic();
	GedFactoryAssetList::GetClassStatic();
	GedFactoryFileList::GetClassStatic();

	///////////////////////////////////////////

	object::Connect( & mSignalNewObject, & mSlotNewObject );

	gAllObjectModels.insert(this);

}

void ObjModel::FlushAllQueues()
{
	for( orkset<ObjModel*>::const_iterator it=gAllObjectModels.begin(); it!=gAllObjectModels.end(); it++ )
	{
		ObjModel* pmodel = (*it);

		pmodel->FlushQueue();
	}
	ork::msleep(0);
}

///////////////////////////////////////////////////////////////////////////////

ObjModel::~ObjModel()
{
	DisconnectAll();
	gAllObjectModels.erase(this);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::QueueUpdateAll()
{
	mbQueueUpdateAll = true;
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::FlushQueue()
{
	if( mbQueueUpdateAll )
	{
		Attach(CurrentObject());
		SigModelInvalidated();
		//SigNewObject(pobj);
		mbQueueUpdateAll = false;
	}
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigModelInvalidated()
{
	mSignalModelInvalidated(&ObjModel::SigModelInvalidated);
}
void ObjModel::SigPreNewObject()
{	auto lamb = [=]()
	{	
		this->mSignalPreNewObject(&ObjModel::SigPreNewObject);
	};
	UpdateSerialOpQ().push(Op(lamb));
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigPropertyInvalidated(ork::Object* pobj, const reflect::IObjectProperty* prop)
{	auto lamb = [=]()
	{	
		this->mSignalPropertyInvalidated(&ObjModel::SigPropertyInvalidated,pobj,prop);
	};
	UpdateSerialOpQ().push(Op(lamb));	
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigRepaint()
{
	mSignalRepaint(&ObjModel::SigRepaint);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SigNewObject(ork::Object*pobj)
{
	SigPreNewObject();
	mSignalNewObject(&ObjModel::SigNewObject,pobj);
	SigRepaint();
	SigPostNewObject(pobj);

}

void ObjModel::SigPostNewObject(ork::Object*pobj)
{
	mSignalPostNewObject(&ObjModel::SigPostNewObject,pobj);
}

void ObjModel::SigSpawnNewGed( ork::Object* pOBJ )
{
	mSignalSpawnNewGed(&ObjModel::SigSpawnNewGed,pOBJ);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotNewObject(ork::Object *pobj)
{
	Attach( mCurrentObject );
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectDeleted(ork::Object *pobj) 
{
	Attach(0);
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::ProcessQueue()
{
	if( mQueueObject )
	{
		Attach( mQueueObject );
	}
	mQueueObject = 0;
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectSelected( ork::Object* pobj )
{
	Attach( pobj );
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::SlotObjectDeSelected( ork::Object* pobj )
{
	Attach(0);
}

//////////////////////////////////////////////////////////////////////////////

GedItemNode* ObjModel::Recurse( ork::Object* root_object, const char* pname, bool binline )
{	/////////////////////////////////
	GedItemNode* rval = 0;
	ork::Object* cur_obj = root_object;
	if( cur_obj )
	{
		ObjectGedVisitEvent gev;
		cur_obj->Notify( & gev );
	}
	object::ObjectClass* objclass = rtti::downcast<object::ObjectClass*>( cur_obj->GetClass() );
	///////////////////////////////////////////////////
	//editor.object.ops
	///////////////////////////////////////////////////
	any16 obj_ops_anno = objclass->Description().GetClassAnnotation( "editor.object.ops" );
	ConstString obj_ops = obj_ops_anno.IsSet() ? obj_ops_anno.Get<ConstString>() : "";
	const char* usename = (pname!=0) ? pname : cur_obj->GetClass()->Name().c_str();
	GedGroupNode* ObjContainerW = binline ? 0 : new GedGroupNode( *this, usename, 0, cur_obj );
	if( cur_obj == root_object )
	{	rval = ObjContainerW;
	}
	if( ObjContainerW )
	{	GetGedWidget()->AddChild( ObjContainerW );
		GetGedWidget()->PushItemNode( ObjContainerW );
	}
	if( obj_ops.length() )
	{	OpsNode* popnode = new OpsNode( *this, "ops", 0, cur_obj );
		GetGedWidget()->AddChild( popnode );			
	}
	///////////////////////////////////////////////////
	//editor.class
	///////////////////////////////////////////////////
	
	any16 ClassEditorAnno = objclass->Description().GetClassAnnotation( "editor.class" );
	if( ClassEditorAnno.IsSet() )
	{
		ConstString anno_edclass = ClassEditorAnno.Get<ConstString>();
		if( anno_edclass.length() )
		{	rtti::Class *AnnoEditorClass = rtti::Class::FindClass(anno_edclass);
			if( AnnoEditorClass )
			{
				ork::object::ObjectClass* pclass = rtti::safe_downcast<ork::object::ObjectClass*>(AnnoEditorClass);
				ork::rtti::ICastable* factory = pclass->CreateObject();
				GedFactory* qf = rtti::safe_downcast<GedFactory*>(factory);
				if( qf )
				{
					if(pname == 0 )
						pname = anno_edclass.c_str();
						
					GetGedWidget()->AddChild(qf->CreateItemNode(*this,pname,0,root_object));
				
					if( ObjContainerW )
					{	GetGedWidget()->PopItemNode( ObjContainerW );
						ObjContainerW->CheckVis();
					}
					GetGedWidget()->DoResize();
					return rval;
				}
			}
		}
	}
	
	///////////////////////////////////////////////////
	// walk classes to root class
	// mark properties, optionally sorting them by "editor.prop.groups" annotation
	///////////////////////////////////////////////////
	sortnode	root_sortnode;
	EnumerateNodes( root_sortnode, objclass );
	///////////////////////////////////////////////////
	// walk marked properties
	///////////////////////////////////////////////////
	std::queue<const sortnode*>	sort_stack;
	sort_stack.push( &root_sortnode );
	int igcount = 0;
	while( sort_stack.empty() == false )
	{	const sortnode* snode = sort_stack.front();
		sort_stack.pop();
		////////////////////////////////////////////////////////////////////////////////////////
		GedGroupNode* PropGroupNode = 0;
		if( igcount )
		{	const std::string& GroupName = snode->Name;
			PropGroupNode = new GedGroupNode( *this, GroupName.c_str(), 0, cur_obj );
			GetGedWidget()->AddChild(PropGroupNode);
			GetGedWidget()->PushItemNode( PropGroupNode );
		}
		////////////////////////////////////////////////////////////////////////////////////////
		{	// Possibly In Group
		////////////////////////////////////////////////////////////////////////////////////////
			for( orkvector< std::pair< std::string, reflect::IObjectProperty * > >::const_iterator 
				it =  snode->PropVect.begin();
				it != snode->PropVect.end();
				it ++  )
			{	const std::string& Name = (*it).first;
				const reflect::IObjectProperty *prop = (*it).second;
				GedItemNode* PropContainerW = 0;
				if( 0 == prop ) continue;
				if( false == IsNodeVisible( prop ) ) continue;
				//////////////////////////////////////////////////
				PropContainerW = CreateNode( Name, prop, cur_obj );
				//////////////////////////////////////////////////
				if( PropContainerW ) GetGedWidget()->AddChild(PropContainerW);
				//////////////////////////////////////////////////
			}
		////////////////////////////////////////////////////////////////////////////////////////
		}	// Possibly In Group
		////////////////////////////////////////////////////////////////////////////////////////
		if( PropGroupNode )
		{	GetGedWidget()->PopItemNode( PropGroupNode );
			PropGroupNode->CheckVis();
		}
		////////////////////////////////////////////////////////////////////////////////////////
		for( const auto& it : snode->GroupVect )
		{	const std::string& Name = it.first;
			const sortnode* child_group = it.second;
			sort_stack.push( child_group );
			igcount++;
		}
		////////////////////////////////////////////////////////////////////////////////////////
	}
	if( ObjContainerW )
	{	GetGedWidget()->PopItemNode( ObjContainerW );
		ObjContainerW->CheckVis();
	}
	GetGedWidget()->DoResize();
	return rval;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

bool ObjModel::IsNodeVisible( const reflect::IObjectProperty* prop )
{	ConstString anno_vis = prop->GetAnnotation( "editor.visible" );
	ConstString anno_ediftageq = prop->GetAnnotation( "editor.iftageq" );
	if( anno_vis.length() )
	{
		if( 0 == strcmp( anno_vis.c_str(), "false" ) ) return false;
	}
	if( anno_ediftageq.length() )
	{	orkvector<std::string> AnnoSplit;
		SplitString(std::string(anno_ediftageq.c_str()),AnnoSplit,":");
		OrkAssert(AnnoSplit.size()==2);
		const std::string& key = AnnoSplit[0];
		const std::string& val = AnnoSplit[1];
		GedItemNode* parentnode = GetGedWidget()->ParentItemNode();
		orkmap<std::string,std::string>::const_iterator ittag=parentnode->mTags.find( key );
		if( ittag != parentnode->mTags.end() )
		{	if( val != ittag->second )
			{	return false;
			}
		}			
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

struct yo
{
	std::string		mUrlType;
	tokenlist		mTokens;
};
void ObjModel::EnumerateNodes( sortnode& in_node, object::ObjectClass* the_class )
{	
	orkvector<object::ObjectClass*> ClassVect;
	while( the_class != ork::Object::GetClassStatic() )
	{	
		ClassVect.push_back( the_class );
		the_class = rtti::downcast<ork::object::ObjectClass*>( the_class->Parent() );
	}
	int inumclasses = int(ClassVect.size());
	for( int ic=(inumclasses-1); ic>=0; ic-- ) 
	{	object::ObjectClass* pclass = ClassVect[ic];
		ork::reflect::Description::PropertyMapType& propmap = pclass->Description().Properties();
		any16 eg_anno = pclass->Description().GetClassAnnotation( "editor.prop.groups" );
		ConstString eg = eg_anno.IsSet() ? eg_anno.Get<const char*>() : "";
		if( eg.length() )
		{	FixedString<1024> str_rep = eg.c_str();
			str_rep.replace_in_place("//", "// ");
			tokenlist src_toklist = CreateTokenList( str_rep.c_str(), " " );
			/////////////////////////////////////////////////
			// enumerate groups
			/////////////////////////////////////////////////
			orkvector< yo > Groups;
			tokenlist* curtoklist = 0;
			for( tokenlist::const_iterator it=src_toklist.begin(); it!=src_toklist.end(); it++ )
			{	const std::string& str = (*it);
				ork::file::Path aspath( str.c_str() );
				if( aspath.HasUrlBase() ) // START NEW GROUP	
				{	Groups.push_back( yo() );
					orkvector< yo >::iterator itnew = Groups.end()-1;
					curtoklist = & itnew->mTokens;
					itnew->mUrlType = str;
				}
				else // ADD TO LAST GROUP
				{	if( curtoklist )
					{	curtoklist->push_back( str );
					}
				}
			}
			/////////////////////////////////////////////////
			// process groups
			/////////////////////////////////////////////////
			for( orkvector< yo >::const_iterator 
				itg = Groups.begin();
				itg !=Groups.end();
				itg ++ )
			{	const yo& the_yo = (*itg);
				const std::string& UrlType = the_yo.mUrlType;
				const tokenlist& iter_toklist = the_yo.mTokens;
				tokenlist::const_iterator itp = iter_toklist.end();
				sortnode* pnode = 0;
				if( UrlType.find( "grp" ) != std::string::npos ) // GroupNode
				{	itp = iter_toklist.begin();
					const std::string& GroupName = (*itp);
					std::pair< std::string, sortnode* > the_pair;
					the_pair.first = GroupName;
					the_pair.second = new sortnode;
					in_node.GroupVect.push_back( the_pair );
					pnode = (in_node.GroupVect.end()-1)->second;
					pnode->Name = GroupName;
					itp++;
				}
				else if( UrlType.find( "sort" ) != std::string::npos ) // SortNode
				{	pnode = & in_node;
					itp = iter_toklist.begin();
				}
				if( pnode ) for( ; itp!=iter_toklist.end(); itp++ )
				{	const std::string& str = (*itp);
					ork::reflect::Description::PropertyMapType::iterator itf = propmap.find( str.c_str() );
					ork::reflect::IObjectProperty* prop = (itf!=propmap.end()) ? itf->second : 0;
					if( prop )
					{	pnode->PropVect.push_back( std::make_pair( str.c_str(), prop ) );
					}
				}
			}
			/////////////////////////////////////////////////
		}
		else for( ork::reflect::Description::PropertyMapType::iterator it=propmap.begin(); it!=propmap.end(); it++ )
		{	const ConstString& Name = (*it).first;
			ork::reflect::IObjectProperty *prop = (*it).second;
			if( prop )
			{	in_node.PropVect.push_back( std::make_pair( Name.c_str(), prop ) );
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

GedItemNode* ObjModel::CreateNode( const std::string& Name, const reflect::IObjectProperty *prop, Object* pobject )
{	
	rtti::Class *AnnoEditorClass = 0;
	/////////////////////////////////////////////////////////////////////////
	// check editor class anno on property
	/////////////////////////////////////////////////////////////////////////
	ConstString anno_edclass = prop->GetAnnotation( "editor.class" );
	if( anno_edclass.length() )
	{	AnnoEditorClass = rtti::Class::FindClass(anno_edclass);
	}
	/////////////////////////////////////////////////////////////////////////
	if( AnnoEditorClass )
	{
		ork::object::ObjectClass* pclass = rtti::safe_downcast<ork::object::ObjectClass*>(AnnoEditorClass);
		ork::rtti::ICastable* factory = pclass->CreateObject();
		GedFactory* qf = rtti::safe_downcast<GedFactory*>(factory);
		if( qf )
		{	return qf->CreateItemNode(*this,Name.c_str(),prop,pobject);
		}
	}
	/////////////////////////////////////////////////////////////////////////
	const reflect::IObjectPropertyType<PoolString>* psprop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<Char8>* c8prop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<int>* intprop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<bool>* boolprop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<float>* floatprop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<CMatrix4>* mtx44prop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<CVector4>* vec4prop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<CVector3>* vec3prop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<CVector2>* vec2prop = rtti::autocast( prop );
	const reflect::IObjectPropertyType<ork::rtti::ICastable*>* castprop = rtti::autocast( prop );
	const reflect::IObjectPropertyObject* objprop = rtti::autocast( prop );
	const reflect::DirectObjectPropertyType<ork::Object*>* dobjprop = rtti::autocast( prop );
	const reflect::IObjectMapProperty* MapProp = rtti::autocast(prop);
	const reflect::IObjectArrayProperty* ArrayProp = rtti::autocast(prop);
	/////////////////////////////////////////////////////////////////////////
	ConstString anno_ucdclass = prop->GetAnnotation( "ged.userchoice.delegate" );
	bool HasUserChoiceDelegate = ( anno_ucdclass.length() );
	/////////////////////////////////////////////////////////////////////////
	if( c8prop )		return new GedLabelNode( *this, Name.c_str(), prop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( psprop )	return new GedSimpleNode< GedIoDriver<PoolString>, PoolString >( *this, Name.c_str(), psprop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( boolprop )	return new GedBoolNode<PropSetterObj>( *this, Name.c_str(), boolprop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( floatprop )return new GedFloatNode< GedIoDriver<float> >( *this, Name.c_str(), floatprop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( vec4prop )	return new GedSimpleNode< GedIoDriver<CVector4>, CVector4 >( *this, Name.c_str(), vec4prop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( vec3prop )	return new GedSimpleNode< GedIoDriver<CVector3>, CVector3 >( *this, Name.c_str(), vec3prop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( vec2prop )	return new GedSimpleNode< GedIoDriver<CVector2>, CVector2 >( *this, Name.c_str(), vec2prop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( mtx44prop )return new GedSimpleNode< GedIoDriver<CMatrix4>, CMatrix4 >( *this, Name.c_str(), mtx44prop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( castprop )	return new GedObjNode<PropSetterObj>( *this, Name.c_str(), prop, pobject );
	/////////////////////////////////////////////////////////////////////////
	else if( intprop )
	{	return HasUserChoiceDelegate  
		?	(GedItemNode*) new GedSimpleNode< GedIoDriver<int>, int >( *this, Name.c_str(), intprop, pobject )
		:	(GedItemNode*) new GedIntNode< GedIoDriver<int> >( *this, Name.c_str(), intprop, pobject );
	}
	/////////////////////////////////////////////////////////////////////////
	else if( objprop )
	{	ork::Object* psubobj = objprop->Access( pobject );
		if( psubobj ) Recurse(psubobj,Name.c_str());
		else return new GedObjNode<PropSetterObj>( *this, Name.c_str(), prop, pobject );
	}
	/////////////////////////////////////////////////////////////////////////
	else if( dobjprop )
	{	ork::Object* psubobj = 0;
		dobjprop->Get( psubobj, pobject );
		if( psubobj ) Recurse(psubobj);
		return new GedObjNode<PropSetterObj>( *this, Name.c_str(), prop, pobject );
	}
	/////////////////////////////////////////////////////////////////////////
	else if( MapProp )
	{	const reflect::IObjectMapProperty* mapprop = 
		rtti::downcast< const reflect::IObjectMapProperty* >( prop );
		if( mapprop ) return new GedMapNode( *this, Name.c_str(), mapprop, pobject );
	}
	/////////////////////////////////////////////////////////////////////////
	else return new GedLabelNode( *this, Name.c_str(), prop, pobject );
	/////////////////////////////////////////////////////////////////////////
	return 0;
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void ObjModel::Attach( ork::Object* root_object, bool bclearstack, GedItemNode* rootw )
{	this->mRootObject = root_object;
	bool bnewobj = (mCurrentObject!=mRootObject);
	if( bclearstack )
	{	while( false == mBrowseStack.empty() )
		{	mBrowseStack.pop();
		}
	}
	if( mRootObject )
	{	mCurrentObject = mRootObject;
		if( bclearstack )
		{	PushBrowseStack( mCurrentObject );
		}
	}
	if( rootw )
	{	GetGedWidget()->PushItemNode( rootw );
		Recurse( root_object );
		GetGedWidget()->PopItemNode(rootw);
	}
	else
	{	if( GetGedWidget() )
		{	GetGedWidget()->GetRootItem()->DestroyChildren();
		}
		if( root_object )
		{	GetGedWidget()->PushItemNode( GetGedWidget()->GetRootItem() );
			Recurse( root_object );
			GetGedWidget()->PopItemNode( GetGedWidget()->GetRootItem() );
		}
		Detach();
	}
	if( GetGedWidget() )
	{	if( GetGedWidget()->GetViewport() )
		{	if( bnewobj )
			{	GetGedWidget()->GetViewport()->ResetScroll();
			}
		}
		GetGedWidget()->DoResize();
	}
	// dont spam refresh, please
	if( mbEnablePaint ) SigRepaint();
}

//////////////////////////////////////////////////////////////////////////////

void ObjModel::Detach( )
{
	mRootObject = 0;
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::PushBrowseStack( ork::Object* pobj )
{
	mBrowseStack.push( pobj );
}

void ObjModel::PopBrowseStack()
{
	mBrowseStack.pop();
}
ork::Object* ObjModel::BrowseStackTop() const
{	ork::Object* rval = 0;
	if( mBrowseStack.size() )
	{	rval = mBrowseStack.top();
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void ObjModel::Dump( const char* header) const
{	orkprintf( "OBJMODELDUMP<%s>\n", header );
	GedWidget* qw = this->GetGedWidget();
	std::queue<GedItemNode*> ItemQueue;
	if( qw )
	{	ItemQueue.push( qw->GetRootItem() );
	}
	while( ! ItemQueue.empty() )
	{	GedItemNode* node = ItemQueue.front();
		ItemQueue.pop();
		int inumc = node->GetNumItems();
		for( int ic=0; ic<inumc; ic++ )
		{	GedItemNode* pchild = node->GetItem(ic);
			ItemQueue.push( pchild );
		}
		orkprintf( "NODE<%08x> Name<%s>\n", node, node->mName.c_str() );
	}
}

///////////////////////////////////////////////////////////////////////////////

void PersistMapContainer::Describe()
{	ork::reflect::RegisterMapProperty( "Maps", & PersistMapContainer::mPropPersistMap );
}

PersistMapContainer::PersistMapContainer()
{
}
PersistMapContainer::~PersistMapContainer()
{
}

void PersistMapContainer::CloneFrom( const PersistMapContainer& oth )
{	for( orklut<int, PersistantMap*>::const_iterator 
		it=oth.mPropPersistMap.begin();
		it!=oth.mPropPersistMap.end();
		it++ )
	{	ork::Object* pclone = it->second->Clone();
		PersistantMap* pclone_map = rtti::autocast(pclone);
		mPropPersistMap.AddSorted( it->first, pclone_map );
	}
}

void PersistantMap::Describe()
{
	ork::reflect::RegisterMapProperty( "CollapeState", & PersistantMap::mProperties );
}

PersistantMap::PersistantMap()
{
}

PersistantMap::~PersistantMap()
{
}

PersistHashContext::PersistHashContext()
	: mProperty(0)
	, mObject(0)
	, mString(0)
{
}

int PersistHashContext::GenerateHash() const
{	U32 phash = 0;
	U32 ohash = 0;
	const char* classname = 0;
	if( mProperty )
	{	ork::rtti::Class* pclass = mProperty->GetClass();
		const ork::PoolString & name = pclass->Name();
		const char* pname = name.c_str();
		phash = CCRC::HashMemory( pname, int(strlen(pname)));
	}
	if( mObject )
	{	ork::rtti::Class* pclass = mObject->GetClass();
		const ork::PoolString & name = pclass->Name();
		ohash = CCRC::HashMemory( name.c_str(), int(strlen(name.c_str())));
	}
	U32 key = phash^ohash;
	int ikey = *reinterpret_cast<int*>(&key);
	return ikey;
}

PersistantMap* ObjModel::GetPersistMap( const PersistHashContext& Ctx )
{	PersistantMap* prval = 0;
	int key = Ctx.GenerateHash();
	orklut<int, PersistantMap* >::const_iterator it = mPersistMapContainer.GetMap().find( key );
	if( it== mPersistMapContainer.GetMap().end() )
	{	prval = new PersistantMap;
		mPersistMapContainer.GetMap().AddSorted( key , prval );
	}
	else
	{	prval =  it->second;
	}
	return prval;
}

///////////////////////////////////////////////////////////////////////////////

const std::string& PersistantMap::GetValue( const std::string& key )
{	orklut<std::string,std::string>::const_iterator it = mProperties.find( key );
	if( it == mProperties.end() )
	{	mProperties.AddSorted(key,"");
		it = mProperties.find( key );
	}
	return it->second;
}

///////////////////////////////////////////////////////////////////////////////

void PersistantMap::SetValue( const std::string& key, const std::string& val )
{	orklut<std::string,std::string>::iterator it = mProperties.find( key );
	if( it == mProperties.end() )
	{	it = mProperties.AddSorted(key,val);
	}
	else
	{	it->second = val;
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

} } }
///////////////////////////////////////////////////////////////////////////////
