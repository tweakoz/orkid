////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/orkmath.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include "gl.h"
#include <stdlib.h>
#include <ork/lev2/ui/ui.h>
//#include <ork/lev2/gfx/modeler/modeler_base.h>

////////////////////////////////////////////////////////////////////////////////

static const bool USEVBO = true;
#define USEIBO 0

namespace ork { namespace lev2 {

////////////////////////////////////////////////////////////////////////////////
//	GL Vertex Buffer Implementation
///////////////////////////////////////////////////////////////////////////////

#if !defined(HAVE_MAP_BUFFER_RANGE)&&defined(USE_GL3)
#define HAVE_MAP_BUFFER_RANGE
#endif

enum edynvbopath
{
	EVB_BUFFER_SUBDATA = 0,
#if defined(HAVE_MAP_BUFFER_RANGE)
	EVB_MAP_BUFFER_RANGE,
#endif
#if defined(ORK_OSX)
	EVB_APPLE_FLUSH_RANGE, // seems broken
#endif
};

#if defined(ORK_OSX) && !defined(USE_GL3)
static edynvbopath gDynVboPath = EVB_APPLE_FLUSH_RANGE;
#define glGenVertexArrays glGenVertexArraysAPPLE 
#define glBindVertexArray glBindVertexArrayAPPLE 
#else
static edynvbopath gDynVboPath = EVB_MAP_BUFFER_RANGE;
#endif

void GfxTargetGL::TakeThreadOwnership()
{
	MakeCurrentContext();
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct GlVtxBufMapPool;
struct GlVtxBufMapData;

typedef ork::MpMcBoundedQueue<GlVtxBufMapData*> vtxbufmapdata_q_t;

///////////////////////////////////////////////////////////

struct GlVtxBufMapData
{
	vtxbufmapdata_q_t& mParentQ;
	int mPotSize;
	int mCurSize;
	void* mpData;

	GlVtxBufMapData(vtxbufmapdata_q_t&oq,int potsize)
		: mParentQ(oq)
		, mPotSize(potsize) 
		, mCurSize(0)
	{
		mpData = malloc(potsize);		
	}
	~GlVtxBufMapData()
	{
		free(mpData);
	}
	void ReturnToPool()
	{
		mParentQ.push(this);
	}
};

///////////////////////////////////////////////////////////

struct GlVtxBufMapPool
{
	std::map<int,vtxbufmapdata_q_t> mVbmdMaps;

	GlVtxBufMapData* GetVbmd( int isize )
	{
		GlVtxBufMapData* rval = nullptr;
		int potsize = 1;
		while(potsize<isize) potsize<<=1;
		assert(potsize>=isize);
		std::map<int,vtxbufmapdata_q_t>::iterator it = mVbmdMaps.find(potsize);
		if( it==mVbmdMaps.end() )
		{	vtxbufmapdata_q_t q;
			mVbmdMaps.insert(std::make_pair(potsize,q));
			it = mVbmdMaps.find(potsize);
		}
		vtxbufmapdata_q_t& q = it->second;
		if( false == q.try_pop(rval) )
			rval = new GlVtxBufMapData(q,potsize);
		return rval;
	}
};

///////////////////////////////////////////////////////////

static ork::MpMcBoundedQueue<GlVtxBufMapPool*> gBufMapPool;

static GlVtxBufMapPool* GetBufMapPool()
{
	GlVtxBufMapPool* rval = nullptr;
	if( false == gBufMapPool.try_pop(rval) )
	{
		rval = new GlVtxBufMapPool;
	}
	return rval;
}
static void RetBufMapPool(GlVtxBufMapPool* p)
{
	assert(p!=nullptr);
	gBufMapPool.push(p);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

struct GLIdxBufHandle
{
	u32	mIBO;
	const U16* mBuffer;
	int mNumIndices;
	int mMinIndex;
	int mMaxIndex;

	GLIdxBufHandle() : mIBO(0), mMinIndex(0), mMaxIndex(0), mBuffer(nullptr), mNumIndices(0) {}
};

struct GLVaoHandle
{
	GLuint mVAO;
	
	const GLIdxBufHandle* mIBO;
	bool mInited;

	GLVaoHandle() : mVAO(0), mIBO(nullptr), mInited(false) {}

};

struct GLVtxBufHandle
{
	u32	mVBO;
	long mBufSize;
	int	miLockBase;
	int	miLockCount;
	bool mbSetupSource;
	GlVtxBufMapData* mMappedRegion;

	std::map<size_t,GLVaoHandle*> mVaoMap;

	GLVtxBufHandle() 
		: mVBO(0)
		, mBufSize(0)
		, miLockBase(0)
		, miLockCount(0)
		, mbSetupSource(true)
		, mMappedRegion(nullptr)
	{

	}
	GLVaoHandle* GetVAO(const void*plat_h,const void* vao_key)
	{	GLVaoHandle* rval = nullptr;
		size_t k1 = size_t(plat_h);
		size_t k2= size_t(vao_key);
		size_t key = k1 xor k2;
		const auto& it = mVaoMap.find(key);
		if( it==mVaoMap.end() )
		{
			rval = new GLVaoHandle;
			glGenVertexArrays(1,&rval->mVAO);
			mVaoMap.insert(std::make_pair(key,rval));
			//glBindVertexArray(mVAO);
		}
		else
			rval = it->second;
		return rval;
	}
	GLVaoHandle* BindVao(const void*plat_h,const void* vao_key)
	{
		GLVaoHandle* r = GetVAO(plat_h,vao_key);
		assert(r!=nullptr);
		glBindVertexArray(r->mVAO);
		return r;
	}
	void CreateVbo(VertexBufferBase & VBuf)
	{
		VertexBufferBase * pnonconst = const_cast<VertexBufferBase *>( & VBuf );

		printf( "CreateVBO()\n");

		// Create A VBO and copy data into it
		mVBO = 0;
		glGenBuffers( 1, (GLuint*) & mVBO );
		printf( "CreateVBO:: genvbo<%d>\n", int(mVBO));
		

		//hPB = GLuint(ubh);
		//pnonconst->SetPBHandle( (void*)hPB );
		glBindBuffer( GL_ARRAY_BUFFER, mVBO );
		GL_ERRORCHECK();
		int iVBlen = VBuf.GetVtxSize()*VBuf.GetMax();

		//orkprintf( "CreateVBO<%p> len<%d> ID<%d>\n", & VBuf, iVBlen, int(mVBO) );
		
		bool bSTATIC = VBuf.IsStatic();
		
		static void* gzerobuf = calloc( 64<<20, 1 );
		//glBufferData( GL_ARRAY_BUFFER, iVBlen, bSTATIC ? gzerobuf : 0, bSTATIC ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW );
		glBufferData( GL_ARRAY_BUFFER, iVBlen, gzerobuf, bSTATIC ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW );
		GL_ERRORCHECK();
		
		//////////////////////////////////////////////
		// we always update dynamic VBOs sequentially
		//  we also dont want to pay the cost of copying any data
		//  so we will use a VBO map range extension
		//  either GL_ARB_map_buffer_range or GL_APPLE_flush_buffer_range
		 
		if( false == bSTATIC ) switch( gDynVboPath )
		{
			case EVB_BUFFER_SUBDATA:
				break;
#if defined(HAVE_MAP_BUFFER_RANGE)
			case EVB_MAP_BUFFER_RANGE:
			{	
				/*
				GLuint gl_BUFFER_SERIALIZED_MODIFY_APPLE = 0x8A12;
        		GLuint gl_BUFFER_FLUSHING_UNMAP_APPLE = 0x8A13;
				glBufferParameteriAPPLE(GL_ARRAY_BUFFER, gl_BUFFER_SERIALIZED_MODIFY_APPLE, GL_FALSE);
				GL_ERRORCHECK();
				glBufferParameteriAPPLE(GL_ARRAY_BUFFER, gl_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
				GL_ERRORCHECK();
				*/
				break;
			}
#elif defined(ORK_OSX)
			case EVB_APPLE_FLUSH_RANGE:
			{	GLuint gl_BUFFER_SERIALIZED_MODIFY_APPLE = 0x8A12;
        		GLuint gl_BUFFER_FLUSHING_UNMAP_APPLE = 0x8A13;
				glBufferParameteriAPPLE(GL_ARRAY_BUFFER, gl_BUFFER_SERIALIZED_MODIFY_APPLE, GL_FALSE);
				GL_ERRORCHECK();
				glBufferParameteriAPPLE(GL_ARRAY_BUFFER, gl_BUFFER_FLUSHING_UNMAP_APPLE, GL_FALSE);
				GL_ERRORCHECK();
				break;
			}
#endif
		}

		//////////////////////////////////////////////

		GLint ibufsize = 0;
		glGetBufferParameteriv( GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &ibufsize );
		GL_ERRORCHECK();
		mBufSize = ibufsize;

		OrkAssert(mBufSize>0);
	}
};

///////////////////////////////////////////////////////////////////////////////

GlGeometryBufferInterface::GlGeometryBufferInterface( GfxTargetGL& target )
	: mTargetGL(target)
	, mLastComponentMask(0)
{
}

static void ClearVao()
{
	glBindVertexArray(0);
}

///////////////////////////////////////////////////////////////////////////////

void* GlGeometryBufferInterface::LockVB( VertexBufferBase & VBuf, int ibase, int icount )
{
	mTargetGL.MakeCurrentContext();


	OrkAssert( false == VBuf.IsLocked() );

	void* rVal = 0;
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());

	//////////////////////////////////////////////////////////
	// create the vbo ?
	//////////////////////////////////////////////////////////
	if( 0 == hBuf )
	{
		hBuf = new GLVtxBufHandle;
		VBuf.SetHandle( reinterpret_cast<void*> (hBuf) );

		hBuf->CreateVbo( VBuf );
	}

	int iMax = VBuf.GetMax();

	int ibasebytes = ibase*VBuf.GetVtxSize();
	int isizebytes = icount*VBuf.GetVtxSize();

	ClearVao();

	//////////////////////////////////////////////////////////
	// bind the vbo
	//////////////////////////////////////////////////////////

	GL_ERRORCHECK();
	glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
	GL_ERRORCHECK();

	if( VBuf.IsStatic() )
	{
		if( isizebytes )
		{
			rVal = glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY );
			OrkAssert( rVal );
		}
	}
	else
	{
		//printf( "LOCKVB<WRITE> VB<%p> vboid<%d> ibase<%d> icount<%d> isizebytes<%d> mBufSize<%d>\n", & VBuf, int(hBuf->mVBO), ibase, icount, isizebytes, int(hBuf->mBufSize) );
		//////////////////////////////////////////////
		// we always update dynamic VBOs sequentially (no overrwrite)
		//  we also dont want to pay the cost of copying any data
		//  so we will use a VBO map range extension
		//  either GL_ARB_map_buffer_range or GL_APPLE_flush_buffer_range
		//////////////////////////////////////////////
		OrkAssert(isizebytes);

		if( isizebytes )
		{
			hBuf->miLockBase = ibase;
			hBuf->miLockCount = icount;

			switch( gDynVboPath )
			{
#if defined(HAVE_MAP_BUFFER_RANGE)
				case EVB_MAP_BUFFER_RANGE:
					//rVal = glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY|GL_MAP_UNSYNCHRONIZED_BIT|GL_MAP_FLUSH_EXPLICIT_BIT ); // MAP_UNSYNCHRONIZED_BIT?
					rVal = glMapBufferRange( GL_ARRAY_BUFFER, ibasebytes, isizebytes, GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_RANGE_BIT|GL_MAP_UNSYNCHRONIZED_BIT|GL_MAP_FLUSH_EXPLICIT_BIT); // MAP_UNSYNCHRONIZED_BIT?
					//rVal = glMapBufferRange( GL_ARRAY_BUFFER, ibasebytes, isizebytes, GL_MAP_WRITE_BIT ); // MAP_UNSYNCHRONIZED_BIT?
					GL_ERRORCHECK();
					OrkAssert( rVal );
					//rVal = (void*) (((char*)rVal)+ibasebytes);
					break;
#elif defined(ORK_OSX)
				case EVB_APPLE_FLUSH_RANGE:
					rVal = glMapBuffer( GL_ARRAY_BUFFER, GL_WRITE_ONLY );
					GL_ERRORCHECK();
					OrkAssert( rVal );
					rVal = (void*) (((char*)rVal)+ibasebytes);
					break;
#endif
				case EVB_BUFFER_SUBDATA:
				{	GlVtxBufMapPool* pool = GetBufMapPool();
					hBuf->mMappedRegion = pool->GetVbmd(isizebytes);
					assert(hBuf->mMappedRegion!=nullptr);
					rVal = hBuf->mMappedRegion->mpData;
					RetBufMapPool(pool);
					break;
				}
			}

		}
	}

	//////////////////////////////////////////////////////////
	// boilerplate stuff all devices do
	//////////////////////////////////////////////////////////

	VBuf.Lock();

	//////////////////////////////////////////////////////////

	return rVal;
}

///////////////////////////////////////////////////////////////////////////////

const void* GlGeometryBufferInterface::LockVB( const VertexBufferBase & VBuf, int ibase, int icount )
{
	OrkAssert( false == VBuf.IsLocked() );

	void* rVal = 0;
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());

	OrkAssert( hBuf != 0 );
	
	int iMax = VBuf.GetMax();

	if( icount == 0 )
	{
		icount = VBuf.GetMax();
	}
	int ibasebytes = ibase*VBuf.GetVtxSize();
	int isizebytes = icount*VBuf.GetVtxSize();

	OrkAssert(isizebytes);

	//printf( "ibasebytes<%d> isizebytes<%d> icount<%d> \n", ibasebytes, isizebytes, icount );

	ClearVao();

	//////////////////////////////////////////////////////////
	// bind the vbo
	//////////////////////////////////////////////////////////

	GL_ERRORCHECK();
	glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
	GL_ERRORCHECK();

	//////////////////////////////////////////////////////////

	if( isizebytes )
	{
#if defined(HAVE_MAP_BUFFER_RANGE)
		rVal = glMapBufferRange( GL_ARRAY_BUFFER, ibasebytes, isizebytes, GL_MAP_READ_BIT); // MAP_UNSYNCHRONIZED_BIT?
		OrkAssert( rVal );
		OrkAssert( rVal!=(void*)0xffffffff );
		hBuf->miLockBase = ibase;
		hBuf->miLockCount = icount;
#else
		rVal = glMapBuffer( GL_ARRAY_BUFFER, GL_READ_ONLY );
		OrkAssert( rVal );
		rVal = (void*) (((char*)rVal)+ibasebytes);
		hBuf->miLockBase = ibasebytes;
		hBuf->miLockCount = isizebytes;
	#endif

	}
	
	GL_ERRORCHECK();

	//////////////////////////////////////////////////////////

	VBuf.Lock();

	return rVal;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockVB( VertexBufferBase& VBuf )
{
	OrkAssert( VBuf.IsLocked() );
	
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
		
	if( VBuf.IsStatic() )
	{
		GL_ERRORCHECK();

		glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
		glUnmapBuffer( GL_ARRAY_BUFFER );
		glBindBuffer( GL_ARRAY_BUFFER, 0 );
		
		VBuf.Unlock();
	}
	else
	{		
		int basebytes = VBuf.GetVtxSize()*hBuf->miLockBase;
		int countbytes = VBuf.GetVtxSize()*hBuf->miLockCount;
		
		//printf( "UNLOCK VB<%p> base<%d> count<%d>\n", & VBuf, basebytes, countbytes );
		
		GL_ERRORCHECK();
		glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
		GL_ERRORCHECK();

		switch( gDynVboPath )
		{
#if defined(HAVE_MAP_BUFFER_RANGE)
			case EVB_MAP_BUFFER_RANGE:
				GL_ERRORCHECK();
				glFlushMappedBufferRange(GL_ARRAY_BUFFER, (GLintptr)0, countbytes);
				glUnmapBuffer( GL_ARRAY_BUFFER );

				break;
#endif
#if defined(ORK_OSX)
			case EVB_APPLE_FLUSH_RANGE:
				glFlushMappedBufferRangeAPPLE(GL_ARRAY_BUFFER, (GLintptr)(basebytes), countbytes);
				GL_ERRORCHECK();
				glUnmapBuffer( GL_ARRAY_BUFFER );
				break;
#endif
			case EVB_BUFFER_SUBDATA:
			{	assert(hBuf->mMappedRegion!=nullptr);
				glBufferSubData( GL_ARRAY_BUFFER,basebytes,countbytes,hBuf->mMappedRegion->mpData);
				hBuf->mMappedRegion->ReturnToPool();
				break;
			}
		}

		GL_ERRORCHECK();
		glBindBuffer( GL_ARRAY_BUFFER, 0 );
		GL_ERRORCHECK();

		VBuf.Unlock();
	}
	ClearVao();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockVB( const VertexBufferBase& VBuf )
{
	OrkAssert( VBuf.IsLocked() );
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
	GL_ERRORCHECK();

	GL_ERRORCHECK();
	glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
	GL_ERRORCHECK();
	glUnmapBuffer( GL_ARRAY_BUFFER );
	GL_ERRORCHECK();
	glBindBuffer( GL_ARRAY_BUFFER, 0 );
	GL_ERRORCHECK();
	VBuf.Unlock();

	ClearVao();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::ReleaseVB( VertexBufferBase& VBuf )
{
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
	
	if( hBuf )
	{
		GL_ERRORCHECK();
		glDeleteBuffers( 1, (GLuint*) & hBuf->mVBO );
		GL_ERRORCHECK();
	}
}

///////////////////////////////////////////////////////////////////////////////
#if defined(_USE_GLSLFX)
///////////////////////////////////////////////////////////////////////////////
struct vtx_config
{
	const std::string 		mName;
	const int         		mNumComponents;
	const GLenum      		mType;
	const bool        		mNormalize;
	const int         		mOffset;
	const GlslFxPass*		mPass;
	GlslFxAttribute* 		mAttr;

	uint32_t bind_to_attr(const GlslFxPass* pfxpass, int istride)
	{
		uint32_t rval = 0;
		if( mPass != pfxpass )
		{
			const auto& it = pfxpass->mAttributes.find(mName);
			if(it!=pfxpass->mAttributes.end())
			{
				mAttr = it->second;
			}
			else
			{
				printf( "gbi::bind_attr attr<%s> istride<%d> not found!\n", mName.c_str(), istride );
			}
			mPass = pfxpass;
		}
		if( mAttr )
		{	
			//printf( "gbi::bind_attr istride<%d> loc<%d> numc<%d> offs<%d>\n", istride, mAttr->mLocation, mNumComponents, mOffset );
			glVertexAttribPointer(	mAttr->mLocation, 
									mNumComponents,
									mType,
									mNormalize,
									istride,
									(void*) mOffset );	
			rval = 1<<mAttr->mLocation;
		}
		else
		{
			//printf( "gbi::bind_attr no_attr\n" );
		}
		return rval;
	}
	static void enable_attrs(const uint32_t cmask)
	{	for( int iloc=0; iloc<5; iloc++ )
		{	uint32_t tmask = 1<<iloc;

			bool bena = (tmask&cmask);

			//printf( "gbi::enable_attrs iloc<%d> : %d\n", iloc, int(bena) );


			if( bena )
				glEnableVertexAttribArray( iloc );
			else
				glDisableVertexAttribArray( iloc );
		}
	}
};
#endif // _USE_GLSLFX
///////////////////////////////////////////////////////////////////////////////

static bool EnableVtxBufComponents(const VertexBufferBase& VBuf,const svarp_t priv_data)
{
	printf( "EnableVtxBufComponents\n");
	bool rval = false;
	//////////////////////////////////////////////
	auto pfxpass = priv_data.Get<const GlslFxPass*>();
	//////////////////////////////////////////////
	#if defined(WIN32)
	const GLenum kGLVTXCOLS = GL_BGRA;
	#else
	const GLenum kGLVTXCOLS = 4;
	#endif
	//////////////////////////////////////////////
	uint32_t component_mask = 0;
	//////////////////////////////////////////////
	EVtxStreamFormat eStrFmt = VBuf.GetStreamFormat();
	int iStride = VBuf.GetVtxSize();
	//////////////////////////////////////////////
	GL_ERRORCHECK();
	switch( eStrFmt )
	{
		case lev2::EVTXSTREAMFMT_V12N12B12T16:
		{
			/*
			// VNB
			if( 0 )
			{
				GL_ERRORCHECK();
				glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, iStride, (void*) 0 );
				GL_ERRORCHECK();
				glEnableVertexAttribArray( 0 );
				GL_ERRORCHECK();
			}
			else
			{

				GL_ERRORCHECK();
				glEnableClientState( GL_VERTEX_ARRAY );
				glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );

				glNormalPointer( GL_FLOAT, iStride, (void*) 12 );	
				glEnableClientState( GL_NORMAL_ARRAY );
				glClientActiveTextureARB(GL_TEXTURE2);
				glEnableClientState( GL_TEXTURE_COORD_ARRAY );
				glTexCoordPointer( 3, GL_FLOAT,	iStride, (void*) 24 );	// T8
				// UV01
				glClientActiveTextureARB(GL_TEXTURE0);
				glEnableClientState( GL_TEXTURE_COORD_ARRAY );
				glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 36 );	// T8
				glClientActiveTextureARB(GL_TEXTURE1);
				glEnableClientState( GL_TEXTURE_COORD_ARRAY );
				glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 48 );	// T8

				glDisableClientState( GL_COLOR_ARRAY );
				glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			}
			rval = true;*/
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12B12T8I4W4:
		{	/*
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );
			glEnableClientState( GL_VERTEX_ARRAY );
			glNormalPointer( GL_FLOAT, iStride, (void*) 12 );	
			glEnableClientState( GL_NORMAL_ARRAY );
			//glBinormalPointerARB( GL_FLOAT, iStride, (void*) 24 );	
			glClientActiveTextureARB(GL_TEXTURE1);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 3, GL_FLOAT,	iStride, (void*) 24 );	// T8
			//glEnableClientState( GL_BINORMAL_ARRAY_ARB );

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 36 );	// T8

			glDisableClientState( GL_COLOR_ARRAY );
			//glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 40 );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;*/
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12T8I4W4:
		{	
			static vtx_config cfgs[] = 
			{	{"POSITION",	3,	GL_FLOAT,			false,	0,		0,0},
				{"NORMAL",		3,	GL_FLOAT,			true,	12,		0,0},
				{"TEXCOORD0",	2,	GL_FLOAT,			false,	24,		0,0},
				{"BONEINDICES",	4,	GL_UNSIGNED_BYTE,	true,	32,		0,0},
				{"BONEWEIGHTS",	4,	GL_UNSIGNED_BYTE,	true,	36,		0,0},
			};
			for( vtx_config& vcfg : cfgs )
				component_mask |= vcfg.bind_to_attr(pfxpass,iStride);
			rval = true;
			break;
		}
		case EVTXSTREAMFMT_V12N12B12T8C4:
		{	/*
			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );
			glNormalPointer( GL_FLOAT, iStride, (void*) 12 );	
			glClientActiveTextureARB(GL_TEXTURE1); // binormals
			glTexCoordPointer( 3, GL_FLOAT,	iStride, (void*) 24 );	// T8
			glClientActiveTextureARB(GL_TEXTURE0); // texture UV
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 36 );	// T8

			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 44 );
			GL_ERRORCHECK();

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE1);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_COLOR_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );

			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;*/
			break;
		}
		case EVTXSTREAMFMT_V12C4N6I2T8:
		{	/*

			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );	// V12
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 12 );	// C4
			glNormalPointer( GL_SHORT, iStride, (void*) 16 );	// N6
			GL_ERRORCHECK();

			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_COLOR_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );

			glClientActiveTextureARB(GL_TEXTURE0);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;*/
			break;
		}
		case EVTXSTREAMFMT_V12I4N12T8:
		{	/*
			glClientActiveTextureARB(GL_TEXTURE0);
			glTexCoordPointer( 2, GL_FLOAT,	iStride, (void*) 28 );	// T8

			glVertexPointer( 3, GL_FLOAT, iStride, (void*) 0 );	// V12
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 12 );	// C4
			glNormalPointer( GL_FLOAT, iStride, (void*) 16 );	// N6
			GL_ERRORCHECK();

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_COLOR_ARRAY );
			glEnableClientState( GL_NORMAL_ARRAY );

			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			rval = true;*/
			break;
		}
		case lev2::EVTXSTREAMFMT_V12N12T16C4:
		{	
			static vtx_config cfgs[] = 
			{	{"POSITION",	3,	GL_FLOAT,			false,	0,		0,0},
				{"NORMAL",		3,	GL_FLOAT,			true,	12,		0,0},
				{"TEXCOORD0",	2,	GL_FLOAT,			false,	24,		0,0},
				{"TEXCOORD1",	2,	GL_FLOAT,			false,	32,		0,0},
				{"COLOR0",		4,	GL_UNSIGNED_BYTE,	true,	40,		0,0},
			};
			for( vtx_config& vcfg : cfgs )
				component_mask |= vcfg.bind_to_attr(pfxpass,iStride);
			rval = true;
			break;
		}
		case EVTXSTREAMFMT_V12C4T16:
		{
			static vtx_config cfgs[] = 
			{	{"POSITION",	3,	GL_FLOAT,			false,	0,		0,0},
				{"COLOR0",		4,	GL_UNSIGNED_BYTE,	true,	12,		0,0},
				{"TEXCOORD0",	2,	GL_FLOAT,			false,	16,		0,0},
				{"TEXCOORD1",	2,	GL_FLOAT,			false,	24,		0,0}
			};
			for( vtx_config& vcfg : cfgs )
				component_mask |= vcfg.bind_to_attr(pfxpass,iStride);
			rval = true;
			break;
		}
		case EVTXSTREAMFMT_V4C4:
		{
			/*glVertexPointer( 2, GL_SHORT, iStride, (void*) 0 );
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 4);
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_COLOR_ARRAY );

			glClientActiveTextureARB(GL_TEXTURE0);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_NORMAL_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );*/
			rval = false;

			break;
		}
		case EVTXSTREAMFMT_V4T4:
		{
			/*glVertexPointer( 2, GL_SHORT, iStride, (void*) 0 );
			glClientActiveTextureARB(GL_TEXTURE0);
			glTexCoordPointer( 2, GL_SHORT,	iStride, (void*) 4 );

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glEnableClientState( GL_VERTEX_ARRAY );

			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_NORMAL_ARRAY );
			glDisableClientState( GL_COLOR_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );*/
			rval = false;


			break;
		}
		case EVTXSTREAMFMT_V4T4C4:
		{
			/*glVertexPointer( 2, GL_SHORT, iStride, (void*) 0 );
			glClientActiveTextureARB(GL_TEXTURE0);
			glTexCoordPointer( 2, GL_SHORT,	iStride, (void*) 4 );
			glColorPointer( 4, GL_UNSIGNED_BYTE, iStride, (void*) 8 );

			glClientActiveTextureARB(GL_TEXTURE0);
			glEnableClientState( GL_TEXTURE_COORD_ARRAY );
			glEnableClientState( GL_VERTEX_ARRAY );
			glEnableClientState( GL_COLOR_ARRAY );
			
			glClientActiveTextureARB(GL_TEXTURE1);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );
			glDisableClientState( GL_NORMAL_ARRAY );
			glDisableClientState( GL_SECONDARY_COLOR_ARRAY );
			glClientActiveTextureARB(GL_TEXTURE2);
			glDisableClientState( GL_TEXTURE_COORD_ARRAY );*/
			rval = false;

			break;
		}

		default:
		{	OrkAssert(false);
			break;
		}
	}
	GL_ERRORCHECK();
	//////////////////////////////////////////////
	#if defined(USE_GL3)
	vtx_config::enable_attrs(component_mask);
	#endif
	//////////////////////////////////////////////
	if( false == rval )
	{
		printf( "unhandled vtxfmt<%d>\n", int(eStrFmt) );
	}

	assert(rval);
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool GlGeometryBufferInterface::BindVertexStreamSource( const VertexBufferBase& VBuf )
{
	svarp_t evb_priv;
	////////////////////////////////////////////////////////////////////
	GlslFxInterface* pFXI = static_cast<GlslFxInterface*>(mTargetGL.FXI());
	const GlslFxPass* pfxpass = pFXI->GetActiveEffect()->mActivePass;
	OrkAssert( pfxpass!=nullptr );
	evb_priv.Set<const GlslFxPass*>(pfxpass);
	////////////////////////////////////////////////////////////////////
	// setup VBO or DL
	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
	OrkAssert( hBuf );
	GL_ERRORCHECK();

	void* plat_h = mTargetGL.GetPlatformHandle();
	auto vao_key = (void*) pfxpass;

	GLVaoHandle* vao_obj = hBuf->BindVao(plat_h,vao_key);

	bool rval = true;

	if( vao_obj->mInited == false )
	{
		vao_obj->mInited = true;
		glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
		//printf( "VBO<%d>\n", int(hBuf->mVBO) );
		GL_ERRORCHECK();
		//////////////////////////////////////////////
		rval = EnableVtxBufComponents(VBuf,evb_priv);
		//////////////////////////////////////////////	
		hBuf->mbSetupSource = true;
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

bool GlGeometryBufferInterface::BindStreamSources( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf )
{
	bool rval = false;

	////////////////////////////////////////////////////////////////////

	svarp_t evb_priv;

	GlslFxInterface* pFXI = static_cast<GlslFxInterface*>(mTargetGL.FXI());
	const GlslFxPass* pfxpass = pFXI->GetActiveEffect()->mActivePass;
	OrkAssert( pfxpass!=nullptr );
	evb_priv.Set<const GlslFxPass*>(pfxpass);

	////////////////////////////////////////////////////////////////////

	GLVtxBufHandle* hBuf = reinterpret_cast<GLVtxBufHandle*>(VBuf.GetHandle());
	OrkAssert( hBuf );
	GL_ERRORCHECK();

	void* plat_h = mTargetGL.GetPlatformHandle();

	const auto ph = (const GLIdxBufHandle*) IdxBuf.GetHandle();
	OrkAssert( ph!=0 );

	size_t k1 = size_t(ph);
	size_t k2 = size_t(pfxpass);
	auto vao_key = (void*)(k1 xor k2);

	GLVaoHandle* vao_container = hBuf->BindVao(plat_h, vao_key );

	//printf( "vao_container<%p> ibo<%p>\n", vao_container, vao_container->mIBO );

	assert(vao_container!=nullptr);

	if( nullptr == vao_container->mIBO )
	{
		glBindBuffer( GL_ARRAY_BUFFER, hBuf->mVBO );
		//printf( "VBO<%d>\n", int(hBuf->mVBO) );
		GL_ERRORCHECK();
		//////////////////////////////////////////////
		rval = EnableVtxBufComponents(VBuf,evb_priv);
		GL_ERRORCHECK();
		//////////////////////////////////////////////	
		assert(ph->mIBO!=0);
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ph->mIBO );
		vao_container->mIBO = ph;
		GL_ERRORCHECK();
	}
	else
	{
		rval = true;
		//GLuint ibo = it->second;
		//glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ibo );
	}

	//#error assign idxbuf to VAO here
	//////////////////////////////////////////////	
	hBuf->mbSetupSource = true;
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::DrawPrimitive( const VertexBufferBase& VBuf, EPrimitiveType eTyp, int ivbase, int ivcount)
{
	int imax = VBuf.GetMax();

	GL_ERRORCHECK();
	if( imax )
	{
		int inumpasses = mTargetGL.GetCurMaterial()->BeginBlock(&mTargetGL);

		for( int ipass=0; ipass<inumpasses; ipass++ )
		{
			bool bDRAW = mTargetGL.GetCurMaterial()->BeginPass( &mTargetGL,ipass );

			if( bDRAW )
			{
				static bool lbwire = false;

				if( EPRIM_NONE == eTyp )
				{	
					eTyp = VBuf.GetPrimType();
				}

				DrawPrimitiveEML( VBuf, eTyp, ivbase, ivcount );

				mTargetGL.GetCurMaterial()->EndPass(&mTargetGL);
			}

		}
		
		mTargetGL.GetCurMaterial()->EndBlock(&mTargetGL);

	}

	GL_ERRORCHECK();

}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::DrawIndexedPrimitive( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf , EPrimitiveType eType, int ivbase, int ivcount)
{
	int imax = VBuf.GetMax();

	GL_ERRORCHECK();
	if( imax )
	{
		int inumpasses = mTargetGL.GetCurMaterial()->BeginBlock(&mTargetGL);

		for( int ipass=0; ipass<inumpasses; ipass++ )
		{
			bool bDRAW = mTargetGL.GetCurMaterial()->BeginPass( &mTargetGL,ipass );

			if( bDRAW )
			{
				static bool lbwire = false;

				if( EPRIM_NONE == eType )
				{	
					eType = VBuf.GetPrimType();
				}

				DrawIndexedPrimitiveEML( VBuf, IdxBuf, eType, ivbase, ivcount );

				mTargetGL.GetCurMaterial()->EndPass(&mTargetGL);
			}

		}
		
		mTargetGL.GetCurMaterial()->EndBlock(&mTargetGL);

	}

	GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::DrawPrimitiveEML( const VertexBufferBase& VBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
	////////////////////////////////////////////////////////////////////
	GL_ERRORCHECK();
	bool bOK = BindVertexStreamSource( VBuf );
	if( false == bOK ) return;
	////////////////////////////////////////////////////////////////////

	//glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );

	int inum = (ivcount==0) ? VBuf.GetNumVertices() : ivcount;

	if( eType == EPRIM_NONE )
	{
		eType = VBuf.GetPrimType();
	}
	if( inum )
	{
		GL_ERRORCHECK();
		switch( eType )
		{
			case EPRIM_LINES:
			{	//orkprintf( "drawarrays: <ivbase %d> <inum %d> lines\n", ivbase, inum );
				glDrawArrays( GL_LINES, ivbase, inum );
				break;
			}
			case EPRIM_QUADS:
				//orkprintf( "drawarrays: %d quads\n", inum );
				//GL_ERRORCHECK();
				//glDrawArrays( GL_QUADS, ivbase, inum );
				//GL_ERRORCHECK();
				//miTrianglesRendered += (inum/2);
				break;
			case EPRIM_TRIANGLES:
				//printf( "drawarrays: <ivbase %d> <inum %d> tris\n", ivbase, inum );
				glDrawArrays( GL_TRIANGLES, ivbase, inum );
				miTrianglesRendered += (inum/3);
				break;
			case EPRIM_TRIANGLESTRIP:
				//printf( "drawarrays: <ivbase %d> <inum %d> tristrip\n", ivbase, inum );
				glDrawArrays( GL_TRIANGLE_STRIP, ivbase, inum );
				miTrianglesRendered += (inum-2);
				break;
/*			case EPRIM_POINTS:
				GL_ERRORCHECK();
				glDrawArrays( GL_POINTS, 0, iNum );
				GL_ERRORCHECK();
				break;
			case EPRIM_POINTSPRITES:
				GL_ERRORCHECK();
				glPointSize( mTargetGL.GetCurMaterial()->mfParticleSize );
											
				glEnable( GL_POINT_SPRITE_ARB );
				glDrawArrays( GL_POINTS, 0, iNum );
				glDisable( GL_POINT_SPRITE_ARB );

				GL_ERRORCHECK();
				break;
			default:
				glDrawArrays( GL_POINTS, 0, iNum );
				//OrkAssert( false );
				break;*/
			default:
				break;
		}
		GL_ERRORCHECK();
	}
	GL_ERRORCHECK();
}

///////////////////////////////////////////////////////////////////////////////
//epass thru

void GlGeometryBufferInterface::DrawIndexedPrimitiveEML( const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType, int ivbase, int ivcount)
{
	GL_ERRORCHECK();
	////////////////////////////////////////////////////////////////////

	BindStreamSources( VBuf, IdxBuf );	

	int iNum = IdxBuf.GetNumIndices();

	auto plat_handle = static_cast<const GLIdxBufHandle*>( IdxBuf.GetHandle() );

	int imin = plat_handle->mMinIndex;
	int imax = plat_handle->mMaxIndex;

	//GLint maxidx = 0;
	//glGetIntegerv( GL_MAX_ELEMENTS_INDICES, & maxidx );
	//printf( "maxidx<%d>\n", maxidx );

	//glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );

	if( iNum )
	{
		GL_ERRORCHECK();
		switch( eType )
		{
			case EPRIM_LINES:
			{	//orkprintf( "drawarrays: %d lines\n", iNum );
				glDrawElements( GL_LINES, iNum, GL_UNSIGNED_SHORT, nullptr );
				break;
			}
			case EPRIM_QUADS:
				//GL_ERRORCHECK();
				//glDrawElements( GL_QUADS, iNum, GL_UNSIGNED_SHORT, nullptr );
				//GL_ERRORCHECK();
				//miTrianglesRendered += (iNum/2);
				break;
			case EPRIM_TRIANGLES:
				//printf( "drawindexedtris inum<%d> imin<%d> imax<%d>\n", iNum/3, imin, imax );
				glDrawRangeElements( GL_TRIANGLES, imin, imax, iNum, GL_UNSIGNED_SHORT, nullptr );
				miTrianglesRendered += (iNum/3);
				break;
			case EPRIM_TRIANGLESTRIP:
				//printf( "drawindexedtristrip inum<%d>\n", iNum-2 );
				//glDrawElements( GL_TRIANGLE_STRIP, iNum, GL_UNSIGNED_SHORT, nullptr );
				glDrawRangeElements( GL_TRIANGLE_STRIP, imin, imax, iNum, GL_UNSIGNED_SHORT, nullptr );
				miTrianglesRendered += (iNum-2);
				break;
			case EPRIM_POINTS:
				glDrawElements( GL_POINTS, iNum, GL_UNSIGNED_SHORT, nullptr );
				break;
			case EPRIM_POINTSPRITES:
				//GL_ERRORCHECK();
				//glPointSize( mTargetGL.GetCurMaterial()->mfParticleSize );
	//GL_ERRORCHECK();
											
				//glEnable( GL_POINT_SPRITE_ARB );
//	GL_ERRORCHECK();
//				glDrawElements( GL_POINTS, iNum, GL_UNSIGNED_SHORT, pindices );
//	GL_ERRORCHECK();
//				glDisable( GL_POINT_SPRITE_ARB );
//	GL_ERRORCHECK();

//				GL_ERRORCHECK();
				break;
			default:
				//glDrawArrays( GL_POINTS, 0, iNum );
				OrkAssert( false );
				break;
		}
		GL_ERRORCHECK();
	}
	GL_ERRORCHECK();
	//glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
}

///////////////////////////////////////////////////////////////////////////////

//u32	mIBO;
//const U16* mBuffer;
//int mNumIndices;

void* GlGeometryBufferInterface::LockIB( IndexBufferBase& IdxBuf, int ibase, int icount )
{
	void* rval = nullptr;
	assert(ibase==0);
	assert(icount==0);

	if( icount == 0 )
		icount = IdxBuf.GetNumIndices();


	if( nullptr == IdxBuf.GetHandle() )
	{

		GLIdxBufHandle* plat_handle = new GLIdxBufHandle;

		auto p16 = new U16[icount];
		plat_handle->mBuffer = p16;
		rval = (void*) p16;
		plat_handle->mNumIndices = icount;

		IdxBuf.SetHandle( (void*) plat_handle );

	}
	else
	{
		assert(false);
	}
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockIB( IndexBufferBase& IdxBuf)
{
	auto plat_handle = (GLIdxBufHandle*) IdxBuf.GetHandle();
	assert(plat_handle!=nullptr);
	{
		const void* src_data = plat_handle->mBuffer;
		int iblen = plat_handle->mNumIndices*sizeof(U16);

		printf( "UNLOCKIBO\n");
		glGenBuffers( 1, (GLuint*) & plat_handle->mIBO );
		glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, plat_handle->mIBO );
		glBufferData( GL_ELEMENT_ARRAY_BUFFER, iblen, src_data, GL_STATIC_DRAW );

		auto p16 = static_cast<const uint16_t*>(src_data);

		uint16_t umin = 65535;
		uint16_t umax = 0;
		for( int i=0; i<plat_handle->mNumIndices; i++ )
		{
			uint16_t u = p16[i];
			if( u>umax ) umax=u;
			if( u<umin ) umin=u;
		}
		plat_handle->mMinIndex = int(umin);
		plat_handle->mMaxIndex = int(umax);

		printf( "umin<%d> umax<%d>\n", int(umin), int(umax) );
		//glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
	}
}


///////////////////////////////////////////////////////////////////////////////

const void* GlGeometryBufferInterface::LockIB( const IndexBufferBase& IdxBuf, int ibase, int icount )
{
	auto ph = (GLIdxBufHandle*) IdxBuf.GetHandle();
	assert(ph!=nullptr);
	const void* rval = ph->mBuffer;
	return rval;
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::UnLockIB( const IndexBufferBase& IdxBuf)
{
}

///////////////////////////////////////////////////////////////////////////////

void GlGeometryBufferInterface::ReleaseIB( IndexBufferBase& IdxBuf )
{
	auto plat_handle = (GLIdxBufHandle*) IdxBuf.GetHandle();

	if( plat_handle )
		delete plat_handle;

	IdxBuf.SetHandle(0);
}

///////////////////////////////////////////////////////////////////////////////

} } //namespace ork::lev2
