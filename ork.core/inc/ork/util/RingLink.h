////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _ORK_UTIL_RINGLINK_H_
#define _ORK_UTIL_RINGLINK_H_

namespace ork { namespace util {

namespace ring_link_implementation {

class RingLinkBase
{
public:	
	RingLinkBase();
	RingLinkBase(RingLinkBase &);
	~RingLinkBase();

	void Unlink();
	void LinkAfter(RingLinkBase *prev);
	void LinkBefore(RingLinkBase *next);
	
	const RingLinkBase &operator=(RingLinkBase &);
protected:
	RingLinkBase *Next() const;
	RingLinkBase *Prev() const;
private:
	RingLinkBase *mNextLink, *mPrevLink;
};

}

template<typename T>
class RingLink : public ring_link_implementation::RingLinkBase
{
public:
	RingLink();
	~RingLink();
	
	void Unlink();
	void LinkAfter(RingLink *prev);
	void LinkBefore(RingLink *next);

	class iterator
	{
	public:
		iterator(RingLink *sentinal);
		
		T &operator *();
		T *operator ->();
		const iterator &operator ++();
		iterator operator ++(int);
		
		bool operator ==(const iterator &other) const;
		bool operator !=(const iterator &other) const;
	private:
		RingLink *mSentinal;
		RingLink *mCurrent;
	};
	
	iterator begin();
	iterator end();
private:
	friend class iterator;
	RingLink *Next();
	RingLink *Prev();
};

} }

#endif 
