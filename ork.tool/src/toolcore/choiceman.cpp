///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/kernel/string/string.h>

namespace ork {
namespace tool {
	
std::string ChoiceFunctor::ComputeValue( const std::string & ValueStr ) const
{
	return ValueStr;
}

///////////////////////////////////////////////////////////////////////////////

CChoiceList::CChoiceList()
	: mHierarchy( new SlashTree )
{
}

///////////////////////////////////////////////////////////////////////////////

CChoiceList::~CChoiceList()
{
	for( auto item : mChoicesVect ) delete item;
	delete mHierarchy;
}

///////////////////////////////////////////////////////////////////////////////

const CAttrChoiceValue* CChoiceList::FindFromLongName( const std::string &longname  ) const
{
	CAttrChoiceValue* rval = OrkSTXFindValFromKey( mNameMap, longname, (CAttrChoiceValue*) 0 );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

const CAttrChoiceValue* CChoiceList::FindFromShortName( const std::string &shortname ) const
{
	CAttrChoiceValue* rval = OrkSTXFindValFromKey( mShortNameMap, shortname, (CAttrChoiceValue*) 0 );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

const CAttrChoiceValue* CChoiceList::FindFromValue( const std::string & uval ) const
{
	CAttrChoiceValue* rval = OrkSTXFindValFromKey( mValueMap, uval, (CAttrChoiceValue*) 0 );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void CChoiceList::add( const CAttrChoiceValue & val )
{
	std::string LongName = val.GetName();

	//////////////////////////////////////
	// insert leading slash if not present

	file::Path apath( LongName.c_str() );

	if( apath.HasUrlBase() == false )
	{
		char ch0 = val.GetName().c_str()[0];

		if( ch0 != '/' )
		{
			LongName = CreateFormattedString( "/%s", LongName.c_str() );
		}
	}

	//////////////////////////////////////

	CAttrChoiceValue *pNewVal = new CAttrChoiceValue( LongName, val.GetValue(), val.GetShortName() );
	pNewVal->SetFunctor( val.GetFunctor() );
	mChoicesVect.push_back( pNewVal );
	int nch = (int) mChoicesVect.size() - 1;
	CAttrChoiceValue * ChcVal = mChoicesVect[nch];
	void * pData = (void *) ChcVal;
	SlashNode *pnode = mHierarchy->add_node( LongName.c_str(), pData );
	pNewVal->SetSlashNode( pnode );
	pNewVal->CopyKeywords( val );
	pNewVal->SetCustomData( val.GetCustomData() );

	OrkSTXMapInsert( mNameMap,      LongName,           pNewVal );
	OrkSTXMapInsert( mShortNameMap, val.GetShortName(), pNewVal );
	OrkSTXMapInsert( mValueMap,     val.GetValue(),     pNewVal );

}

///////////////////////////////////////////////////////////////////////////////

void CChoiceList::remove( const CAttrChoiceValue & val )
{
	int inumchoices = (int) mChoicesVect.size();

	for( int iv=0; iv<inumchoices; iv++ )
	{
		CAttrChoiceValue * ChcVal = mChoicesVect[iv];

		if( (ChcVal->GetName() == val.GetName()) && (ChcVal->GetShortName() == val.GetShortName()) )
		{
			SlashNode *pnode = ChcVal->GetSlashNode();

			if( pnode )
			{
				mHierarchy->remove_node( pnode );
			}

			inumchoices = (int) mChoicesVect.size();

			mChoicesVect[iv] = mChoicesVect[inumchoices-1];
			mChoicesVect.pop_back();

			delete ChcVal;

		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void CChoiceList::UpdateHierarchy( void ) // update hierarchy
{
	delete mHierarchy;
	mHierarchy = new SlashTree;
	mValueMap.clear();

	size_t nch = mChoicesVect.size();
	for( size_t i=0; i<nch; i++ )
	{
		CAttrChoiceValue *val = mChoicesVect[i];
		mHierarchy->add_node( val->GetName().c_str(), (void *) val );
		OrkSTXMapInsert( mValueMap, val->GetValue(), val );
	}
}

///////////////////////////////////////////////////////////////////////////////

void CChoiceList::dump( void )
{
	mHierarchy->dump();
}

///////////////////////////////////////////////////////////////////////////////

CChoiceManager::CChoiceManager()
{
	
}

///////////////////////////////////////////////////////////////////////////////

CChoiceManager::~CChoiceManager()
{
	for( orkmap<std::string,CChoiceList*>::const_iterator it=mChoiceListMap.begin(); it!=mChoiceListMap.end(); it++ )
	{
		CChoiceList* plist = it->second;
		delete plist;
	}
}

///////////////////////////////////////////////////////////////////////////////

void CChoiceManager::AddChoiceList( const std::string & ListName, CChoiceList *plist )
{
	orkmap<std::string,CChoiceList*>::const_iterator it = mChoiceListMap.find( ListName );
	
	CChoiceList *plist2 = (it==mChoiceListMap.end()) ? 0 : it->second;

	OrkAssert( 0 == plist2 );

	if( 0 == plist2 )
	{
		OrkSTXMapInsert( mChoiceListMap, ListName, plist );
	}
}

///////////////////////////////////////////////////////////////////////////////

CChoiceList *CChoiceManager::GetChoiceList( const std::string & ListName )
{
	CChoiceList *plist = 0;

	orkmap<std::string,CChoiceList*>::const_iterator it = mChoiceListMap.find( ListName );
	
	if( it != mChoiceListMap.end() )
	{
		plist = it->second;
	}
	
	return plist;
}

///////////////////////////////////////////////////////////////////////////////

const CChoiceList *CChoiceManager::GetChoiceList( const std::string & ListName ) const 
{
	const CChoiceList *plist = 0;

	orkmap<std::string,CChoiceList*>::const_iterator it = mChoiceListMap.find( ListName );
	
	if( it != mChoiceListMap.end() )
	{
		plist = it->second;
	}
	
	return plist;
}

///////////////////////////////////////////////////////////////////////////////


void CChoiceList::clear( void )
{
	mHierarchy = new SlashTree;
	mChoicesVect.clear();

	mValueMap.clear();
	mNameMap.clear();
	mShortNameMap.clear();
}

} // namespace tool
} // namespace ork

