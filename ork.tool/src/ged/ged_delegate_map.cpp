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
#include "ged_delegate_asset.hpp"
#include <orktool/ged/ged_io.h>
#include <ork/reflect/IProperty.h>
#include <ork/reflect/IObjectProperty.h>
#include <ork/reflect/IObjectMapProperty.h>
#include <ork/reflect/IObjectPropertyObject.h>
#include <ork/reflect/IDeserializer.h>
#include <ork/reflect/serialize/XMLSerializer.h>
#include <ork/reflect/serialize/XMLDeserializer.h>
#include <ork/stream/StringOutputStream.h>
#include <ork/stream/StringInputStream.h>
#include <ork/stream/FileOutputStream.h>
#include <ork/stream/FileInputStream.h>
#include <ork/reflect/serialize/LayerSerializer.h>
#include <ork/reflect/serialize/LayerDeserializer.h>
#include <ork/reflect/serialize/ShallowSerializer.h>
#include <ork/reflect/serialize/NullSerializer.h>
#include <ork/reflect/serialize/NullDeserializer.h>
#include <ork/application/application.h>

#include <orktool/toolcore/dataflow.h>
#include <ork/reflect/Command.h>
#include <QtWidgets/QInputDialog>
#include <ork/asset/Asset.h>

#include "ged_delegate_map.h"
#include "ged_delegate.hpp"
#include "ged_delegate_file.hpp"

INSTANTIATE_TRANSPARENT_RTTI( ork::tool::ged::GedMapNode, "ged.mapnode" );

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool { namespace ged {
///////////////////////////////////////////////////////////////////////////////
MapTraverseSerializer::MapTraverseSerializer(	GedMapNode& mapnode,
												ISerializer &serializer,ObjModel&model,
												ork::Object*pobj,
												const reflect::IObjectProperty *prop )
	: reflect::serialize::LayerSerializer(serializer)
	, mModel( model )
	, mProp( prop )
	, mObject( pobj )
	, mMapNode( mapnode )
	, miMultiIndex( 0 )
	, mKeyDeco( "", 0 )
	, miFloatCounter( 0 )
	, mbIsKey( true )
{
	mMapValueTypeAnno  = prop->GetAnnotation( "editor.map.value.type" );
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::IsMultiKey() const
{
	return mMapNode.IsMultiMap();
}
PropTypeString MapTraverseSerializer::GetDecoKeyString() const
{
	if( IsMultiKey() )
	{
		return mKeyDeco.DecoratedName();
	}
	else
	{
		return mKeyDeco.mActualKey;
	}
}
///////////////////////////////////////////////////////////////////////////////
void MapTraverseSerializer::Hint(const PieceString &value)
{
	ArrayString<64> astr(value);
	mLastHint = AddPooledString(astr.c_str());

	if( mLastHint == FindPooledString("map_key") )
	{
		miFloatCounter=0;
		mbIsKey = true;
	}
	else if( mLastHint == FindPooledString("map_value") )
	{
		miFloatCounter=0;
		mbIsKey = false;
	}
}
///////////////////////////////////////////////////////////////////////////////
void MapTraverseSerializer::Hint(const PieceString &key,  intptr_t ival)
{
	ArrayString<64> astr(key);
	mLastHint = AddPooledString(astr.c_str());

	if( astr == "MultiIndex" )
	{
		miMultiIndex = ival;
	}
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const reflect::IObjectProperty *prop, const Object *pser)
{
	return reflect::serialize::LayerSerializer::Serialize(prop,pser);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const bool &value)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const char &value)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const short &value)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const int &value)
{
	bool iskey = IsKey();

	if( iskey )
	{
		PropType<int>::ToString( value, mKeyString );
		mKeyDeco = KeyDecoName( mKeyString.c_str(), miMultiIndex );
		mMapNode.AddKey( mKeyDeco );
	}
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const long &value)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const float &value)
{
	bool iskey = IsKey();

	if( iskey )
	{
		PropType<float>::ToString( value, mKeyString );
		mKeyDeco = KeyDecoName( mKeyString.c_str(), miMultiIndex );
		mMapNode.AddKey( mKeyDeco );
		//OrkAssertNotImpl();
	}
	else
	{
		if( mLastHint==FindPooledString("fvec3") )
		{
			if( miFloatCounter == 2 )
			{
				GedSimpleNode< GedMapIoDriver, fvec3 >* simplenode = new GedSimpleNode< GedMapIoDriver, fvec3 >
					( 
					mModel, 
					GetDecoKeyString().c_str(),
					mProp,
					mObject
					);

				simplenode->RefIODriver().SetKey(mKeyDeco);
				mModel.GetGedWidget()->AddChild( simplenode );
			}
		}
		else
		{
			//GedSimpleNode< GedMapIoDriver, float >* simplenode = new GedSimpleNode< GedMapIoDriver, float >
			//	( 
			//	mModel, 
			//	GetDecoKeyString().c_str(),
			//	mProp,
			//	mObject
			//	);
			//			
			GedFloatNode<GedMapIoDriver>* pfloatnode = new GedFloatNode<GedMapIoDriver>(
				mModel,
				GetDecoKeyString().c_str(),
				mProp,
				mObject
				);
			pfloatnode->RefIODriver().SetKey(mKeyDeco);
			pfloatnode->ReSync();
			mModel.GetGedWidget()->AddChild( pfloatnode );
		}
	}
	miFloatCounter++;
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const double &value)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const rtti::ICastable *value)
{
	bool iskey = IsKey();
	if( iskey )
	{
		OrkAssertNotImpl();
	}
	else
	{
		const char* pname = mKeyString.c_str();
		const ork::Object* pcsubobj = rtti::autocast(value);

		ConstString anno = mProp->GetAnnotation( "editor.assettype" );
		if( anno.length() )
		{
			GedAssetNode<GedMapIoDriver>* itemnode = new GedAssetNode<GedMapIoDriver>( 
				mModel, 
				GetDecoKeyString().c_str(),
				mProp,
				mObject
				);

			itemnode->RefIODriver().SetKey(mKeyDeco);
			mModel.GetGedWidget()->AddChild( itemnode );
			itemnode->SetLabel();
		}
		else
		{
			ork::Object* psubobj = const_cast<ork::Object*>(pcsubobj);
			if( psubobj )
			{
				ork::object::ObjectClass* pclass = psubobj->GetClass();
				const char* pclassname = pclass->Name().c_str();
				std::string fstr = CreateFormattedString( "<%s> : %s", GetDecoKeyString().c_str(), pclassname );
				GedItemNode* pchild = mModel.Recurse( psubobj, fstr.c_str() );
			}
			else
			{
				std::string fstr = CreateFormattedString( "%s :: Create", GetDecoKeyString().c_str() );
				GedMapFactoryNode* itemnode = new GedMapFactoryNode( mModel, fstr.c_str(), mProp, mObject );
				itemnode->RefIODriver().SetKey(mKeyDeco);
				mModel.GetGedWidget()->AddChild( itemnode );
				any64 key;
				key.Set( AddPooledString(pname) );
				itemnode->SetKey(key);
			}
		}
	}
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapTraverseSerializer::Serialize(const PieceString &value)
{
	ArrayString<1024> mstr(value);
	bool iskey = IsKey();
	if( iskey )
	{
		mKeyString.set( mstr.c_str() );
		mKeyDeco = KeyDecoName( mstr.c_str(), miMultiIndex );
		mMapNode.AddKey( mKeyDeco );
	}
	else
	{
		GedItemNode* itemnode = 0;

		if( mMapValueTypeAnno.length() )
		{
			OrkAssert( mMapValueTypeAnno == ConstString("filelist" ) );

			GedFileNode<GedMapIoDriver>* filenode = new GedFileNode<GedMapIoDriver>( 
				mModel, 
				GetDecoKeyString().c_str(),
				mProp,
				mObject
				);
	
			filenode->RefIODriver().SetKey(mKeyDeco);

			filenode->SetLabel();

			itemnode = filenode;

		}
		else
		{
			GedSimpleNode< GedMapIoDriver, PoolString >* simplenode = new GedSimpleNode< GedMapIoDriver, PoolString >
				( 
				mModel, 
				GetDecoKeyString().c_str(),
				mProp,
				mObject
				);

			simplenode->RefIODriver().SetKey(mKeyDeco);

			itemnode = simplenode;
		}

		mModel.GetGedWidget()->AddChild( itemnode );
	}
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
MapItemWriteSerializer::MapItemWriteSerializer( GedMapIoDriver& iodriver )
	: reflect::serialize::LayerDeserializer(mNullDeser)
	, mIoDriver( iodriver )
	, mMapProp( 0 )
	, mValueCastable( 0 )
	, mFloat( 0.0f )
	, mbIsKey( true )
	, miFloatIndex( 0 )
{
	mMapProp = rtti::autocast( mIoDriver.GetProp() );
}	
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::Hint(const PieceString &value)
{
	ArrayString<64> astr(value);
	mLastHint = AddPooledString(astr.c_str());

	if( mLastHint == FindPooledString("map_key") )
	{
		miFloatIndex = 0;
		mbIsKey = true;
	}
	else if( mLastHint == FindPooledString("map_value") )
	{
		miFloatIndex = 0;
		mbIsKey = false;
	}
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(MutableString &mstr)
{
	bool iskey = IsKey();

	mstr.format( "%s", mArrayString.c_str() );
	mArrayString = ork::ArrayString<1024>("");
	bool bok = true;
	return bok;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(rtti::ICastable *& castable)
{
	bool iskey = IsKey();
	castable = mValueCastable;
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(double &)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(float &fv)
{
	bool iskey = IsKey();

	switch( meWriteType )
	{
		case EWT_FLOAT:
			fv = mFloat;
			break;
		case EWT_CVECTOR3:
			switch( miFloatIndex )
			{
				case 0: fv=mVector3.GetX(); break;		
				case 1: fv=mVector3.GetY(); break;		
				case 2: fv=mVector3.GetZ(); break;		
				case 3: 
					OrkAssert(false);
					break;		
			}
			miFloatIndex++;
			break;
	}
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(long &)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(int &)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(short &)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(char &)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(bool &)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return true;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::Deserialize(ork::ResizableString &rs)
{
	bool iskey = IsKey();

	rs.format( "%s", mArrayString.c_str() );
	mArrayString = ork::ArrayString<1024>("");
	bool bok = true;
	return bok;
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemWriteSerializer::DeserializeData(unsigned char *,size_t)
{
	OrkAssertNotImpl();
	bool iskey = IsKey();
	return true;
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::Insert( const char* pchar )
{
	//////////////////////////////////////////////////
	// DEFAULT VALUES FOR INSERT

	mArrayString = ArrayString<1024>( "" );
	mFloat = 0.0f;

	//////////////////////////////////////////////////

	ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	MapKeyWriter keyser( mIoDriver );
	mIoDriver.SetKey( pchar );
	bool bok = mMapProp->DeserializeItem( this, keyser, ork::reflect::IObjectMapProperty::kDeserializeInsertItem, mIoDriver.GetObject() );
	OrkAssert( bok );
}
///////////////////////////////////////////////////////////////////////////////

void MapItemWriteSerializer::Remove( const KeyDecoName& kdeco )
{
	ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	MapKeyWriter keyser( mIoDriver );
	mIoDriver.SetKey( kdeco.mActualKey.c_str() );
	//mIoDriver.GetObject()->Notify( );
	bool bok = mMapProp->DeserializeItem( 0, keyser, kdeco.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert( bok );
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::Move( const KeyDecoName& kdeco, const char* pname )
{
	ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	
	MapKeyWriter keyser( mIoDriver );
	mIoDriver.SetKey( kdeco.mActualKey.c_str() );

	////////////////////////////////////////////////
	// serialize item to temp area
	////////////////////////////////////////////////

	ArrayString<16384> astr;
	MutableString pstr(astr);
	ork::stream::StringOutputStream ostream(pstr);
	ork::reflect::serialize::XMLSerializer oser(ostream);
	bool bok = mMapProp->SerializeItem( oser, keyser, kdeco.miMultiIndex, mIoDriver.GetObject() );

	if( false == bok ) return;

	////////////////////////////////////////////////
	// delete old key
	////////////////////////////////////////////////

	bok = mMapProp->DeserializeItem( 0, keyser, kdeco.miMultiIndex, mIoDriver.GetObject() );

	////////////////////////////////////////////////
	// write temp item to new key
	////////////////////////////////////////////////

	GedMapIoDriver niodriver( mIoDriver.GetModel(), mIoDriver.GetProp(), mIoDriver.GetObject() );
	niodriver.SetKey( KeyDecoName(pname,0) );
	MapKeyWriter keyser2( niodriver );
	ork::stream::StringInputStream istream(pstr.c_str());
	ork::reflect::serialize::XMLDeserializer iser(istream);

	bok = mMapProp->DeserializeItem( &iser, keyser2, ork::reflect::IObjectMapProperty::kDeserializeInsertItem, mIoDriver.GetObject() );

	OrkAssert( bok );

	////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::Duplicate( const KeyDecoName& kdeco, const char* pname )
{
	ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	
	MapKeyWriter keyser( mIoDriver );
	mIoDriver.SetKey( kdeco.mActualKey.c_str() );

	////////////////////////////////////////////////
	// serialize item to temp area
	////////////////////////////////////////////////

	ArrayString<16384> astr;
	MutableString pstr(astr);
	ork::stream::StringOutputStream ostream(pstr);
	ork::reflect::serialize::XMLSerializer oser(ostream);
	bool bok = mMapProp->SerializeItem( oser, keyser, kdeco.miMultiIndex, mIoDriver.GetObject() );

	if( false == bok ) return;

	////////////////////////////////////////////////
	// delete old key
	////////////////////////////////////////////////

	//bok = mMapProp->DeserializeItem( 0, keyser, kdeco.miMultiIndex, mIoDriver.GetObject() );

	////////////////////////////////////////////////
	// write temp item to new key
	////////////////////////////////////////////////

	GedMapIoDriver niodriver( mIoDriver.GetModel(), mIoDriver.GetProp(), mIoDriver.GetObject() );
	niodriver.SetKey( KeyDecoName(pname,0) );
	MapKeyWriter keyser2( niodriver );
	ork::stream::StringInputStream istream(pstr.c_str());
	ork::reflect::serialize::XMLDeserializer iser(istream);

	bok = mMapProp->DeserializeItem( &iser, keyser2, ork::reflect::IObjectMapProperty::kDeserializeInsertItem, mIoDriver.GetObject() );

	OrkAssert( bok );

	////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::Import( const KeyDecoName& kdeco, const char* pname )
{
	QString FileName = QFileDialog::getOpenFileName( 0, "Import Map Item from mit File", 0, "MapItemFile (*.mit)");
	std::string fname = FileName.toStdString();
	if( fname.length() )
	{
		ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
		
		////////////////////////////////////////////////
		// write temp item to new key
		////////////////////////////////////////////////

		lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
		{
			stream::FileInputStream istream(fname.c_str());
			reflect::serialize::XMLDeserializer iser(istream);
		
			GedMapIoDriver niodriver( mIoDriver.GetModel(), mIoDriver.GetProp(), mIoDriver.GetObject() );
			niodriver.SetKey( KeyDecoName(pname,0) );
			MapKeyWriter keyser2( niodriver );

			bool bok = mMapProp->DeserializeItem( &iser, keyser2, ork::reflect::IObjectMapProperty::kDeserializeInsertItem, mIoDriver.GetObject() );

			OrkAssert( bok );
		}
		lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	}

	////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::Export( const KeyDecoName& kdeco, const char* pname )
{
	QString FileName = QFileDialog::getSaveFileName( 0, "Export MapItem to File", 0, "MapItem (*.mit)");
	file::Path::NameType fname = FileName.toStdString().c_str();
	if( fname.length() )
	{
		//SetRecentSceneFile(FileName.toAscii().data(),SCENEFILE_DIR);
		if( ork::FileEnv::filespec_to_extension( fname ).length() == 0 ) fname += ".mit";

		ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	
		lev2::GfxEnv::GetRef().GetGlobalLock().Lock();
		{
			MapKeyWriter keyser( mIoDriver );
			mIoDriver.SetKey( kdeco.mActualKey.c_str() );
			////////////////////////////////////////////////
			// serialize item to temp area
			////////////////////////////////////////////////

			ork::stream::FileOutputStream ostream(fname.c_str());
			ork::reflect::serialize::XMLSerializer oser(ostream);
			bool bok = mMapProp->SerializeItem( oser, keyser, kdeco.miMultiIndex, mIoDriver.GetObject() );

			////////////////////////////////////////////////
		}
		lev2::GfxEnv::GetRef().GetGlobalLock().UnLock();
	}
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::SetValue( rtti::ICastable *castable )
{
	mValueCastable = castable;
	ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	MapKeyWriter keyser( mIoDriver );
	bool bok = mMapProp->DeserializeItem( this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert( bok );
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::SetValue( const char* pstr )
{
	//mValuePoolString = ps;
	ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	MapKeyWriter keyser( mIoDriver );
	mArrayString = ArrayString<1024>( pstr );
	bool bok = mMapProp->DeserializeItem( this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert( bok );
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::SetValue( float flt )
{
	ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	MapKeyWriter keyser( mIoDriver );
	meWriteType = EWT_FLOAT;
	mFloat = flt;
	bool bok = mMapProp->DeserializeItem( this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert( bok );
}
///////////////////////////////////////////////////////////////////////////////
void MapItemWriteSerializer::SetValue( const fvec3& v3 )
{
	ork::PropSetContext pctx( ork::PropSetContext::EPROPEDITOR );
	MapKeyWriter keyser( mIoDriver );
	meWriteType = EWT_CVECTOR3;
	mVector3 = v3;
	bool bok = mMapProp->DeserializeItem( this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert( bok );
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
MapItemReadSerializer::MapItemReadSerializer( const GedMapIoDriver& ioDriver)	
	: reflect::serialize::LayerSerializer(mNullSer)
	, mIoDriver( ioDriver )
	, mMapProp( 0 )
	, mpObject( 0 )
	, miFloatIndex( 0 )
{
	mMapProp = rtti::autocast( mIoDriver.GetProp() );
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemReadSerializer::Serialize(const PieceString &value) //virtual
{
	switch( meReadType )
	{
		case ERT_POOLSTRING:
		{	mPoolString = AddPooledString( value );
			break;
		}
		case ERT_PATHOBJECT:
		{	ArrayString<1024> arstr( value );
			mPathObject = file::Path( arstr.c_str() );
			break;
		}
		default:
			OrkAssert(false);
	}
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemReadSerializer::Serialize(const rtti::ICastable *value) //virtual
{
	mpObject = rtti::autocast( value );
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
bool MapItemReadSerializer::Serialize(const float& value)
{	
	switch( meReadType )
	{
		case ERT_CVECTOR3:
		{
			switch( miFloatIndex )
			{
				case 0:	mVector3.SetX( value ); break;	
				case 1:	mVector3.SetY( value );	break;	
				case 2:	mVector3.SetZ( value );	break;	
				case 3:	
					OrkAssert(false);
			}
			miFloatIndex++;
			break;
		}
		default:
			mFloat = value;
			break;
	}
	
	return reflect::serialize::LayerSerializer::Serialize(value);
}
///////////////////////////////////////////////////////////////////////////////
const ork::asset::Asset* MapItemReadSerializer::GetAsset()
{	
	meReadType = ERT_NEWASSET;
	if( mIoDriver.mDecoKey.mActualKey == "" )
	{
		return 0;
	}
	MapKeyWriter keyser( mIoDriver );
	bool bok = mMapProp->SerializeItem( *this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert(bok);
	const ork::asset::Asset* passet = rtti::autocast(mpObject);
	return passet;
}
///////////////////////////////////////////////////////////////////////////////
PoolString MapItemReadSerializer::GetPoolString()
{	
	meReadType = ERT_POOLSTRING;
	if( mIoDriver.mDecoKey.mActualKey == "" )
	{
		return AddPooledString("");
	}
	MapKeyWriter keyser( mIoDriver );
	bool bok = mMapProp->SerializeItem( *this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert(bok);
	return mPoolString;
}
///////////////////////////////////////////////////////////////////////////////
file::Path MapItemReadSerializer::GetPathObject()
{	
	meReadType = ERT_PATHOBJECT;
	if( mIoDriver.mDecoKey.mActualKey == "" )
	{
		return AddPooledString("");
	}
	MapKeyWriter keyser( mIoDriver );
	bool bok = mMapProp->SerializeItem( *this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert(bok);
	return mPathObject;
}
///////////////////////////////////////////////////////////////////////////////
float MapItemReadSerializer::GetFloat()
{	
	meReadType = ERT_FLOAT;
	if( mIoDriver.mDecoKey.mActualKey == "" )
	{
		return 0.0f;
	}
	MapKeyWriter keyser( mIoDriver );
	bool bok = mMapProp->SerializeItem( *this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert(bok);
	return mFloat;
}
///////////////////////////////////////////////////////////////////////////////
fvec3 MapItemReadSerializer::Getfvec3()
{	
	meReadType = ERT_CVECTOR3;
	if( mIoDriver.mDecoKey.mActualKey == "" )
	{
		return fvec3();
	}
	MapKeyWriter keyser( mIoDriver );
	bool bok = mMapProp->SerializeItem( *this, keyser, mIoDriver.mDecoKey.miMultiIndex, mIoDriver.GetObject() );
	OrkAssert(bok);
	return mVector3;
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
void GedMapNode::Describe()
{
}
///////////////////////////////////////////////////////////////////////////////
GedMapNode::GedMapNode( ObjModel& mdl, const char* name, const reflect::IObjectProperty* prop, Object* obj )
	: GedItemNode( mdl, name, prop, obj )
	, mMapProp( rtti::autocast( prop ) )
	, mKeyNode( 0 )
	, mCurrentKey( "" )
	, mItemIndex( 0 )
	, mbSingle( true )
	, mbConst( false )
	, mbImpExp( false )
	, mbIsMultiMap( (mMapProp && obj) ? mMapProp->IsMultiMap( obj ) : false )
{
	///////////////////////////////////////////
	PersistHashContext HashCtx;
	HashCtx.mObject = obj;
	HashCtx.mProperty = prop;
	PersistantMap* pmap = mdl.GetPersistMap( HashCtx );
	///////////////////////////////////////////

	const std::string& str_single = pmap->GetValue( "single" );

	if( str_single == "false" )
	{
		mbSingle = false;
	}

	const std::string& str_index = pmap->GetValue( "index" );

	if( str_index != "" )
	{
		sscanf( str_index.c_str(), "%d", & mItemIndex );
	}

	/////////////////////////////////
	SetName( name );
	/////////////////////////////////
	int isize = mMapProp->GetSize(obj);
	/////////////////////////////////
	ConstString anno_const = prop->GetAnnotation( "editor.map.policy.const" );
	/////////////////////////////////
	//bool bsinglemode = false;
	/////////////////////////////////
	if( anno_const.length() )
	{	if( anno_const == "true" )
		{	mbConst = true;
		}
	}
	/////////////////////////////////
	ConstString anno_impexp = prop->GetAnnotation( "editor.map.policy.impexp" );
	bool bimpexpmode = false;
	if( anno_impexp.length() )
	{	if( anno_impexp == "true" )
		{	mbImpExp = true;
		}
	}
	/////////////////////////////////
	ConstString anno_vclass = prop->GetAnnotation( "editor.map.value.class" );
	rtti::Class* pvalueclass = anno_vclass.length() ? rtti::Class::FindClass( anno_vclass ) : 0;
	//////////////////////////////////////////////////////////////////
	mdl.GetGedWidget()->PushItemNode( this );
	{
		reflect::serialize::NullSerializer nser;
		MapTraverseSerializer mapser(*this,nser,mdl,obj,prop);
		mapser.Serialize( prop, obj );
	}
	mdl.GetGedWidget()->PopItemNode( this );
	//////////////////////////////////////////////////////////////////
	CheckVis();
}
///////////////////////////////////////////////////////////////////////////////
//static const int kdim = GedMapNode::klabelsize-2;
///////////////////////////////////////////////////////////////////////////////
void GedMapNode::AddKey( const KeyDecoName& pkey )
{
	mMapKeys.insert( std::make_pair(pkey.DecoratedName(), pkey) );
}
///////////////////////////////////////////////////////////////////////////////
bool GedMapNode::IsKeyPresent( const KeyDecoName& pkey ) const
{
	PropTypeString deco = pkey.DecoratedName();

	bool bret = (mMapKeys.find(deco) != mMapKeys.end());
	return bret;
}
///////////////////////////////////////////////////////////////////////////////
void GedMapNode::AddItem(const ork::ui::Event& ev)
{
	const int klabh = get_charh();
	const int kdim = klabh-2;

	int ibasex = (kdim+4)*2+3;

	QString qstr = GedInputDialog::getText ( ev, this, 0, ibasex, 0, miW-ibasex-6, klabh );

	std::string sstr = qstr.toStdString();
	if( sstr.length() )
	{
		KeyDecoName kdeca( sstr.c_str() );

		if( IsKeyPresent( kdeca ) )
		{
			if( false==IsMultiMap() ) return;
		}

		mModel.SigPreNewObject();
		GedMapIoDriver iodriver( mModel, mMapProp, GetOrkObj() );
		iodriver.insert( sstr.c_str() );
	}
}
///////////////////////////////////////////////////////////////////////////////
KeyDecoName::KeyDecoName( const char* pkey ) // precomposed name/index
{
	ArrayString<256> tempstr( pkey );
	const char* pfindcolon = strstr( tempstr.c_str(), ":" );
	if( pfindcolon )
	{
		size_t ilen = strlen(tempstr.c_str());
		int ipos = (pfindcolon-tempstr.c_str());
		const char* pindexstr = pfindcolon+1;
		
		size_t inumlen = ilen-ipos;

		if( inumlen )
		{
			miMultiIndex = atoi( 	pindexstr );
			char* pchar = tempstr.begin() + ipos;
			pchar[0] = 0;
			mActualKey.set( tempstr.c_str() );
		}
	}
	else
	{
		mActualKey.set( pkey );
		miMultiIndex = 0;
	}
}
///////////////////////////////////////////////////////////////////////////////
KeyDecoName::KeyDecoName( const char* pkey, int index ) // decomposed name/index
	: miMultiIndex( index )
	, mActualKey( pkey )
{


}
///////////////////////////////////////////////////////////////////////////////
PropTypeString KeyDecoName::DecoratedName() const
{
	PropTypeString rval;
	rval.format( "%s:%d", mActualKey.c_str(), miMultiIndex );
	return rval;
}
///////////////////////////////////////////////////////////////////////////////
void GedMapNode::RemoveItem(const ork::ui::Event& ev)
{
	const int klabh = get_charh();
	const int kdim = klabh-2;

	int ibasex = (kdim+4)*3+3;

	QString qstr = GedInputDialog::getText ( ev, this, 0, ibasex, 0, miW-ibasex-6, klabh );

	std::string sstr = qstr.toStdString();
	if( sstr.length() )
	{
		KeyDecoName kdec( sstr.c_str() );

		if( IsKeyPresent( kdec ) )
		{
			mModel.SigPreNewObject();
			GedMapIoDriver iodriver( mModel, mMapProp, GetOrkObj() );
			iodriver.remove( kdec );
			mModel.Attach(mModel.CurrentObject());
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
void GedMapNode::DuplicateItem(const ork::ui::Event& ev)
{
	const int klabh = get_charh();
	const int kdim = klabh-2;

	int ibasex = (kdim+4)*3+3;

	QString qstra = GedInputDialog::getText ( ev, this, 0, ibasex, 0, miW-ibasex-6, klabh );
	
	ork::msleep(100);
	QString qstrb = GedInputDialog::getText ( ev, this, 0, ibasex, 0, miW-ibasex-6, klabh );

    std::string sstra = qstra.toStdString();
    std::string sstrb = qstrb.toStdString();

	if( sstra.length() && sstrb.length() && sstra!=sstrb )
	{
		KeyDecoName kdeca( sstra.c_str() );
		KeyDecoName kdecb( sstrb.c_str() );

		if( IsKeyPresent( kdecb ) )
		{
			if( false==IsMultiMap() ) return;
		}
		if( false == IsKeyPresent( kdeca ) )
		{
			return;
		}

		mModel.SigPreNewObject();

		GedMapIoDriver iodriver( mModel, mMapProp, GetOrkObj() );
		iodriver.duplicate( kdeca, sstrb.c_str() );
		
		mModel.Attach(mModel.CurrentObject());

	}
}
///////////////////////////////////////////////////////////////////////////////
void GedMapNode::MoveItem(const ork::ui::Event& ev)
{
	const int klabh = get_charh();
	const int kdim = klabh-2;

	int ibasex = (kdim+4)*3+3;

	QString qstra = GedInputDialog::getText ( ev, this, 0, ibasex, 0, miW-ibasex-6, klabh );
	
	ork::msleep(100);
	QString qstrb = GedInputDialog::getText ( ev, this, 0, ibasex, 0, miW-ibasex-6, klabh );

	std::string sstra = qstra.toStdString();
	std::string sstrb = qstrb.toStdString();

	if( sstra.length() && sstrb.length() )
	{
		KeyDecoName kdeca( sstra.c_str() );
		KeyDecoName kdecb( sstrb.c_str() );

		if( IsKeyPresent( kdecb ) )
		{
			if( false==IsMultiMap() ) return;
		}
		if( IsKeyPresent( kdeca ) )
		{
			mModel.SigPreNewObject();

			GedMapIoDriver iodriver( mModel, mMapProp, GetOrkObj() );
			iodriver.move( kdeca, sstrb.c_str() );
			
			mModel.Attach(mModel.CurrentObject());
		}
	}
}
void GedMapNode::ImportItem(const ork::ui::Event& ev)
{
	const int klabh = get_charh();
	const int kdim = klabh-2;
	int ibasex = (kdim+4)*3+3;
	QString qstra = GedInputDialog::getText ( ev, this, 0, ibasex, 0, miW-ibasex-6, klabh );
	std::string sstra = qstra.toStdString();
	if( sstra.length() )
	{	KeyDecoName kdeca( sstra.c_str() );
		if( IsKeyPresent( kdeca ) )
		{
			if( false==IsMultiMap() ) return;
		}
		mModel.SigPreNewObject();
		GedMapIoDriver iodriver( mModel, mMapProp, GetOrkObj() );
		iodriver.importfile( kdeca, sstra.c_str() );
		mModel.Attach(mModel.CurrentObject());
	}
}
void GedMapNode::ExportItem(const ork::ui::Event& ev)
{
	const int klabh = get_charh();
	const int kdim = klabh-2;
	int ibasex = (kdim+4)*3+3;
	QString qstra = GedInputDialog::getText ( ev, this, 0, ibasex, 0, miW-ibasex-6, klabh );
	std::string sstra = qstra.toStdString();
	if( sstra.length() )
	{	KeyDecoName kdeca( sstra.c_str() );
		if( IsKeyPresent( kdeca ) )
		{
			GedMapIoDriver iodriver( mModel, mMapProp, GetOrkObj() );
			iodriver.exportfile( kdeca, sstra.c_str() );
			mModel.Attach(mModel.CurrentObject());
		}
	}
}
///////////////////////////////////////////////////////////////////////////////
static const int koff = 1;
void GedMapNode::OnMouseDoubleClicked (const ork::ui::Event& ev)
{
	const int klabh = get_charh();
	const int kdim = klabh-2;
	//Qt::MouseButtons Buttons = pEV->buttons();
	//Qt::KeyboardModifiers modifiers = pEV->modifiers();

	int ix = ev.miX;
    int iy = ev.miY;

	printf( "GedMapNode<%p> ilx<%d> ily<%d>\n", this, ix, iy );

	if( ix >= koff && ix <= kdim 
     && iy >= koff && iy <= kdim ) // drop down
	{
		mbSingle = ! mbSingle;

		///////////////////////////////////////////
		PersistHashContext HashCtx;
		HashCtx.mObject = GetOrkObj();
		HashCtx.mProperty = GetOrkProp();
		PersistantMap* pmap = mModel.GetPersistMap( HashCtx );
		///////////////////////////////////////////
	
		pmap->SetValue( "single", mbSingle ? "true" : "false" );

		CheckVis();
		return;
	}

	///////////////////////////////////////

	if( mbConst == false )
	{
		ix -= (kdim+4);
		if( ix >= koff && ix <= kdim 
         && iy >= koff && iy <= kdim ) // drop down
		{
			ObjModel& model = mModel; 
			AddItem(ev);
			//model.Attach(model.CurrentObject());
			printf( "MAPADDITEM\n");
			model.QueueUpdate();
			return;
		}
		ix -= (kdim+4);
		if( ix >= koff && ix <= kdim 
         && iy >= koff && iy <= kdim ) // drop down
		{
			ObjModel& model = mModel; 
			RemoveItem(ev);
			//model.Attach(model.CurrentObject());
			model.QueueUpdate();
			return;
		}
		ix -= (kdim+4);
		if( ix >= koff && ix <= kdim 
         && iy >= koff && iy <= kdim ) // Move Item
		{
			ObjModel& model = mModel; 
			MoveItem(ev);
			//model.Attach(model.CurrentObject());
			model.QueueUpdate();
			return;
		}
		ix -= (kdim+4);
		if( ix >= koff && ix <= kdim 
         && iy >= koff && iy <= kdim ) // Move Item
		{
			ObjModel& model = mModel; 
			DuplicateItem(ev);
			//model.Attach(model.CurrentObject());
			model.QueueUpdate();
			return;
		}
	}

	///////////////////////////////////////

	if( mbImpExp ) // Import Export
	{
		ix -= (kdim+4);
		if( ix >= koff && ix <= kdim 
         && iy >= koff && iy <= kdim ) // drop down
		{
			ObjModel& model = mModel; 
			ImportItem(ev);
			//model.Attach(model.CurrentObject());
			model.QueueUpdate();
			return;
		}
		ix -= (kdim+4);
		if( ix >= koff && ix <= kdim 
         && iy >= koff && iy <= kdim ) // drop down
		{
			ObjModel& model = mModel; 
			ExportItem(ev);
			//model.Attach(model.CurrentObject());
			model.QueueUpdate();
			return;
		}
	}

	///////////////////////////////////////

	int inumitems = GetNumItems();

	QMenu *pmenu = new QMenu(0);

	for( int it=0; it<inumitems; it++ )
	{
		GedItemNode* pchild = GetItem(it);
		const char* pname = pchild->mName.c_str();
		QAction *pchildact = pmenu->addAction( pname );
		QString qstr( CreateFormattedString("%d",it).c_str() );
		QVariant UserData( qstr );
		pchildact->setData(UserData);
	}
	QAction* pact = pmenu->exec(QCursor::pos());
	if( pact )
	{
		QVariant UserData = pact->data();
		std::string pname = UserData.toString().toStdString();
		int index = 0;
		sscanf( pname.c_str(), "%d", & index );
		mItemIndex = index;
		///////////////////////////////////////////
		PersistHashContext HashCtx;
		HashCtx.mObject = GetOrkObj();
		HashCtx.mProperty = GetOrkProp();
		PersistantMap* pmap = mModel.GetPersistMap( HashCtx );
		///////////////////////////////////////////
		pmap->SetValue( "index", CreateFormattedString("%d",mItemIndex)  );
		CheckVis();
	}
	///////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
void GedMapNode::CheckVis()
{
	int inumitems = GetNumItems();

	if( mbSingle )
	{
		for( int it=0; it<inumitems; it++ )
		{
			GetItem(it)->SetVisible( it == mItemIndex );
		}
	}
	else
	{
		for( int it=0; it<inumitems; it++ )
		{
			GetItem(it)->SetVisible( true );
		}
	}
	mModel.GetGedWidget()->DoResize();
}
///////////////////////////////////////////////////////////////////////////////
void GedMapNode::DoDraw( lev2::GfxTarget* pTARG )
{
	const int klabh = get_charh();
	const int kdim = klabh-2;

	int inumind = mbConst ? 0 : 4;
	if( mbImpExp ) inumind += 2;

	/////////////////
	// drop down box
	/////////////////

	int ioff = koff;
	int idim = (kdim);

	int dbx1 = miX+ioff;
	int dbx2 = dbx1+idim;
	int dby1 = miY+ioff;
	int dby2 = dby1+idim;

	int labw = this->GetNameWidth();
	

	int ity = get_text_center_y();

	GetSkin()->DrawBgBox( this, miX, miY, miW, miH, GedSkin::ESTYLE_BACKGROUND_1 );
	GetSkin()->DrawOutlineBox( this, miX, miY, miW, miH, GedSkin::ESTYLE_DEFAULT_OUTLINE );

	GetSkin()->DrawBgBox( this, miX, miY, miW, klabh, GedSkin::ESTYLE_BACKGROUND_MAPNODE_LABEL );

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

	if( mbConst == false )
	{
		int idimh = idim>>1;

		GetSkin()->DrawOutlineBox( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
		GetSkin()->DrawLine( this, dbx1+idimh, dby1, dbx1+idimh, dby1+idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
		GetSkin()->DrawLine( this, dbx1, dby1+idimh, dbx2, dby1+idimh, GedSkin::ESTYLE_BUTTON_OUTLINE );

		dbx1 += (idim+4);
		dbx2 = 	dbx1+idim;

		GetSkin()->DrawOutlineBox( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
		GetSkin()->DrawLine( this, dbx1, dby1+idimh, dbx2, dby1+idimh, GedSkin::ESTYLE_BUTTON_OUTLINE );

		dbx1 += (idim+4);
		dbx2 = 	dbx1+idim;
		int dbxc = dbx1+idimh;

		GetSkin()->DrawOutlineBox( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
		GetSkin()->DrawText( this, dbx1+1, dby1+1, "R" );

		dbx1 += (idim+4);
		dbx2 = 	dbx1+idim;
		dbxc = dbx1+idimh;
		GetSkin()->DrawOutlineBox( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
		GetSkin()->DrawText( this, dbx1+1, dby1+1, "D" );

		dbx1 += (idim+4);
		dbx2 = 	dbx1+idim;
	}


	if( mbImpExp )
	{
		int idimh = idim>>1;

		GetSkin()->DrawOutlineBox( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
		GetSkin()->DrawText( this, dbx1+1, dby1+1, "I" );

		dbx1 += (idim+4);
		dbx2 = 	dbx1+idim;
		int dbxc = dbx1+idimh;

		GetSkin()->DrawOutlineBox( this, dbx1, dby1, idim, idim, GedSkin::ESTYLE_BUTTON_OUTLINE );
		GetSkin()->DrawText( this, dbx1+1, dby1+1, "O" );

		dbx1 += (idim+4);
		dbx2 = 	dbx1+idim;
		//GetSkin()->DrawLine( this, dbx1, dby1, dbxc, dby1+idimh, GedSkin::ESTYLE_BUTTON_OUTLINE );
		//GetSkin()->DrawLine( this, dbxc, dby1+idimh, dbx2, dby1, GedSkin::ESTYLE_BUTTON_OUTLINE );
	}
	GetSkin()->DrawText( this, dbx1, ity, mName.c_str() );

}
///////////////////////////////////////////////////////////////////////////////

static std::string fix_map_key( const char* pname )
{
	std::string tstr( pname );

	for( std::string::iterator it=tstr.begin(); it!=tstr.end(); it++ )
	{
		char& ch = (*it);

		if( ch>='0' && ch<='9' )
		{
		}
		else if( ch>='a' && ch<='z' )
		{
		}
		else if( ch>='A' && ch<='Z' )
		{
		}
		else if( ch=='.' || ch=='_' )
		{
		}
		else
		{
			ch = '_';
		}
	}
	return tstr;
}

///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::insert( const char* pname )
{
	MapItemWriteSerializer deser_key( *this );
	std::string nkey = fix_map_key( pname );
	deser_key.Insert( nkey.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::remove( const KeyDecoName& kdeco )
{
	MapItemWriteSerializer deser_key( *this );
	deser_key.Remove( kdeco );
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::move( const KeyDecoName& kdeco, const char* pname )
{
	MapItemWriteSerializer deser_key( *this );
	std::string nkey = fix_map_key( pname );
	deser_key.Move( kdeco, nkey.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::duplicate( const KeyDecoName& kdeco, const char* pname )
{
	MapItemWriteSerializer deser_key( *this );
	std::string nkey = fix_map_key( pname );
	deser_key.Duplicate( kdeco, nkey.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::importfile( const KeyDecoName& kdeco, const char* pname )
{
	MapItemWriteSerializer deser_key( *this );
	std::string nkey = fix_map_key( pname );
	deser_key.Import( kdeco, nkey.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::exportfile( const KeyDecoName& kdeco, const char* pname )
{
	MapItemWriteSerializer deser_key( *this );
	std::string nkey = fix_map_key( pname );
	deser_key.Export( kdeco, nkey.c_str() );
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::SetValue(ork::Object* pobject)
{
	MapItemWriteSerializer deser_key( *this );
	deser_key.SetValue( pobject );
	//GetModel().QueueUpdateAll();
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::GetValue( const ork::Object* &rp ) const
{
	MapItemReadSerializer ser( *this );
	const asset::Asset* passet = ser.GetAsset();
	rp = passet;
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::SetValue(PoolString ps)
{
	MapItemWriteSerializer deser_key( *this );
	deser_key.SetValue( ps.c_str() );
	ObjectGedEditEvent ev;
	ev.mProperty = GetProp();
	GetObject()->Notify(&ev);
	//GetModel().QueueUpdateAll();
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::GetValue( PoolString& ps ) const
{
	MapItemReadSerializer ser( *this );
	ps = ser.GetPoolString();
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::SetValue(file::Path ps)
{
	MapItemWriteSerializer deser_key( *this );
	deser_key.SetValue( ps.c_str() );
	//GetModel().QueueUpdateAll();
	ObjectGedEditEvent ev;
	ev.mProperty = GetProp();
	GetObject()->Notify(&ev);
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::GetValue( file::Path& ps ) const
{
	MapItemReadSerializer ser( *this );
	ps = ser.GetPathObject();
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::SetValue(float flt)
{
	MapItemWriteSerializer deser_key( *this );
	deser_key.SetValue( flt );
	//GetModel().QueueUpdateAll();
	ObjectGedEditEvent ev;
	ev.mProperty = GetProp();
	GetObject()->Notify(&ev);
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::GetValue( float& flt ) const
{
	MapItemReadSerializer ser( *this );
	flt = ser.GetFloat();
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::SetValue(const fvec3& flt)
{
	MapItemWriteSerializer deser_key( *this );
	deser_key.SetValue( flt );
	ObjectGedEditEvent ev;
	ev.mProperty = GetProp();
	GetObject()->Notify(&ev);
}
///////////////////////////////////////////////////////////////////////////////
void GedMapIoDriver::GetValue( fvec3& flt ) const
{
	MapItemReadSerializer ser( *this );
	flt = ser.Getfvec3();
}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

void GedMapFactoryNode::OnMouseDoubleClicked(const ork::ui::Event& ev)
{
	orkset<object::ObjectClass*> Factories;
	EnumerateFactories( GetOrkObj(), GetOrkProp(), Factories );
	QMenu * qmenu = CreateFactoryMenu( Factories );

	ConstString assettype_anno = GetOrkProp()->GetAnnotation( "editor.assettype" );
	
	
	QAction* pact = qmenu->exec(QCursor::pos());

	if( pact )
	{
		QVariant UserData = pact->data();
		std::string sname = UserData.toString().toStdString();
		const char* pname = sname.c_str();
		rtti::Class* pclass = rtti::Class::FindClass(pname); 
		ork::object::ObjectClass* poclass = rtti::autocast(pclass);
		if( poclass )
		{
			ork::Object* pobj = rtti::autocast(poclass->CreateObject());

			orkprintf( "GedMapFactoryNode Created Object<%s>\n", pname );

			mModel.SigPreNewObject();

			mIoDriver.SetValue( pobj );

			MapItemCreationEvent mice;
			mice.mProperty = GetOrkProp();
			mice.mNewItem.Set(pobj);
			mice.mKey = GetKey();

			GetOrkObj()->Notify( & mice );

			mModel.QueueUpdate();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

template class GedSimpleNode< GedMapIoDriver , PoolString >;
template<> void GedAssetNode<GedMapIoDriver>::Describe() {}
template class GedAssetNode<GedMapIoDriver>;
template class GedFileNode<GedMapIoDriver>;

} } }
///////////////////////////////////////////////////////////////////////////////
typedef ork::tool::ged::GedAssetNode<ork::tool::ged::GedMapIoDriver> GedAssetNodeMapItem;
typedef ork::tool::ged::GedFileNode<ork::tool::ged::GedMapIoDriver> GedFileNodeMapItem;
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(GedAssetNodeMapItem,"GedAssetNodeMapItem");
INSTANTIATE_TRANSPARENT_TEMPLATE_RTTI(GedFileNodeMapItem,"GedFileNodeMapItem");
///////////////////////////////////////////////////////////////////////////////
//template class ork::tool::ged::GedPoolStringNode< ork::tool::ged::GedMapIoDriver >;
///////////////////////////////////////////////////////////////////////////////
