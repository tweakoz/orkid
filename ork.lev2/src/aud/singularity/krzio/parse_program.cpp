#include "krzio.h"
#include <fstream>

using namespace rapidjson;

///////////////////////////////////////////////////////////////////////////////

void filescanner::ParseProgram( const datablock& db, datablock::iterator& it,  int iObjectID, std::string ObjName )
{

	//printf( "/////////////////////////////////////// NEW PROGRAM <%d>\n", iObjectID );
	//printf( "/////////////////////////////////////// NEW PROGRAM <%d>\n", iObjectID );
	//printf( "/////////////////////////////////////// NEW PROGRAM <%d>\n", iObjectID );

	auto prg = new Program;
	prg->_programID = iObjectID;
	prg->_programName = ObjName;

	if( iObjectID >= 189 && iObjectID <= 191 )
	{	prg->_debug = true;
		printf( "/////////////////////////////////////// NEW PROGRAM <%d:%p:%s>\n", iObjectID, prg, ObjName.c_str() );
	}

	_curProgram = prg;

	_programs[iObjectID] = prg;

	//parent.AddMember("type", "program", _japrog);
	const int kbaseindex = it.miIndex;
	
	bool bOK;
	u8 u8v;
	s8 s8v;
	///////////////////////
	// 00 program segment tag
	///////////////////////
	bOK = db.GetData( u8v, it );
	//printf( "Program Segment Tag<0x%02x>\n", int(u8v) );
	assert(u8v==0x08);
	///////////////////////
	// 01 program format
	///////////////////////
	bOK = db.GetData( u8v, it );
	const int iProgramFormat = int(u8v);
	//printf( "Program Format <0x%02x>\n", iProgramFormat );
	switch( iProgramFormat )
	{	case 1:
			prg->_programFormat = "Obsolete";
			break;
		case 2:
			prg->_programFormat = "K2000";
			break;
		case 3:
			prg->_programFormat = "K2500";
			break;
		case 4:
			prg->_programFormat = "2500/KDFX/KB3";
			break;
		default:
			prg->_programFormat = "Invalid";
			assert(false);
			break;
	}
	///////////////////////
	// 02 Layer Count
	///////////////////////
	bOK = db.GetData( u8v, it );
	const int iNumberOfLayers = int(u8v);
	printf( " Number of Layers <%d>\n", iNumberOfLayers );
	///////////////////////
	// 03 Play Modes
	///////////////////////
	bOK = db.GetData( u8v, it );
	prg->_mono = (u8v&1);
	prg->_porto = (u8v&2);
	prg->_enableGlobals = (u8v&4);
	prg->_atkPorto = (u8v&8);
	prg->_legato = (u8v&16);
	//printf( " MONO<%d> PORTO<%d> GLOBALS<%d> ATKPORTO<%d> LEGATO<%d>\n", int(bMONO), int(bPORTOMENTO), int(bGLOBALS), int(bATTACKPORTOMENTO), int(bLEGATOPLAY) );
	///////////////////////
	// 04 PitchBendRange
	///////////////////////
	bOK = db.GetData( s8v, it );
	//printf( "PitchBendRange<0x%02x>\n", int(s8v));
	//jsonCOMMON.AddMember("pbrange", int(s8v), _japrog);
	///////////////////////
	bOK = db.GetData( u8v, it ); // 05 COMMON (Monophonic) Portamento Rate: unsigned byte
	bOK = db.GetData( u8v, it ); // 06 ...
	bOK = db.GetData( u8v, it ); // 07 ...
	bOK = db.GetData( u8v, it ); // 08 ...
	bOK = db.GetData( u8v, it ); // 09 ...
	bOK = db.GetData( u8v, it ); // 0A ...
	bOK = db.GetData( u8v, it ); // 0B ...
	bOK = db.GetData( u8v, it ); // 0C ...
	bOK = db.GetData( u8v, it ); // 0D ...
	bOK = db.GetData( u8v, it ); // 0E ...
	bOK = db.GetData( u8v, it ); // 0F EFFECT Realtime Effect Parameter 2
	///////////////////////
	///////////////////////
	bool bDONE=false;
	int ilayer=0;

	_curLayer = nullptr;

	while( false == bDONE )
	{
		int irelindex = it.miIndex-kbaseindex;
		
		//bDONE = irelindex<kobjectlength;
		
		bOK = db.GetData( u8v, it ); // 00
		//printf( "ProgramTag<%02x> relindex<%d>\n", int(u8v), irelindex );
		switch( u8v )
		{
			case 0x0: // ???
				bDONE=true;
				break;
			////////////////////////////////////////////////
			case 0x68: // KDFX::FXRT (2500 only)
			case 0x69: // KDFX::FXPT (2500 only)
				it.SkipData(8-1);
				break;
			case 0x0f: // Effect Control Segment (8 bytes)
				it.SkipData(8-1);
				break;
			////////////////////////////////////////////////
			case 0x10: // ASR1 Segment (8 bytes)
			case 0x11: // ASR2/GASR2 Segment (8 bytes)
			case 0x14: // LFO1
			case 0x15: // LFO2/GLFO2
			case 0x18: // FUN1 Segment (4 bytes) 
			case 0x1a: // FUN3
			case 0x19: // FUN2/GFUN2 Segment (4 bytes)
			case 0x1b: // FUN4/GFUN4
			case 0x20: // Envelope Control Segment (16 bytes)
			case 0x21: // AMPENV Segment (16 bytes)
			case 0x22: // ENV2 Segment (16 bytes)
			case 0x23: // ENV3 Segment (16 bytes)
			case 0x27: // Impact Envelope Control Segment (16 bytes) K2500 Only
				parseControllers(db,it,u8v);
				break;
			////////////////////////////////////////////////
			case 0x50: // hobbes f1
			case 0x51: // hobbes f2
			case 0x52: // hobbes f3
			case 0x53: // hobbes f4
			{	parseHobbes(db,it,u8v);
				break;
			}
			////////////////////////////////////////////////
			case 0x78: // Hammond Segment 1 (2500 only)
			case 0x79: // Hammond Segment 2 (2500 only)
			case 0x7A: // Hammond Segment 3 (2500 only)
				it.SkipData(32-1);
				break;
			case 0x40: // Calvin Segment (32 bytes)
			{	parseCalvin(db,it);
				break;
			}
			case 0x09: // Layer Segment (16 bytes)
			{
				while(_algschmq.size()){
					_algschmq.pop();
				}

				_curLayer = prg->newLayer();

				//printf( "////////////////// LAYER %d ///////////////////////\n", ilayer);

				ilayer++;

				u8 loEnable = db.GetTypedData<u8>( it ); 	// 01
				_curLayer->_transpose = db.GetTypedData<u8>( it ); 	// 02
				_curLayer->_tune = db.GetTypedData<u8>( it ); 		// 03
				_curLayer->_loKey = db.GetTypedData<u8>( it ); 		// 04
				_curLayer->_hiKey = db.GetTypedData<u8>( it ); 		// 05
				u8 vRange = db.GetTypedData<u8>( it ); 		// 06
				u8 eSwitch = db.GetTypedData<u8>( it ); 		// 07
				u8 flags = db.GetTypedData<u8>( it ); // 08
				u8 moreFlags = db.GetTypedData<u8>( it ); // 09
				u8 vTrig = db.GetTypedData<u8>( it ); // 0a
				u8 hiEnable = db.GetTypedData<u8>( it ); // 0b
				u8 dlyCtl = db.GetTypedData<u8>( it ); // 0c
				u8 dlyMin = db.GetTypedData<u8>( it ); // 0d
				u8 dlyMax = db.GetTypedData<u8>( it ); // 0e
				u8 xfade = db.GetTypedData<u8>( it ); // 0f

				_curLayer->_loVel = (flags>>3)&0x07;
				_curLayer->_hiVel = 7-(flags&0x07);

				{	auto& CLE = _curLayer->_ctrlLayerEnable;
					CLE._source = getControlSourceName(eSwitch);
					CLE._flip = bool(moreFlags&0x04);
					CLE._min = int(loEnable);
					CLE._max = int(hiEnable);
				}
				{	auto& DEL = _curLayer->_ctrlDelay;
					DEL._source = getControlSourceName(dlyCtl);
					DEL._flip = false; //bool(moreFlags&0x04);
					DEL._min = int(dlyMin);
					DEL._max = int(dlyMax);
				}

				_curLayer->_ignRels = bool(flags&0x01);
				_curLayer->_ignSust = bool(flags&0x02);
				_curLayer->_ignSost = bool(flags&0x04);
				_curLayer->_ignSusp = bool(flags&0x08);
				_curLayer->_atkHold = bool(flags&0x10);
				_curLayer->_susHold = bool(flags&0x20);


				_curLayer->_bendMode = int(moreFlags&0x03);
				_curLayer->_opaqueLayer = int(moreFlags&0x08);
				_curLayer->_xfadeSense = int(moreFlags&0x10);
				_curLayer->_stereoLayer = int(moreFlags&0x20);
				_curLayer->_chanNum = int(moreFlags&0x40);
				_curLayer->_trigOnKeyUp = int(moreFlags&0x80);

				_curLayer->_xfade = int(xfade);

				_curLayer->_vt1._sense = bool(vTrig&0x08);
				_curLayer->_vt2._sense = bool(vTrig&0x80);

				_curLayer->_vt1._level = int(vTrig&0x07);
				_curLayer->_vt2._level = int(vTrig&0x70)>>4;
				break;
			}
			default:
				assert(false);
				break;
		}
	}
	///////////////////////
	
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitProgram(const Program* p, rapidjson::Value& parent)
{
	if( p->_programID > 200 )
		return;

	if( p->_debug )
		printf( "/////////////////////////////////////// EMIT PROGRAM <%d:%p:%s>\n", p->_programID, p, p->_programName.c_str() );

	rapidjson::Value prgobject(kObjectType); 

	AddStringKVMember(prgobject,"Program", p->_programName);
	prgobject.AddMember("objectID", p->_programID, _japrog);

	AddMember(prgobject,"format", p->_programFormat);

	Value jsonCOMMON(kObjectType);

	jsonCOMMON.AddMember("mono", p->_mono, _japrog);
	jsonCOMMON.AddMember("porto", p->_porto, _japrog);
	jsonCOMMON.AddMember("atkPorto", p->_atkPorto, _japrog);
	jsonCOMMON.AddMember("globCtrl", p->_enableGlobals, _japrog);
	jsonCOMMON.AddMember("legato", p->_legato, _japrog);
	prgobject.AddMember("COMMON",jsonCOMMON,_japrog);

	prgobject.AddMember("numLayers", int(p->_layers.size()), _japrog);

	rapidjson::Value layersArrayObject(kArrayType); 
	for( auto l : p->_layers )
	{
		emitLayer( l, layersArrayObject );
	}
	AddMember(prgobject,"LAYERS", layersArrayObject);

	parent.PushBack(prgobject, _japrog);
}

///////////////////////////////////////////////////////////////////////////////

void filescanner::emitLayer(const Layer* l, rapidjson::Value& parent)
{
	rapidjson::Value layerobject(kObjectType); 
	layerobject.AddMember("layerId", (int)l->_layerIndex, _japrog);

	////////////////

	layerobject.AddMember("loKey", int(l->_loKey), _japrog );
	layerobject.AddMember("hiKey", int(l->_hiKey), _japrog );
	layerobject.AddMember("loVel", l->_loVel, _japrog );
	layerobject.AddMember("hiVel", l->_hiVel, _japrog );

	layerobject.AddMember("trans", int(l->_transpose), _japrog );
	layerobject.AddMember("tune", int(l->_tune), _japrog );

	{	auto& CLE = l->_ctrlLayerEnable;
		Value jsonCLE(kObjectType);
		AddMember(jsonCLE,"controller", CLE._source );
		AddMember(jsonCLE,"ctrlMin", CLE._min );
		AddMember(jsonCLE,"ctrlMax", CLE._max );
		AddMember(jsonCLE,"ctrlFlip", CLE._flip );
		AddMember(layerobject,"layerEnable", jsonCLE );
	}
	{	auto& DEL = l->_ctrlLayerEnable;
		Value jsonDEL(kObjectType);
		AddMember(jsonDEL,"controller", DEL._source );
		AddMember(jsonDEL,"ctrlMin", DEL._min );
		AddMember(jsonDEL,"ctrlMax", DEL._max );
		AddMember(jsonDEL,"ctrlFlip", DEL._flip );
		AddMember(layerobject,"layerDelay", jsonDEL );
	}
	{
		Value jsonFLAGS(kObjectType);
		AddMember(jsonFLAGS,"xfade", l->_xfade );

		AddMember(jsonFLAGS,"ignRels", l->_ignRels );
		AddMember(jsonFLAGS,"ignSust", l->_ignSust );
		AddMember(jsonFLAGS,"ignSost", l->_ignSost );
		AddMember(jsonFLAGS,"ignSusp", l->_ignSusp );
		AddMember(jsonFLAGS,"atkHold", l->_atkHold );
		AddMember(jsonFLAGS,"susHold", l->_susHold );

		AddMember(jsonFLAGS,"bendMode",    l->_bendMode );
		AddMember(jsonFLAGS,"opaqueLayer", l->_opaqueLayer );
		AddMember(jsonFLAGS,"xfadeSense",  l->_xfadeSense );
		AddMember(jsonFLAGS,"stereoLayer", l->_stereoLayer );
		AddMember(jsonFLAGS,"chanNum", 	   l->_chanNum );
		AddMember(jsonFLAGS,"trigOnKeyUp", l->_trigOnKeyUp );

		AddMember(layerobject,"misc", jsonFLAGS );
	}
	{
		Value jsonVTRIG(kObjectType);
		std::string nrmstr("Normal"), revstr("Reversed");
		AddMember(jsonVTRIG,"vt1Level", l->_vt1._level );
		AddMember(jsonVTRIG,"vt1Sense", l->_vt1._sense ? revstr : nrmstr );
		AddMember(jsonVTRIG,"vt2Level", l->_vt2._level );
		AddMember(jsonVTRIG,"vt1Sense", l->_vt2._sense ? revstr : nrmstr );
		AddMember(layerobject,"VTRIG", jsonVTRIG );
	}

	////////////////
	// Controllers
	////////////////

	if( l->_envc )
		emitEnvControl(*l->_envc,layerobject);

	for( auto it : l->_asrmap )
	{	auto asr = it.second;
		emitAsr(asr,layerobject);
	}
	for( auto it : l->_envmap )
	{	auto env = it.second;
		bool natural = (env->_name=="AMPENV" && l->_envc->_mode=="Natural");
		if( ! natural )
			emitEnv(env,layerobject);
	}
	for( auto it : l->_funmap )
	{	auto fun = it.second;
		emitFun(fun,layerobject);
	}
	for( auto it : l->_lfomap )
	{	auto lfo = it.second;
		emitLfo(lfo,layerobject);
	}

	////////////////
	// calvin and hobbes
	////////////////

	emitCalvin(l->_calvin,layerobject); 
	emitHobbes(l->_hobbes[0],layerobject); 
	emitHobbes(l->_hobbes[1],layerobject); 
	emitHobbes(l->_hobbes[2],layerobject); 
	emitHobbes(l->_hobbes[3],layerobject); 

	////////////////

	parent.PushBack(layerobject,_japrog);
}

///////////////////////////////////////////////////////////////////////////////

Program::Program()
	: _programID(0)
	, _glfo2(nullptr)
	, _gasr2(nullptr)
	, _mono(false)
	, _porto(false)
	, _atkPorto(false)
	, _enableGlobals(false)
	, _debug(false)
{

}

Layer* Program::newLayer()
{
	auto l = new Layer(this);
	l->_layerIndex = _layers.size();
	_layers.push_back(l);
	return l;
}

Layer::Layer(Program*p) 
	: _program(p)
{
	_calvin = new Calvin(this);
	for( int i=0; i<4; i++ )
		_hobbes[i] = new Hobbes;
}

