////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined( _PS2 )

#include <input/input.h>
#include <input/ps2/inputps2.h>

///////////////////////////////////////////////////////////////////////////////

//#include <libpad.h>
#include <ee/include/libpad.h>

///////////////////////////////////////////////////////////////////////////////

static U128 upadbuffer[2][256] MEMALIGN(64);


enum ButtonsEnum
{
	LStickX = 0,
	LStickY,
	RStickX,
	RStickY,

	L1,
	L2,
	R1,
	R2,

	DLeft,
	DRight,
	DUp,
	DDown,

	Square,
	Circle,
	Triangle,
	Cross,

	L3,
	R3,
	Select,
	Start,

	LDeadX,
	LDeadY,
	RDeadX,
	RDeadY,

	LSteadyX,
	LSteadyY,
	RSteadyX,
	RSteadyY
};

typedef unsigned char BoolButtons[28];

typedef float AnalogButtons[28];

typedef struct {

	BoolButtons Held;
	BoolButtons Pushed;
	BoolButtons Released;
	AnalogButtons Analog;

	BoolButtons LastHeld;
	BoolButtons ShiftCount;
	BoolButtons TapCount;

} PadType;


extern enum PadStateEnum {
	Disconnected=0,
	NotSupported=1,
	Busy=2,
	Valid=3
} PadState[2];

#define PadDeadZone 0.27f
#define PadSteadyZone (2.0f/128.0f)

PadType Pad[2];
u8 PadData[2][32] MEMALIGN(64);
enum PadStateEnum PadState[2];

///////////////////////////////////////////////////////////////////////////////

CInputDevicePS2::CInputDevicePS2(void)
	: CInputDevice()
	, mbInitialize(true)
{
}


void CInputDevicePS2::ClassInit( CClass *pClass )
{
}


CInputDevicePS2::~CInputDevicePS2()
{
}




void CInputDevicePS2::Input_Poll()
{
	mConnectionStatus = CONN_STATUS_ACTIVE;

	/////////////////////////////////////////////
	// first time init
	U32 ret;
	if( mbInitialize )
	{
#if 0 //nope can't initlize pads here look in  gfxenvps2
		ret = padPortOpen( 0, 0, upadbuffer[0] );
		orkprintf( "Port Open %d \n",ret);
		if (ret==0) orkprintf( "ERROR: scePadPortOpen\n" );
  		padPortOpen( 0, 1, upadbuffer[1] );
		int i;
		for (i=0; i<2; i++)
		{
			PadState[i]=Disconnected;
			memset(&Pad[i],0,sizeof(Pad[i]));
		}


    	int hwstate=padGetState(0, 0);
    	int lastState = -1;
    	while((hwstate != PAD_STATE_STABLE) && (hwstate != PAD_STATE_FINDCTP1)) {
    		char stateString[16];
    		if (hwstate != lastState) {
    		    padStateInt2String(hwstate, stateString);
				orkprintf( "Pad Not ready %d %s\n",hwstate, stateString);
    		}
    		lastState = hwstate;
    		hwstate=padGetState(0, 0);
    	}
#endif

		for (int i=0; i<2; i++)
		{
			PadState[i]=Disconnected;
			memset(&Pad[i],0,sizeof(Pad[i]));
		}

		mbInitialize = false;

	}

	/////////////////////////////////////////////
	int pn;
	for (pn=0; pn<1; pn++)
	{
 #if 1
    	struct padButtonStatus buttons[pn];
    	char stateString[16];
		PadType* pad=&Pad[pn];
		enum PadStateEnum *state=&PadState[pn];
		u8* data=(u8 *) &buttons[pn];
		int hwstate =  padGetState(pn,0);
	  	//e orkprintf( "Pad 0 %d %s\n",hwstate, stateString);
    	int lastState = -1;

        //e scePadStateIntToStr(hwstate, stateString);
        //e padStateInt2String(state, stateString);

		switch(hwstate)
		{
		case PAD_STATE_DISCONN:

			//if (*state!=Disconnected) orkprintf("d\n");
			*state=Disconnected;//0x79
			break;
		case PAD_STATE_FINDCTP1:
			//orkprintf("o\n");
			*state=NotSupported;
			break;
		case PAD_STATE_STABLE:

			padRead(pn,0,&buttons[pn]);
			//e orkprintf( "Input Poll %x\n",data);
			if ( data[0]!=0 )
			{
				//orkprintf("0\n");
				*state=Disconnected;
				break;
			}
			if (data[1]!=0x79)
			{
				//orkprintf("padid=%02x  ",data[1]);
				if ((data[1]&0x70)!=0x70)
				{
					if (data[1]!=0x41)
					{
						//orkprintf("n\n");
						*state=NotSupported;
						break;
					}
					//orkprintf("a\n");
					padSetMainMode(pn,0,1,3);
					*state=Busy;
					break;
				}
				if (padInfoPressMode(pn,0)!=1)
				{
					//orkprintf("n\n");
					*state=NotSupported;
					break;
				}
				//orkprintf("p\n");
				padEnterPressMode(pn,0);
				*state=Busy;
				break;
			}

			//if (*state!=Valid) orkprintf("v\n");
			*state=Valid;

			// get analog info
			switch( pn )
			{
				case 0:
				{

						S8 inputx =     (S8) (data[6]  - 128);
						 S8  inputy =     (S8)(data[7]  - 128);
						 S8  inputrx =    (S8)(data[4]  - 128) ;
						 S8  inputry =    (S8)(data[5]  - 128);






						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LANA_XAXIS, inputx ) ;
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LANA_YAXIS, inputy );
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RANA_XAXIS, inputrx );
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RANA_YAXIS, inputry );

						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LDIG_UP, data[10] ? 127 : 0 );
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LDIG_DOWN, data[11] ? 127 : 0 );
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LDIG_LEFT, data[ 9] ? 127 : 0 );
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_LDIG_RIGHT, data[ 8] ? 127 : 0 );

						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_UP, data[12] ? 127 : 0 );
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_DOWN, data[14] ? 127 : 0 );
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_LEFT, data[ 15] ? 127 : 0 );
						CInputManager::GetRef().SetTrigger(ETRIG_RAW_JOY0_RDIG_RIGHT, data[ 13] ? 127 : 0 );


						orkprintf( "Pad: left ana: x:%d y:%d : right ana: x:%d y:%d \n",data[6],data[7],data[4],data[5]);
						orkprintf( "Pad: left ana: x:%d y:%d : right ana: x:%d y:%d \n",inputx,inputy,inputrx,inputry);
						inputx = CInputManager::GetRef().GetTrigger(ETRIG_RAW_JOY0_LANA_XAXIS ) ;
						inputy = CInputManager::GetRef().GetTrigger(ETRIG_RAW_JOY0_LANA_YAXIS ) ;
						orkprintf( "From Pad: left ana: x:%d y:%d : right ana: x:%d y:%d \n",inputx,inputy,inputrx,inputry);



				}
			}


			{
				char *held=(char*)&pad->Held;
				char *lastheld=(char*)&pad->LastHeld;
				int i;

				for (i=0; i<28; i++)
				{
					*lastheld++=*held++;
				}
			}

			pad->Held[ L3 ] = ((data[2]&2)==0);
			pad->Held[ R3 ] = ((data[2]&4)==0);
			pad->Held[ Select ] = ((data[2]&1)==0);
			pad->Held[ Start ] = ((data[2]&8)==0);

			pad->Analog[ L3     ] = pad->Held[ L3     ] ? 1.0f : 0.0f;
			pad->Analog[ R3     ] = pad->Held[ R3     ] ? 1.0f : 0.0f;
			pad->Analog[ Select ] = pad->Held[ Select ] ? 1.0f : 0.0f;
			pad->Analog[ Start  ] = pad->Held[ Start  ] ? 1.0f : 0.0f;

			{
				char *held=(char*)&pad->Held;
				char *lastheld=(char*)&pad->LastHeld;
				char *pushed=(char*)&pad->Pushed;
				char *released=(char*)&pad->Released;
				float *analog=(float*)&pad->Analog;
				int i;

				for (i=0; i<28; i++)
				{
					if (i<16)
					{
						if (i<4)
						{
							*held=(fabsf(*analog)>=PadDeadZone);

							held[ LDeadX ] = *held;
							held[ LSteadyX ] = *held;

							// LDeadX etc.
							if (*held)
							{
								if (analog[0]<0.0f)
								{
									analog[LDeadX]=(analog[0]+PadDeadZone)/(1.0f-PadDeadZone);
								}
								else
								{
									analog[LDeadX]=(analog[0]-PadDeadZone)/(1.0f-PadDeadZone);
								}
							}
							else
							{
								analog[LDeadX]=0.0f;
							}

							// LSteadyX etc.
							if (fabsf(analog[0]-analog[LSteadyX])>PadSteadyZone)
							{
								analog[LSteadyX]=analog[0];
							}

							analog++;
						}
						else
						{
							*held=(*analog++!=0);
						}
					}

					*pushed++=*held & !*lastheld;
					*released++=!*held++ & *lastheld++;

				}

				if( pad->Held[ Select ] )
				{ // shift enables ShiftCount
					for (i=0; i<28; i++)
					{
						if( pad->Pushed[ i ] )
						{
							pad->ShiftCount[ i ]++;
						}
					}
				}
				else
				{ // no-shift enable TapCount
					for (i=0; i<28; i++)
					{
						if( pad->Pushed[ i ] )
						{
							pad->TapCount[ i ]++;
						}
					}
				}


			}

			break;
		default:
			//if (*state!=Busy) orkprintf("b\n");
			*state=Busy;
			break;
		}

		if (*state!=Valid)
		{
			memset(pad,0,sizeof(*pad));
		}
#endif

	}


}

void CInputDevicePS2::Input_Init(void)
{
}

void CInputDevicePS2::Input_Configure(void)
{
}


/*
loadelf: fname host:orkid_dbg_ps2dev.elf secname all
Input ELF format filename = host:orkid_dbg_ps2dev.elf
0 00200000 002d08bc ..............................................
Loaded, host:orkid_dbg_ps2dev.elf
start address 0x2000e0
gp address 00000000
Enter RegisterSingleton [Name CConsoleManager] [InitCB 003888a0]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CConsoleManager] [InitCB 003888a0]
CObject::CObject [Inst 005043e0] [Class CConsoleManager] [TAG 00000000]
Enter RegisterSingleton [Name CSystem] [InitCB 00388754]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CSystem] [InitCB 00388754]
CObject::CObject [Inst 005048d0] [Class CSystem] [TAG 01000000]
Enter RegisterSingleton [Name CEventManager] [InitCB 0033a714]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CEventManager] [InitCB 0033a714]
CObject::CObject [Inst 00504a10] [Class CEventManager] [TAG 02000000]
Enter RegisterSingleton [Name CManipManager] [InitCB 00338318]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CManipManager] [InitCB 00338318]
CObject::CObject [Inst 00505bc0] [Class CManipManager] [TAG 03000000]
Enter RegisterSingleton [Name CInputManager] [InitCB 00260604]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CInputManager] [InitCB 00260604]
CObject::CObject [Inst 00505f10] [Class CInputManager] [TAG 04000000]
Enter RegisterSingleton [Name ISceneManager] [InitCB 0020dc18]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name ISceneManager] [InitCB 0020dc18]
CObject::CObject [Inst 005062f0] [Class ISceneManager] [TAG 05000000]
Enter RegisterSingleton [Name CPerformanceTracker] [InitCB 00388a0c]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CPerformanceTracker] [InitCB 00388a0c]
CObject::CObject [Inst 00506410] [Class CPerformanceTracker] [TAG 06000000]
ClassmanInit:1
ClassmanInit:2
Enter RegisterSingleton [Name CGfxEnv] [InitCB 0033955c]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CGfxEnv] [InitCB 0033955c]
CObject::CObject [Inst 0050a6b0] [Class CGfxEnv] [TAG 28000000]
CObject::CObject [Inst 0050ac50] [Class CGfxMaterialUI] [TAG 0d000000]
CObject::CObject [Inst 0050acc0] [Class CGfxMaterial3DTest] [TAG 10000000]
ClassmanInit:3
Enter RegisterSingleton [Name CFileEnv] [InitCB 00355450]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CFileEnv] [InitCB 00355450]
CObject::CObject [Inst 0050ba30] [Class CFileEnv] [TAG 2d000000]
IO Init [0]
open name host:data\orkid_console.cfg flag 1 data 41378
open fd = 2
host:data\orkid_console.cfg Handle 2
fread [orkid_console.cfg] [to 0050bb40] [size 000002b2]
fclose orkid_console.cfg [fd 2]
addtoken System
addtoken {
addtoken setstring
addtoken modelengine
addtoken CModelEnginePickles
addtoken setstring
addtoken animengine
addtoken CAnimEnginePickles
addtoken setstring
addtoken MainCommand
addtoken pickles
addtoken }
addtoken PluginManager
addtoken {
addtoken plugin_load
addtoken orkid_pickles
addtoken plugin_load
addtoken orkid_gfxtex2txx
addtoken }
addtoken ClassManager
addtoken {
addtoken }
addtoken Console
addtoken {
addtoken }
Constructing CGfxTargetPS2
starting timer
Init IOP [0]
loadmodule: fname host:/usr/local/sce/iop/modules/sio2man.irx args 0 arg
loadmodule: id 33, ret 0
loadmodule: fname host:/usr/local/sce/iop/modules/padman.irx args 0 arg
loadmodule: id 34, ret 0
Initializing GS
Initialized GS.. [DmaChanGIF 1000a000] [DmaChanVIF0 10008000] [DmaChanVIF1 10009000]
Initializing Packet Buffers
LoadRawPacket [shaders/ps2/test.dat]
open name host:data\shaders\ps2\test.dat flag 1 data 41378
open fd = 2
host:data\shaders\ps2\test.dat Handle 2
size(bytes) 192
fread [shaders/ps2/test.dat] [to 0050be20] [size 000000c0]
pktdata 0050be20
fclose shaders/ps2/test.dat [fd 2]
inum64 24
Enter RegisterSingleton [Name CGfxPrimitives] [InitCB 00364ed8]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CGfxPrimitives] [InitCB 00364ed8]
CObject::CObject [Inst 0050bf30] [Class CGfxPrimitives] [TAG 2e000000]
Done
Requesting state MAIN MENU STATE
open name host:data\ork\files\fnts\fontgame.fnt flag 1 data 41378
open fd = 2
host:data\ork\files\fnts\fontgame.fnt Handle 2
fclose ork/FILES/FNTS/FONTGAME.FNT [fd 2]
open name host:data\ork\files\fnts\fontgame.fnt flag 1 data 41378
open fd = 2
host:data\ork\files\fnts\fontgame.fnt Handle 2
fread [ork/FILES/FNTS/FONTGAME.FNT] [to 00896b40] [size 0000043c]
fclose ork/FILES/FNTS/FONTGAME.FNT [fd 2]
Enter RegisterSingleton [Name CFontMan] [InitCB 00368138]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CFontMan] [InitCB 00368138]
CObject::CObject [Inst 00896f80] [Class CFontMan] [TAG 2f000000]
CObject::CObject [Inst 008a90e0] [Class CGfxMaterialUIText] [TAG 0e000000]
CObject::CObject [Inst 008bb1e0] [Class CGfxMaterialUIText] [TAG 0e000001]
CObject::CObject [Inst 008bb2e0] [Class CCamera_persp] [TAG 0b000000]
CObject::CObject [Inst 008bbad0] [Class CCamera_pickles] [TAG 29000000]
Enter RegisterSingleton [Name CCamMan] [InitCB 0033966c]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CCamMan] [InitCB 0033966c]
CObject::CObject [Inst 008bc2c0] [Class CCamMan] [TAG 30000000]
Entering PS2::MainLoop
PS2 Pad Library Open
Enter RegisterSingleton [Name CRendMan] [InitCB 0036b0b0]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CRendMan] [InitCB 0036b0b0]
CObject::CObject [Inst 008bc3b0] [Class CRendMan] [TAG 31000000]
Enter RegisterSingleton [Name CTexMan] [InitCB 0033b7a8]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CTexMan] [InitCB 0033b7a8]
CObject::CObject [Inst 008bcd00] [Class CTexMan] [TAG 32000000]
Enter RegisterSingleton [Name CShadMan] [InitCB 0025ebd8]
RegisterSingleton CP1
RegisterSingleton CP2
RegisterSingleton CP3
RegisterSingleton CP4
RegisterSingleton CP5
RegisterSingleton CP6
RegisterSingleton CP7
RegisterSingleton CP8
RegisterSingleton CP9
Exit RegisterSingleton [Name CShadMan] [InitCB 0025ebd8]
Initialising state MAIN MENU STATE
open name host:data\ork\files\fnts\fontgame.fnt flag 1 data 41378
open fd = 2
host:data\ork\files\fnts\fontgame.fnt Handle 2
fclose ork/FILES/FNTS/FONTGAME.FNT [fd 2]
open name host:data\ork\files\fnts\fontgame.fnt flag 1 data 41378
open fd = 2
host:data\ork\files\fnts\fontgame.fnt Handle 2
fread [ork/FILES/FNTS/FONTGAME.FNT] [to 00896b40] [size 0000043c]
fclose ork/FILES/FNTS/FONTGAME.FNT [fd 2]
current game state MAIN MENU STATE
*/

#endif

