////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

///////////////////////////////////////////////////////////////////////////////

#include <ork/file/efileenum.h>
#include <ork/file/path.h>
#include <ork/file/filedevcontext.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////
extern const bool kPerformBuffering;
class File;
class FileDev;
///////////////////////////////////////////////////////////////////////////////

class FileContext
{
private:

	File * mpCurFile;
	int     miCtxPosition;
	U32     muDeviceFlags;
};

///////////////////////////////////////////////////////////////////////////////

class FileProgressWatcher
{
	bool mbEnable;

public:

	FileProgressWatcher() : mbEnable(false) {}

	void SetEnable( bool bena ) { mbEnable=bena; }
	bool GetEnable() const { return mbEnable; }

	virtual void BeginBlock() = 0;
	virtual void EndBlock() = 0;
	virtual void BeginFile(const File* file) = 0;
	virtual void EndFile(const File* file) = 0;
	virtual void Reading( const File* file, size_t ibytes ) = 0;
};

///////////////////////////////////////////////////////////////////////////////

class FileDev
{
	virtual EFileErrCode DoRead( File &rFile, void *pTo, size_t iSize, size_t& iactualread ) = 0;
	virtual EFileErrCode DoOpenFile( File &rFile ) = 0;
	virtual EFileErrCode DoCloseFile( File &rFile ) = 0;
	virtual EFileErrCode DoSeekFromStart( File &rFile, size_t iTo ) = 0;
	virtual EFileErrCode DoSeekFromCurrent( File &rFile, size_t iOffset ) = 0;
	virtual EFileErrCode DoGetLength( File &rFile, size_t& riLength ) = 0;

public:

	static const int kBUFFERMASK = 0xffffffe0;
	static const int kBUFFERALIGN = 32;
	static const int kBUFFERALIGNM1 = (kBUFFERALIGN-1);
	static const int kSEEKALIGN = 4;

	void           PushContext( FileContext * pCTX );
	FileContext * PopContext( void );

	EFileErrCode CheckFileDevCaps( File &rFile );

	inline bool CanRead( void ) { return (bool)( muDeviceCaps & EFDF_CAN_READ ); }
	inline bool CanWrite( void ) { return (bool)( muDeviceCaps & EFDF_CAN_WRITE ); }
	inline bool CanReadAsync( void ) { return (bool)( muDeviceCaps & EFDF_CAN_READ_ASYNC ); }
	inline bool IsPakFileActive( void ) { return (bool)( muDeviceCaps & EFDF_PAKFILE_ACTIVE ); }
	inline void SetPrependFilesystemBase(bool setting) { mbPrependFSBase=setting; } 
	inline bool GetPrependFilesystemBase(void) const { return mbPrependFSBase; } 
    inline void SetFileSystemBaseAbs( const file::Path::NameType& Base ) { mFsBaseAbs = Base; } 
    inline void SetFileSystemBaseRel( const file::Path::NameType& Base ) { mFsBaseRel = Base; } 
	inline const file::Path::NameType & GetFilesystemBaseAbs( void ) const { return mFsBaseAbs; }
	inline const file::Path::NameType & GetFilesystemBaseRel( void ) const { return mFsBaseRel; }

	//////////////////////////////////////////

	EFileErrCode Read( File &rFile, void *pTo, size_t iSize );
	EFileErrCode OpenFile( File &rFile );
	EFileErrCode CloseFile( File &rFile );
	EFileErrCode SeekFromStart( File &rFile, size_t iTo );
	EFileErrCode SeekFromCurrent( File &rFile, size_t iOffset );
	EFileErrCode GetLength( File &rFile, size_t& riLength );

	//////////////////////////////////////////
	// Abstract Interface

	virtual EFileErrCode Write( File &rFile, const void *pFrom, size_t iSize ) = 0;
	virtual EFileErrCode GetCurrentDirectory( file::Path::NameType& directory ) = 0;
	virtual EFileErrCode SetCurrentDirectory( const file::Path::NameType& directory ) = 0;

	virtual bool DoesFileExist( const file::Path& filespec ) = 0;
	virtual bool DoesDirectoryExist( const file::Path& filespec ) = 0;
	virtual bool IsFileWritable( const file::Path& filespec ) = 0;

	FileDev( file::Path::NameType devicename, file::Path fsbase, U32 devcaps );

	virtual ~FileDev() {}
	
	//void PushParamContext( void );
	//void PopParamContext( void );
	//FileDevContext & RefParamContext( void ) { return mFileDevContext[ mFileDevContextStackDepth ]; }
	//const FileDevContext & RefParamContext( void ) const  { return mFileDevContext[ mFileDevContextStackDepth ]; }

	void SetWatcher( FileProgressWatcher* watcher ) { mWatcher=watcher; }
	FileProgressWatcher* GetWatcher() const { return mWatcher; }

protected:

	static const int kFileDevContextStackMax = 4;
	int	mFileDevContextStackDepth;

	FileDevContext	mFileDevContext[ kFileDevContextStackMax ];

	file::Path::NameType	mFsBaseAbs;
	file::Path::NameType	mFsBaseRel;
	bool					mbPrependFSBase;

	file::Path::NameType	msDeviceName;
	U32						muDeviceCaps;
	FileContext *			mpCurrentContext;
	//FileContext *			maContextStack[ MAX_FILE_CONTEXT ];
	u8*						mReadBuffer;
	FileProgressWatcher*	mWatcher;

};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////
