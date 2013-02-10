///////////////////////////////////////////////////////////////////////////////
//
//	Orkid QT User Interface Glue
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/lev2/ui/ui.h>

#if defined(IX)
#include <cxxabi.h>
#endif

#if defined( ORK_CONFIG_QT )

namespace ork {

#if defined(IX)
std::string GccDemangle( const std::string& inname )
{
	int status;
	const char *pmangle = abi::__cxa_demangle( inname.c_str(), 0, 0, & status );
	return std::string(pmangle);
}
#endif
	
std::string TypeIdNameStrip(const char* name)
{
	std::string strippedName(name);

#if defined( IX )
	strippedName = GccDemangle(strippedName);
#endif

	size_t classLength = strlen("class ");;
	size_t classPosition;
	while((classPosition = strippedName.find("class ")) != std::string::npos)
	{
		strippedName.swap(strippedName.erase(classPosition, classLength));
	}
	
	return strippedName;
}
std::string MethodIdNameStrip(const char* name) // mainly used for QT signals and slots
{

	std::string inname( name );

#if defined( IX )
	inname = GccDemangle(inname);
#endif

	FixedString<65536> newname = inname.c_str();
	newname.replace_in_place("std::", "");
	newname.replace_in_place("__thiscall ","");
	return newname.c_str();
}
	
namespace lev2 {

///////////////////////////////////////////////////////////////////////////////
#if 0
CQNoMocBase*& CQNoMocBase::GetPreMainMocBaseIter()
{
	static CQNoMocBase* gpPreMainMocBaseIter = 0;
	return gpPreMainMocBaseIter;
}

CQNoMocBase::CQNoMocBase( const std::string &classname, MocInitFunc initfunc )
	: mClassVersion( 1 )
	, mClassName( classname )
	, mpQtMetaData( 0 )
	, mPrevMocBase( GetPreMainMocBaseIter() )
	, mInitFunc( initfunc )
{
	GetPreMainMocBaseIter() = this;
}

void CQNoMocBase::MocInit()
{
	if( mInitFunc )
	{
		mInitFunc();
	}
}

void CQNoMocBase::MocInitAll()	
{
	//////////////////////
	// MocInit nodes
	//////////////////////
	{
		CQNoMocBase* piter = GetPreMainMocBaseIter();
		while( piter )
		{	piter->MocInit();
			piter = piter->GetPreviousMocBase();
		}
	}
	//////////////////////
	// Compile nodes
	//////////////////////
	{
		CQNoMocBase* piter = GetPreMainMocBaseIter();
		while( piter )
		{	piter->Compile();
			//orkprintf( "MocNode<%s> Compiled\n", piter->GetThisMeta()->className() );
			piter = piter->GetPreviousMocBase();
		}
	}
	//////////////////////
	// Link nodes
	//////////////////////
	{
		CQNoMocBase* piter = GetPreMainMocBaseIter();
		while( piter )
		{	
			piter->Link();
			piter = piter->GetPreviousMocBase();
		}
	}

}
	
///////////////////////////////////////////////////////////////////////////////
void CQNoMocBase::Link()
{

	const QMetaObject *pparmeta = GetParentMeta();
	QMetaObjectPrivate *pparpriv = (QMetaObjectPrivate*) pparmeta->d.data;
	int inumpar_Props = pparpriv->propertyCount;
	//////////////////////
	miNumParentMethods = pparpriv->methodCount;
	staticMetaObject->d.superdata = pparmeta;
	//////////////////////
	//orkprintf( "Linking MocNode<%s> to Parent<%s>\n", GetThisMeta()->className(), GetParentMeta()->className() );

	//////////////////////

	const QMetaObject *walkmeta = GetThisMeta();
	const char * LeafClassName = walkmeta->className();
	while( walkmeta != 0 )
	{
		const char * WalkClassName = walkmeta->className();
		int imethoff = walkmeta->methodOffset();
		int imethods = walkmeta->methodCount();
		for( int i=0; i<imethods; i++ )
		{
			QMetaMethod method = walkmeta->method( i ); 
			auto siga = method.methodSignature();
			const char* sig = siga.constData(); 
			printf( "walkmeta leaf<%p:%s> walk<%s> methid<%d> method<%s>\n", LeafClassName,LeafClassName, WalkClassName, i, sig );
		}
		walkmeta = walkmeta->superClass();
	}		
	//assert(false);
}

///////////////////////////////////////////////////////////////////////////////

void CQNoMocBase::Compile( void )
{
	size_t inumslots = mSlots.size();
	size_t inumsignals = mSignals.size();

	///////////////////////////////////////
	// build string block first!
	///////////////////////////////////////
	struct strbuilder
	{
		std::map<std::string,size_t> string_set;
		std::vector<char> string_block;
		std::vector<size_t> strblklut;

		void finalize()
		{
			size_t loc = string_block.size();
			strblklut.push_back(loc);
			string_set[""]=loc;
			string_block.push_back(0);
		}

		void insert(const std::string& s )
		{
			if( s.length() )
			{
				if( string_set.find(s)==string_set.end())
				{
					size_t loc = string_block.size();
					strblklut.push_back(loc);
					string_set[s]=loc;
					int ilen = s.length();
					for( int i=0; i<ilen; i++ )
						string_block.push_back(s[i]);
					string_block.push_back(0);					
				}
			}
		}

	};

	strbuilder the_builder;


//	string_block.insert( "" );
	the_builder.insert( mClassName.c_str() );
	for( size_t isig=0; isig<inumsignals; isig++ )
	{
		MocFunctorBase* pbase = mSignals[ isig ];
		const std::string & signame = pbase->mMethodName;
		the_builder.insert( signame.c_str() );
		the_builder.insert( pbase->Params() );
	}

	for( size_t islot=0; islot<inumslots; islot++ )
	{
		MocFunctorBase* pbase = mSlots[ islot ];
		const std::string & slotname = pbase->mMethodName;
		the_builder.insert( slotname.c_str() );
		the_builder.insert( pbase->Params() );
	}

	the_builder.finalize();

	///////////////////////////////////////

	int inumstrings = the_builder.string_set.size();
	mStringBlockLen = (int) the_builder.string_block.size();

	size_t strtab_header_length = inumstrings*sizeof(QByteArrayData);
	size_t strtab_length = strtab_header_length+mStringBlockLen+16;
	char* strtab_mem = (char*) malloc(strtab_length);
	memcpy( (void*)(strtab_mem+strtab_header_length), the_builder.string_block.data(), mStringBlockLen );
	printf( "memcpy numstr<%d> offs<%d> sblen<%d>\n", inumstrings, int(strtab_header_length), mStringBlockLen );
	// release the string block data
	//new(&string_block) StringBlock;
	std::map<std::string,int> strlut;
	auto kbads = (const QByteArrayData*) strtab_mem;
	for( int i=0; i<inumstrings; i++ )
	{	size_t offs = the_builder.strblklut[i];
		auto bstr = std::string(the_builder.string_block.data()+offs);
		strlut[bstr.c_str()]=i;
		size_t istrlen = strlen(bstr.c_str());
		size_t qaryoff = sizeof(QByteArrayData)*i;
		QByteArrayData* bad = (QByteArrayData*) (strtab_mem+qaryoff);

		// ref size alloc cap offset
		bad->ref.atomic.store(-1);
		bad->size = istrlen;
		bad->alloc = 0;
		bad->capacityReserved = 0;
		bad->offset = strtab_header_length+offs;
		const char* pstr = strtab_mem + bad->offset;
		printf( "bad<%p> idx<%d> siz<%d> soff<%d> offph<%d> strlen<%d> srcstr<%s> str<%s>\n", bad, i, int(bad->size), int(bad->offset-strtab_header_length), int(bad->offset), int(istrlen), bstr.c_str(), pstr);

		//memcpy( qarydat->data(), string_block_data, mStringBlockLen+1 );

	}

	///////////////////////////////////////


	///////////////////////////////////////
	// moc header

	int inilstring = strlut[""];
	int iclassstring = strlut[mClassName];
	//printf( "inilstring<%d>\n", inilstring );
	printf( "iclassstring<%d>\n", iclassstring );

	mMocPrivate.revision = QMetaObjectPrivate::OutputRevision;
	mMocPrivate.className = iclassstring;
	mMocPrivate.classInfoCount = 0;
	mMocPrivate.classInfoData = 0;
	mMocPrivate.methodCount = int(inumslots+inumsignals);
	mMocPrivate.methodData = 14;
	mMocPrivate.propertyCount = 0;
	mMocPrivate.propertyData = 0;
	mMocPrivate.enumeratorCount = 0;
	mMocPrivate.enumeratorData = 0;
	mMocPrivate.constructorCount = 0;
	mMocPrivate.flags = 0;

	mMocUInt.push_back( mMocPrivate.revision );
	mMocUInt.push_back( mMocPrivate.className );
	mMocUInt.push_back( mMocPrivate.classInfoCount );
	mMocUInt.push_back( mMocPrivate.classInfoData );
	mMocUInt.push_back( mMocPrivate.methodCount );
	mMocUInt.push_back( mMocPrivate.methodData );
	mMocUInt.push_back( mMocPrivate.propertyCount );
	mMocUInt.push_back( mMocPrivate.propertyData );
	mMocUInt.push_back( mMocPrivate.enumeratorCount );
	mMocUInt.push_back( mMocPrivate.enumeratorData );
	mMocUInt.push_back( mMocPrivate.constructorCount );
	mMocUInt.push_back( mMocPrivate.constructorData );
	mMocUInt.push_back( mMocPrivate.flags );
	mMocUInt.push_back( inumsignals );

	assert(mMocUInt.size()==mMocPrivate.methodData);

	/*printf( "moc class %s\n", mClassName.c_str() );
	printf( "moc revision %d\n", mMocPrivate.mRevision );
	printf( "moc classname %d\n", mMocPrivate.mClassName );
	printf( "moc classInfoCount %d\n", mMocPrivate.mClassInfoCount );
	printf( "moc classInfoData %d\n", mMocPrivate.mClassInfoData );
	printf( "moc methodCount %d\n", mMocPrivate.mMethodCount );
	printf( "moc methodData %d\n", mMocPrivate.mMethodData );
	printf( "moc propertyCount %d\n", mMocPrivate.mPropertyCount );
	printf( "moc propertyData %d\n", mMocPrivate.mPropertyData );
	printf( "moc enumeratorCount %d\n", mMocPrivate.mEnumeratorCount );
	printf( "moc enumeratorData %d\n", mMocPrivate.mEnumeratorData );
	printf( "moc constructorCount %d\n", mMocPrivate.mConstructorCount );
	printf( "moc constructorData %d\n", mMocPrivate.mConstructorData );
	printf( "moc flags %d\n", mMocPrivate.mEnumeratorCount );
*/
	///////////////////////////////////////
	// signals

	for( size_t isig=0; isig<inumsignals; isig++ )
	{
		MocFunctorBase* pbase = mSignals[ isig ];
		const std::string & signame = pbase->mMethodName;

		int isigname = strlut[signame];

		size_t istr = kbads[isigname].offset;

		const char* signame_cstr = strtab_mem + istr;
		const char* signature = pbase->Params();
		int iparams = strlut[signature];
		QArgumentTypeArray typary;
		//QByteArray methname = QMetaObjectPrivate::decodeMethodSignature(signature,typary);

		mMocUInt.push_back( isigname );	// method name
		mMocUInt.push_back( inilstring );	// method sig
		mMocUInt.push_back( iparams );	// method params
		mMocUInt.push_back( inilstring );	// method type

		U32 uflags = AccessProtected | MethodSignal;

		mMocUInt.push_back( uflags );
		
	    orkprintf( "moc signame<%d:%s> [argc %d] [params %d] [typ %d] [uflags %08x]\n", isigname, signame_cstr, inilstring, iparams, inilstring, uflags );

		
	}

	///////////////////////////////////////
	// slots

	for( size_t islot=0; islot<inumslots; islot++ )
	{
		MocFunctorBase* pbase = mSlots[ islot ];
		const std::string & slotname = pbase->mMethodName;

		int islotname = strlut[slotname];

		int iparams = strlut[pbase->Params()];

		mMocUInt.push_back( islotname );	// method name
		mMocUInt.push_back( inilstring );	// method sig
		mMocUInt.push_back( iparams );	// method params
		mMocUInt.push_back( inilstring );	// method type

		U32 uflags = AccessPublic | MethodSlot;

		mMocUInt.push_back( uflags );
		
		orkprintf( "moc [slot %s]\n", slotname.c_str() );
		orkprintf( "moc [slotname %d<%s>] [sig %d] [params %d] [typ %d] [flags %08x]\n", islotname, slotname.c_str(), inilstring, iparams, inilstring, uflags );
		
		assert(strstr(slotname.c_str(),"::UserSelectionChanged" )==0);
	}

	///////////////////////////////////////
	// props

	///////////////////////////////////////

	mMocUInt.push_back( 0 ); // eod


	//////////////////////

	staticMetaObject = GetThisMeta();
	//auto qarydat = QByteArrayData::allocate(1,sizeof(void*),mStringBlockLen+1);//(string_block_data);
	//memcpy( qarydat->data(), string_block_data, mStringBlockLen+1 );

	//qarydat->ref.atomic.store(-1);
	//printf( "qarydat<%p> atom<%d>\n", qarydat, qarydat->ref.atomic.load() );
	//qarydat->alloc = 0;
	//qarydat->capacityReserved = 0;
	staticMetaObject->d.stringdata = (QByteArrayData*) strtab_mem;
	staticMetaObject->d.data = & mMocUInt[ 0 ];
	staticMetaObject->d.extradata = 0;

	//////////////////////

	//assert(false);
}

#endif

void CQtGfxWindow::OnShow()
{
	ork::lev2::GfxTarget *pTARG = GetContext();

	//if( mbinit )
	{
		//mbinit = false;
		/////////////////////////////////////////////////////////////////////
		mpviewport->SetX( GetContextX() );		// TODO add resize call
		mpviewport->SetY( GetContextY() );
		mpviewport->SetW( GetContextW() );
		mpviewport->SetH( GetContextH() );		
		SetViewport( mpviewport );
	}

	//SetTopUIGroup( mpviewport );

}
}
}

#endif
