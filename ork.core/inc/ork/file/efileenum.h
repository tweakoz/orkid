////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_EFIlE_ENUMS_H_
#define _ORK_EFIlE_ENUMS_H_

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

enum EFileDevFlags
{
	EFDF_CAN_READ       = 0x00000001,
	EFDF_CAN_WRITE      = 0x00000002,
	EFDF_CAN_READ_ASYNC = 0x00000004,
	EFDF_PAKFILE_ACTIVE = 0x80000000
};

///////////////////////////////////////////////////////////////////////////////

enum EFileErrCode
{
	EFEC_FILE_OK = 0,
	EFEC_FILE_DOES_NOT_EXIST,
	EFEC_FILE_NOT_OPEN,
	EFEC_FILE_INVALID_ADDRESS,
	EFEC_FILE_INVALID_CAPS,
	EFEC_FILE_INVALID_MODE,
	EFEC_FILE_INVALID_SIZE,
	EFEC_FILE_UNSUPPORTED,
	EFEC_FILE_UNKNOWN
};

///////////////////////////////////////////////////////////////////////////////

enum EFileMode
{
	EFM_READ    = 0x00000001,
	EFM_ASCII   = 0x00000002,
	EFM_ASYNC   = 0x00000004,
	EFM_WRITE   = 0x00000008,
	EFM_APPEND  = 0x00000010,
	EFM_FAST    = 0x00000020,
	EFM_READ_FAST  = EFM_READ | EFM_FAST,
};

///////////////////////////////////////////////////////////////////////////////
}
///////////////////////////////////////////////////////////////////////////////

#endif // _ORK_EFIlE_ENUMS_H_
