#include "krzio.h"
#include <fstream>

using namespace rapidjson;

///////////////////////////////////////////////////////////////////////////////

filescanner::filescanner( const char* pname ) 
	: mpFile(0)
	, miSize(0)
	, mpData(0)
	, _japrog(_joprog.GetAllocator())
	, _joprogroot(kObjectType)
	, _joprogobjs(kArrayType)
	, _globalsFlag(false)
	//, _curLayerObject(nullptr)
{
	printf( "Opening<%s>\n", pname );
	mpFile = fopen( pname, "rb" );
	printf( "file<%p>\n", mpFile );
	fseek( mpFile, 0, SEEK_END );
	int ilen = ftell(mpFile);
	miSize = ilen;
	printf( "length<%d>\n", ilen);
	mpData = malloc(ilen);
	fseek( mpFile, 0, SEEK_SET );
	fread( mpData, ilen, 1, mpFile );
	mMainDataBlock.AddBytes( mpData, ilen );

	//Value jsonKRZ(kArrayType);
	//jsonKRZ.SetString("KRZ");

	//_jopstack.push(jsonKRZ);

}

///////////////////////////////////////////////////////////////////////////////

filescanner::~filescanner()
{
	fclose(mpFile);
	free(mpData);
}

/////////////////////////////////////////////

void filescanner::SkipData( int ibytes )
{
	mMainIterator.SkipData(ibytes);
}

/////////////////////////////////////////////

void filescanner::scanAndDump()
{
	size_t inumb = mDatablocks.size();
	printf( "NumBlocks<%d>\n", int(inumb) );

	//printf( "///////////////////////////////////////////////////////////////////////\n");
	//printf( "///////////////////////////////////////////////////////////////////////\n");

	/////////////////////////////////////////////
	// parse blocks
	/////////////////////////////////////////////

	for( size_t index=0; index<inumb; index++ )
		ParseBlock( mDatablocks[index] );

	/////////////////////////////////////////////

	for( auto km : _keymaps )
		emitKeymap( km.second, _joprogobjs );

	for( auto ms : _samples )
		emitMultiSample( ms.second, _joprogobjs );

	for( auto p : _programs )
		emitProgram( p.second, _joprogobjs );



	/////////////////////////////////////////////

	_joprogroot.AddMember ( "objects", _joprogobjs, _japrog );

	_joprog.SetObject();
	_joprog.AddMember ( "KRZ", _joprogroot, _japrog );

	rapidjson::StringBuffer strbuf;
	rapidjson::	PrettyWriter<rapidjson::StringBuffer> writer(strbuf);
	_joprog.Accept(writer);

	std::ofstream of ("krzdump.json");
	of << "\n";
	of << strbuf.GetString();
	of << "\n";

	//printf( "JSONOUT\n\n%s\n\n", strbuf.GetString() );
}

///////////////////////////////////////////////////////////////////////////////

void ParseSong( const datablock& db, datablock::iterator& it )
{
}

///////////////////////////////////////////////////////////////////////////////

void ParseEffect( const datablock& db, datablock::iterator& it )
{
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::ParseBlock( const datablock& db )
{
	datablock::iterator	it;
	u16 u16v;
	u8 u8v;
	bool bOK;

	bool bdone = false;
	
	int idatablocksize = int(db.mData.size());
	
	//printf( "dbsize<%08x> itidx<%08x>\n", idatablocksize, it.miIndex );
	
	while( false==bdone )
	{
		u8 objhdr = *db.RefData(it);
		int ibase = it.miIndex;
		//printf( "objhdr<%d> ibase<%d>\n", (int) objhdr, ibase );
		if( objhdr )
		{
			ParseObject( db, it );
		}
		objhdr = *db.RefData(it);
		int iend = it.miIndex;
		int isize = iend-ibase;
		//printf( "dbsize<%08x> itidx<%08x> isize<%d> nobjhdr<%08x>\n", idatablocksize, it.miIndex, isize, int(objhdr) );
		bdone = (objhdr==0)||(it.miIndex>=(idatablocksize-4));
	}
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::ParseObject( const datablock& db, datablock::iterator& it )
{
	const datablock::iterator kit = it;
	bool bOK;
	/////////////////////////////////////
	// Parse TYPE/ID
	/////////////////////////////////////

	u16 uObjTypeID = 0;
	u16 uObjLength = 0;
	u16 uObjOffset = 0;
	const u8* pObjName = 0;

	bOK = db.GetData( uObjTypeID, it );
	bOK = db.GetData( uObjLength, it );
	bOK = db.GetData( uObjOffset, it );
	pObjName = db.RefData(it);
	std::string ObjectName;
	if( (pObjName == 0) || (pObjName[0]==0) )
		ObjectName = "noname";
	else
		ObjectName = (const char*) pObjName;

	//printf( "obj<%s>\n",ObjectName.c_str());
	//Value jsonobj(kObjectType);

	//Value nameobj(kStringType);
	//nameobj.SetString(ObjectName.c_str(),_japrog);
	//jsonobj.AddMember("name", nameobj, _japrog);

	for( std::string::iterator its=ObjectName.begin(); its!=ObjectName.end(); its++ )
	{
		if( *its == '/' )
			*its = '_';
		if( *its == ' ' )
			*its = '_';
		if( *its == '&' )
			*its = '+';
		if( *its == '<' )
			*its = '(';
		if( *its == '>' )
			*its = ')';
	}

	//printf( "uObjTypeID<%4x>\n", int(uObjTypeID) );
	//printf( "uObjLength<%4x>\n", int(uObjLength) );
	//printf( "uObjOffset<%4x>\n", int(uObjOffset) );
	//printf( "ObjName<%s>\n", (const char*) pObjName );
	
	bool sixbittype = (uObjTypeID&0x8000);
	//printf( "sixbittype<%d>\n", int(sixbittype));

	int iObjectTYPE = 0;
	int iObjectID = 0;

	if( sixbittype ) // 6bit type / 10bit ID
	{
		iObjectTYPE = (uObjTypeID&0xfc00)>>8;
		iObjectID = (uObjTypeID&0x03ff);
	}
	else // 8 bit type / 8 bit ID
	{
		iObjectTYPE = (uObjTypeID&0xff00)>>8;
		iObjectID = (uObjTypeID&0x00ff);		
	}
	printf( "ObjID<%03d> ObjTyp<0x%02x> ObjName<%s>\n", iObjectID, iObjectTYPE,  (const char*) pObjName );

	//jsonobj.AddMember("objectID", iObjectID, _japrog);
	/////////////////////////////////////
	
	/////////////////////////////////////
	// advance to data
	
	it.miIndex = kit.miIndex+uObjOffset+4;
	
	//printf( "NewIndex<%d>\n")
	/////////////////////////////////////
	
	static int ilastot = iObjectTYPE;
	
	if( ilastot != iObjectTYPE )
	{	//printf( "////////////////////////////////////////////////////////////////////////////////////\n" );
		//printf( "////////////////////////////////////////////////////////////////////////////////////\n" );
	}
	ilastot = iObjectTYPE;
	bool writeobj = false;
	switch( iObjectTYPE )
	{
		case 0x90: // program
		{	ParseProgram( db, it, iObjectID, ObjectName );
			writeobj = true;
			break;
		}
		case 0x94: // keymap
		{	ParseKeyMap( db, it, iObjectID, ObjectName );
			//writeobj = true;
			break;
		}
		case 0x98: // sample header
		{	ParseSampleHeader( db, it, iObjectID, ObjectName );
			//writeobj = true;
			break;
		}
		case 0x9C: // k2000 setup
		{	break;
		}
		case 0x70: // song
		{	ParseSong( db, it );
			break;
		}
		case 0x71: // effect
		{	ParseEffect( db, it );
			break;
		}
		case 0x6f: // QABANK
		{	break;
		}
		case 0x67: // tuning table
		{	break;
		}
		case 0x68: // vel table
		{	break;
		}
		case 0x69: // ??? table
		{	break;
		}
		case 0x64: // ??? table
		{	break;
		}
		default:
			printf( "[UNKNOWN] ObjectType<%x> ObjectID<%d> ObjectName<%s>\n", iObjectTYPE, iObjectID, ObjectName.c_str() );
			assert(false);
			break;
	}
	it.miIndex = kit.miIndex+uObjLength;
	
	if( it.miIndex&1 )
	{
		printf( "ERROR! odd index\n" );
		it.miIndex++;
		//exit(0);
	}
	if( writeobj )
	{
		//_joprogobjs.PushBack(jsonobj,_japrog);
	}
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::AddStringKVMember( rapidjson::Value& parent, const std::string& key, const std::string& val )
{
	Value jsonKEY, jsonVAL;
	jsonKEY.SetString(key.c_str(),_japrog);
	jsonVAL.SetString(val.c_str(),_japrog);
	parent.AddMember(jsonKEY, jsonVAL, _japrog);

}

///////////////////////////////////////////////////////////////////////////////




