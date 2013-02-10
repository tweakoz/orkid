////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/util/RingLink.h>

namespace ork { namespace util { namespace ring_link_implementation {

RingLinkBase::RingLinkBase()
	: mNextLink(this)
	, mPrevLink(this)
{}

RingLinkBase::~RingLinkBase()
{
	Unlink();
}

RingLinkBase *RingLinkBase::Next() const
{
	OrkAssert(mPrevLink->mNextLink == this);
	OrkAssert(mNextLink->mPrevLink == this);
	return mNextLink;
}

RingLinkBase *RingLinkBase::Prev() const
{
	OrkAssert(mPrevLink->mNextLink == this);
	OrkAssert(mNextLink->mPrevLink == this);
	return mPrevLink;
}

// non-const right side prevents copy construction.
// need const-cast somewhere if we make the pointers const mutable
// to enable copy construction.
RingLinkBase::RingLinkBase(RingLinkBase &other)
	: mNextLink(this)
	, mPrevLink(this)
{
	LinkBefore(&other);
}

const RingLinkBase &RingLinkBase::operator =(RingLinkBase &other)
{
	OrkAssert(this != &other);

	OrkAssert(mPrevLink->mNextLink == this);
	OrkAssert(mNextLink->mPrevLink == this);

	if(this != &other)
	{
		LinkBefore(&other);
	}

	OrkAssert(mPrevLink->mNextLink == this);
	OrkAssert(mNextLink->mPrevLink == this);

	return *this;
}

void RingLinkBase::Unlink()
{
	OrkAssert(mPrevLink->mNextLink == this);
	OrkAssert(mNextLink->mPrevLink == this);

	mPrevLink->mNextLink = mNextLink;
	mNextLink->mPrevLink = mPrevLink;
	mPrevLink = mNextLink = this;
}

void RingLinkBase::LinkAfter(RingLinkBase *prev)
{
	OrkAssert(this != prev);
	OrkAssert(mPrevLink->mNextLink == this);
	OrkAssert(mNextLink->mPrevLink == this);
	OrkAssert(prev->mNextLink->mPrevLink == prev);

	Unlink();

	mNextLink = prev->mNextLink;
	mPrevLink = prev;

	mNextLink->mPrevLink = this;
	mPrevLink->mNextLink = this;
}

void RingLinkBase::LinkBefore(RingLinkBase *next)
{
	OrkAssert(this != next);
	OrkAssert(mPrevLink->mNextLink == this);
	OrkAssert(mNextLink->mPrevLink == this);
	OrkAssert(next->mPrevLink->mNextLink == next);

	Unlink();

	mNextLink = next;
	mPrevLink = next->mPrevLink;


	mNextLink->mPrevLink = this;
	mPrevLink->mNextLink = this;
}


} } }
