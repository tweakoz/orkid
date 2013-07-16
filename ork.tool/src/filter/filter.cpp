////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>
#include <ork/application/application.h>

#include <ork/lev2/gfx/gfxenv.h>
#include <orktool/filter/filter.h>
#include "aud/soundfont.h"

#include <ork/file/filedev.h>
#include <ork/file/fileenv.h>

// Filters
#include <orktool/filter/gfx/meshutil/meshutil.h>

#include <orktool/toolcore/FunctionManager.h>
#include <ork/kernel/string/string.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/reflect/Functor.h>
#include <ork/rtti/downcast.h>

namespace ork { namespace MeshUtil 
{
#if defined(_USE_D3DX)
	void TexToVtx( const tokenlist& options );
#endif
	void PartitionMesh_FixedGrid3d_Driver( const tokenlist& options );
	void TerrainTest(const tokenlist& toklist);
} }

namespace ork { namespace tool {

bool WavToMkr( const tokenlist& toklist );

void RegisterColladaFilters();
void RegisterArchFilters();

void CAssetFilterBase::Describe()
{

}

#if defined (ORK_OSXX)
bool VolTexAssemble( const tokenlist& toklist );
bool QtzComposerToPng( const tokenlist& toklist );

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class VolTexAssembleFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(VolTexAssembleFilter,CAssetFilterBase);
public: //
	VolTexAssembleFilter(  )
	{
	}
	virtual bool ConvertAsset( const tokenlist& toklist )
	{
		VolTexAssemble( toklist );
		return true;
	}
};
void VolTexAssembleFilter::Describe() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class QtzComposerToPngFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(QtzComposerToPngFilter,CAssetFilterBase);
public: //
	QtzComposerToPngFilter(  )
	{
	}
	virtual bool ConvertAsset( const tokenlist& toklist )
	{
		QtzComposerToPng( toklist );
		return true;
	}
};
void QtzComposerToPngFilter::Describe() {}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
bool Tga2DdsFilterDriver( const tokenlist& toklist );

class TGADDSFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(TGADDSFilter,CAssetFilterBase);
public: //
	TGADDSFilter(  )
	{
	}
	virtual bool ConvertAsset( const tokenlist& toklist )
	{
		return Tga2DdsFilterDriver( toklist );
	}
};
void TGADDSFilter::Describe() {}


#if defined(_USE_D3DX)
class UvAtlasFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(UvAtlasFilter,CAssetFilterBase);
public: //
	UvAtlasFilter(  )
	{
	}
	virtual bool ConvertAsset( const tokenlist& toklist )
	{
		MeshUtil::GenerateUVAtlas( toklist );
		return true;
	}
};
void UvAtlasFilter::Describe() {}
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class Tex2VtxBakeFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(Tex2VtxBakeFilter,CAssetFilterBase);
public: //
	Tex2VtxBakeFilter(  )
	{
	}
	virtual bool ConvertAsset( const tokenlist& toklist )
	{
		MeshUtil::TexToVtx( toklist );
		return true;
	}
};
void Tex2VtxBakeFilter::Describe() {}
#endif

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class fg3dFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(fg3dFilter,CAssetFilterBase);
public: //
	fg3dFilter(  )
	{
	}
	virtual bool ConvertAsset( const tokenlist& toklist )
	{
		MeshUtil::PartitionMesh_FixedGrid3d_Driver( toklist );
		return true;
	}
};
void fg3dFilter::Describe() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

class WAVMKRFilter : public CAssetFilterBase
{
	RttiDeclareConcrete(WAVMKRFilter,CAssetFilterBase);
public: //
	WAVMKRFilter(  )
	{
	}
	virtual bool ConvertAsset( const tokenlist& toklist )
	{
		return ork::tool::WavToMkr( toklist );
	}
};
void WAVMKRFilter::Describe() {}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

static void RegisterFilters()
{
	static bool binit = true;

	if(binit)
	{
		CAssetFilter::RegisterFilter("wav:mkr", WAVMKRFilter::DesignNameStatic().c_str());
		///////////////////////////////////////////////////
		#if defined(_USE_SOUNDFONT)
		CAssetFilter::RegisterFilter("sf2:xab", SF2XABFilter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("sf2:gab", SF2GABFilter::DesignNameStatic().c_str());
		#endif
		///////////////////////////////////////////////////
		#if defined(USE_FCOLLADA)
		ork::tool::RegisterColladaFilters();
		#endif
		///////////////////////////////////////////////////		
		ork::tool::RegisterArchFilters();
		///////////////////////////////////////////////////		
		#if defined(_USE_D3DX)
		CAssetFilter::RegisterFilter("x:obj", MeshUtil::D3DX_OBJ_Filter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("xgm:x", MeshUtil::XGM_D3DX_Filter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("obj:x", MeshUtil::OBJ_D3DX_Filter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("uvatlas", UvAtlasFilter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("tex2vtx", Tex2VtxBakeFilter::DesignNameStatic().c_str());
		#endif
		///////////////////////////////////////////////////
		CAssetFilter::RegisterFilter("xgm:obj", MeshUtil::XGM_OBJ_Filter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("obj:obj", MeshUtil::OBJ_OBJ_Filter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("obj:xgm", MeshUtil::OBJ_XGM_Filter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("tga:dds", TGADDSFilter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("fg3d", fg3dFilter::DesignNameStatic().c_str());
		/////////////////////////
		#if defined(ORK_OSXX)
		CAssetFilter::RegisterFilter("qtz:png", QtzComposerToPngFilter::DesignNameStatic().c_str());
		CAssetFilter::RegisterFilter("vtc:dds", VolTexAssembleFilter::DesignNameStatic().c_str());
		#endif
		/////////////////////////
		binit = false;
	}
}

///////////////////////////////////////////////////////////////////////////////

CAssetFilterBase::CAssetFilterBase( )
{
}

///////////////////////////////////////////////////////////////////////////////

orkmap<ork::PoolString, SFilterInfo* > CAssetFilter::smFilterMap;

void CAssetFilter::RegisterFilter( const char* filtername, const char* classname, const char* pathmethod, const char* pathloc )
{
	SFilterInfo *pinfo = new SFilterInfo;
	pinfo->filtername = AddPooledString(filtername);
	pinfo->classname = AddPooledString(classname);
	pinfo->pathmethod = AddPooledString(pathmethod);
	pinfo->pathloc = AddPooledString(pathloc);
	bool badded = OrkSTXMapInsert( smFilterMap, AddPooledString(filtername), pinfo );
	OrkAssert( badded );
}

///////////////////////////////////////////////////////////////////////////////

bool CAssetFilter::ConvertFile( const char* Filter, const tokenlist& toklist )
{
	bool rval = false;

	SFilterInfo* FilterInfo = OrkSTXFindValFromKey( smFilterMap, FindPooledString(Filter), (SFilterInfo*) 0 );

	if( FilterInfo )
	{
		PoolString classname = FilterInfo->classname;

		rtti::Class* pclass = rtti::Class::FindClass( classname.c_str() );

		OrkAssert( pclass != 0 );

		CAssetFilterBase *pfilter = rtti::safe_downcast<CAssetFilterBase*>( pclass->CreateObject() );

		OrkAssert( pfilter != 0 );
	
		rval = pfilter->ConvertAsset( toklist );

		delete( pfilter );
	}

	return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool CAssetFilter::ConvertTree( const char* Filter, const std::string &InTree, const std::string &OutDir )
{
	OrkAssertNotImplI("CAssetFilter::ConvertTree is not implemented!");
	return false;
}


///////////////////////////////////////////////////////////////////////////////

bool CAssetFilter::ListFilters()
{	
	int idx = 0;
	orkmessageh( "///////////////////////////////////////\n" );
	orkmessageh( "// Orkid Filter List\n" );
	orkmessageh( "///////////////////////////////////////\n" );
	for( orkmap<PoolString,SFilterInfo*>::const_iterator it=smFilterMap.begin(); it!=smFilterMap.end(); it++ )
	{	std::pair<PoolString,SFilterInfo*> pr = *it;
		SFilterInfo *pinfo = pr.second;
		orkmessageh( "Filter %02d [%s]\n", idx, pinfo->filtername.c_str() );
		idx++;
	}
	orkmessageh( "///////////////////////////////////////\n" );

	return true;
}

///////////////////////////////////////////////////////////////////////////////

class CNullAppWindow : public ork::lev2::GfxWindow
{	public: //
	
	CNullAppWindow( int iX, int iY, int iW, int iH ) 
		: ork::lev2::GfxWindow( iX, iY, iW, iH )
	{
		CreateContext();
	}
	
	virtual void Draw( void ) {}
//	virtual void Show( void ) {};
//	virtual void Hide( void ) {};
};

///////////////////////////////////////////////////////////////////////////////

int Main_Filter( tokenlist toklist )
{	
	RegisterFilters();

	CFileEnv::SetFilesystemBase( "./" );

	//////////////////////////////////////////
	// Register fxshader:// data urlbase

	static SFileDevContext FxShaderFileContext;
	file::Path::NameType fxshaderbase = ork::file::GetStartupDirectory()+"data/src/shaders/dummy";
	file::Path fxshaderpath( fxshaderbase.c_str() ); 
	FxShaderFileContext.SetFilesystemBaseAbs( fxshaderpath.c_str() );
	FxShaderFileContext.SetPrependFilesystemBase( true );

	CFileEnv::RegisterUrlBase( "fxshader://", FxShaderFileContext );

	//////////////////////////////
	// need a gfx context for some filters

//	ork::lev2::GfxEnv::GetRef().SetCurrentRenderer( ork::lev2::EGFXENVTYPE_DUMMY );
	ork::lev2::GfxEnv::SetTargetClass(ork::lev2::GfxTargetDummy::GetClassStatic());

	CNullAppWindow *w = new CNullAppWindow( 0, 0, 640, 480 );
	ork::lev2::GfxEnv::GetRef().RegisterWinContext(w);
	ork::lev2::GfxEnv::GetRef().SetLoaderTarget( w->GetContext() );

	//////////////////////////////

	bool blist = toklist.empty();

	tokenlist::iterator it = toklist.begin();
	it++;
	const std::string & ftype = *it++;
	
	if( blist || (ftype == (std::string) "list") )
	{
		CAssetFilter::ListFilters();
	}
	else
	{
		SFilterInfo* FilterInfo = OrkSTXFindValFromKey(CAssetFilter::smFilterMap, FindPooledString(ftype.c_str()),  (SFilterInfo*) 0 );
		printf( "collada Main_Filter find<%s> finfo<%p>\n", ftype.c_str(), FilterInfo );
		if(FilterInfo)
		{
			tokenlist newtoklist;

			newtoklist.insert( newtoklist.begin(), it, toklist.end() );

			bool bret = CAssetFilter::ConvertFile( ftype.c_str(), newtoklist );
			return bret ? 0 : -1;
		}
	}
	
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int Main_FilterTree( tokenlist toklist )
{	
	RegisterFilters();

	CFileEnv::SetFilesystemBase( "./" );

    //////////////////////////////
	// need a gfx context for some filters

	//ork::lev2::GfxEnv::GetRef().SetCurrentRenderer( ork::lev2::EGFXENVTYPE_DUMMY );
	ork::lev2::GfxEnv::SetTargetClass(ork::lev2::GfxTargetDummy::GetClassStatic());
	CNullAppWindow *w = new CNullAppWindow( 0, 0, 640, 480 );

	//////////////////////////////

	std::string ftype, treename, outdest;

	toklist.erase( toklist.begin() ); // absorb -filtertree

	if( false == toklist.empty() )
	{
        ftype = *toklist.begin();
		toklist.erase( toklist.begin() );

		if( false == toklist.empty() )
		{
			treename = *toklist.begin();
			toklist.erase( toklist.begin() );

			if( false == toklist.empty() )
			{
				outdest = *toklist.begin();
				toklist.erase( toklist.begin() );
			}
		}
	}

	orkmessageh( "Converting Directory Tree [%s]\n", treename.c_str() );
	CAssetFilter::ConvertTree( ftype.c_str(), treename, outdest );
	
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

void FilterOption::SetValue( const char* defval )
{
	mValue = std::string(defval);
}

///////////////////////////////////////////////////////////////////////////////

void FilterOption::SetDefault( const char* defval )
{
	mDefaultValue = std::string(defval);
}

const std::string& FilterOption::GetValue() const
{
	if( 0 == mValue.length() )
	{
		return mDefaultValue;
	}
	else return mValue;
}

///////////////////////////////////////////////////////////////////////////////

FilterOption* FilterOptMap::GetOption( const std::string& key )
{
	orkmap<std::string, FilterOption>::iterator it = moptions_map.find( key );
	if( it != moptions_map.end() )
	{
		return & it->second;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

const FilterOption* FilterOptMap::GetOption( const std::string& key ) const
{
	orkmap<std::string, FilterOption>::const_iterator it = moptions_map.find( key );
	if( it != moptions_map.end() )
	{
		return & it->second;
	}
	return 0;
}

///////////////////////////////////////////////////////////////////////////////

bool FilterOptMap::HasOption( const std::string& key ) const
{
	orkmap<std::string, FilterOption>::const_iterator it = moptions_map.find( key );
	return (it!=moptions_map.end());
}

///////////////////////////////////////////////////////////////////////////////

void FilterOptMap::SetOptions( const tokenlist& options )
{
	for( tokenlist::const_iterator it=options.begin(); it!=options.end(); it++ )
	{
		const std::string& key = (*it);

		if( key[0] == '-' )
		{
			if( moptions_map.find(key)==moptions_map.end() )
			{
				moptions_map[key] = FilterOption( key.c_str(), "" );
			}

			FilterOption* popt = GetOption( key );

			if( it != options.end() )
			{
				tokenlist::const_iterator itn = it; itn++;

				if( itn != options.end() )
				{
					const std::string& val = (*itn);

					if( val[0] != '-' )
					{
						popt->SetValue(val.c_str());
						it++;
					}
				}
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////

void FilterOptMap::SetDefault( const char* name, const char* defval )
{
	orkmap<std::string, FilterOption>::iterator it = moptions_map.find( std::string(name) );

	if( it == moptions_map.end() )
	{
		moptions_map[ std::string(name) ] = FilterOption( name, defval );
	}
	else
	{
		it->second.SetDefault( defval );
	}
}

///////////////////////////////////////////////////////////////////////////////

FilterOptMap::FilterOptMap()
{
}

///////////////////////////////////////////////////////////////////////////////

void FilterOptMap::DumpDefaults() const
{
	orkprintf( "/////////////////////////////\n" );
	orkprintf( "// Options (defaults)\n" );

	for( orkmap<std::string, FilterOption>::const_iterator it=moptions_map.begin(); it!=moptions_map.end(); it++ )
	{
		const std::string& key = it->first;
		const FilterOption& val = it->second;

		orkprintf( "// Option key <%s> default<%s>\n", key.c_str(), val.GetDefault().c_str() );

	}
	orkprintf( "/////////////////////////////\n" );
}

///////////////////////////////////////////////////////////////////////////////

void FilterOptMap::DumpOptions() const
{
	orkprintf( "/////////////////////////////\n" );
	orkprintf( "// Options (set values)\n" );

	for( orkmap<std::string, FilterOption>::const_iterator it=moptions_map.begin(); it!=moptions_map.end(); it++ )
	{
		const std::string& key = it->first;
		const FilterOption& val = it->second;

		orkprintf( "// Option key <%s> value<%s>\n", key.c_str(), val.GetValue().c_str() );

	}	orkprintf( "/////////////////////////////\n" );
}
///////////////////////////////////////////////////////////////////////////////
} }
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
#if defined(_USE_D3DX)
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::UvAtlasFilter,"UvAtlasFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::Tex2VtxBakeFilter,"Tex2VtxBakeFilter");
#endif
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::CAssetFilterBase,"CAssetFilterBase");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::fg3dFilter,"fg3dFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::WAVMKRFilter,"WAVMKRFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::TGADDSFilter,"TGADDSFilter");

#if defined(ORK_OSXX)
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::VolTexAssembleFilter,"VolTexAssembleFilter");
INSTANTIATE_TRANSPARENT_RTTI(ork::tool::QtzComposerToPngFilter,"QtzComposerToPngFilter");
#endif
