////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
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
