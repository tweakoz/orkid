////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////
#include <string>
#include <ork/orktypes.h>
#include <ork/file/efileenum.h>
#include <ork/kernel/kernel.h>
#include <ork/kernel/core/singleton.h>
#include <ork/file/path.h>
#include <ork/file/filedevcontext.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

class CFileDev;
class CFile;

struct CFileEnvDir
{
    long					handle; /* -1 for failed rewind */
    file::Path::NameType	result; /* d_name null iff first time */
	file::Path::NameType	name;  /* null-terminated file::Path::NameType */

	u8						info;
};

///////////////////////////////////////////////////////////////////////////////
namespace file {
///////////////////////////////////////////////////////////////////////////////

	file::Path::NameType	GetStartupDirectory();
	EFileErrCode 			SetCurDir( const file::Path::NameType & inspec );
	file::Path::NameType	GetCurDir();

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

enum ELINFILEMODE
{
	ELFM_NONE = 0,
	ELFM_READ,
	ELFM_WRITE,
};

///////////////////////////////////////////////////////////////////////////////

class CFileEnv : public NoRttiSingleton< CFileEnv >
{
	// The constructor of a singleton must be private to show users that they must use GetRef instead.
	// This ugly friend declaration is a side-effect of that. The only alternative is to ditch the template
	// and just use singleton as a pattern.
	friend class NoRttiSingleton< CFileEnv >;

	CFileDev *mpDefaultDevice;
	orkmap<ork::file::Path::SmallNameType, SFileDevContext> mUrlRegistryMap;

public:
	CFileEnv();

	// These should all be forwarded to CFileDev
	CFileEnvDir*			OpenDir(const char *);
	int						CloseDir(CFileEnvDir *);
	file::Path::NameType	ReadDir(CFileEnvDir *);
	void					RewindDir(CFileEnvDir *);

	const orkmap<ork::file::Path::SmallNameType, SFileDevContext>& RefUrlRegistry() const { return mUrlRegistryMap; }

	CFileDev* GetDefaultDevice() const { return mpDefaultDevice; }
	CFileDev* GetDeviceForUrl(const file::Path &fileName) const;

	//////////////////////////////////////////

	void SetDefaultDevice(CFileDev *pDevice) { mpDefaultDevice = pDevice; }

	//////////////////////////////////////////
	// Caps And Flags

	static bool CanRead( void );
	static bool CanWrite( void );
	static bool CanReadAsync( void );

	//////////////////////////////////////////
	// misc support

	static bool						filespec_isunix( const file::Path::NameType & inspec );
	static bool						filespec_isdos( const file::Path::NameType & inspec );

	static file::Path::NameType		filespec_to_extension( const file::Path::NameType & inspec );
	static file::Path::NameType		filespec_no_extension( const file::Path::NameType & inspec );
	static file::Path::NameType		filespec_strip_base( const file::Path::NameType & inspec, const file::Path::NameType & base );

	static orkvector<file::Path::NameType>	filespec_separate_terms( const file::Path::NameType & inspec );
	static orkvector<file::Path::NameType>	filespec_search( const file::Path::NameType & wildcards, const ork::file::Path& initdir );
	static orkset<file::Path::NameType>		filespec_search_sorted( const file::Path::NameType & wildcards, const ork::file::Path& initdir );
	static FileStampH						EncodeFileStamp( int year, int month, int day, int hour, int minute, int second );
	static void								DecodeFileStamp( FileStampH stamp, int& year, int& month, int& day, int& hour, int& minute, int& second );
	static FileStampH						GetFileStamp( const file::Path::NameType & filespec );

	/// This is a helper function for FilespecToContainingDirectory, but hell, feel free to use it
	/// yourself. It simply finds the last occurence of any character from the set of chars that make
	/// up setOfChars, and returns the substring of stringToTruncate up until (but not including) the
	/// last char.
	///
	/// @param stringToTruncate This is the string that will be be searching and truncating.
	/// @param setOfChars Think of this param as set of independant chars (order does not matter) to search for inside
	///        stringToTruncate.
	/// @return The truncated string from the first char up until the last char found from setOfChars. If no
	///         char is found, then stringToTruncate is returned.
	static file::Path::NameType TruncateAtFirstCharFromSet(const file::Path::NameType& stringToTruncate, const file::Path::NameType& setOfChars);

	/// For a given path, this function returns the folder path which contains the represented file. If path is
	/// a directory then the function will return path. Trailing separators ('/' or '\\') will be stripped. The
	/// function works for UNIX or DOS filenames.
	///
	/// @param path The string on which to find the containing folder.
	/// @return The containing folder (does not contain a trailing separator).
	/// @pre filespec_isunix(path) || filespec_isdos(path)
	static file::Path::NameType FilespecToContainingDirectory(const file::Path::NameType& path);

	//////////////////////////////////////////

	static bool						DoesFileExist( const file::Path&  filespec );
	static bool						DoesDirectoryExist( const file::Path& filespec );

	static bool						IsFileWritable( const file::Path& filespec );

	static void							SetPrependFilesystemBase(bool setting);
	static bool							GetPrependFilesystemBase(void);
	static void							SetFilesystemBase( file::Path::NameType fbase );
	static const file::Path::NameType&	GetFilesystemBase( void );

	//////////////////////////////////////////

	static const SFileDevContext& UrlBaseToContext( const file::Path::SmallNameType &UrlName );

	//////////////////////////////////////////

	/// Determine is a path has a URL at the front. Note that is does not check if
	/// the URL is registered. This function is purely checking for the *:// pattern
	/// at the front of the path. Use IsUrlBaseRegister to find out if it's actually
	/// registered.
	///
	/// @param PathName The path to check for a URL
	/// @return Whether or not PathName has a URL
	static bool						PathIsUrlForm(const file::Path& PathName);

	/// Is this URL actually registeted?
	///
	/// @urlBase A URL base string (for example "lev2://" or "data://")
	/// @return Whether or the not the URL base is registered
	static bool						IsUrlBaseRegistered(const file::Path::SmallNameType& urlBase);

	static void					RegisterUrlBase(const file::Path::SmallNameType& UrlBase, const SFileDevContext& PathBase);
	static ork::file::Path		GetPathFromUrlExt( const file::Path::NameType& UrlName, const file::Path::NameType & subfolder = "", const file::Path::SmallNameType & ext = "");

	static bool IsCharAlpha( char c )
	{
		return ( ((c>='a') && (c<='z'))	|| ((c>='A') && (c<='Z')) );
	}

	static bool IsCharNumeric( char c )
	{
		return ( ((c>='0') && (c<='9')) );
	}

	static bool IsCharPunc( char c )
	{
		return ( (c=='_')||(c=='.')||(c==' ') );
	}

	static bool IsCharUnix( char c )
	{
		return (( c == '/' ) || IsCharAlpha(c) || IsCharNumeric(c) || IsCharPunc(c) );
	}
	static bool IsCharDos( char c )
	{
		return ( (c == '\\') || (c == ':') || IsCharAlpha(c) || IsCharNumeric(c) || IsCharPunc(c) );
	}

	static file::Path::SmallNameType UrlNameToBase( const file::Path::NameType& UrlName );
	static file::Path::NameType UrlNameToPath( const file::Path::NameType& UrlName );

	static void BeginLinFile( const file::Path& lfn, ELINFILEMODE emode );
	static void EndLinFile();

	
	static ELINFILEMODE GetLinFileMode() { return GetRef().meLinFileMode; }
	static const file::Path& GetLinFileName() { return GetRef().mLinFileName; }
	static CFile* GetLinFile() { return GetRef().mpLinFile; }

	private:

	/// Strips any URL from a path. The URL does not have to be regsitered.
	///
	/// @param urlName The path to strip a URL from
	/// @return The path without the URL
	static file::Path::NameType StripUrlFromPath(const file::Path::NameType& urlName);
	file::Path				mLinFileName;
	ELINFILEMODE			meLinFileMode;
	CFile*					mpLinFile;
};

typedef void (*FileAsyncDoneCallback)( void );

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

