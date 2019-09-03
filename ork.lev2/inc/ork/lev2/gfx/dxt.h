////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////


#ifndef _ORK_LEV2_DXT_H
#define _ORK_LEV2_DXT_H

namespace ork { namespace lev2 { namespace dxt {

	enum DxtFormat
	{
		EFMT_DXT1 = 0,
		EFMT_DXT3,
		EFMT_DXT5,
		EFMT_BGRA8,
		EFMT_BGR8,
		EFMT_BGR5A1,
		EFMT_BGR565,
		EFMT_INDEX8,
		EFMT_END,
	};

	struct DdsLoadInfo {
	  bool compressed;
	  bool swap;
	  bool palette;
	  uint32_t divSize;
	  uint32_t blockBytes;
	  //GLenum internalFormat;
	  //GLenum externalFormat;
	  //GLenum type;
	};

	extern const DdsLoadInfo loadInfoDXT1;
	extern const DdsLoadInfo loadInfoDXT3;
	extern const DdsLoadInfo loadInfoDXT5;
	extern const DdsLoadInfo loadInfoBGRA8;
	extern const DdsLoadInfo loadInfoBGR8;
	extern const DdsLoadInfo loadInfoBGR5A1;
	extern const DdsLoadInfo loadInfoBGR565;
	extern const DdsLoadInfo loadInfoIndex8;
	
	
	//  DDS_header.dwFlags
	const uint32_t DDSD_CAPS			= 0x00000001;
	const uint32_t DDSD_HEIGHT			= 0x00000002;
	const uint32_t DDSD_WIDTH			= 0x00000004;
	const uint32_t DDSD_PITCH			= 0x00000008;
	const uint32_t DDSD_PIXELFORMAT	= 0x00001000;
	const uint32_t DDSD_MIPMAPCOUNT	= 0x00020000;
	const uint32_t DDSD_LINEARSIZE		= 0x00080000;
	const uint32_t DDSD_DEPTH			= 0x00800000;
    const uint32_t DDSD_RGB			= 0x00000040;
    const uint32_t DDSD_RGBA			= 0x00000041;

	const uint32_t DDS_MAGIC  = 0x20534444;
	const uint32_t DDS_FOURCC = 0x00000004;

    const uint32_t DDS_COMPLEX = 0x00000008;
    const uint32_t DDS_CUBEMAP = 0x00000200;
    const uint32_t DDS_VOLUME  = 0x00200000;

    const uint32_t FOURCC_DXT1 = 0x31545844; //(MAKEFOURCC('D','X','T','1'))
    const uint32_t FOURCC_DXT3 = 0x33545844; //(MAKEFOURCC('D','X','T','3'))
    const uint32_t FOURCC_DXT5 = 0x35545844; //(MAKEFOURCC('D','X','T','5'))

    struct DDS_PIXELFORMAT
    {
        uint32_t dwSize;
        uint32_t dwFlags;
        uint32_t dwFourCC;
        uint32_t dwRGBBitCount;
        uint32_t dwRBitMask;
        uint32_t dwGBitMask;
        uint32_t dwBBitMask;
        uint32_t dwABitMask;

		void FixEndian();
	};

    struct DXTColBlock
    {
        uint16_t col0;
        uint16_t col1;

        unsigned char row[4];
    };

    struct DXT3AlphaBlock
    {
        uint16_t row[4];
    };

    struct DXT5AlphaBlock
    {
        unsigned char alpha0;
        unsigned char alpha1;

        unsigned char row[6];
    };

    struct DDS_HEADER
    {
		uint32_t dwMagic;
		uint32_t dwSize;
        uint32_t dwFlags;
        uint32_t dwHeight;
        uint32_t dwWidth;
        uint32_t dwPitchOrLinearSize;
        uint32_t dwDepth;
        uint32_t dwMipMapCount;
        uint32_t dwReserved1[11];
        DDS_PIXELFORMAT ddspf;
        uint32_t dwCaps1;
        uint32_t dwCaps2;
        uint32_t dwReserved2[3];

		void FixEndian();
    };
	bool IsLUM( DDS_PIXELFORMAT& pf );
	bool IsBGR5A1( DDS_PIXELFORMAT& pf );
	bool IsBGR8( dxt::DDS_PIXELFORMAT& pf );
	bool IsBGRA8( DDS_PIXELFORMAT& pf );
	bool IsDXT1( DDS_PIXELFORMAT& pf );
	bool IsDXT3( DDS_PIXELFORMAT& pf );
	bool IsDXT5( DDS_PIXELFORMAT& pf );
	
	bool IsABGR8( DDS_PIXELFORMAT& pf );
	bool IsRGB8( DDS_PIXELFORMAT& pf );
	bool IsXBGR8( DDS_PIXELFORMAT& pf );

	struct DDFile
	{
		DDFile( const ork::file::Path& pth );
		
		DDS_HEADER		mHeader;
		DdsLoadInfo		mLoadInfo;
		DxtFormat		meFormat;
		int				miWidth;
		int				miHeight;
		int				miDepth;
		int				miNumBlocksW;
		int				miNumBlocksH;
		bool			mbVOLUMETEX;
		const char*		mpData;
		const char*		mpImageDataBase;
		
		
		bool HasBlocks() const 
		{
			bool rv = false;
			switch( meFormat )
			{
				case EFMT_DXT1:
				case EFMT_DXT3:
				case EFMT_DXT5:
					rv = true;
					break;
				default:
					break;
			}
			return rv;
		}
	};

}}}

#endif
