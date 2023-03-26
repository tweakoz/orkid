////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include "cl.h"
#if 0
////////////////////////////////////////////////////////////////////////////////

#pragma comment( lib, "OpenCL.lib" )

////////////////////////////////////////////////////////////////////////////////

void SourceBuffer::CreateCLprogram(const CLDevice* pdev, cl_program& prg, cl_kernel& krn, const char* kernname )
{
	////////////////////////////////////////////////////////////
	// create program
	////////////////////////////////////////////////////////////
	int err = 0;
	prg = clCreateProgramWithSource(pdev->context(), 1, (const char **) & mpSource, NULL, &err);
	if(0==prg)
	{
		OrkAssertI(false,"could not create program!\n");
	}
	////////////////////////////////////////////////////////////
	// build program
	////////////////////////////////////////////////////////////
	class yo
	{
	public:
		static void __stdcall buildnot(cl_program prg, void* pdata )
		{
			const char* pstr = (const char*) pdata;
			printf( "not<%s>\n", pstr );
			fflush(stdout);
		}
	};
	//void (*pfn_notify)(cl_program, void *user_data), 
	err = clBuildProgram(prg, 0, NULL, NULL, yo::buildnot, NULL);
	static const int kbufsize = 1<<20;
	char* buffer = new char[kbufsize];
	size_t len;
	switch( err )
	{
	case CL_INVALID_PROGRAM:
		OrkAssert(false);
		break;
	case CL_INVALID_VALUE:
		OrkAssert(false);
		break;
	case CL_INVALID_DEVICE:
		OrkAssert(false);
		break;
	case CL_INVALID_BINARY:
		OrkAssert(false);
		break;
	case CL_INVALID_BUILD_OPTIONS:
		OrkAssert(false);
		break;
	case CL_INVALID_OPERATION:
		OrkAssert(false);
		break;
	case CL_COMPILER_NOT_AVAILABLE:
		OrkAssert(false);
		break;
	case CL_BUILD_PROGRAM_FAILURE:
	{
		cl_build_status bstat;
		cl_int i = clGetProgramBuildInfo(prg, pdev->GetDeviceID(), CL_PROGRAM_BUILD_STATUS , sizeof(bstat), &bstat, &len);
		i = clGetProgramBuildInfo(prg, pdev->GetDeviceID(), CL_PROGRAM_BUILD_LOG, kbufsize, buffer, &len);
		OrkAssert(i==0);
		printf( "BuildProgramFailure Log:\n%s\n", buffer );
		switch( bstat )
		{
			case CL_BUILD_NONE:
//				OrkAssert(false);
				break;
			case CL_BUILD_ERROR:
//				OrkAssert(false);
				break;
			case CL_BUILD_SUCCESS:
//				OrkAssert(false);
				break;
			case CL_BUILD_IN_PROGRESS:
//				OrkAssert(false);
				break;
		}
		
		OrkAssert(false);
		break;
	}
	case CL_OUT_OF_HOST_MEMORY:
		OrkAssert(false);
		break;
	}
	if(CL_SUCCESS!=err)
	{
		OrkAssertI(false,"could not build program\n");
	}
	//printf("%s\n", buffer);
	////////////////////////////////////////////////////////////
	// create kernel
	////////////////////////////////////////////////////////////
	krn = clCreateKernel(prg, kernname, &err);
	if((false==krn) || (CL_SUCCESS!=err))
	{
		OrkAssertI(false,"could not create compute kernel\n");
	}
}

////////////////////////////////////////////////////////////////////////////////

CLFromHostBuffer::CLFromHostBuffer()
	: mCLhandle(0)
	, miSize(0)
	, mpBuffer0(0)
	, mpBuffer1(0)
{
}

////////////////////////////////////////////////////////////////////////////////

CLFromHostBuffer::~CLFromHostBuffer()
{
	if( 0!=mCLhandle ) clReleaseMemObject(mCLhandle);
	if( 0!=mpBuffer0 ) delete[] mpBuffer0;
	if( 0!=mpBuffer1 ) delete[] mpBuffer1;
}

////////////////////////////////////////////////////////////////////////////////

int gmemalloxsize = 0;
void CLFromHostBuffer::resize(int isize,const CLDevice* dev)
{
	if( 0!=mCLhandle ) clReleaseMemObject(mCLhandle);
	if( 0!=mpBuffer0 ) delete[] mpBuffer0;
	if( 0!=mpBuffer1 ) delete[] mpBuffer1;
	//////////////////////////
	miSize = isize;
	mpBuffer0 = new char[ miSize ];
	mpBuffer1 = new char[ miSize ];
	//////////////////////////
	mCLhandle = clCreateBuffer(dev->context(), CL_MEM_READ_ONLY, miSize, 0, NULL);
	//////////////////////////
	gmemalloxsize+=miSize;
	if( 0 == mCLhandle )
	{
		OrkAssertI(false,"could not allocate device memory!\n");
	}
}

void CLFromHostBuffer::TransferAndBlock( const CLDevice* pdev, int isize )
{
	char* pbufregion = GetBufferRegionPriv();

	OrkAssert( isize<miSize );

	if( (isize!=0) && (isize < miSize) )
	{
		int err = clEnqueueWriteBuffer(	
							pdev->GetCmdQueue(),			// cmdq
							mCLhandle,						// buffer
							CL_FALSE,						// blocking ?
							0,								// offset
							isize,							// size
							pbufregion,					// src
							0,								// num evs in wait list
							NULL,							// ev wait list*
							NULL							// ev
							);

		if (err != CL_SUCCESS)
		{
			OrkAssertI(false,"cannot read buffer\n");
		}
	}
	miWriteIndex++;
}
////////////////////////////////////////////////////////////////////////////////

CLToHostBuffer::CLToHostBuffer()
	: mCLhandle(0)
	, miSize(0)
	, mpBuffer0(0)
	, mpBuffer1(0)
{
}

////////////////////////////////////////////////////////////////////////////////

CLToHostBuffer::~CLToHostBuffer()
{
	if( 0!=mCLhandle ) clReleaseMemObject(mCLhandle);
	if( 0!=mpBuffer0 ) delete[] mpBuffer0;
	if( 0!=mpBuffer1 ) delete[] mpBuffer1;
}

////////////////////////////////////////////////////////////////////////////////

void CLToHostBuffer::resize(int isize,const CLDevice* dev)
{
	if( 0!=mCLhandle ) clReleaseMemObject(mCLhandle);
	if( 0!=mpBuffer0 ) delete[] mpBuffer0;
	if( 0!=mpBuffer1 ) delete[] mpBuffer1;
	//////////////////////////
	miSize = isize;
	mpBuffer0 = new char[ miSize ];
	mpBuffer1 = new char[ miSize ];
	//////////////////////////
	mCLhandle = clCreateBuffer(dev->context(), CL_MEM_WRITE_ONLY, miSize, 0, NULL);
	//////////////////////////
	if( 0 == mCLhandle )
	{
		OrkAssertI(false,"could not allocate device memory!\n");
	}
}

void CLToHostBuffer::Transfer( const CLDevice* pdev, int isize, bool bblock )
{
	miReadIndex++;

	char* pbufregion = GetBufferRegionPriv();

	OrkAssert( isize<miSize );

	if( (isize!=0) && (isize < miSize) )
	{
		int err = clEnqueueReadBuffer(	
							pdev->GetCmdQueue(),			// cmdq
							mCLhandle,						// buffer
							bblock ? CL_TRUE : CL_FALSE,	// blocking ?
							0,								// offset
							isize,							// size
							pbufregion,					// src
							0,								// num evs in wait list
							NULL,							// ev wait list*
							NULL							// ev
							);

		if (err != CL_SUCCESS)
		{
			OrkAssertI(false,"cannot read buffer\n");
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CLFromHostBuffer::SetArg(cl_kernel k, int iarg)
{
	int err = clSetKernelArg(k, iarg, sizeof(cl_mem), &mCLhandle);
	if (err != CL_SUCCESS)
	{
		OrkAssertI(false,"cannot read buffer\n");
	}
}

////////////////////////////////////////////////////////////////////////////////

void CLToHostBuffer::SetArg(cl_kernel k, int iarg)
{
	int err = clSetKernelArg(k, iarg, sizeof(cl_mem), &mCLhandle);
	if (err != CL_SUCCESS)
	{
		OrkAssertI(false,"cannot read buffer\n");
	}
}

////////////////////////////////////////////////////////////////////////////////

void CLKernel::Enqueue(const CLDevice* dev, int idim, size_t* gwsize, size_t* lwsize ) const
{
	//dev->Lock();
	int err = clEnqueueNDRangeKernel(	dev->GetCmdQueue(),	// cmdq
										mKernel,			// kernel
										idim,				// work dim
										NULL,				// global work offset
										gwsize,				// global work size
										lwsize,				// local work size
										0,					// num events in wait list
										NULL,				// event wait list*
										NULL				// event
								);

	OrkAssert( err==CL_SUCCESS );
	//dev->UnLock();
}

////////////////////////////////////////////////////////////////////////////////

CLKernel::CLKernel()
	: mProgram(0)
	, mKernel(0)
{
}

////////////////////////////////////////////////////////////////////////////////

CLKernel::~CLKernel()
{
	if( 0!=mProgram )	clReleaseProgram(mProgram);
	if( 0!=mKernel )	clReleaseKernel(mKernel);
}

////////////////////////////////////////////////////////////////////////////////

void CLKernel::Compile( const CLDevice* pdev, SourceBuffer& srcbuf, const char* name )
{
	if( 0!=mProgram )	clReleaseProgram(mProgram);
	if( 0!=mKernel )	clReleaseKernel(mKernel);
	srcbuf.CreateCLprogram( pdev, mProgram, mKernel, name );

	////////////////////////////////////////////////////////////
	// query max workgroup size for kernel on the device
	////////////////////////////////////////////////////////////
	size_t MaxWorkGroupSize = 0;
	int err = clGetKernelWorkGroupInfo(mKernel, pdev->GetDeviceID(), CL_KERNEL_WORK_GROUP_SIZE, sizeof(size_t), &MaxWorkGroupSize, NULL);
	OrkAssert( err==CL_SUCCESS );
	miMaxWorkGroupSize = int(MaxWorkGroupSize);
}

////////////////////////////////////////////////////////////////////////////////

void CLengine::NextDevice()
{
	if( mpCurrentDevice ) mpCurrentDevice->Sync();
	if( mpCurrentDevice ) delete mpCurrentDevice;

	miCurrentDevice = (miCurrentDevice+1)%int(mDevices.size());
	cl_device_id devid = mDevices[miCurrentDevice];

	mpCurrentDevice = new CLDevice(*this);
	//mpCurrentDevice->Init( devid, 0 );
}

////////////////////////////////////////////////////////////////////////////////

void CLDevice::Sync() const
{
	for( int i=0; i<10; i++ )
	{
		clFinish(mCmdQueue); // flush command queue
#if defined(OSX)
		usleep(100000);
		Sleep(10);
#else
#endif
	}
}


////////////////////////////////////////////////////////////////////////////////
/*
int CLKernel::GetMaxWorkgroupSize( const CLDevice* pdev ) const
{
	

	////////////////////////////////////////////////////////////
	// run the kernel over dataset using all possible resources
	////////////////////////////////////////////////////////////

//	int iwiX = 1; //miWidth>>5;		// /32
//	int iwiY = 1; //miHeight>>5;		// /32
								// == 1024 pixels per workitem

//	fcd.mGlobalSizeD2[0] = iwiX;
//	fcd.mGlobalSizeD2[1] = iwiY;
///	fcd.mLocalSizeD2[0] = iwiX>>2;	// 16 LocalItems per ComputeUnit
//	fcd.mLocalSizeD2[1] = iwiY>>2;	// 16K pixels per ComputeUnit ?

//	if( MaxWorkGroupSize==1 )
//	{
//		fcd.mLocalSizeD2[0] = 1;
//		fcd.mLocalSizeD2[1] = 1;
//	}
//	int inuml = fcd.mLocalSizeD2[0]*fcd.mLocalSizeD2[1];
//	int inumg = fcd.mGlobalSizeD2[0]*fcd.mGlobalSizeD2[1];

	printf( "inuml<%d:%d:%d> inumg<%d:%d:%d>\n",
			int(LocalSizeD2[0]),
			int(LocalSizeD2[1]),
			inuml,
			int(GlobalSizeD2[0]),
			int(GlobalSizeD2[1]),
			inumg );

	//////////////////////
	//The number of work-items in each dimension (local_x, local_y, and local_z)
	//	in a single work-group must be less than the values returned for
	//	the device from clGetCLDevice(CL_DEVICE_MAX_WORK_ITEM_SIZES).
	//////////////////////
	//The total number of work-items in a work-group (local_x*local_y*local_z)
	//		must be less than or equal to the value returned by
	//	clGetKernelWorkGroupInfo(CL_KERNEL_WORK_GROUP_SIZE).
//	OrkAssert( inuml<=int(MaxWorkGroupSize) );
	//////////////////////
	//The number of work-items in each dimension in a single work-group
	//		must divide evenly into the total number of work-items in
	//		that dimension (global_n mod local_n = 0).
//	OrkAssert( (fcd.mGlobalSizeD2[0]%fcd.mLocalSizeD2[0]) == 0 );
//	OrkAssert( (fcd.mGlobalSizeD2[1]%fcd.mLocalSizeD2[1]) == 0 );

  printf( "idiv<%d> imod<%d>\n",
			inumg/inuml,
			inumg%inuml );
}
*/
////////////////////////////////////////////////////////////////////////////////
/*
void FXengine::ReadBack(bool breadback)
{
	////////////////////////////////////////////////////////////
	// read back the framebuffer
	////////////////////////////////////////////////////////////


	//	clFinish(thedevice.mCmdQueue); // flush command queue
	//}
	////////////////////////////////////////////////////////////
	// capture
	////////////////////////////////////////////////////////////
	
	if( mbEnableCapture )
	{
		orkprintf( "capture frame<%d>\n", miCaptureFrame );
		int iframe = miCaptureFrame;
		memcpy( (void*) (mpCaptureBuffer+(iframe*miNumPixels)), mpCurrentDevice->mpFrameBufferCur, sizeof(u32)*miNumPixels );

		miCaptureFrame++;

		if( miCaptureFrame==kcaptureframes )
		{
			EndCapture();
			WriteCapture();
		}
	}
}*/

////////////////////////////////////////////////////////////////////////////////

CLDevice::~CLDevice()
{
	if( mCmdQueue ) clReleaseCommandQueue(mCmdQueue);
	if( mContext ) clReleaseContext(mContext);
//	if( mpFrameBufferCur ) delete[] mpFrameBufferCur;
}

////////////////////////////////////////////////////////////////////////////////

CLDevice::CLDevice( CLengine& cle )
	: mContext(0)
	, mCmdQueue(0)
	, mDeviceID(0)
	, mEngine(cle)
	, mMutex( "cldevicelock" )
	, mLock( mMutex )
{
}

///////////////////////////////////////////////////////////////////////////////

void CLDevice::Init( cl_device_id& devid )
{
	mDeviceID = devid;
	////////////////////////////////////////////////////////////
	// create compute-context
	////////////////////////////////////////////////////////////
	int err = 0;
	mContext = clinitContext(0, 1, &mDeviceID, NULL, NULL, &err);
	if(false==mContext)
	{
		OrkAssertI(false,"could not create a compute context\n");
	}

	////////////////////////////////////////////////////////////
	// create commandqueue
	////////////////////////////////////////////////////////////
	mCmdQueue = clCreateCommandQueue(mContext, mDeviceID, 0, &err);
	if(false==mCmdQueue)
	{
		OrkAssertI(false, "could not create command queue\n");
	}

	////////////////////////////////////////////////////////////

	size_t p_size;
	size_t arr_tsize[3];
	size_t ret_size;
	char param[100];
	cl_uint entries;
	cl_ulong long_entries;
	//cl_bool bool_entries;
	cl_device_local_mem_type mem_type;
	cl_device_type dev_type;
	//cl_device_fp_config fp_conf;
	//cl_device_exec_capabilities exec_cap;


	clGetDeviceInfo(mDeviceID,CL_DEVICE_TYPE,sizeof(dev_type),&dev_type,&ret_size);

	//printf("Device Type:\t\t");

	if(dev_type & CL_DEVICE_TYPE_GPU)			mDeviceType = "CL_DEVICE_TYPE_GPU ";
	if(dev_type & CL_DEVICE_TYPE_CPU)			mDeviceType = "CL_DEVICE_TYPE_CPU ";
	if(dev_type & CL_DEVICE_TYPE_ACCELERATOR)		mDeviceType = "CL_DEVICE_TYPE_ACCELERATOR ";
	if(dev_type & CL_DEVICE_TYPE_DEFAULT)		mDeviceType = "CL_DEVICE_TYPE_DEFAULT ";

	clGetDeviceInfo(mDeviceID,CL_DEVICE_NAME,sizeof(param),param,&ret_size);
	mDeviceName = param;

	/*clGetCLDevice(mDeviceID,CL_DEVICE_VENDOR,sizeof(param),param,&ret_size);
	orkprintf("Vendor: \t\t%s\n",param);

	clGetCLDevice(mDeviceID,CL_DEVICE_VENDOR_ID,sizeof(cl_uint),&entries,&ret_size);
	orkprintf("Vendor ID:\t\t%d\n",entries);*/

	clGetDeviceInfo(mDeviceID,CL_DEVICE_VERSION,sizeof(param),param,&ret_size);
	mDeviceVersion = param;

	clGetDeviceInfo(mDeviceID,CL_DEVICE_PROFILE,sizeof(param),param,&ret_size);
	//orkprintf("Profile:\t\t%s\n",param);
	clGetDeviceInfo(mDeviceID,CL_DEVICE_EXECUTION_CAPABILITIES,sizeof(param),param,&ret_size);

	clGetDeviceInfo(mDeviceID,CL_DRIVER_VERSION,sizeof(param),param,&ret_size);
	//orkprintf("Driver: \t\t%s\n",param);

	clGetDeviceInfo(mDeviceID,CL_DEVICE_EXTENSIONS,sizeof(param),param,&ret_size);
	mDeviceExtensions = param;
	printf("Extensions:\t\t%s\n",param);

	clGetDeviceInfo(mDeviceID,CL_DEVICE_MAX_WORK_ITEM_SIZES,3*sizeof(size_t),arr_tsize,&ret_size);
	char stringbuffer[256];
	sprintf_s( stringbuffer,  256, "(%d,%d,%d)", int(arr_tsize[0]),int(arr_tsize[1]),int(arr_tsize[2]) );
	mDeviceMaxWIS = stringbuffer;
	printf("Max Work-Item Sizes:\t(%d,%d,%d)\n",int(arr_tsize[0]),int(arr_tsize[1]),int(arr_tsize[2]));

	

	clGetDeviceInfo(mDeviceID,CL_DEVICE_MAX_WORK_GROUP_SIZE,sizeof(size_t),&p_size,&ret_size);
	sprintf_s( stringbuffer,  256, "%d", int(p_size) );
	mDeviceMaxWGS = stringbuffer;
	//orkprintf("Max Work Group Size:\t%d\n",int(p_size));

	clGetDeviceInfo(mDeviceID,CL_DEVICE_MAX_COMPUTE_UNITS,sizeof(cl_uint),&entries,&ret_size);
	sprintf_s( stringbuffer,  256, "%d", int(entries) );
	mDeviceMaxCU = stringbuffer;
//	orkprintf("Max Compute Units:\t%d\n",entries);

	clGetDeviceInfo(mDeviceID,CL_DEVICE_MAX_CLOCK_FREQUENCY,sizeof(cl_uint),&entries,&ret_size);
	sprintf_s( stringbuffer,  256, "%d MHz", int(entries) );
	mDeviceMaxFRQ = stringbuffer;
	//orkprintf("Max Frequency (Mhz):\t%d\n",entries);

	clGetDeviceInfo(mDeviceID,CL_DEVICE_GLOBAL_MEM_CACHELINE_SIZE,sizeof(cl_uint),&entries,&ret_size);
	printf("Cache Line (bytes):\t%d\n",entries);

	clGetDeviceInfo(mDeviceID,CL_DEVICE_GLOBAL_MEM_SIZE,sizeof(cl_ulong),&long_entries,&ret_size);
	printf("Global Memory (MB):\t%llu\n",long_entries/1024/1024);

	clGetDeviceInfo(mDeviceID,CL_DEVICE_LOCAL_MEM_SIZE,sizeof(cl_ulong),&long_entries,&ret_size);
	printf("Local Memory (MB):\t%llu\n",long_entries/1024/1024);

	clGetDeviceInfo(mDeviceID,CL_DEVICE_LOCAL_MEM_TYPE,sizeof(cl_device_local_mem_type),&mem_type,&ret_size);

	if(mem_type & CL_LOCAL)			printf("Local Memory Type:\tCL_LOCAL\n");
	else if(mem_type & CL_GLOBAL)	printf("Local Memory Type:\tCL_GLOBAL\n");
	else								printf("Local Memory Type:\tUNKNOWN\n");

//	mbDirty = true;
}


////////////////////////////////////////////////////////////////////////////////

ImageBuffer::ImageBuffer()
	: mpBuffer(0)
	, miCurrentSize(0)
{
}

////////////////////////////////////////////////////////////////////////////////

ImageBuffer::~ImageBuffer()
{
	if( 0!=mpBuffer ) delete[] mpBuffer;
}

////////////////////////////////////////////////////////////////////////////////

void ImageBuffer::resize( int isize )
{
	if( miCurrentSize != isize )
	{
		if( 0!=mpBuffer ) delete[] mpBuffer;
		mpBuffer = new u32[ isize ];
		miCurrentSize = isize;
	}
}

////////////////////////////////////////////////////////////////////////////////

SourceBuffer::SourceBuffer()
	: mpSource( 0 )
{
}

////////////////////////////////////////////////////////////////////////////////

SourceBuffer::~SourceBuffer()
{
	if( 0 != mpSource ) delete[] mpSource;
}

////////////////////////////////////////////////////////////////////////////////

void SourceBuffer::Load( const char* filename )
{
	FILE* fin = fopen( filename, "rt" );
	if( fin )
	{
		fseek( fin, 0, SEEK_END );
		int isize = ftell(fin);
		fseek( fin, 0, SEEK_SET );
		char* KernelSource = new char[isize+1];
		for( int i=0; i<=isize; i++ ) KernelSource[i] = 0;
		fread( KernelSource, isize, 1, fin );
		fclose(fin);
		KernelSource[isize] = 0;
		mpSource = KernelSource;
	}
}

////////////////////////////////////////////////////////////////////////////////

Kernel* CLkgroup::GetKernel()
{
	return mpCurrentKernel;
}

////////////////////////////////////////////////////////////////////////////////

void CLkgroup::NextKernel()
{
	miCurrentKernel = (miCurrentKernel+1)%int(mKernels.size());
	mpCurrentKernel = mKernels[miCurrentKernel];
}

////////////////////////////////////////////////////////////////////////////////

void CLkgroup::AddKernel( Kernel* pk )
{
	mKernels.push_back(pk);
	mpCurrentKernel = mKernels[0];
}

////////////////////////////////////////////////////////////////////////////////

/*void CLengine::NextKernel()
{
	if( mpCurrentDevice ) mpCurrentDevice->Sync();

	if( mpCurrentGroup )
	{
		mpCurrentGroup->NextKernel();
		mpCurrentDevice->Dirty();
	}

}*/

////////////////////////////////////////////////////////////////////////////////

void CLengine::NextGroup()
{
	if( mpCurrentDevice ) mpCurrentDevice->Sync();
	//miCurrentGroup = (miCurrentGroup+1)%int(mGroups.size());
	//mpCurrentGroup = mGroups[miCurrentGroup];
//	mpCurrentDevice->Dirty();
}

////////////////////////////////////////////////////////////////////////////////

/*void CLengine::Calculate(bool breadback)
{
	if( 0 == mpCurrentDevice ) return;
//	if( 0 == mpCurrentGroup ) return;
//	if( 0 == GetKernel() ) return;

	miFrame ++;
	///////////////////////////////////
#if defined(WIN32)
	static SYSTEMTIME ltv;
	SYSTEMTIME mtv;
	GetSystemTime(&mtv);
	static SYSTEMTIME btv = mtv;
	int imilli = mtv.wSecond*1000+mtv.wMilliseconds;
	int imilli2 = ltv.wSecond*1000+ltv.wMilliseconds;
	int imilli3 = btv.wSecond*1000+btv.wMilliseconds;
	int deltamsec = imilli-imilli2;
	int deltamsecB = imilli-imilli3;
	ltv = mtv;
#else
	struct timeval mtv;
	static struct timeval ltv;
	void* pnull = 0;
	gettimeofday(&mtv,&pnull);
	long deltamsec = timevaldiff(&ltv,&mtv);
	ltv = mtv;
#endif
	////////////////////////////////////////////////////////////
	mTimeStore[miFrame%kavgsize] = deltamsec;
	float favg = 0.0f;
	for( int i=0; i<32; i++ )
	{
		favg += float( mTimeStore[i] ) / 32.0f;
	}
	mFPS = 1000.0f/favg;
	mMPPS = mFPS*float(miWidth)*float(miHeight)/1000000.0f;
	mMSPS = mMPPS*25.0f;
	mfTime = float(deltamsecB)*0.001f;
	////////////////////////////////////////////////////////////

	//GetKernel()->Execute( mpCurrentDevice );

	mpCurrentDevice->Clean();

}*/

////////////////////////////////////////////////////////////////////////////////

//void CLengine::InitTex()
//{
	/*//void  TexImage2D (  enum  target,  int  level,  int  internalformat,  sizei  width,  sizei  height,  int  border,  enum  format,  enum  type,  void  *data )  ;
	orkprintf( "TexObj<%08x>\n", mTextureObj );
	//glGenFramebuffers(1 & mTextureObj);
	u32* pu32 = new u32[ miNumPixels ];
	for( int i=0; i<miNumPixels; i++ ) pu32[i] = rand()|rand()<<16;
	glEnable( GL_TEXTURE_2D );
	glBindTexture(GL_TEXTURE_2D,mTextureObj);
	//glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, miWidth, miHeight, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, pu32 );
	gluBuild2DMipmaps( GL_TEXTURE_2D, 3, miWidth, miHeight, GL_RGBA, GL_UNSIGNED_BYTE, pu32 );
	delete[] pu32;
	int ierr = 0;
	//mOutput = clCreateFromGLTexture2D(
	//	mContext, //cl_context context,
	//	CL_MEM_READ_ONLY, //CL_MEM_WRITE_ONLY, //cl_mem_flags flags,
	//	GL_TEXTURE_2D,//GLenum target,
	//	0, //GLint miplevel,
	//	mTextureObj, //GLuint texture,
	//	& ierr ); ////int *errcode_ret);
	//OrkAssert( ierr == 0 );*/
//}

///////////////////////////////////////////////////////////////////////////////

//void CLengine::RenderGL(int iw, int ih, int icw, int ich )
//{
//	if( GetKernel() )
//	{
//		GetKernel()->RenderGL(iw,ih,icw,ich);
//	}
//}

//const Kernel* CLengine::GetKernel() const
//{
//	if( mpCurrentGroup )
//	{
//		return mpCurrentGroup->GetKernel();
//	}
//	return 0;
//}

//Kernel* CLengine::GetKernel()
//{
//	if( mpCurrentGroup )
//	{
//		return mpCurrentGroup->GetKernel();
//	}
//	return 0;
//}

///////////////////////////////////////////////////////////////////////////////

CLengine::~CLengine()
{
}

////////////////////////////////////////////////////////////////////////////////

CLengine::CLengine()
	: miCurrentDevice(0)
	, mpCurrentDevice(0)
{
	////////////////////////////////////////////////////////////
	// get a device
	////////////////////////////////////////////////////////////
	u32 platform_count = 0;
	clGetPlatformIDs(0, NULL, &platform_count);//get count
	OrkAssert(platform_count!=0);
	cl_platform_id *ids = new cl_platform_id[platform_count];//alocate array of platforms_id
	clGetPlatformIDs(platform_count, ids, NULL);//get devices

	//for every platform_id
	for( u32 i=0; i<platform_count; i++ )
	{
		cl_uint num_dev;
		clGetDeviceIDs(ids[i], CL_DEVICE_TYPE_ALL, 0, NULL, &num_dev);
		cl_device_id *devices = new cl_device_id[num_dev];
		clGetDeviceIDs(ids[i], CL_DEVICE_TYPE_ALL, num_dev, &devices[0], NULL);

		for( cl_uint id=0; id<num_dev; id++ )
		{
			mDevices.push_back(devices[id]);
			CLDevice* pdev = new CLDevice(*this);
			pdev->Init( devices[id] );
			mpCurrentDevice = pdev;
		}
	}

//	Kernel1* k1a = new Kernel1("kernel1a.cl");
//	Kernel1* k1b = new Kernel1("kernel1b.cl");
//	Kernel1* k1c = new Kernel1("kernel1c.cl");
//	Kernel1* k1d = new Kernel1("kernel1d.cl");
//	Kernel1* k1e = new Kernel1("kernel1e.cl");

//	CLkgroup* g1 = new CLkgroup();

//	g1->AddKernel( k1a );
//	g1->AddKernel(  k1b );
//	g1->AddKernel(  k1c );
//	g1->AddKernel(  k1d );
//	g1->AddKernel(  k1e );

	//NextDevice();

//	Kernel2* k2a = new Kernel2("kernel1a","kernel1","kernel1a");
//	Kernel2* k2b = new Kernel2("kernel1b","kernel1","kernel1b");
//	Kernel2* k2c = new Kernel2("kernel1c","kernel1","kernel1c");
//	Kernel2* k2d = new Kernel2("kernel1d","kernel1","kernel1d");

//	CLkgroup* g2 = new CLkgroup();

//	g2->AddKernel(  k2a );
//	g2->AddKernel(  k2b );
//	g2->AddKernel(  k2c );
//	g2->AddKernel(  k2d );

//	mGroups.push_back(g2);

//	mpCurrentGroup = mGroups[0];

//	Resize(miWidth,miHeight);
}

////////////////////////////////////////////////////////////////////////////////
#endif
