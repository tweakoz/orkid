///////////////////////////////////////////////////////////////////////////////
//
//	Orkid QT User Interface Glue
//
///////////////////////////////////////////////////////////////////////////////

#include <orktool/qtui/qtui_tool.h>

///////////////////////////////////////////////////////////////////////////////

#include <orktool/manip/manip.h>
#include <orktool/filter/gfx/collada/collada.h>
//#include <gfx/gfxanim.h>
#include <ork/file/tinyxml/tinyxml.h>
#include <ork/file/path.h>
//#include <ork/entity/ReferenceArchetype.h>
#include <orktool/toolcore/builtinchoices.h>
#include <ork/lev2/lev2_asset.h>

namespace ork {
namespace tool {

///////////////////////////////////////////////////////////////////////////

bool CChoiceList::DoesSlashNodePassFilter( const SlashNode *pnode, const ChoiceListFilters *Filter ) const
{
	bool bpass = true;

	if( Filter )
	{
		bpass = false;

		std::stack<const SlashNode*> NodeStack;

		NodeStack.push( pnode );

		while( false == NodeStack.empty() )
		{
			const SlashNode* snode = NodeStack.top();

			NodeStack.pop();

			int inumchildren = snode->GetNumChildren();

			const orkmap< std::string, SlashNode * >& children = snode->GetChildren();

			for( orkmap< std::string, SlashNode * >::const_iterator it=children.begin(); it!=children.end(); it++ )
			{

				SlashNode* pchild = it->second;

				if( pchild->IsLeaf() )
				{
					if( Filter )
					{
						if( Filter->mFilterMap.size() )
						{
							const CAttrChoiceValue *chcval = FindFromLongName( pchild->getfullpath() );

							if( chcval )
							{
								for( orkmultimap<std::string,std::string>::const_iterator it=Filter->mFilterMap.begin(); it!=Filter->mFilterMap.end(); it++ )
								{
									const std::string & key = it->first;
									const std::string & val = it->second;

									bpass |= chcval->HasKeyword( key + "=" + val );
								}
							}
						}
					}
				}
				else
				{
					NodeStack.push( pchild );
				}
			}
		}
	}
	return bpass;
}


///////////////////////////////////////////////////////////////////////////

struct StackNode
{
	const SlashNode*	mpnode;
	QMenu*				mpmenu;
	StackNode( const SlashNode*pnode, QMenu*pmenu ) { mpnode=pnode; mpmenu=pmenu; }
};

QMenu *CChoiceList::CreateMenu( const ChoiceListFilters *Filter ) const
{
	QMenu *pMenu = new QMenu(0);

	const SlashTree *phier = GetHierarchy();

	const SlashNode *prootnode = phier->GetRoot();;


	std::queue<StackNode> NodeStack;

	NodeStack.push( StackNode( prootnode,pMenu ) );

	while( false == NodeStack.empty() )
	{
		StackNode snode = NodeStack.front();

		NodeStack.pop();

		int inumchildren = snode.mpnode->GetNumChildren();

		const orkmap< std::string, SlashNode * >& children = snode.mpnode->GetChildren();

		for( orkmap< std::string, SlashNode * >::const_iterator it=children.begin(); it!=children.end(); it++ )
		{
			const SlashNode *pchild = it->second;

			bool IsASlash = ( pchild->GetNodeName() == "/" );

			if( pchild->IsLeaf() )
			{
				bool badd = true;

				if( Filter )
				{
					badd = false;

					if( Filter->mFilterMap.size() )
					{
						const CAttrChoiceValue *chcval = FindFromLongName( pchild->getfullpath() );

						if( chcval )
						{
							for( orkmultimap<std::string,std::string>::const_iterator it=Filter->mFilterMap.begin(); it!=Filter->mFilterMap.end(); it++ )
							{
								const std::string & key = it->first;
								const std::string & val = it->second;

								badd |= chcval->HasKeyword( key + "=" + val );
							}
						}
					}
				}
				if( badd )
				{
					QAction *pchildact = snode.mpmenu->addAction( pchild->GetNodeName().c_str() );

					const CAttrChoiceValue *chcval = FindFromLongName( pchild->getfullpath() );

					pchildact->setProperty( "chcval", QVariant::fromValue((void *)chcval) );

					QVariant UserData( QString( pchild->getfullpath().c_str() ) );

					pchildact->setData(UserData);
				}
			}
			else
			{
				if( DoesSlashNodePassFilter( pchild, Filter ) )
				{
					if( IsASlash )
					{
						NodeStack.push( StackNode( pchild, snode.mpmenu ) );
					}
					else
					{
						QMenu *pchildmenu = snode.mpmenu->addMenu( pchild->GetNodeName().c_str() );
						NodeStack.push( StackNode( pchild, pchildmenu ) );
					}
				}
			}
		}

	}

	return pMenu;
}

///////////////////////////////////////////////////////////////////////////

#if defined(USE_FCOLLADA)
void ColladaChoiceCache( const file::Path& sdir, CChoiceList *ChcList, const std::string & ext, const CColladaAsset::EAssetType TestType )
{
	//TiXmlDeclaration * XmlDeclNode = new TiXmlDeclaration( "1.0", "", "" );
	//TiXmlElement *CacheRootNode = new TiXmlElement( CacheName );
	//CacheDoc.LinkEndChild( XmlDeclNode );
	//CacheDoc.LinkEndChild( CacheRootNode );
	//file::Path searchdir = CFileEnv::GetRef().GetPathFromUrlExt("data://");
	file::Path::NameType wildcard = CreateFormattedString("*.%s",ext.c_str()).c_str();
	orkvector<file::Path::NameType> files = CFileEnv::filespec_search( wildcard, sdir );
	int inumfiles = (int) files.size();
	file::Path::NameType searchdir( sdir.ToAbsolute().c_str() );
	searchdir.replace_in_place("\\","/");
	for( int ifile=0; ifile<inumfiles; ifile++ )
	{
		file::Path::NameType ObjPtrStr = CFileEnv::filespec_no_extension( CFileEnv::filespec_strip_base( files[ifile], "./" ) );
		ObjPtrStr.replace_in_place(searchdir.c_str(),"");
		ChcList->add( CAttrChoiceValue( ObjPtrStr.c_str(), ObjPtrStr.c_str() ) );
	}
}
#endif

///////////////////////////////////////////////////////////////////////////

void CModelChoices::EnumerateChoices( bool bforcenocache )
{
	clear();
	FindAssetChoices( "data://", "*.xgm" );
	//ColladaChoiceCache( "data://", this, "xgm", CColladaAsset::ECOLLADA_MODEL );
}

///////////////////////////////////////////////////////////////////////////

void CAnimChoices::EnumerateChoices( bool bforcenocache )
{
	clear();
#if defined(USE_FCOLLADA)
	ColladaChoiceCache( "data://", this, "xga", CColladaAsset::ECOLLADA_ANIM );
#endif
}

///////////////////////////////////////////////////////////////////////////

void CChoiceList::FindAssetChoices(const file::Path& sdir, const std::string& wildcard)
{
	orkvector<file::Path::NameType> files = CFileEnv::filespec_search( wildcard.c_str(), sdir.c_str() );
	int inumfiles = (int) files.size();
	file::Path::NameType searchdir( sdir.ToAbsolute().c_str() );
	searchdir.replace_in_place("\\","/");
	for( int ifile=0; ifile<inumfiles; ifile++ )
	{
		auto the_file = files[ifile];
		auto the_stripped = CFileEnv::filespec_strip_base( the_file, "./" );
		file::Path::NameType ObjPtrStr = CFileEnv::filespec_no_extension( the_stripped );
		file::Path::NameType ObjPtrStrA;
		ObjPtrStrA.replace(ObjPtrStr.c_str(), searchdir.c_str(), "" );
		//OrkSTXFindAndReplace( ObjPtrStrA, searchdir, file::Path::NameType("") );
		file::Path::NameType ObjPtrStr2 = file::Path::NameType(sdir.c_str()) + ObjPtrStrA;
		file::Path OutPath( ObjPtrStr2.c_str() );
		//OutPath.SetUrlBase( sdir.GetUrlBase().c_str() );
		add( CAttrChoiceValue( OutPath.c_str(), OutPath.c_str() ) );
		//orkprintf( "FOUND ASSERT<%s>\n", the_file.c_str() );
	}
}

///////////////////////////////////////////////////////////////////////////

void AudioStreamChoices::EnumerateChoices( bool bforcenocache )
{
	clear();
	FindAssetChoices( "data://", "*.xwma" );
	//FindAssetChoices( "data://", "*.wav" );
	//FindAssetChoices( "data://", "*.mp3" );
}

///////////////////////////////////////////////////////////////////////////

void AudioBankChoices::EnumerateChoices( bool bforcenocache )
{
	clear();
	FindAssetChoices( "data://", "*.xab" );
}

///////////////////////////////////////////////////////////////////////////

void ScriptChoices::EnumerateChoices( bool bforcenocache )
{
	clear();
	FindAssetChoices( "data://", "*.scr" );
}

///////////////////////////////////////////////////////////////////////////

void CTextureChoices::EnumerateChoices( bool bforcenocache )
{
	clear();

	auto items = lev2::TextureAsset::GetClassStatic()->EnumerateExisting();

	for( const auto& i : items )
	{
		add( CAttrChoiceValue( i.c_str(), i.c_str() ) );
	}
	/*FindAssetChoices( "data://", "*.vds" );
	FindAssetChoices( "data://", "*.qtz" );
	FindAssetChoices( "data://", "*.dds" );
	FindAssetChoices( "data://", "*.png" );
//	FindAssetChoices( "lev2://", "*.tga" );
	FindAssetChoices( "lev2://", "*.dds" );*/
}

///////////////////////////////////////////////////////////////////////////

void FxShaderChoices::EnumerateChoices( bool bforcenocache )
{
	clear();
	FindAssetChoices( "data://", "*.fx" );
}

///////////////////////////////////////////////////////////////////////////

void ChsmChoices::EnumerateChoices( bool bforcenocache )
{
	clear();
	FindAssetChoices( "data://chsms/", "*.mox" );
}

///////////////////////////////////////////////////////////////////////////

ChsmChoices::ChsmChoices()
{
	EnumerateChoices();
}

///////////////////////////////////////////////////////////////////////////

FxShaderChoices::FxShaderChoices()
{
	EnumerateChoices();
}

AudioStreamChoices::AudioStreamChoices()
{
	EnumerateChoices();
}

AudioBankChoices::AudioBankChoices()
{
	EnumerateChoices();
}

CModelChoices::CModelChoices()
{
	EnumerateChoices();
}

CAnimChoices::CAnimChoices()
{
	EnumerateChoices();
}

CTextureChoices::CTextureChoices()
{
	EnumerateChoices();
}
ScriptChoices::ScriptChoices()
{
	EnumerateChoices();
}

} } // namespace ork::tool
