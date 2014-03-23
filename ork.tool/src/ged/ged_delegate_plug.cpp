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
#include <ork/reflect/IObjectPropertyType.h>
#include <ork/reflect/AccessorObjectPropertyObject.h>
#include "ged_delegate.hpp"
#include <ork/dataflow/dataflow.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

class GedPlugDriver : public IoDriverBase
{
public:
	typedef float datatype;
	GedPlugDriver( ObjModel& Model, const reflect::IObjectProperty* prop, Object* obj )
		: IoDriverBase( Model, prop, obj )
		, mPlugFloat(0)
	{
	}
	dataflow::inplug<float>*	mPlugFloat;
	dataflow::inplug<CVector3>*	mPlugVect3;

	void SetPlug( dataflow::inplug<float>* pplug ) { mPlugFloat=pplug; }
	void SetPlug( dataflow::inplug<CVector3>* pplug ) { mPlugVect3=pplug; }
	
	void SetValue( float val )
	{
		if( mPlugFloat ) mPlugFloat->SetDefault(val);
		ObjectGedEditEvent ev;
		ev.mProperty = GetProp();
		GetObject()->Notify(&ev);
	}
	void SetValue( const CVector3& val )
	{
		if( mPlugVect3 ) mPlugVect3->SetDefault(val);
		ObjectGedEditEvent ev;
		ev.mProperty = GetProp();
		GetObject()->Notify(&ev);
	}
	
	void GetValue( float& outval )
	{
		if( mPlugFloat ) outval = mPlugFloat->GetDefault();
	}
	void GetValue( CVector3& outval )
	{
		if( mPlugVect3 ) outval = mPlugVect3->GetDefault();
	}
};

///////////////////////////////////////////////////////////////////////////////

class GedPlugWidget : public GedItemNode
{

	static const int koff = 1;
	//static const int kdim = klabelsize-2;

public:
	typedef float datatype;
	GedPlugDriver&									RefIODriver() { return mIoDriver; }
private:
	GedPlugDriver									mIoDriver;
	const std::string *								ValueStrings;
	int												NumStrings;
	bool											mbEmitModelRefresh;
	Slider<	GedPlugWidget >							mFloatSlider;
	Slider<	GedPlugWidget >							mFloatSliderX;
	Slider<	GedPlugWidget >							mFloatSliderY;
	Slider<	GedPlugWidget >							mFloatSliderZ;
	dataflow::inplugbase*							mInputPlug;
	bool											mbSingle;
	bool											mbCollapsor;
	ork::file::Path::NameType						mPersistID;


	void ReSync() // virtual
	{
		float fval = -1.0f;
		mIoDriver.GetValue( fval );
		printf( "plug<%p:%f>\n", this, fval );
		mFloatSlider.SetVal( fval );
	}

	void DoDraw( lev2::GfxTarget* pTARG ) // virtual
	{
		const int klabh = get_charh();
		const int kdim = klabh-2;
		GetSkin()->DrawBgBox( this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1 );

		//////////////////////////////////////

		int ioff = 3;
		int idim = (klabh-4);
		int ifdim = klabh;
		int igdim = klabh+4;

		//////////////////////////////////////

		int dbx1 = miX+ioff;
		int dbx2 = dbx1+idim;
		int dby1 = miY+ioff;
		int dby2 = dby1+idim;

		int labw = this->GetNameWidth() + 16;

		//////////////////////////////////////


		if( mbCollapsor )
		{
			if( mbSingle )
			{
				GetSkin()->DrawRightArrow( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
				GetSkin()->DrawLine( this, dbx1+1, dby1, dbx1+1, dby2, GedSkin::ESTYLE_BUTTON_OUTLINE );
			}
			else
			{
				GetSkin()->DrawDownArrow( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
				GetSkin()->DrawLine( this, dbx1, dby1+1, dbx2, dby1+1, GedSkin::ESTYLE_BUTTON_OUTLINE );
			}

			dbx1 += (idim+4);
			dbx2 = 	dbx1+idim;

		}

		for( int i=0; i<=6; i+=3 )
		{	
			int j = (i*2);
			GetSkin()->DrawOutlineBox( this, dbx1+i, dby1+i, idim-j, idim-j, GedSkin::ESTYLE_BUTTON_OUTLINE );
		}

		dbx1 += ifdim;
		int dbw = miW-(dbx1-miX);

		GetSkin()->DrawOutlineBox( this, dbx1, miY, dbw, igdim, GedSkin::ESTYLE_DEFAULT_OUTLINE );

		//////////////////////////////////////
		// plugrate BG
		//////////////////////////////////////

		int ichw = get_charw();

		GetSkin()->DrawBgBox( this, dbx1-(ichw>>1)+2, miY+2, ichw*3-1, igdim-4, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT );
		GetSkin()->DrawOutlineBox( this, dbx1-(ichw>>1)+2, miY+2, ichw*3-1, igdim-4, GedSkin::ESTYLE_DEFAULT_OUTLINE );
		
		//////////////////////////////////////
		// plugrate
		//////////////////////////////////////

		const char *ptstr = "??";
		switch( mInputPlug->GetPlugRate() )
		{
			case dataflow::EPR_EVENT:		ptstr = "EV";	break;
			case dataflow::EPR_UNIFORM:		ptstr = "UN";	break;
			case dataflow::EPR_VARYING1:	ptstr = "V1";	break;
			case dataflow::EPR_VARYING2:	ptstr = "V2";	break;
		}

		GetSkin()->DrawText( this, dbx1, miY+4, ptstr );

		dbx1 += ichw*3;
		dbw -= ichw*3;

		//labw += 24;

		//ork::FixedString<128> pstr;
		//pstr.format( "%s <%s>", mName.c_str(), ptstr );
		GetSkin()->DrawText( this, dbx1, miY+4, mName.c_str() );

		//dbx1+= 20;
		//////////////////////////////////////
		GetSkin()->DrawBgBox( this, dbx1+labw, miY+2, dbw-labw, igdim-3, GedSkin::ESTYLE_BACKGROUND_2 );
		//////////////////////////////////////

		const ork::reflect::AccessorObjectPropertyObject* pprop = rtti::autocast( GetOrkProp() );

		if( pprop )
		{
			dataflow::inplugbase* pinpplug = rtti::autocast(pprop->Access(GetOrkObj()));

			if( pinpplug )
			{
				const dataflow::outplugbase* poutplug = pinpplug->GetExternalOutput();

				if( poutplug )
				{
					dataflow::module* pmodule = poutplug->GetModule();

					ork::FixedString<128> pstr;
					pstr.format( "/%s/%s", pmodule->GetName().c_str(), poutplug->GetName().c_str() );
					GetSkin()->DrawText( this, dbx1+labw, miY+4, pstr.c_str() );
				}
				else
				{
					dataflow::inplug<float>* pfloatplug = rtti::autocast(pinpplug);
					if( pfloatplug )
					{
						mFloatSlider.resize(dbx1,miY,dbw,miH);
						int ixi = int(mFloatSlider.GetIndicPos()) - dbx1;
						GetSkin()->DrawBgBox( this, dbx1, miY, dbw, igdim, GedSkin::ESTYLE_BACKGROUND_1 );
						GetSkin()->DrawBgBox( this, dbx1+2, miY+2, dbw-3, igdim-4, GedSkin::ESTYLE_BACKGROUND_2 );
						GetSkin()->DrawBgBox( this, dbx1+2, miY+4, ixi, igdim-8, GedSkin::ESTYLE_DEFAULT_HIGHLIGHT );

						ork::PropTypeString pstr;
						float fval = 0.0f;
						mIoDriver.GetValue(fval);
						float finp = mFloatSlider.GetTextPos();
						int itxi = miX+(finp);
						PropTypeString& str = mFloatSlider.ValString();

						mLabel = str.c_str();
						int itextlen = GetLabelWidth();


						GetSkin()->DrawText( this, dbx1+dbw-(itextlen+8), miY+4, str.c_str() );
					}
					else
					{
						GetSkin()->DrawText( this, dbx1+labw, miY+4, "-nc-" );
					}
				}
			}
		}
	}

public:

	GedPlugWidget( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj )
		: GedItemNode( mdl, name, prop, obj)
		, ValueStrings(0)
		, NumStrings(0)
		, mbEmitModelRefresh( false )
		, mFloatSlider( *this, 0.0f, 1.0f, 0.0f )
		, mFloatSliderX( *this, 0.0f, 1.0f, 0.0f )
		, mFloatSliderY( *this, 0.0f, 1.0f, 0.0f )
		, mFloatSliderZ( *this, 0.0f, 1.0f, 0.0f )
		, mInputPlug(0)
		, mbSingle( true )
		, mIoDriver( mdl, prop, obj )
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
				//fixname += CreateFormattedString( "_%s_", parname );
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
					fixname[i] = '_';
					break;
			}
		}

		mPersistID.format( "%s_plug_collapse", fixname.c_str() );
		//printf( "plug persistID <%s>\n", mPersistID.c_str() );
		
		///////////////////////////////////////////
		PersistHashContext HashCtx;
		HashCtx.mObject = obj;
		HashCtx.mProperty = prop;
		PersistantMap* pmap = mdl.GetPersistMap( HashCtx );
		const std::string& str_single = pmap->GetValue( mPersistID.c_str() );
		if( str_single == "false" )
		{	mbSingle = false;
		}
		///////////////////////////////////////////

		const ork::reflect::AccessorObjectPropertyObject* pprop = rtti::autocast( GetOrkProp() );
		if( pprop )
		{	mInputPlug = rtti::autocast(pprop->Access(obj));
		}
		dataflow::inplug<float>*pfloatplug = rtti::autocast(mInputPlug);
		dataflow::inplug<CVector3>*pvect3plug = rtti::autocast(mInputPlug);
		////////////////////////////////////////////////////
		if( pfloatplug )
		{	mIoDriver.SetPlug( pfloatplug );
			ConstString annomin = prop->GetAnnotation( "editor.range.min" );
			ConstString annomax = prop->GetAnnotation( "editor.range.max" );
			ConstString annolog = prop->GetAnnotation( "editor.range.log" );
			float fmin = 0.0f;
			float fmax = 1.0f;
			bool blogmode = false;
			if( annomin.length() )	{ sscanf( annomin.c_str(), "%f", & fmin ); }
			if( annomax.length() )	{ sscanf( annomax.c_str(), "%f", & fmax ); }
			if( annolog.length() )
			{	if( annolog == "true" )
				{	blogmode = true; 
				}
			}
			mFloatSlider.SetLogMode( blogmode );
			mFloatSlider.SetMinMax( fmin,fmax );
            float fval = pfloatplug->GetValue();
            printf( "FVAL<%f> ISCON<%d>\n", fval, int(pfloatplug->IsConnected()) );
			mFloatSlider.SetVal( fval );
		}
		dataflow::floatxfinplug* pxfplug = rtti::autocast(mInputPlug);
		if( pxfplug )
		{	mbCollapsor = true;
			mdl.GetGedWidget()->PushItemNode( this );
			{	mdl.Recurse( mInputPlug, 0, true );
			}
			mdl.GetGedWidget()->PopItemNode( this );
		}
		////////////////////////////////////////////////////
		if( pvect3plug )
		{	mIoDriver.SetPlug( pvect3plug );
			ConstString annomin = prop->GetAnnotation( "editor.range.min" );
			ConstString annomax = prop->GetAnnotation( "editor.range.max" );
			ConstString annolog = prop->GetAnnotation( "editor.range.log" );
			float fmin = 0.0f;
			float fmax = 1.0f;
			bool blogmode = false;
			if( annomin.length() )	{ sscanf( annomin.c_str(), "%f", & fmin ); }
			if( annomax.length() )	{ sscanf( annomax.c_str(), "%f", & fmax ); }
			if( annolog.length() )
			{	if( annolog == "true" )
				{	blogmode = true; 
				}
			}
			//mFloatSlider.SetLogMode( blogmode );
			//mFloatSlider.SetMinMax( fmin,fmax );
			//mFloatSlider.SetVal( pfloatplug->GetValue() );
		}
		dataflow::vect3xfinplug* pvxfplug = rtti::autocast(mInputPlug);
		if( pvxfplug )
		{	mbCollapsor = true;
			mdl.GetGedWidget()->PushItemNode( this );
			{	mdl.Recurse( mInputPlug, 0, true );
			}
			mdl.GetGedWidget()->PopItemNode( this );
		}
		////////////////////////////////////////////////////
		CheckVis();
	}
	void mouseMoveEvent( QMouseEvent* pEV )
	{	mFloatSlider.mouseMoveEvent(pEV);
		mModel.SigRepaint();
	}
	void mouseReleaseEvent( QMouseEvent* pEV )
	{	mFloatSlider.mouseReleaseEvent(pEV);
		mModel.SigRepaint();
	}
	void mouseDoubleClickEvent ( QMouseEvent * pEV )
	{
		const int klabh = get_charh();
		const int kdim = klabh-2;
	
		Qt::MouseButton button = pEV->button();
		Qt::KeyboardModifiers modifiers = pEV->modifiers();
		int ix = pEV->x() - this->miX;
		int iy = pEV->y() - this->miY;
		bool isCTRL = (modifiers&Qt::ControlModifier);
		////////////////////////////////
		// check collapsor
		////////////////////////////////
		if( mbCollapsor )
		{	if( ix >= koff && ix <= kdim && iy >= koff && iy <= kdim ) // drop down
			{	mbSingle = ! mbSingle;
				///////////////////////////////////////////
				PersistHashContext HashCtx;
				HashCtx.mObject = GetOrkObj();
				HashCtx.mProperty = GetOrkProp();
				PersistantMap* pmap = mModel.GetPersistMap( HashCtx );
				///////////////////////////////////////////
				pmap->SetValue( mPersistID.c_str(), mbSingle ? "true" : "false" );
				///////////////////////////////////////////
				if( isCTRL ) // also do siblings
				{
					GedItemNode* par = GetParent();
					if( par )
					{
						int inumc = par->GetNumItems();
						for( int i=0; i<inumc; i++ )
						{
							GedItemNode* item = par->GetItem( i );
							GedPlugWidget* pgroup = rtti::autocast(item);
							if( pgroup )
							{
								if( pgroup->mbCollapsor )
								{
									pgroup->mbSingle = mbSingle;
									pgroup->CheckVis();
									pmap->SetValue( pgroup->mPersistID.c_str(), mbSingle ? "true" : "false" );
									//pmap->SetValue( pgroup->mPersistID.c_str(), mbCollapsed ? "true" : "false" );
								}
							}
							
						}
					}
				}
				///////////////////////////////////////////
				CheckVis();
				return;
			}
			ix -= (kdim+4);
		}
		////////////////////////////////
		////////////////////////////////
		////////////////////////////////
		dataflow::inplug<float>* pfloatplug = rtti::autocast(mInputPlug);
		dataflow::inplug<CVector3>* pvect3plug = rtti::autocast(mInputPlug);
		////////////////////////////////
		// check connector
		////////////////////////////////
		bool bdrop = ((0==pfloatplug) && (0==pvect3plug));
        if( ((ix >= koff) && (ix <= kdim) && (iy >= koff) && (iy <= kdim)) || bdrop ) // drop down
		{	ConstString anno_ucdclass = GetOrkProp()->GetAnnotation( "ged.plug.delegate" );
			if( anno_ucdclass.length() )
			{	ork::Object* pobj = GetOrkObj();
				rtti::Class *the_class = rtti::Class::FindClass(anno_ucdclass);
				if( the_class )
				{	ork::object::ObjectClass* pucdclass = rtti::autocast(the_class);
					ork::rtti::ICastable* ucdo = the_class->CreateObject();
					IPlugChoiceDelegate* ucd = rtti::autocast(ucdo);
					if( ucd )
					{	IPlugChoiceDelegate::OutPlugMapType choices;
						ucd->EnumerateChoices( this, choices );
						///////////////////////////////////////////////////////////////////////////////
						class PlugChoices : public tool::CChoiceList
						{
						public:
							const IPlugChoiceDelegate::OutPlugMapType& choices;
							virtual void EnumerateChoices( bool bforcenocache=false )
							{	typedef IPlugChoiceDelegate::OutPlugMapType::const_iterator iter_t;
								for( iter_t it=choices.begin(); it!=choices.end(); it++ )
								{	const std::string& name = it->first;
									CAttrChoiceValue myval( name, name );
									myval.SetCustomData( it->second );
									add( myval );
								}
							}
							PlugChoices( const IPlugChoiceDelegate::OutPlugMapType& chc ) 
								: choices(chc)
							{	EnumerateChoices();
								CAttrChoiceValue none( "none", "none" );
								add( none );
							}
						};
						///////////////////////////////////////////////////////////////////////////////
						PlugChoices uchc( choices );
						QMenu* qm = uchc.CreateMenu();
						///////////////////////////////////////////
						QAction* pact = qm->exec(QCursor::pos());
						if( pact )
						{	QVariant UserData = pact->data();
							QString UserName = UserData.toString();
							QVariant chcvalprop = pact->property( "chcval" );
							const CAttrChoiceValue* chcval	= chcvalprop.isValid()
															? (const CAttrChoiceValue*)chcvalprop.value<void*>()
															: (const CAttrChoiceValue*)0;
							if( chcval )
							{	if( mInputPlug )
								{	const any64& customdata = chcval->GetCustomData();
									if( customdata.IsA<ork::dataflow::outplugbase*>() )
									{	ork::dataflow::outplugbase* outplug = 
											customdata.Get<ork::dataflow::outplugbase*>();

										dataflow::dgmodule* pmod = rtti::autocast(mInputPlug->GetModule());
										if( pmod )
										{
											mInputPlug->SafeConnect(*pmod->GetParent(),outplug);
										}
									}
									else
									{	mInputPlug->Disconnect();
									}
								}
							}
						}
						///////////////////////////////////////////
					}
				}
			}
			return;
		}
		else if( pfloatplug )
		{	
			if( false == pfloatplug->IsConnected() )
			{
				mFloatSlider.mouseDoubleClickEvent(pEV);
			}
			mModel.SigRepaint();	
		}
		else if( pvect3plug )
		{	
			if( false == pvect3plug->IsConnected() )
			{
				//mFloatSlider.mouseDoubleClickEvent(pEV);
			}
			mModel.SigRepaint();	
		}
	}
	void CheckVis()
	{	int inumitems = GetNumItems();
		for( int it=0; it<inumitems; it++ )
		{	GetItem(it)->SetVisible( false==mbSingle );
		}
		mModel.GetGedWidget()->DoResize();
	}
	bool DoDrawDefault() const // virtual
	{
		return false;
	}
};

///////////////////////////////////////////////////////////////////////////////

class GedFactoryPlug : public GedFactory
{
	RttiDeclareConcrete(GedFactoryPlug,GedFactory);
public:

	GedItemNode* CreateItemNode(	ObjModel&mdl,
									const ConstString& Name,
									const reflect::IObjectProperty *prop,
									Object* obj ) const
	{
		GedItemNode* PropContainerW = new GedPlugWidget( 
			mdl, 
			Name.c_str(),
			prop,
			obj
			);
		return PropContainerW;
	}

};

void GedFactoryPlug::Describe()
{
}		

///////////////////////////////////////////////////////////////////////////////

class OutPlugChoiceDelegate : public tool::ged::IPlugChoiceDelegate
{	
	RttiDeclareConcrete( OutPlugChoiceDelegate, tool::ged::IPlugChoiceDelegate );
public:
	OutPlugChoiceDelegate() : IPlugChoiceDelegate() , mpgraph(nullptr) {}
private:
	void EnumerateChoices( tool::ged::GedItemNode* pnode, OutPlugMapType& Choices ); // virtual 
	//ProcTex* mptex;
	dataflow::graph_data* mpgraph;
};

void OutPlugChoiceDelegate::Describe(){}
void OutPlugChoiceDelegate::EnumerateChoices( tool::ged::GedItemNode* pnode, OutPlugMapType& Choices )
{	dataflow::inplugbase* pinputplug = 0;
	const ork::reflect::AccessorObjectPropertyObject* pprop = rtti::autocast( pnode->GetOrkProp() );
	if( pprop )
	{	dataflow::inplugbase* pinpplug = rtti::autocast(pprop->Access(pnode->GetOrkObj()));
		if( pinpplug )
		{	pinputplug = pinpplug;
			dataflow::dgmodule* pmodule = rtti::autocast(pinputplug->GetModule());
			mpgraph = pmodule->GetParent();
			//mptex = rtti::autocast(pgraph);
		}
		struct yo
		{
			static void doit(	const ork::PoolString& name,
								dataflow::inplugbase* pinputplug,
								dataflow::module* pmodule,
								dataflow::graph_data* pgraph,
								OutPlugMapType& Choices )
			{
				int inumoutputs = pmodule->GetNumOutputs();
				for( int i=0; i<inumoutputs; i++ )
				{	ork::dataflow::outplugbase* pout = pmodule->GetOutput(i);
					if( pinputplug->GetPlugRate() >= pout->GetPlugRate() ) // make sure input sample rate is >= output sample rate
					if( pgraph->CanConnect( pinputplug, pout ) )
					{	int imaxfo = pout->MaxFanOut();
						int inumc = (int) pout->GetNumExternalOutputConnections();
						bool bok = (imaxfo==0) ? true : (inumc<imaxfo);							
						if( bok )
						{
							std::string PlugName = CreateFormattedString( "/%s/%s", name.c_str(), pout->GetName().c_str() );
							Choices[PlugName]=pout;
						}
					}
				}
			}
		};
		if( mpgraph && pinputplug )
		{	const orklut<ork::PoolString,ork::Object*>& modules = mpgraph->Modules();
			for( orklut<ork::PoolString,ork::Object*>::const_iterator it=modules.begin(); it!=modules.end(); it++ )
			{	const ork::PoolString& name = it->first;
				dataflow::module* pmodule = rtti::autocast( it->second );
                if(	(pmodule && pmodule != pinputplug->GetModule())
					||	(pinputplug->GetDataTypeId() == typeid(float))
					||	(pinputplug->GetDataTypeId() == typeid(CVector3))
				)
				{	dataflow::dgmodule* dgmod = rtti::autocast(pmodule);
					dataflow::graph_data* pgraph2 = dgmod->GetParent();
					if( pgraph2 )
					{	OrkAssert( mpgraph==pgraph2 );
						yo::doit( name, pinputplug, pmodule, mpgraph, Choices );
					}
				}
			}
			/*const orklut<ork::PoolString,ork::Object*>& externals = mpgraph->Externals();
			for( orklut<ork::PoolString,ork::Object*>::const_iterator it=externals.begin(); it!=externals.end(); it++ )
			{	const ork::PoolString& name = it->first;
				dataflow::module* pmodule = rtti::autocast( it->second );
				if( pmodule != pinputplug->GetModule() || pinputplug->GetDataTypeId() == typeid(float) )
				{	dataflow::dgmodule* dgmod = rtti::autocast(pmodule);
					dataflow::graph_inst* pgraph2 = dgmod->GetParent();
					if( pgraph2 )
					{	yo::doit( name, pinputplug, pmodule, mpgraph, Choices );
					}
				}
			}*/
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
}}}
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::GedFactoryPlug, "ged.factory.plug" );

INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::OutPlugChoiceDelegate, "ged::OutPlugChoiceDelegate");
