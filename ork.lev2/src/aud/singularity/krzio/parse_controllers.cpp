#include "krzio.h"
#include <fstream>

// crankitup
// layer3
// keystart - C-1 unipolar
// NOT C10
// eff kt should prob be 0
// not -6
using namespace rapidjson;

void filescanner::parseControllers(	const datablock& db,
									datablock::iterator& it,
									u8 code )
{
	bool OK = false;

	assert(_curProgram);

	bool globena = _curProgram->_enableGlobals;


	switch( code )
	{
		case 0x18: // FUN1 Segment (4 bytes) 
		case 0x1a: // FUN3
			assert(_curLayer);
		case 0x19: // FUN2/GFUN2 Segment (4 bytes)
		case 0x1b: // FUN4/GFUN4
		{
			int funID = int(code)-0x18;

			auto fun = new Fun;

			u8 op = db.GetTypedData<u8>( it ); 						// 01
			u8 arg1 = db.GetTypedData<u8>(it ); 						// 01
			u8 arg2 = db.GetTypedData<u8>( it ); 						// 01


			fun->_op = getFunOpName(int(op));

			fun->_a._source = getControlSourceName(arg1);
			fun->_b._source = getControlSourceName(arg2);

			std::string name = "FUNX";

			name[3]='1'+funID;

			if( _curLayer )
			{
				_curLayer->_funmap[funID] = fun;
			}
			else
			{	
				assert( globena );
				name = "G" + name;
				_curProgram->_gfunmap[funID] = fun;
			}
			fun->_name = name;

			break;
		}
		case 0x10: // ASR1 Segment (8 bytes)
			assert(_curLayer);
		case 0x11: // ASR2/GASR2 Segment (8 bytes)
		{	
			auto asr = new Asr;

			u8 f[8];

			for( int i=1; i<=7; i++ )
				f[i] = db.GetTypedData<u8>( it );

			asr->_triggerSource = getControlSourceName(f[2]);
			
			switch( f[3]&3 )
			{
				case 0:
					asr->_mode = "Normal";
					break;
				case 1:
					asr->_mode = "Hold";
					break;
				case 2:
					asr->_mode = "Repeat";
					break;
				case 3:
					asr->_mode = "Mode3???";
					break;
			}

			asr->_attack = getAsrTime(int(f[5]));
			asr->_delay = getAsrTime(int(f[4]));
			asr->_release = getAsrTime(int(f[7]));

			if( code==0x10 )
			{
				asr->_name = "ASR1";
				_curLayer->_asrmap[code] = asr;

			}
			else if( code==0x11 )
			{

				if( _curLayer )
				{
					asr->_name = "ASR2";
					_curLayer->_asrmap[code] = asr;
				}
				else
				{
					asr->_name = "GASR2";
					_curProgram->_gasr2 = asr;
				}
			}
			break;
		}
		case 0x14: // LFO1
			assert(_curLayer);
		case 0x15: // LFO2/GLFO2
		{
			u8 f[8];

			for( int i=1; i<8; i++ )
				f[i] = db.GetTypedData<u8>( it );

			auto lfo = new Lfo;

			if( code == 0x14 )
				lfo->_name = "LFO1";
			else if( code == 0x15 )
				lfo->_name = _curLayer ? "LFO2" : "GLFO2";

			if( _curLayer )
				_curLayer->_lfomap[code] = lfo;
			else
				_curProgram->_glfo2 = lfo;

			lfo->_ctrlRate._source = getControlSourceName(f[2]);
			lfo->_ctrlRate._min = getLfoRate86(f[3]);
			lfo->_ctrlRate._max = getLfoRate86(f[4]);
			lfo->_phase = int(f[5]);
			lfo->_shape = getLfoShape87(f[6]);

			break;
		}
		case 0x20: // Envelope Control Segment (16 bytes)
		{	
			assert(_curLayer);

			u8 f[16];

			for( int i=1; i<=15; i++ )
				f[i] = db.GetTypedData<u8>( it );

			bool is_natural = f[2]&1;

			auto ec = new EnvControl;
			_curLayer->_envc = ec;
			auto& atkc = ec->_atkControl;
			auto& decc = ec->_decControl;
			auto& relc = ec->_relControl;

			ec->_mode = is_natural ? "Natural" : "User";

			ec->_atkAdjust = getEnvCtrl(f[3]);
			ec->_atkKeyTrack = getEnvCtrl(f[4]);
			ec->_atkVelTrack = getEnvCtrl(f[5]);
			ec->_atkControl = getControlSourceName(f[6]);
			ec->_atkDepth = getEnvCtrl(f[7]);

			ec->_decAdjust = getEnvCtrl(f[8]);
			ec->_decKeyTrack = getEnvCtrl(f[9]);
			ec->_decControl = getControlSourceName(f[10]);
			ec->_decDepth = getEnvCtrl(f[11]);

			ec->_relAdjust = getEnvCtrl(f[12]);
			ec->_relKeyTrack = getEnvCtrl(f[13]);
			ec->_relControl = getControlSourceName(f[14]);
			ec->_relDepth = getEnvCtrl(f[15]);

			break;
		}
		case 0x21: // AMPENV Segment (16 bytes)
		case 0x22: // ENV2 Segment (16 bytes)
		case 0x23: // ENV3 Segment (16 bytes)
		{	
			assert(_curLayer);

			auto env = new Env;
			env->_code = code;
			if( code == 0x21 )
				env->_name = "AMPENV";
			else if( code == 0x22 )
				env->_name = "ENV2";
			else if( code == 0x23 )
				env->_name = "ENV3";

			_curLayer->_envmap[code-0x20] = env;

			bool level_unsigned = (code==0x21);

			u8 flags;
			OK = db.GetData( flags, it );

			env->_loopSeg = int(flags&7);
			env->_loopCount = int(flags>>3);

			for( int i=0; i<7; i++ )
			{
				u8 level, rate;
				OK = db.GetData( level, it );
				OK = db.GetData( rate, it );
				float famp = 0.0f;
				if( level_unsigned )
					famp = float(level)*0.01f;
				else
					famp = float(makeSigned(level))*0.01f;
				float time = getAsrTime(rate);
				env->_rates[i]=(time);
				env->_levels[i]=(famp);
			}

			break;
		}
		case 0x27: // Impact Envelope Control Segment (16 bytes) K2500 Only
		//case 0x2b: // ???
			it.SkipData(16-1);
			break;		
	}
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitAsr(const Asr* a, rapidjson::Value& parent)
{
	Value jsonseg(kObjectType);

	AddMember(jsonseg,"trigger", a->_triggerSource );
	AddMember(jsonseg,"mode", a->_mode );

	jsonseg.AddMember("delay", a->_delay, _japrog );
	jsonseg.AddMember("attack", a->_attack, _japrog );
	jsonseg.AddMember("release", a->_release, _japrog );

	AddMember( parent, a->_name, jsonseg );


}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitEnv(const Env* e, rapidjson::Value& parent)
{
	Value jsonenvseg(kObjectType);
	Value jsonrates(kArrayType);
	Value jsonamps(kArrayType);

	for( int i=0; i<7; i++ )
	{
		float flevl = e->_levels[i];
		float frate = e->_rates[i];
		jsonrates.PushBack(frate, _japrog );
		jsonamps.PushBack(flevl, _japrog );
	}
	AddMember(jsonenvseg,"loopSeg", e->_loopSeg );
	AddMember(jsonenvseg,"loopCnt", e->_loopCount );
	AddMember(jsonenvseg,"rates", jsonrates );
	AddMember(jsonenvseg,"levels", jsonamps );

	AddMember( parent, e->_name, jsonenvseg );
	
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitEnvControl(const EnvControl& ec, rapidjson::Value& parent)
{
	Value jsonecseg(kObjectType);
					

	AddStringKVMember(jsonecseg,"ampenv_mode",ec._mode);

	jsonecseg.AddMember("AtkAdjust", ec._atkAdjust, _japrog );
	jsonecseg.AddMember("AtkKeyTrack", ec._atkKeyTrack, _japrog );
	jsonecseg.AddMember("AtkVelTrack", ec._atkVelTrack, _japrog );
	AddStringKVMember(jsonecseg,"AttControl", ec._atkControl );
	jsonecseg.AddMember("AtkDepth", ec._atkDepth, _japrog );

	jsonecseg.AddMember("DecAdjust", ec._decAdjust, _japrog );
	jsonecseg.AddMember("DecKeyTrack", ec._decKeyTrack, _japrog );
	AddStringKVMember(jsonecseg,"DecControl", ec._decControl );
	jsonecseg.AddMember("DecDepth", ec._decDepth, _japrog );

	jsonecseg.AddMember("RelAdjust", ec._relAdjust, _japrog );
	jsonecseg.AddMember("RelKeyTrack", ec._relKeyTrack, _japrog );
	AddStringKVMember(jsonecseg,"RelControl", ec._relControl );
	jsonecseg.AddMember("RelDepth", ec._relDepth, _japrog );
	

	parent.AddMember("ENVCTRL", jsonecseg, _japrog );

}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitFun(const Fun* f, rapidjson::Value& parent)
{
	Value jsonfunseg(kObjectType);
	AddMember(jsonfunseg,"op", f->_op );
	AddMember(jsonfunseg,"a", f->_a._source );
	AddMember(jsonfunseg,"b", f->_b._source );
	AddMember(parent, f->_name, jsonfunseg );
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitLfo(const Lfo* lfo, rapidjson::Value& parent)
{
	const auto& ratec = lfo->_ctrlRate;

	Value jsonlfoseg(kObjectType);

	Value ratesrc;
	ratesrc.SetString(ratec._source.c_str(),_japrog);

	AddMember(jsonlfoseg,"rateCtl", ratesrc );
	AddMember(jsonlfoseg,"minRate(hz)", ratec._min );
	AddMember(jsonlfoseg,"maxRate(hz)", ratec._max );
	AddMember(jsonlfoseg,"phase", lfo->_phase );
	AddStringKVMember(jsonlfoseg,"shape", lfo->_shape );

	AddMember(parent,lfo->_name, jsonlfoseg );
}
Asr::Asr()
	: _attack(0)
	, _delay(0)
	, _release(0)
{

}
Env::Env()
	: _loopSeg(0)
	, _loopCount(0)
{
	for( int i=0; i<7; i++ )
	{
		_rates[i] = 0;
		_levels[i] = 0;
	}
}
Lfo::Lfo()
	: _phase(0)
{

}

