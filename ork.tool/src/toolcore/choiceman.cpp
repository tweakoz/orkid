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

ChoiceList::ChoiceList()
	: mHierarchy( new SlashTree )
{
}

///////////////////////////////////////////////////////////////////////////////

ChoiceList::~ChoiceList()
{
	for( auto item : mChoicesVect ) delete item;
	delete mHierarchy;
}

///////////////////////////////////////////////////////////////////////////////

const AttrChoiceValue* ChoiceList::FindFromLongName( const std::string &longname  ) const
{
	AttrChoiceValue* rval = OrkSTXFindValFromKey( mNameMap, longname, (AttrChoiceValue*) 0 );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

const AttrChoiceValue* ChoiceList::FindFromShortName( const std::string &shortname ) const
{
	AttrChoiceValue* rval = OrkSTXFindValFromKey( mShortNameMap, shortname, (AttrChoiceValue*) 0 );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

const AttrChoiceValue* ChoiceList::FindFromValue( const std::string & uval ) const
{
	AttrChoiceValue* rval = OrkSTXFindValFromKey( mValueMap, uval, (AttrChoiceValue*) 0 );
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceList::add( const AttrChoiceValue & val )
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

	AttrChoiceValue *pNewVal = new AttrChoiceValue( LongName, val.GetValue(), val.GetShortName() );
	pNewVal->SetFunctor( val.GetFunctor() );
	mChoicesVect.push_back( pNewVal );
	int nch = (int) mChoicesVect.size() - 1;
	AttrChoiceValue * ChcVal = mChoicesVect[nch];
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

void ChoiceList::remove( const AttrChoiceValue & val )
{
	int inumchoices = (int) mChoicesVect.size();

	for( int iv=0; iv<inumchoices; iv++ )
	{
		AttrChoiceValue * ChcVal = mChoicesVect[iv];

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

void ChoiceList::UpdateHierarchy( void ) // update hierarchy
{
	delete mHierarchy;
	mHierarchy = new SlashTree;
	mValueMap.clear();

	size_t nch = mChoicesVect.size();
	for( size_t i=0; i<nch; i++ )
	{
		AttrChoiceValue *val = mChoicesVect[i];
		mHierarchy->add_node( val->GetName().c_str(), (void *) val );
		OrkSTXMapInsert( mValueMap, val->GetValue(), val );
	}
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceList::dump( void )
{
	mHierarchy->dump();
}

///////////////////////////////////////////////////////////////////////////////

ChoiceManager::ChoiceManager()
{
	
}

///////////////////////////////////////////////////////////////////////////////

ChoiceManager::~ChoiceManager()
{
	for( orkmap<std::string,ChoiceList*>::const_iterator it=mChoiceListMap.begin(); it!=mChoiceListMap.end(); it++ )
	{
		ChoiceList* plist = it->second;
		delete plist;
	}
}

///////////////////////////////////////////////////////////////////////////////

void ChoiceManager::AddChoiceList( const std::string & ListName, ChoiceList *plist )
{
	orkmap<std::string,ChoiceList*>::const_iterator it = mChoiceListMap.find( ListName );
	
	ChoiceList *plist2 = (it==mChoiceListMap.end()) ? 0 : it->second;

	OrkAssert( 0 == plist2 );

	if( 0 == plist2 )
	{
		OrkSTXMapInsert( mChoiceListMap, ListName, plist );
	}
}

///////////////////////////////////////////////////////////////////////////////

ChoiceList *ChoiceManager::GetChoiceList( const std::string & ListName )
{
	ChoiceList *plist = 0;

	orkmap<std::string,ChoiceList*>::const_iterator it = mChoiceListMap.find( ListName );
	
	if( it != mChoiceListMap.end() )
	{
		plist = it->second;
	}
	
	return plist;
}

///////////////////////////////////////////////////////////////////////////////

const ChoiceList *ChoiceManager::GetChoiceList( const std::string & ListName ) const 
{
	const ChoiceList *plist = 0;

	orkmap<std::string,ChoiceList*>::const_iterator it = mChoiceListMap.find( ListName );
	
	if( it != mChoiceListMap.end() )
	{
		plist = it->second;
	}
	
	return plist;
}

///////////////////////////////////////////////////////////////////////////////


void ChoiceList::clear( void )
{
	mHierarchy = new SlashTree;
	mChoicesVect.clear();

	mValueMap.clear();
	mNameMap.clear();
	mShortNameMap.clear();
}

} // namespace tool
} // namespace ork

