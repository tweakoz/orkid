////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#ifndef _AUDIOMATH_H
#define _AUDIOMATH_H

///////////////////////////////////////////////////////////////////////////////
namespace ork{
///////////////////////////////////////////////////////////////////////////////

class CAudioMath
{
	public: //

	static float log_base( float base, float inp );
	static float pow_base( float base, float inp );
	static float linear_time_to_timecent( float time );
	static float timecent_to_linear_time( float timecent );
	static float decibel_to_linear_amp_ratio( float decibel );
	static float linear_amp_ratio_to_decibel( float linear );
	static float centibel_to_linear_amp_ratio( float centibel );
	static float linear_amp_ratio_to_centibel( float linear );
	static float linear_freq_ratio_to_cents( float freq_ratio );
	static float cents_to_linear_freq_ratio( float cents );

	static S32 round_to_nearest( float in );

	static float midi_note_to_frequency( float midinote );
	static float frequency_to_midi_note( float frequency );

	static float clip_float( float in, float minn, float maxx );
};

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////

#endif
