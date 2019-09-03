///////////////////////////////////////////////////////////////////////////////
//
//
///////////////////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orktypes.h>
#include <ork/file/efileenum.h>
#include <ork/file/path.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class FileDev;
class File;

///////////////////////////////////////////////////////////////////////////////

enum ETocMode
{
	ETM_NO_TOC = 0,
	ETM_CREATE_TOC,
	ETM_USE_TOC,
};

///////////////////////////////////////////////////////////////////////////////

class SFileDevContext
{
public:

	

	typedef bool (*path_converter_type) ( file::Path& pth );


	SFileDevContext();
	SFileDevContext( const SFileDevContext& oth );

	void AddPathConverter( path_converter_type cvr ) { mPathConverters.push_back( cvr ); }
	const orkvector<path_converter_type>& GetPathConverters() const { return mPathConverters; }

	void SetFilesystemBaseAbs( file::Path::NameType base );
	void SetFilesystemBaseRel( file::Path::NameType base );
	void SetFilesystemBaseEnable( bool bv ) { mbPrependFilesystemBase=bv; }
	const file::Path::NameType& GetFilesystemBaseAbs() const { return msFilesystemBaseAbs; }
	const file::Path::NameType& GetFilesystemBaseRel() const { return msFilesystemBaseRel; }
	bool GetPrependFilesystemBase() const { return mbPrependFilesystemBase; }
	void SetPrependFilesystemBase(bool bv) { mbPrependFilesystemBase=bv; }
	FileDev *GetFileDevice() const { return mpFileDevice; }
	void SetFileDevice(FileDev *pFileDevice) { mpFileDevice = pFileDevice; }

	void CreateToc(const file::Path::SmallNameType& UrlName);
	void SetTocMode( ETocMode etm ) { meTocMode=etm; }
	ETocMode GetTocMode() const { return meTocMode; }

	const orkmap<file::Path::NameType,int>& GetTOC() const { return mTOC; }
	orkmap<file::Path::NameType,int>& GetTOC() { return mTOC; }

private:

	file::Path::NameType msFilesystemBaseAbs;
	file::Path::NameType msFilesystemBaseRel;
	bool mbPrependFilesystemBase;
	FileDev *mpFileDevice;
	orkvector<path_converter_type>		mPathConverters;
	orkmap<file::Path::NameType,int>	mTOC;
	ETocMode							meTocMode;
};

} // namespace ork