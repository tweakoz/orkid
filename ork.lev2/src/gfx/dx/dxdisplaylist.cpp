////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/lev2/gfx/gfxmaterial.h>
#include <ork/file/path.h>
#include "dx.h"

///////////////////////////////////////////////////////////////////////////////
namespace ork { namespace lev2 {
///////////////////////////////////////////////////////////////////////////////

struct DxDisplayListItem
{
	const VertexBufferBase*	mvbuf;
	const IndexBufferBase*	mibuf;
	EPrimitiveType			meprim;

	DxDisplayListItem(const VertexBufferBase&vb,const IndexBufferBase&ib,EPrimitiveType eType)
		: mvbuf(&vb)
		, mibuf(&ib)
		, meprim(eType)
	{
	}

	DxDisplayListItem(const VertexBufferBase&vb)
		: mvbuf(&vb)
		, mibuf(0)
		, meprim(vb.GetPrimType())
	{
	}

	void Draw(GfxTargetDX* ptarg)
	{
		if( mibuf )
		{
			int inumidx = mibuf->GetNumIndices();

			ptarg->GBI()->DrawIndexedPrimitiveEML(*mvbuf,*mibuf,meprim);
		}
		else
		{
			ptarg->GBI()->DrawPrimitiveEML(*mvbuf,meprim);
		}
	}
};

///////////////////////////////////////////////////////////////////////////////

struct DxDisplayList
{
	orkvector< DxDisplayListItem* > mItems;
};

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DisplayListBegin( DisplayList& dlist )
{
	DxDisplayList* dxlist = new DxDisplayList;
	dxlist->mItems.clear();

	dlist.SetPlatformHandle( (void*) dxlist );
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DisplayListAddPrimitiveEML( DisplayList& dlist, const VertexBufferBase& VBuf )
{
	DxDisplayList* dxlist = (DxDisplayList*) dlist.GetPlatformHandle();
	if( dxlist )
	{
		dxlist->mItems.push_back( new DxDisplayListItem( VBuf ) );
	}
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DisplayListAddIndexedPrimitiveEML( DisplayList& dlist, const VertexBufferBase& VBuf, const IndexBufferBase& IdxBuf, EPrimitiveType eType )
{
	DxDisplayList* dxlist = (DxDisplayList*) dlist.GetPlatformHandle();
	if( dxlist )
	{
		dxlist->mItems.push_back( new DxDisplayListItem( VBuf, IdxBuf, eType ) );
	}
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DisplayListEnd( DisplayList& dlist )
{
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DisplayListDraw( const DisplayList& dlist )
{
	DxDisplayList* dxlist = (DxDisplayList*) dlist.GetPlatformHandle();
	if( dxlist )
	{
		int inumpasses = mTargetDX.GetCurMaterial()->BeginBlock(&mTargetDX);

		for( int ipass=0; ipass<inumpasses; ipass++ )
		{
			bool bDRAW = mTargetDX.GetCurMaterial()->BeginPass( &mTargetDX,ipass );

			if( bDRAW )
			{
				size_t inumitems = dxlist->mItems.size();

				for( size_t i=0; i<inumitems; i++ )
				{
					dxlist->mItems[i]->Draw( &mTargetDX );
				}
			}

			mTargetDX.GetCurMaterial()->EndPass(&mTargetDX);

		}
		mTargetDX.GetCurMaterial()->EndBlock(&mTargetDX);

	}
}

///////////////////////////////////////////////////////////////////////////////

void DxGeometryBufferInterface::DisplayListDrawEML( const DisplayList& dlist )
{
	DxDisplayList* dxlist = (DxDisplayList*) dlist.GetPlatformHandle();
	if( dxlist )
	{
		size_t inumitems = dxlist->mItems.size();

		for( size_t i=0; i<inumitems; i++ )
		{
			dxlist->mItems[i]->Draw( &mTargetDX );
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
}}
///////////////////////////////////////////////////////////////////////////////
