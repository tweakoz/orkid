////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// based on code from resample at http://ccrma.stanford.edu/~jos/resample/Free_Resampling_Software.html
// this code under the LGPL http://www.gnu.org/copyleft/lesser.html
///////////////////////////////////////////////////////////////////////////////

#include <orktool/orktool_pch.h>

#include <ork/math/audiomath.h>
#include <orktool/filter/filter.h>
#include "soundfont.h"
#include "sf2tospu.h"
#include "aiff.h"
#include <ork/kernel/string/string.h>

#if defined(_USE_SOUNDFONT)
///////////////////////////////////////////////////////////////////////////////

#define IBUFFSIZE 4096                         // Input buffer size

void OrkHeapCheck( void );

namespace ork { namespace tool {

///////////////////////////////////////////////////////////////////////////////

#define MAX_HWORD (32767)
#define MIN_HWORD (-32768)
#define Nhc       8
#define Na        7
#define Np       (Nhc+Na)
#define Npc      (1<<Nhc)
#define Amask    ((1<<Na)-1)
#define Pmask    ((1<<Np)-1)
#define Nh       16
#define Nb       16
#define Nhxn     14
#define Nhg      (Nh-Nhxn)
#define NLpScl   13

///////////////////////////////////////////////////////////////////////////////

#include "smallfilter.h"
#include "largefilter.h"

///////////////////////////////////////////////////////////////////////////////

#ifdef DEBUG
static S32 pof = 0;		// positive overflow count //
static S32 nof = 0;		// negative overflow count //
#endif

///////////////////////////////////////////////////////////////////////////////

#ifndef PRMAX
#define PRMAX(x,y) ((x)>(y) ?(x):(y))
#endif
#ifndef PRMIN
#define PRMIN(x,y) ((x)<(y) ?(x):(y))
#endif

///////////////////////////////////////////////////////////////////////////////

S32 FilterUD( S16 Imp[], S16 ImpD[],
		     U16 Nwing, bool Interp,
		     S16 *Xp, S16 Ph, S16 Inc, U16 dhb)
{
    S16 a;
    S16 *Hp, *Hdp, *End;
    S32 v, t;
    
    v=0;
    U32 Ho = (Ph*(U32)dhb)>>Np;
    End = &Imp[Nwing];
    if (Inc == 1)		/* If doing right wing...              */
    {				/* ...drop extra coeff, so when Ph is  */
	End--;			/*    0.5, we don't do too many mult's */
	if (Ph == 0)		/* If the phase is zero...           */
	  Ho += dhb;		/* ...then we've already skipped the */
    }				/*    first sample, so we must also  */
				/*    skip ahead in Imp[] and ImpD[] */
    if (Interp)
      while ((Hp = &Imp[Ho>>Na]) < End) {
	  t = *Hp;		/* Get IR sample */
	  Hdp = &ImpD[Ho>>Na];  /* get interp (lower Na) bits from diff table*/
	  a = s16(Ho & Amask);	/* a is logically between 0 and 1 */
	  t += (((S32)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & 1<<(Nhxn-1))	/* Round, if needed */
	    t += 1<<(Nhxn-1);
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Ho += dhb;		/* IR step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    else 
      while ((Hp = &Imp[Ho>>Na]) < End) {
	  t = *Hp;		/* Get IR sample */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & 1<<(Nhxn-1))	/* Round, if needed */
	    t += 1<<(Nhxn-1);
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Ho += dhb;		/* IR step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    return(v);
}

///////////////////////////////////////////////////////////////////////////////

S32 FilterUp(S16 Imp[], S16 ImpD[], 
		     U16 Nwing, bool Interp,
		     S16 *Xp, S16 Ph, S16 Inc)
{
    S16 *Hp, *Hdp = NULL, *End;
    S16 a = 0;
    S32 v, t;
    
    v=0;
    Hp = &Imp[Ph>>Na];
    End = &Imp[Nwing];
    if (Interp) {
	Hdp = &ImpD[Ph>>Na];
	a = Ph & Amask;
    }
    if (Inc == 1)		/* If doing right wing...              */
    {				/* ...drop extra coeff, so when Ph is  */
	End--;			/*    0.5, we don't do too many mult's */
	if (Ph == 0)		/* If the phase is zero...           */
	{			/* ...then we've already skipped the */
	    Hp += Npc;		/*    first sample, so we must also  */
	    Hdp += Npc;		/*    skip ahead in Imp[] and ImpD[] */
	}
    }
    if (Interp)
      while (Hp < End) {
	  t = *Hp;		/* Get filter coeff */
	  t += (((S32)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
	  Hdp += Npc;		/* Filter coeff differences step */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & (1<<(Nhxn-1)))  /* Round, if needed */
	    t += (1<<(Nhxn-1));
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Hp += Npc;		/* Filter coeff step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      } 
    else 
      while (Hp < End) {
	  t = *Hp;		/* Get filter coeff */
	  t *= *Xp;		/* Mult coeff by input sample */
	  if (t & (1<<(Nhxn-1)))  /* Round, if needed */
	    t += (1<<(Nhxn-1));
	  t >>= Nhxn;		/* Leave some guard bits, but come back some */
	  v += t;			/* The filter output */
	  Hp += Npc;		/* Filter coeff step */
	  Xp += Inc;		/* Input signal step. NO CHECK ON BOUNDS */
      }
    return(v);
}

///////////////////////////////////////////////////////////////////////////////

static S16 WordToHword(S32 v, U32 scl)
{
    S16 out;
    U32 llsb = (1<<(scl-1));
    v += llsb;	// round
    v >>= scl;
    if (v>MAX_HWORD)
	{
		#ifdef DEBUG
		if (pof == 0)
			fprintf(stderr, "*** resample: sound sample overflow\n");
		else if ((pof % 10000) == 0)
			fprintf(stderr, "*** resample: another ten thousand overflows\n");
		pof++;
		#endif
		v = MAX_HWORD;
	}
	else if (v < MIN_HWORD)
	{
		#ifdef DEBUG
		if (nof == 0)
			fprintf(stderr, "*** resample: sound sample (-) overflow\n");
		else if ((nof % 1000) == 0)
			fprintf(stderr, "*** resample: another thousand (-) overflows\n");
		nof++;
		#endif
		v = MIN_HWORD;
	}	
    out = (S16) v;
	return out;
}

///////////////////////////////////////////////////////////////////////////////
// Returns the number of output samples

static intptr_t SrcUp(S16 X[], S16 Y[], double factor, U32 *Time,
                 U16 Nx, U16 Nwing, U16 LpScl,
                 S16 Imp[], S16 ImpD[], bool Interp)
{
    S16 *Xp, *Ystart;
    S32 v;
    
    double dt;                  /* Step through input signal */ 
    U32 dtb;                  /* Fixed-point version of Dt */
    U32 endTime;              /* When Time reaches EndTime, return to user */
    
    dt = 1.0/factor;            /* Output sampling period */
    dtb = (u32)(dt*(1<<Np) + 0.5);     /* Fixed-point representation */
    
    Ystart = Y;
    endTime = *Time + (1<<Np)*(S32)Nx;
    while (*Time < endTime)
    {
        Xp = &X[*Time>>Np];      /* Ptr to current input sample */
        /* Perform left-wing inner product */
        v = FilterUp(Imp, ImpD, Nwing, Interp, Xp, (S16)(*Time&Pmask),-1);
        /* Perform right-wing inner product */
        v += FilterUp(Imp, ImpD, Nwing, Interp, Xp+1, 
		      /* previous (triggers warning): (S16)((-*Time)&Pmask),1); */
                      (S16)((((*Time)^Pmask)+1)&Pmask),1);
        v >>= Nhg;              /* Make guard bits */
        v *= LpScl;             /* Normalize for unity filter gain */
        *Y++ = WordToHword(v,NLpScl);   /* strip guard bits, deposit output */
        *Time += dtb;           /* Move to next sample by time increment */
    }
    return (Y - Ystart);        /* Return the number of output samples */
}


///////////////////////////////////////////////////////////////////////////////
// Sampling rate conversion subroutine
		
intptr_t SrcUD( S16 *X, S16 *Y, F64 factor, U32 *Time, U16 Nx, U16 Nwing, U16 LpScl, S16 *Imp, S16 *ImpD, bool Interp )
{
	S16 *Xp, *Ystart;
	S32 v;
	F64 dh;							// Step through filter impulse response
	F64 dt;							// Step through input signal
	U32 endTime;					// When Time reaches EndTime, return to user
	U32 dhb, dtb;					// Fixed-point versions of Dh,Dt

	dt = 1.0/factor;				// Output sampling period
	dtb = (U32)(dt*(1<<Np) + 0.5);	// Fixed-point representation (Np is #defined constant)
	dh = PRMIN(Npc, factor*Npc);		// Filter sampling period
	dhb = (U32) (dh*(1<<Na) + 0.5);	// Fixed-point representation

	Ystart = Y;
	endTime = *Time + (1<<Np)*(S32)Nx;
	while (*Time < endTime)
	{
		Xp = & X[*Time>>Np];			// Ptr to current input sample
		
		v = FilterUD( Imp, ImpD, Nwing, Interp, Xp, (S16)(*Time&Pmask), -1, (U16) dhb);		// Perform left-wing inner product
		
		S32 ttime = - (S32) (*Time);
		ttime &= Pmask; // FYI a signed and happens in unsigned numeric space

		v += FilterUD(Imp, ImpD, Nwing, Interp, Xp+1, (S16) ttime, 1, (U16) dhb);			// Perform right-wing inner product
		
		v >>= Nhg;						// Make guard bits
		v *= LpScl;						// Normalize for unity filter gain
		*Y++ = WordToHword(v,NLpScl);	// strip guard bits, deposit output
		*Time += dtb;					// Move to next sample by time increment
	}
	return (Y - Ystart);			// Return the number of output samples
}

///////////////////////////////////////////////////////////////////////////////

S32 err_ret(char *s)
{	fprintf(stderr,"resample: %s \n\n",s); // Display error message  //
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
// returns:		number of output samples returned
// factor:		Sndout/Sndin
// inCount:		number of input samples to convert
// outCount:	number of output samples to compute
// interpFilt:	TRUE means interpolate filter coeffs

U32 calcnewsize( F64 factor, U32 incount )
{
	U32 rval = 0;
	return rval;
}

///////////////////////////////////////////////////////////////////////////////
// CAUTION: Assumes we call this for only one resample job per program run! //
// return: 0 - notDone //
//        >0 - index of last sample //

U32 CSF2Sample::ResampleReadData( S16 *outptr, U32 bufsize, U32 xoff )
{	U32 rval = 0;
	U32 nsamps = bufsize-xoff;
	for( U32 i=0; i<nsamps; i++ )
	{	OrkAssert( (xoff+i) < bufsize );
		outptr[ (xoff+i) ] = mpsampledata[ mireaddata_offset+i ];
	}
	mireaddata_offset += nsamps;
	if( mireaddata_offset >= misamplelen )
		rval = (((nsamps-(mireaddata_offset-misamplelen)-1)+xoff));
	return rval;
}

//last = readData(infd, inCount, X1, X2, IBUFFSIZE, nChans, (S32)Xread);


///////////////////////////////////////////////////////////////////////////////

void CSF2Sample::ResampleForVag( F64 factor, bool interpFilt, bool largefilter, int & resample_len, S16 * & resample_data )
{
	OrkHeapCheck();
	U32 Time, Time2;		// Current time/pos in input sample
    U32 OBUFFSIZE = (S32)(((F64)IBUFFSIZE)*factor+2.0);
    
	S16 X1[IBUFFSIZE*8];
	S16 *Y1 = new S16[ OBUFFSIZE*8 ];
	U16 Nx;
	S32 Ycount, last;
    
	S32 *obufs = new S32[ OBUFFSIZE*8 ];

	printf( "//////////////////////////////////////\n" );
	printf( "// resampling sample %p\n", this );
	
	/////////////////////////////////////////////////////
	// setup parameters

	U16 Nmult	= largefilter ? LARGE_FILTER_NMULT	: SMALL_FILTER_NMULT;
	U16 LpScl	= U16(0.95 * (largefilter ? LARGE_FILTER_SCALE	: SMALL_FILTER_SCALE));
	U16 Nwing	= largefilter ? LARGE_FILTER_NWING	: SMALL_FILTER_NWING;
	S16 *Imp	= largefilter ? LARGE_FILTER_IMP	: SMALL_FILTER_IMP;
	S16 *ImpD	= largefilter ? LARGE_FILTER_IMPD	: SMALL_FILTER_IMPD;

	/////////////////////////////////////////////////////
    // Account for increased filter gain when using factors less than 1 
    
	if( factor < 1.0 )
	{
		LpScl = (U16) (LpScl*factor + 0.5);
	}

	/////////////////////////////////////////////////////
    // Calc reach of LP filter wing & give some creeping room 

	U16 Xoff = (U16) (((Nmult+1)/2.0) * PRMAX(1.0,1.0/factor) + 10);

    if( IBUFFSIZE < 2*Xoff )		// Check input buffer size 
	{
		OrkAssert( false ); // IBUFFSIZE (or factor) is too small
	}

	/////////////////////////////////////////////////////

	Nx = IBUFFSIZE - 2*Xoff;		// # of samples to process each iteration 
    
    last = 0;						// Have not read last input sample yet 
    Ycount = 0;						// Current sample and length of output file 
    U32 Xp = Xoff;					// Current "now"-sample pointer for input 
    U16 Xread = Xoff;				// Position in input array to read into 
    Time = (Xoff<<Np);				// Current-time pointer for converter 
    
	/////////////////////////////////////////////////////
    
	for( int i=0; i<Xoff; )
	{
		// Need Xoff zeros at begining of sample 
		X1[i++]=0;
	}
	/////////////////////////////////////////////////////
	
	int offset = 0;
	int waveinlen = this->misamplelen;
	int maxoutlen = (int)(((F64)waveinlen)*factor+2.0);
	int pblock = ((waveinlen % IBUFFSIZE) != 0);
	int nblocks = (waveinlen / IBUFFSIZE) + pblock;
	int outlen = maxoutlen;

	resample_len = outlen;
	resample_data = new S16[ outlen ];

	printf( "// input len: %d\n", waveinlen );
	printf( "// output len: %d\n", maxoutlen );
	printf( "// factor: %f\n// ", factor );

	mireaddata_offset = 0;
	int writedata_offset = 0;

	/////////////////////////////////////////////////////
	while( Ycount<outlen ) // Continue until done
	/////////////////////////////////////////////////////
	{
		//printf( "yo1\n" );
		//fflush(stdout);

		if( !last )					// If haven't read last sample yet 
		{
	
			ResampleReadData( X1, IBUFFSIZE, Xread );

			if( last && (last-Xoff<Nx) )	// If last sample has been read... 
			{	Nx = u16(last-Xoff);				// ...calc last sample affected by filter 
				if (Nx <= 0)
				{
					break;
				}
			}
		}

		//printf( "yo2\n" );
		//fflush(stdout);
		//////////////////////////////////
		// Resample stuff in input buffer
		
		Time2 = Time;

		intptr_t Nout = (factor >= 1.0)	? SrcUp( X1, Y1, factor, & Time, Nx, Nwing, LpScl, Imp, ImpD, interpFilt ) 
									: SrcUD( X1, Y1, factor, & Time, Nx, Nwing, LpScl, Imp, ImpD, interpFilt ); // SrcUp() is faster if we can use it

		Time -= (Nx<<Np);					// Move converter Nx samples back in time
		Xp += Nx;							// Advance by number of samples processed
		U32 Ncreep = (Time>>Np) - Xoff;		// Calc time accumulation in Time
		
		if( Ncreep )
		{	
			Time -= (Ncreep<<Np);			// Remove time accumulation
			Xp += Ncreep;					// and add it to read pointer
		}

		Xread = U16(IBUFFSIZE-Xp+Xoff);	// Pos in input buff to read new data into

		for( int i=0; i<Xread; i++ )	// Copy part of input signal
		{
			OrkAssert( i < IBUFFSIZE );
			OrkAssert( (i+Xp-Xoff) < IBUFFSIZE );

			X1[i] = X1[i+Xp-Xoff];			// that must be re-used

		}

		//printf( "yo3\n" );
		//fflush(stdout);

		if( last )							// If near end of sample...
		{	
			last -= Xp;						// ...keep track were it ends
			
			if(!last)						// Lengthen input by 1 sample if...
			{
				last++;						// ...needed to keep flag TRUE
			}
		}

		//printf( "yo4\n" );
		//fflush(stdout);
		Xp = Xoff;
		
		Ycount += Nout;
		
		if( Ycount > outlen )
		{	
			Nout -= (Ycount-outlen);
			Ycount = outlen;
		}
		
		//printf( "yo5\n" );
		//fflush(stdout);
		for ( int i = 0; i < int(Nout); i++)
		{
			OrkAssert( i < int(OBUFFSIZE) );		
			obufs[i] = (S32) Y1[i];
		}
		//printf( "yo6\n" );
		//fflush(stdout);

		//////////////////////////////////////////
		//	write output
		// NB: errors reported within sndlib //
		//mus_write(outfd, 0, Nout - 1, nChans, obufs);
		
		for( int i=0; i<int(Nout); i++ )
		{	
			OrkAssert( (writedata_offset+i) < outlen );		
			OrkAssert( i < int(OBUFFSIZE) );		
			
			resample_data[ writedata_offset+i ] = Y1[i];
		}

		//printf( "yo7\n" );
		//fflush(stdout);

		writedata_offset += Nout;

		printf(".");
		fflush(stdout);

	}
	/////////////////////////////////////////////////////
	/////////////////////////////////////////////////////

	printf( "\n//resampled length: %d\n", Ycount );
		fflush(stdout);

    resample_len = Ycount;
	OrkHeapCheck();

}
			
///////////////////////////////////////////////////////////////////////////////

} }

#endif
