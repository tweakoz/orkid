///////////////////////////////////////////////////////////////////////////////
//
//	Orkid QT User Interface Glue
//
///////////////////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/qtui/qtui.h>
#include <ork/kernel/string/StringBlock.h>
#include <ork/lev2/ui/ui.h>
#include <ork/lev2/ui/viewport.h>

#if defined(IX)
#include <cxxabi.h>
#endif

#if defined( ORK_CONFIG_QT )

namespace ork {

file::Path SaveFileRequester( const std::string& title, const std::string& ext )
{
	auto& gfxenv = lev2::GfxEnv::GetRef();
	gfxenv.GetGlobalLock().Lock();
	QString FileName = QFileDialog::getSaveFileName( 0, "Export ProcTexImage", 0, "PNG (*.png)");
	file::Path::NameType fname = FileName.toStdString().c_str();
	gfxenv.GetGlobalLock().UnLock();
	return fname;
}

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

std::string TypeIdName(const std::type_info*ti)
{
	return (ti!=nullptr) ? TypeIdNameStrip( ti->name() ) : "nil";
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
	const char * RootClassName = walkmeta->className();
	while( walkmeta != 0 )
	{
		const char * WalkClassName = walkmeta->className();
		int imethoff = walkmeta->methodOffset();
		int imethods = walkmeta->methodCount();
		for( int i=0; i<imethods; i++ )
		{
			QMetaMethod method = walkmeta->method( i );
			//const char* sig = method.signature();
			//orkprintf( "walkmeta root<%s> walk<%s> methid<%d> method<%s>\n", RootClassName, WalkClassName, i, sig );
		}
		walkmeta = walkmeta->superClass();
	}
}

///////////////////////////////////////////////////////////////////////////////

void CQNoMocBase::Compile( void )
{
	size_t inumslots = mSlots.size();
	size_t inumsignals = mSignals.size();

	StringBlock string_block;

	///////////////////////////////////////
	// moc header

	mMocPrivate.revision = 1;
	mMocPrivate.className = string_block.AddString( mClassName.c_str() ).Index();
	mMocPrivate.classInfoCount = 0;
	mMocPrivate.classInfoData = 0;
	mMocPrivate.methodCount = int(inumslots+inumsignals);
	mMocPrivate.methodData = 10 + mMocPrivate.classInfoCount;
	mMocPrivate.propertyCount = 0;
	mMocPrivate.propertyData = 0;
	mMocPrivate.enumeratorCount = 0;
	mMocPrivate.enumeratorData = 0;

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

	int inilstring = string_block.AddString( "" ).Index();

	/*orkprintf( "moc class %s\n", mClassName.c_str() );
	orkprintf( "moc revision %d\n", mMocPrivate.revision );
	orkprintf( "moc classname %d\n", mMocPrivate.className );
	orkprintf( "moc classInfoCount %d\n", mMocPrivate.classInfoCount );
	orkprintf( "moc classInfoData %d\n", mMocPrivate.classInfoData );
	orkprintf( "moc methodCount %d\n", mMocPrivate.methodCount );
	orkprintf( "moc methodData %d\n", mMocPrivate.methodData );
	orkprintf( "moc propertyCount %d\n", mMocPrivate.propertyCount );
	orkprintf( "moc propertyData %d\n", mMocPrivate.propertyData );
	orkprintf( "moc enumeratorCount %d\n", mMocPrivate.enumeratorCount );
	orkprintf( "moc enumeratorData %d\n", mMocPrivate.enumeratorData );
*/
	///////////////////////////////////////
	// signals

	for( size_t isig=0; isig<inumsignals; isig++ )
	{
		MocFunctorBase* pbase = mSignals[ isig ];
		const std::string & signame = pbase->mMethodName;

		int isigname = string_block.AddString( signame.c_str() ).Index();

		int iparams = string_block.AddString( pbase->Params() ).Index();

		mMocUInt.push_back( isigname );	// method name
		mMocUInt.push_back( inilstring );	// method sig
		mMocUInt.push_back( iparams );	// method params
		mMocUInt.push_back( inilstring );	// method type

		U32 uflags = AccessProtected | MethodSignal;

		mMocUInt.push_back( uflags );

	//orkprintf( "moc [signal %s]\n", signame.c_str() );
	//orkprintf( "moc [signame %08x] [sig %d] [params %d] [typ %d] [uflags %08x]\n", isigname, inilstring, iparams, inilstring, uflags );


	}

	///////////////////////////////////////
	// slots

	for( size_t islot=0; islot<inumslots; islot++ )
	{
		MocFunctorBase* pbase = mSlots[ islot ];
		const std::string & slotname = pbase->mMethodName;

		int islotname = string_block.AddString( slotname.c_str() ).Index();

		int iparams = string_block.AddString( pbase->Params() ).Index();

		mMocUInt.push_back( islotname );	// method name
		mMocUInt.push_back( inilstring );	// method sig
		mMocUInt.push_back( iparams );	// method params
		mMocUInt.push_back( inilstring );	// method type

		U32 uflags = AccessPublic | MethodSlot;

		mMocUInt.push_back( uflags );

		//orkprintf( "moc [slot %s]\n", slotname.c_str() );
		//orkprintf( "moc [slotname %08x] [sig %d] [params %d] [typ %d] [flags %08x]\n", islotname, inilstring, iparams, inilstring, uflags );

	}

	///////////////////////////////////////
	// props

	///////////////////////////////////////

	mMocUInt.push_back( 0 ); // eod

	///////////////////////////////////////

	const char *string_block_data = string_block.data();
	mStringBlockLen = (int) string_block.size();
	// release the string block data
	new(&string_block) StringBlock;

	//////////////////////

	staticMetaObject = GetThisMeta();
	staticMetaObject->d.stringdata = (const QArrayData *) string_block_data;
	staticMetaObject->d.data = & mMocUInt[ 0 ];
	staticMetaObject->d.extradata = 0;

	//////////////////////
}
#endif // #if 0


}} // namespace ork { namespace lev2 {

#endif
