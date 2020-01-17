////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2020, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef __RANDOM_NUMBERS_H__
#define __RANDOM_NUMBERS_H__

///////////////////////////////////////////////////////////////////////////////

class RandomNumbers
	{
	public:
		RandomNumbers(void) ;
		~RandomNumbers() ;

		void	Restart(U32 seed) ;									// restart random number generator with specified seed
		void	Restart(void) ;										// restart random number generator
		U32		GetInteger(void) ;									// return a random integer
		U32		GetRangedInteger(U32 lower, U32 upper) ;			// return a random integer in a specified range
		F32		GetFloat(void) ;
		
		U32		GetSeed() { return m_curr; }
		// return a random float
	private:
		U32		m_curr;
	} ;

///////////////////////////////////////////////////////////////////////////////

extern RandomNumbers	*g_randomNumber ;

#endif
