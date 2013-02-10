////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#define _CRT_SECURE_NO_WARNINGS 1
#if 0
#include <CL/cl.h>
//#include <CL/clext.h>
#include <ork/kernel/mutex.h>


///////////////////////////////////////////////////////////////////////////////
class DeviceBase;
class CLDevice;
class ImageBuffer;
class CLengine;

struct FrameContextData
{
	size_t	mGlobalSizeD2[2];
	size_t	mLocalSizeD2[2];
};

///////////////////////////////////////////////////////////////////////////////

struct Kernel
{
	virtual void Execute(DeviceBase* device) = 0;
	Kernel() {}
	std::string mName;
};
class SourceBuffer
{
public:
	SourceBuffer();
	~SourceBuffer();
	void Load( const char* filename );
	const char* GetText() const { return mpSource; }
	void CreateCLprogram(const CLDevice* pdev, cl_program& prg, cl_kernel& krn, const char* kernname );
private:

	const char*			mpSource;
};

class CLKernel
{
public:
	void Enqueue( const CLDevice* dev, int idim, size_t* gwsize, size_t* lwsize ) const;
	CLKernel();
	~CLKernel();
	void Compile( const CLDevice* pdev, SourceBuffer& srcbuf, const char* name );
	cl_program& GetProgram() { return mProgram; }
	cl_kernel& GetKernel() { return mKernel; }
	const cl_program& GetProgram() const { return mProgram; }
	const cl_kernel& GetKernel() const { return mKernel; }
	int GetMaxWorkgroupSize() const { return miMaxWorkGroupSize; }

private:

	cl_program	mProgram;
	cl_kernel	mKernel;
	int			miMaxWorkGroupSize;
};

class CLFromHostBuffer
{
public:
	void resize(int isize,const CLDevice*dev);
	cl_mem& GetHandle() { return mCLhandle; }
	char* GetBufferRegion() const { return GetBufferRegionPriv(); }
	CLFromHostBuffer();
	~CLFromHostBuffer();
	void SetArg(cl_kernel k, int iarg);
	void TransferAndBlock( const CLDevice* pdev, int isize );
	int GetSize() const { return miSize; }
protected:
	char* GetBufferRegionPriv() const { return (miWriteIndex&1)?mpBuffer1:mpBuffer0; }
	cl_mem	mCLhandle;
	int		miSize;
	char*	mpBuffer0;
	char*	mpBuffer1;
	int		miWriteIndex;
};

class CLToHostBuffer
{
public:
	void resize(int isize,const CLDevice*dev);
	cl_mem& GetHandle() { return mCLhandle; }
	const char* GetBufferRegion() const { return GetBufferRegionPriv(); }
	CLToHostBuffer();
	~CLToHostBuffer();
	void SetArg(cl_kernel k, int iarg);
	void Transfer( const CLDevice* pdev, int isize, bool bblock );
	int GetSize() const { return miSize; }
protected:
	char* GetBufferRegionPriv() const { return (miReadIndex&1)?mpBuffer1:mpBuffer0; }
	cl_mem	mCLhandle;
	int		miSize;
	U32		mBufferflags;
	char*	mpBuffer0;
	char*	mpBuffer1;
	int		miReadIndex;
};

class ImageBuffer
{
public:
	ImageBuffer();
	~ImageBuffer();
	void resize( int isize );
	const void* GetData() const { return mpBuffer; }
	void* GetData() { return mpBuffer; }
private:
	void*	mpBuffer;
	int		miCurrentSize;
};

///////////////////////////////////////////////////////////////////////////////
/*
class DeviceBase
{
public:
	///////////////////////////////////
	DeviceBase( CLengine& cle );
	~DeviceBase();
	///////////////////////////////////
	void Clean() { mbDirty=false; }
	void Dirty() { mbDirty=true; }
	///////////////////////////////////
	bool IsDirty() const { return mbDirty; }
	///////////////////////////////////
	virtual void Sync() {}
protected:

	bool				mbDirty;
};*/

///////////////////////////////////////////////////////////////////////////////

class CLDevice //: public DeviceBase
{
public:
	///////////////////////////////////
	CLDevice( CLengine& cle );
	~CLDevice();
	///////////////////////////////////
	void Init( cl_device_id& devid );
	void Sync() const ;// virtual
	///////////////////////////////////
	const cl_context& GetContext() const { return mContext; }
	const cl_command_queue& GetCmdQueue() const { return mCmdQueue; }
	const cl_device_id& GetDeviceID() const { return mDeviceID; }
	///////////////////////////////////
	const CLengine& GetEngine() const { return mEngine; }

	void Lock() const { mLock.Lock(); }
	void UnLock() const { mLock.UnLock(); }

private:

	cl_context 			mContext;
	cl_command_queue 	mCmdQueue;
	cl_device_id 		mDeviceID;
	mutable ork::mutex			mMutex;
	mutable ork::mutex::standard_lock	mLock;

	std::string			mDeviceType;
	std::string			mDeviceName;
	std::string			mDeviceVersion;
	std::string			mDeviceExtensions;
	std::string			mDeviceMaxWGS;
	std::string			mDeviceMaxWIS;
	std::string			mDeviceMaxCU;
	std::string			mDeviceMaxFRQ;
	CLengine&			mEngine;


};

///////////////////////////////////////////////////////////////////////////////

class CLkgroup
{
public:
	CLkgroup() : miCurrentKernel(0), mpCurrentKernel(0) {}
	Kernel* GetKernel();
	void NextKernel();
	void AddKernel( Kernel* pk );
private:
	Kernel*						mpCurrentKernel;
	int							miCurrentKernel;
	std::vector<Kernel*>		mKernels;
};

///////////////////////////////////////////////////////////////////////////////

class CLengine
{
public:

	CLengine();
	~CLengine();

	//void Calculate(bool breadback);
	//void BeginCapture();
//	void EndCapture();
//	void ToggleCapture();
//	void WriteCapture();

	//GLuint	mTextureObj;
//	void InitTex();

//	float GetFPS() const { return mFPS; }
//	float GetMPPS() const { return mMPPS; }
//	float GetMSPS() const { return mMSPS; }
//	float GetTime() const { return mfTime; }

	/////////////////////////////////
	void NextDevice();
	//void NextKernel();
	void NextGroup();
	/////////////////////////////////

	const CLDevice* GetDevice() const { return mpCurrentDevice; }

	//void ReadBack();
	//void EndFrame(bool breadback);

	//void RenderGL(int iw, int ih, int icw, int ich );

	//const Kernel* GetKernel() const;
	//Kernel* GetKernel();

	//void ToggleReadback() { mbReadback=!mbReadback; }
	//bool GetReadback() const { return mbReadback; }

private:

	static const int kcaptureframes = 10;

//	float				mFPS;
//	float				mMPPS;
//	float				mMSPS;
//	float				mfTime;
//	static const int	kavgsize = 32;
//	long				mTimeStore[kavgsize];
//	bool				mbReadback;

//	int					miFrame;
//	int					miCaptureFrame;
//	int 				miWidth;
//	int					miHeight;
//	size_t 				miNumPixels;
//	bool				mbEnableCapture;
//	u32*				mpCaptureBuffer;

	std::vector<cl_device_id>	mDevices;
	//std::vector<CLkgroup*>		mGroups;

	CLDevice*			mpCurrentDevice;
	int					miCurrentDevice;

	//int					miCurrentGroup;
	//CLkgroup*			mpCurrentGroup;


};
#endif
