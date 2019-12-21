////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#if defined(ORK_OSX)
#include <orktool/orktool_pch.h>
#include <orktool/filter/filter.h>
#include <ork/lev2/gfx/dxt.h>
#include <ork/file/file.h>
#include <Quartz/Quartz.h>
#include <ork/kernel/objc.h>

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace tool {
///////////////////////////////////////////////////////////////////////////////

struct QcCompositionCache
{
	QcCompositionCache( const char* pname, int iw, int ih, const std::set<float>& times )
		: miNumFilesCached(0)
		, miNumFilesNotCached(0)
	{
		const file::Path pth( pname );
		file::Path abspath = pth.ToAbsolute();
		if( FileEnv::GetRef().DoesFileExist( abspath ) )
		{
			std::string BasePath = abspath.c_str();
			OldStlSchoolFindAndReplace<std::string>( BasePath, "/pc/", "/src/" );
			OldStlSchoolFindAndReplace<std::string>( BasePath, ".qtz", "" );
			BasePath = CreateFormattedString( "%s", BasePath.c_str() );
			printf( "BasePATH<%s>\n", BasePath.c_str() );
			//////////////////////////////////////////
			if( false == FileEnv::GetRef().DoesFileExist( BasePath.c_str() ) )
			{
				std::string cmdstr = CreateFormattedString( "mkdir -p %s", BasePath.c_str() );
				printf( "CMD<%s>\n", cmdstr.c_str() );
				system(cmdstr.c_str());
			}
			//////////////////////////////////////////
			int iframe = 0;
			for( std::set<float>::const_iterator it=times.begin(); it!=times.end(); it++ )
			{
				float ftime = *it;
				std::string hfname = CreateFormattedString( "%s/frame%04d.png", BasePath.c_str(), iframe );
				miNumFilesNotCached++;
				printf( "qccache file %s\n", hfname.c_str() );
				mFrameHashFilenames[ftime] = hfname.c_str();
				iframe++;
			}
			//OrkAssert(false);
		}
	}

	std::map<float,file::Path> mFrameHashFilenames;
	int miNumFilesCached;
	int miNumFilesNotCached;

};

bool QtzToPngSequence(	const file::Path& pth,
						int iFPS, float fstart, float fend,
						int iwidth, int iheight )
{
	////////////////////////////////
	// compute set of frametimes
	////////////////////////////////
	float fstep = 1.0f/float(iFPS);
	std::set<float> frametimes;
	for( float ftime=fstart; ftime<fend; ftime+=fstep )
	{	frametimes.insert(ftime);
	}
	////////////////////////////////
	QcCompositionCache QCCC( pth.c_str(), iwidth, iheight, frametimes );
	int inumnotcached = QCCC.miNumFilesNotCached;
	////////////////////////////////
	QCRenderer* pqcren = 0;
	Objc::Object QCREN;
	file::Path abspath = pth.ToAbsolute();
	bool bQTZPRESENT = FileEnv::GetRef().DoesFileExist( abspath );
	if( bQTZPRESENT )
	{	printf( "found qtz file<%s> inumnotcached<%d>\n", abspath.c_str(), inumnotcached );
		NSString* PathToComp = [NSString stringWithUTF8String:abspath.c_str()];
		////////////////////////////////
		QCComposition* pcomp = [QCComposition compositionWithFile:PathToComp];
		////////////////////////////////
		NSSize size;
		size.width = float(iwidth);
		size.height = float(iheight);
		CGColorSpaceRef csref = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
		pqcren = [	[QCRenderer alloc]
					initOffScreenWithSize:size
					colorSpace:csref
					composition: pcomp ];

		QCREN = pqcren;
		QCREN.Dump();
	}
	////////////////////////////////
	// cache frames of composition into textures
	////////////////////////////////

	if( bQTZPRESENT )
	for( std::set<float>::const_iterator it=frametimes.begin(); it!=frametimes.end(); it++ )
	{
		float ftime = *it;

		std::map<float,file::Path>::const_iterator itF = QCCC.mFrameHashFilenames.find(ftime);
		OrkAssert(itF!=QCCC.mFrameHashFilenames.end());

		const file::Path& texpth = itF->second;

		bool bEXISTS = FileEnv::GetRef().DoesFileExist( texpth.ToAbsolute() );

		/////////////////////////////////////
		// write to disk
		/////////////////////////////////////
		//if( false == bEXISTS )
		{
			BOOL bren = [pqcren renderAtTime:double(ftime) arguments:nil];
			OrkAssert( bren );
			NSBitmapImageRep* pbmap = [pqcren createSnapshotImageOfType:@"NSBitmapImageRep"];
			file::Path pngpth = texpth.ToAbsolute();
			pngpth.SetExtension("png" );
			std::string pngpthS = pngpth.c_str();
			OldStlSchoolFindAndReplace<std::string>( pngpthS, "/pc/", "/src/" );
			NSString* PathToPng = [NSString stringWithUTF8String:pngpthS.c_str()];
			printf( "RENDER QTZCOMP time<%f> fname<%s>\n", ftime, pngpthS.c_str() );
			NSData*data = [pbmap representationUsingType:NSPNGFileType properties:nil];
			[data writeToFile:PathToPng atomically: YES];
		}
	}
	return true;
}

///////////////////////////////////////////////////////////////////////////////

bool QtzComposerToPng( const tokenlist& toklist )
{
	ork::tool::FilterOptMap	OptionsMap;
	OptionsMap.SetDefault( "--in", "yo.qtz" );
	OptionsMap.SetDefault( "--out", "yo.png" );
	OptionsMap.SetDefault( "--width", "256" );
	OptionsMap.SetDefault( "--height", "256" );
	OptionsMap.SetDefault( "--starttime", "0.0" );
	OptionsMap.SetDefault( "--endtime", "1.0" );
	OptionsMap.SetDefault( "--fps", "30.0" );
	OptionsMap.SetOptions( toklist );

	std::string qtz_in = OptionsMap.GetOption( "--in" )->GetValue();
	std::string png_out = OptionsMap.GetOption( "--out" )->GetValue();
	std::string swidth = OptionsMap.GetOption( "--width" )->GetValue();
	std::string sheight = OptionsMap.GetOption( "--height" )->GetValue();
	std::string sstart = OptionsMap.GetOption( "--starttime" )->GetValue();
	std::string send = OptionsMap.GetOption( "--endtime" )->GetValue();
	std::string sFPS = OptionsMap.GetOption( "--fps" )->GetValue();

	int iw, ih, iFPS;
	float fstart, fend;

	sscanf( swidth.c_str(), "%d", & iw );
	sscanf( sheight.c_str(), "%d", & ih );
	sscanf( sFPS.c_str(), "%d", & iFPS );
	sscanf( sstart.c_str(), "%f", & fstart );
	sscanf( send.c_str(), "%f", & fend );

	printf( "IN<%s>\n", qtz_in.c_str() );
	printf( "OUT<%s>\n", png_out.c_str() );
	printf( "WIDTH<%s:%d>\n", swidth.c_str(), iw );
	printf( "HEIGHT<%s:%d>\n", sheight.c_str(), ih );
	printf( "FPS<%s:%d>\n", sFPS.c_str(), iFPS );
	printf( "START<%s:%f>\n", sstart.c_str(), fstart );
	printf( "END<%s:%f>\n", send.c_str(), fend );


	file::Path QtzPath( qtz_in.c_str() );
	file::Path PngPath( png_out.c_str() );

	bool bQtzPresent = FileEnv::GetRef().DoesFileExist( QtzPath );

	if( false == bQtzPresent ) return false;


	return QtzToPngSequence(	QtzPath,
								iFPS, fstart, fend,
								iw, ih );

}

///////////////////////////////////////////////////////////////////////////////

}}
#endif
