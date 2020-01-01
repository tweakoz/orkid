///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#ifndef _ORK_TOOLCORE_CHOICEMAN_H
#define _ORK_TOOLCORE_CHOICEMAN_H

#include <ork/kernel/slashnode.h>
#include <ork/kernel/any.h>

#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////

class QMenu;

namespace ork {
namespace tool {

///////////////////////////////////////////////////////////////////////////////

class ChoiceFunctor
{
public:
	virtual std::string ComputeValue( const std::string & ValueStr ) const;
};
	
///////////////////////////////////////////////////////////////////////////////

class AttrChoiceValue
{
    public: //

    AttrChoiceValue()
		: mName( "defaultchoice" )
		, mValue( "" )
		, mpSlashNode( 0 )
		, mShortName( "" )
		, mpFunctor( 0 )
    {
    }

	AttrChoiceValue( const std::string &nam, const std::string &val, const std::string & shname = ""  )
		: mName( nam )
		, mValue( val )
		, mShortName( shname )
		, mpFunctor( 0 )
	{
    }

	AttrChoiceValue set( const std::string & nname, const std::string &nval, const std::string & shname = "" )
    {
        mName = nname;
        mValue = nval;
        mShortName = shname;
        return (*this);
    }

	std::string GetValue( void ) const
	{
		return mValue;
	}
	std::string EvaluateValue( void ) const
	{
		return (mpFunctor==0) ? mValue : mpFunctor->ComputeValue(mValue);
	}
	void SetValue( const std::string &val ) { mValue = val; }
    SlashNode* GetSlashNode( void ) const { return mpSlashNode; }
    void SetSlashNode( SlashNode *pnode ) { mpSlashNode = pnode; }

    const std::string GetName( void ) const { return mName; }
    void SetName( const std::string &name ) { mName = name; }
    const std::string GetShortName( void ) const { return mShortName; }
    void SetShortName( const std::string &name ) { mShortName = name; }

	void AddKeyword( const std::string & Keyword )
	{
		mKeywords.insert( Keyword );
	}

	bool HasKeyword( const std::string & Keyword ) const { return mKeywords.find(Keyword)!=mKeywords.end(); }

	void CopyKeywords( const AttrChoiceValue &From ) { mKeywords=From.mKeywords; }

	void SetFunctor( const ChoiceFunctor* ftor ) { mpFunctor=ftor; }
	const ChoiceFunctor* GetFunctor( void ) const { return mpFunctor; }

	void SetCustomData( const any64& data ) { mCustomData=data; }
	const any64& GetCustomData() const { return mCustomData; }

private:

	std::string				mValue;
	std::string				mName;
    std::string				mShortName;
	SlashNode*				mpSlashNode;
	orkset<std::string>		mKeywords;
	const ChoiceFunctor*	mpFunctor;
	any64					mCustomData;
};

///////////////////////////////////////////////////////////////////////////////

typedef std::pair<std::string,std::string> ChoiceListFilter;

struct ChoiceListFilters
{
	orkmultimap<std::string,std::string> mFilterMap;


	void AddFilter( const std::string &key, const std::string &val ) { mFilterMap.insert( ChoiceListFilter( key,val ) ); }
	bool KeyMatch( const std::string &key, const std::string &val ) const;

};

class ChoiceList
{
private:

    orkvector< AttrChoiceValue* >						mChoicesVect;
	orkmap< std::string, AttrChoiceValue * >			mValueMap;
    orkmap< std::string, AttrChoiceValue * >			mNameMap;
    orkmap< std::string, AttrChoiceValue * >			mShortNameMap;
    SlashTree*											mHierarchy;

    void UpdateHierarchy( void );

protected:
	
    void remove( const AttrChoiceValue & val );

	void clear( void );

public: //

    ChoiceList();
    virtual ~ChoiceList();

	void FindAssetChoices(const file::Path& sdir, const std::string& wildcard);

    const AttrChoiceValue* FindFromLongName( const std::string &longname  ) const ;
    const AttrChoiceValue* FindFromShortName( const std::string &shortname ) const ;
	const AttrChoiceValue* FindFromValue( const std::string & uval ) const;

	const SlashTree* GetHierarchy( void ) const { return mHierarchy; }

	virtual void EnumerateChoices( bool bforcenocache=false ) = 0;
	void add( const AttrChoiceValue & val );

	QMenu* CreateMenu( const ChoiceListFilters *Filter = 0 ) const;

	bool DoesSlashNodePassFilter( const SlashNode *pnode, const ChoiceListFilters *Filter ) const;

	void dump( void );
};

///////////////////////////////////////////////////////////////////////////////

class ChoiceManager
{
	orkmap<std::string,ChoiceList*>	mChoiceListMap;

public:

	void AddChoiceList( const std::string & ListName, ChoiceList *plist );
	const ChoiceList *GetChoiceList( const std::string & ListName ) const;
	ChoiceList *GetChoiceList( const std::string & ListName );

	ChoiceManager();
	~ChoiceManager();

};


///////////////////////////////////////////////////////////////////////////////

} } // namespace ork::tool

///////////////////////////////////////////////////////////////////////////////

#endif // _ORK_TOOLCORE_CHOICEMAN_H
