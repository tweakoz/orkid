////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/qtui/qtmainwin.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/RegisterProperty.h>
///////////////////////////////////////////////////////////////////////////////
#include <orktool/ged/ged.h>
#include <ork/lev2/gfx/dbgfontman.h>
#include <ork/kernel/orklut.hpp>
#include <ork/util/crc64.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
void GedWidget::Describe()
{
	RegisterAutoSignal( GedWidget, Repaint );

	RegisterAutoSlot( GedWidget, Repaint );
	RegisterAutoSlot( GedWidget, ModelInvalidated );
}

//////////////////////////////////////////////////////////////////////////////

orkvector<GedSkin*> InstantiateSkins();

void GedWidget::IncrementSkin()
{
	miSkin++;
	DoResize();
}

GedWidget::GedWidget( ObjModel& mdl )
	: mModel( mdl )
	, mRootItem( 0 )
	, miW(0)
	, miH(0)
	, miRootH( 0 )
	, mViewport( 0 )
	, mStackHash(0)
	, mRootObject(0)
	, mCTQT(0)
	, miSkin(0)
	, mbDeleteModel(false)
	, ConstructAutoSlot(Repaint)
	, ConstructAutoSlot(ModelInvalidated)
{
	SetupSignalsAndSlots();

	mdl.SetGedWidget(this);	
	mRootItem = new GedRootNode( mdl, "Root", 0, 0 );
	PushItemNode( mRootItem );

	orkvector<GedSkin*> skins = InstantiateSkins();

	for( orkvector<GedSkin*>::iterator it=skins.begin(); it!=skins.end(); it++ )
	{
		GedSkin* pskin = (*it);
		AddSkin(pskin);
	}
}

GedWidget::~GedWidget()
{
	DisconnectAll();
	if( mRootItem )
	{
		delete mRootItem;
		mRootItem = 0;
	}
	if( mbDeleteModel )
	{
		delete & mModel;
	}
}

void GedWidget::BindCTQT(lev2::CTQT*ct)
{
	mCTQT=ct; 
	if( mCTQT )
	{
		object::Connect(	& this->GetSigRepaint(),
							& mCTQT->GetSlotRepaint() );

		//object::Connect(	& this->GetSigModelInvalidated(),
		//					& mCTQT->GetSlotRepaint() );
	}
}


void GedWidget::PropertyInvalidated( ork::Object* pobj, const reflect::IObjectProperty* prop )
{
	if( mRootItem )
	{
		orkstack<GedItemNode*> stackofnodes;
		stackofnodes.push(mRootItem);

		while( false == stackofnodes.empty() )
		{
			GedItemNode* pnode = stackofnodes.top();
			stackofnodes.pop();

			if( pnode->GetOrkObj() == pobj )
			{
				if( pnode->GetOrkProp() == prop )
				{
					pnode->Invalidate();
				}
			}
			int inumkids = pnode->GetNumItems();
			for( int i=0; i<inumkids; i++ )
			{
				GedItemNode* pkid = pnode->GetItem(i);
				stackofnodes.push(pkid);
			}
		}
	}
	SlotRepaint();
}

//////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////

void GedWidget::SlotRepaint( )
{
	if( mCTQT )
	{
		mCTQT->SlotRepaint();
	}
}

void GedWidget::SlotModelInvalidated( )
{
	if( mCTQT )
	{
		mCTQT->SlotRepaint();
	}
}

//////////////////////////////////////////////////////////////////////////////

U64 GedWidget::GetStackHash() const 
{
	return mStackHash;
}

void GedWidget::ComputeStackHash()
{
	//const orkstack<GedItemNode*>& c = mItemStack._Get_container();

	U64 rval = 0;
	boost::Crc64 the_hash;
	boost::crc64_init( the_hash );

	int isize = (int) mItemStack.size();

	crc64_compute( the_hash, & mModel, sizeof(ObjModel*) );
	crc64_compute( the_hash, & isize, sizeof(isize) );

	int idx = 0;
	for( std::deque<GedItemNode*>::const_iterator it=mItemStack.begin(); it!=mItemStack.end(); it++ )
	{
		const GedItemNode* pnode = *(it);

		const GedItemNode::NameType& yo = pnode->mName;

		const char* pname = yo.c_str();

		int ilen = (int)strlen( pname );

		crc64_compute( the_hash, pname, ilen );
		crc64_compute( the_hash, & idx, sizeof(idx) );

		idx++;

	}
	//for( 		mItemStack
	boost::crc64_fin(the_hash);

	mStackHash = the_hash.crc0;// | (the_hash.crc1<<32);
}

//////////////////////////////////////////////////////////////////////////////

void GedWidget::OnSelectionChanged()
{
	orkprintf( "SelectionChanged\n" );
}

//////////////////////////////////////////////////////////////////////////////

GedItemNode* GedWidget::ParentItemNode() const
{
	return mItemStack.front();
}

//////////////////////////////////////////////////////////////////////////////

void GedWidget::PushItemNode(GedItemNode*qw)
{
	mItemStack.push_front(qw);
	ComputeStackHash();
}

//////////////////////////////////////////////////////////////////////////////

void GedWidget::PopItemNode(GedItemNode*qw)
{
	OrkAssert( mItemStack.front() == qw );
	mItemStack.pop_front();
	ComputeStackHash();
}

//////////////////////////////////////////////////////////////////////////////

void GedWidget::AddChild(GedItemNode*pw)
{	
	GedItemNode* TopItem = (mItemStack.size()>0) ? mItemStack.front() : 0;
	if( TopItem==0 ) // our root item widget
	{
	}
	else
	{	TopItem->AddItem( pw );
	}
}
//////////////////////////////////////////////////////////////////////////////

void GedWidget::DoResize()
{
	if( mRootItem )
	{
		miRootH = mRootItem->CalcHeight();
		mRootItem->Layout( 2, 2, miW-4, miH-4);
	}
	else
	{
		miRootH = 0;
	}
}

//////////////////////////////////////////////////////////////////////////////

void GedWidget::Attach( ork::Object* obj )
{
	DoResize();
}

//////////////////////////////////////////////////////////////////////////////

void GedWidget::Draw( lev2::GfxTarget* pTARG, int iscrolly )
{
	///////////////////////////////////////////////
	miW = pTARG->GetW();
	miH = pTARG->GetH();
	GedItemNode* root = GetRootItem();
	///////////////////////////////////////////////
	if( false == pTARG->FBI()->IsPickState() )
	{
		root->Layout( 2, 2, miW-4, miH-4 );
	}
	///////////////////////////////////////////////
	GetSkin()->SetScrollY( iscrolly );
	GetSkin()->Begin(pTARG,mViewport);
	{
		orkstack<GedItemNode*> NodeStack;
		NodeStack.push( root );
		
		while( false == NodeStack.empty() )
		{
			GedItemNode* item = NodeStack.top();
			NodeStack.pop();
			int id = item->GetDepth();
			///////////////////////////////////////////////
			int inumc = item->GetNumItems();
			for( int ic=0; ic<inumc; ic++ )
			{	GedItemNode* child = item->GetItem(ic);
				if( child->IsVisible() )
				{
					child->SetDepth(id+1);
					NodeStack.push(child);
				}
			}
			///////////////////////////////////////////////
			if( item->IsVisible() )
			{
				item->Draw( pTARG );
			}
			///////////////////////////////////////////////
		}
	}
	GetSkin()->End(pTARG);
	///////////////////////////////////////////////
}
//////////////////////////////////////////////////////////////////////////////

GedSkin* GedWidget::GetSkin()
{
	GedSkin* pret = 0;
	int inumskins = int(mSkins.size());
	if( inumskins )
	{
		pret = mSkins[miSkin%inumskins];
	}
	return pret;
}
void GedWidget::AddSkin( GedSkin* psk)
{
	mSkins.push_back(psk);
}

} } }
