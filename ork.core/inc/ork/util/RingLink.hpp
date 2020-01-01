////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/util/RingLink.h>

namespace ork { namespace util {

template<typename T>
RingLink<T>::RingLink()
	: ring_link_implementation::RingLinkBase()
{}

template<typename T>
RingLink<T>::~RingLink()
{}

template<typename T>
RingLink<T> *RingLink<T>::Next()
{
	return static_cast<RingLink *>(RingLinkBase::Next());
}

template<typename T>
RingLink<T> *RingLink<T>::Prev()
{
	return static_cast<RingLink *>(RingLinkBase::Prev());
}

template<typename T>
typename RingLink<T>::iterator RingLink<T>::begin()
{
	iterator it(this);
	return ++it;
}

template<typename T>
typename RingLink<T>::iterator RingLink<T>::end()
{
	return iterator(this);
}

template<typename T>
RingLink<T>::iterator::iterator(RingLink *sentinal)
	: mSentinal(sentinal)
	, mCurrent(sentinal)
{}

template<typename T>
T &RingLink<T>::iterator::operator *()
{
	OrkAssert(mCurrent != mSentinal);
	return *static_cast<T *>(mCurrent);
}

template<typename T>
T *RingLink<T>::iterator::operator ->()
{
	OrkAssert(mCurrent != mSentinal); 
	return static_cast<T *>(mCurrent); 
}

template<typename T>
const typename RingLink<T>::iterator &RingLink<T>::iterator::operator ++() 
{
	mCurrent = mCurrent->Next();
	return *this; 
}

template<typename T>
typename RingLink<T>::iterator RingLink<T>::iterator::operator ++(int) 
{
	iterator old = *this;
	++*this;
	return old; 
}

template<typename T>
bool RingLink<T>::iterator::operator ==(const iterator &other) const
{ 
	OrkAssert(mSentinal == other.mSentinal); 
	return mCurrent == other.mCurrent; 
}

template<typename T>
bool RingLink<T>::iterator::operator !=(const iterator &other) const 
{
	OrkAssert(mSentinal == other.mSentinal); 
	return mCurrent != other.mCurrent;
}

template<typename T>
void RingLink<T>::Unlink()
{
	RingLinkBase::Unlink();
}

template<typename T>
void RingLink<T>::LinkAfter(RingLink *prev)
{
	RingLinkBase::LinkAfter(prev);
}

template<typename T>
void RingLink<T>::LinkBefore(RingLink *next)
{
	RingLinkBase::LinkBefore(next);
}

} }
