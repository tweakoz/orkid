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
#include "ged_delegate.hpp"
#include <ork/reflect/serialize/LayerDeserializer.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/NullSerializer.h>
#include <ork/reflect/serialize/NullDeserializer.h>
#include <ork/reflect/enum_serializer.h>
///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////

class EnumReader : public reflect::serialize::LayerSerializer
{
public:
	/////////////////////////////////////////////////////
	EnumReader(const ork::Object*pobj, const reflect::IObjectProperty* prop)
		: reflect::serialize::LayerSerializer( mNullSer )
	{
		prop->Serialize( *this, pobj );
	}
	const orkmap<std::string,int>& GetEnumMap() const { return mEnumMap; }
	const std::string& GetCurrentValue() const { return mCurrentValue; }

private:
	/////////////////////////////////////////////////////
	const reflect::IObjectMapProperty*	mMapProp;
	reflect::serialize::NullSerializer	mNullSer;
	/////////////////////////////////////////////////////
	const ork::Object*					mpObject;
	orkmap<std::string,int>				mEnumMap;
	std::string							mCurrentValue;
	/////////////////////////////////////////////////////
    void Hint(const PieceString &, intptr_t ival)
	{	ork::reflect::EnumNameMap* penummap = reinterpret_cast<ork::reflect::EnumNameMap*>(ival);
		for(int i = 0; penummap[i].name; i++)
		{	const char* pname = penummap[i].name;
			int enumval = penummap[i].value;
			mEnumMap[pname]=enumval;
		}
	}
	/////////////////////////////////////////////////////
	bool Serialize(const PieceString &value)
	{	ArrayString<128> astr( value );
		mCurrentValue = astr.c_str();
		return reflect::serialize::LayerSerializer::Serialize(value);
	}
	/////////////////////////////////////////////////////
};

///////////////////////////////////////////////////////////////////////////////

class EnumWriter : public reflect::serialize::LayerDeserializer
{
public:
	EnumWriter(const ork::Object*pobj, const reflect::IObjectProperty* prop)
		: reflect::serialize::LayerDeserializer( mNullDeser )
		, mObject( pobj )
		, mProp( prop )
		, mpSetValue( 0 )
	{

	}

	void SetValue( const char* pstr )
	{
		mpSetValue = pstr;
		ork::Object* pobj = const_cast<ork::Object*>(mObject);
		mProp->Deserialize( *this, pobj );
	}

private:
	reflect::serialize::NullDeserializer	mNullDeser;
	const ork::Object*	mObject;
	const reflect::IObjectProperty* mProp;
	const char*						mpSetValue;

    /*virtual*/ bool Deserialize(MutableString &val)
	{
		val = mpSetValue;
		return true;
	}
};

///////////////////////////////////////////////////////////////////////////////

class GedEnumWidget : public GedItemNode
{
	const std::string *								ValueStrings;
	int												NumStrings;
	bool											mbEmitModelRefresh;
	EnumReader										meReader;
	EnumWriter										meWriter;
	std::string										mCurrentValue;

	
	void DoDraw( lev2::GfxTarget* pTARG ) // virtual
	{
		//////////////////////////////////////
		int ilabw = GetNameWidth()+16;
		//GedItemNode::DoDraw( pTARG );
		//////////////////////////////////////
		GetSkin()->DrawBgBox( this, miX+ilabw, miY+2, miW-ilabw, miH-3, GedSkin::ESTYLE_BACKGROUND_2 );
		GetSkin()->DrawText( this, miX+ilabw, miY+5, mCurrentValue.c_str() );
		GetSkin()->DrawText( this, miX+6, miY+5, mName.c_str() );
		//////////////////////////////////////

	}

public:

	GedEnumWidget( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, ork::Object* obj )
		: GedItemNode( mdl, name, prop, obj)
		, ValueStrings(0)
		, NumStrings(0)
		, mbEmitModelRefresh( false )
		, meReader( obj, prop )
		, meWriter( obj, prop )
	{
		ReSync();
	}

	void ReSync()
	{
		mCurrentValue = meReader.GetCurrentValue();
	}

	void OnMouseDoubleClicked(const ork::ui::Event& ev) final
	{
		QMenu *pMenu = new QMenu(0);

		///////////////////////////////////////////

		const orkmap<std::string,int>& enummap = meReader.GetEnumMap();

		///////////////////////////////////////////

		for( orkmap<std::string,int>::const_iterator it=enummap.begin(); it!=enummap.end(); it++ )
		{
			const std::string& str = it->first;
			QAction *pchildmenu = pMenu->addAction( str.c_str() );
			pchildmenu->setData( QVariant( str.c_str() ) );
		}

		QAction* pact = pMenu->exec(QCursor::pos());

		if( pact )
		{
			QVariant UserData = pact->data();
			QString UserName = UserData.toString();
			mCurrentValue = UserName.toStdString();
			meWriter.SetValue( mCurrentValue.c_str() );
			SigInvalidateProperty();
		}
	}
};

///////////////////////////////////////////////////////////////////////////////

GedItemNode* GedFactoryEnum::CreateItemNode(	ObjModel&mdl,
												const ConstString& Name,
												const reflect::IObjectProperty *prop,
												Object* obj 
											) const
{
	ConstString anno_mkgroup = prop->GetAnnotation( "editor.mktag" );

	GedEnumWidget* PropContainerW = new GedEnumWidget( 
		mdl, 
		Name.c_str(),
		prop,
		obj
		);
	if( anno_mkgroup.length() )
	{
		if( anno_mkgroup == "true" )
		{
			//std::string grpname = CreateFormattedString( "%s:%s", name, valstring.c_str() );
			GedItemNode* parent = mdl.GetGedWidget()->ParentItemNode();
			PropTypeString valstring;
			//prop->GetValueString( obj, valstring );
			parent->mTags[Name.c_str()] = valstring.c_str();
		}
	}
	return PropContainerW;
}

///////////////////////////////////////////////////////////////////////////////

void GedFactoryEnum::Describe()
{
}		

///////////////////////////////////////////////////////////////////////////////
} } }
///////////////////////////////////////////////////////////////////////////////
INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::GedFactoryEnum, "ged.factory.enum" );
