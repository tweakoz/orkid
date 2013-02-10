////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#if defined(WII)
#include <ork/lev2/gfx/gfxenv.h>
#include <ork/lev2/gfx/gfxctxdummy.h>
#include <ork/lev2/gfx/texman.h>
#include <ork/lev2/gfx/shadman.h>
#include <ork/mem/wii_mem.h>
#include <ork/file/fileenv.h>
#include <ork/file/filedev.h>

//#include <ork/mem/vrammanager.h>
#include	<revolution.h>
#include	<math.h>
#include	<revolution/wpad.h>
#include	<ork/lev2/input/CInputDeviceWii.h>
#include <ork/lev2/input/CInputDeviceDummy.h>

/***************************************************************
	To show KPAD DPD callback status
 ***************************************************************/
typedef struct MyDpdCallbackStatus {
    u32 count;
    s32 reason;
} MyDpdCallbackStatus;

#define WPAD 0

#define MY_DPD_START_COUNTS_MAX 4
#define MY_DPD_START_COUNT_DEFAULT 400
s32                 MyDpdCallbackLatestIdx;
MyDpdCallbackStatus MyDpdCallbackStatusBuf[MY_DPD_START_COUNTS_MAX];

vu32 MySamplingCount;
/***************************************************************
	Controller
 ***************************************************************/


PADStatus pads[PAD_MAX_CONTROLLERS];
PADStatus padsTmp[PAD_MAX_CONTROLLERS];
PADStatus padsTrig[PAD_MAX_CONTROLLERS];
#define SMPBUF_SIZE   100   // Size of WPADStatus array for auto sampling.


// ring buffer to store data from auto-sampling feature
static WPADStatus s_ringBuf[SMPBUF_SIZE];
/***************************************************************
	KPAD DPD callback
 ***************************************************************/
static void dpd_callback( s32 chan, s32 reason )
{

#pragma unused(chan)
    u32 i;
    u32 smallest = MY_DPD_START_COUNT_DEFAULT;
    u32 smallestIdx = 0;

    for (i = 0; i < MY_DPD_START_COUNTS_MAX; i++) {
        if (MyDpdCallbackStatusBuf[i].count == 0) {
            break;
        } else {
            if (smallest > MyDpdCallbackStatusBuf[i].count) {
                smallest = MyDpdCallbackStatusBuf[i].count;
                smallestIdx = i;
            }
        }
    }
    if (i == MY_DPD_START_COUNTS_MAX) {
        i = smallestIdx;
    }

    MyDpdCallbackStatusBuf[i].count = MY_DPD_START_COUNT_DEFAULT;
    MyDpdCallbackStatusBuf[i].reason = reason;
    MyDpdCallbackLatestIdx = (s32)i;
}

static void sampling_callback( s32 chan )
{
#pragma unused(chan)
    MySamplingCount++;
}





namespace ork { namespace lev2 {

/***************************************************************
 Adjustment item
***************************************************************/
static f32 pointing_scale = 200.0f ; // Screen pointing scale

static f32 obj_interval     = 0.20f ; // TV side light emitting point placement interval (in meters)

static f32 pos_play_radius  = 0.00f ; // 'pos' sensitivity setting
static f32 pos_sensitivity  = 1.00f ;
static f32 pos_play_mode    = (f32)KPAD_PLAY_MODE_LOOSE ;
static f32 hori_play_radius = 0.00f ; // 'horizon' sensitivity setting
static f32 hori_sensitivity = 1.00f ;
static f32 hori_play_mode   = (f32)KPAD_PLAY_MODE_LOOSE ;
static f32 dist_play_radius = 0.00f ; // 'dist' sensitivity setting
static f32 dist_sensitivity = 1.00f ;
static f32 dist_play_mode   = KPAD_PLAY_MODE_LOOSE ;
static f32 acc_play_radius  = 0.00f ; // 'acc' sensitivity setting
static f32 acc_sensitivity  = 1.00f ;
static f32 acc_play_mode    = KPAD_PLAY_MODE_LOOSE ;

static f32 repeat_delay_sec = 0.75f ; // Key repeat settings
static f32 repeat_pulse_sec = 0.25f ;


static void *myAlloc(u32 size)
{
    void *ptr;

	ptr = (void *) (void*) ork::wii::MEM2Alloc( size + 64);
    OrkAssert(ptr);

    return(ptr);

} // myAlloc()

/*---------------------------------------------------------------------------*
 * Name        : myFree()
 * Description : Callback needed by WPAD to free mem from MEM2 heap
 * Arguments   : None.
 * Returns     : Always 1.
 *---------------------------------------------------------------------------*/
static u8 myFree	(void *ptr)
{

    //e delete( (char *)ptr);

    // we should ensure that memory is free'd properly, but oh well
    return(1);

} // myFree()

///////////////////////////////////////////////////////////////////////////////


CInputWii::CInputWii()
{
	s32 wpad_state;
    s32 status;
    u32 type;
    u32 index;

	//eWPADStatus wpad;
    //e memset( (void *)(&wpad), 0, sizeof(WPADStatus));

	//orkprintf("created Dummy Input Device\n");
	WPADRegisterAllocator(myAlloc, myFree);

#if WPAD
	WPADInit();
	do
    {
       wpad_state = WPADGetStatus();
    } while (WPAD_STATE_SETUP != wpad_state);
#else
    KPADInit() ;		// Controller
    KPADSetControlDpdCallback(0, dpd_callback);
    KPADSetSamplingCallback(0, sampling_callback);

    KPADReset() ;
    KPADSetPosParam ( 0,  pos_play_radius,  pos_sensitivity ) ;
    KPADSetHoriParam( 0, hori_play_radius, hori_sensitivity ) ;
    KPADSetDistParam( 0, dist_play_radius, dist_sensitivity ) ;
    KPADSetAccParam ( 0,  acc_play_radius,  acc_sensitivity ) ;
    KPADSetBtnRepeat( 0, repeat_delay_sec, repeat_pulse_sec ) ;


	PADInit();

	KPADDisableStickCrossClamp() ;
	WPADSetAutoSleepTime(5);

#endif


}




CInputDeviceWii::CInputDeviceWii(int channel) : CInputDevice()
	 , m_channel(channel)
	 , mdisconnectframe(0)
{


	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_UP, (int) ETRIG_RAW_JOY0_LDIG_UP );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_DOWN, (int) ETRIG_RAW_JOY0_LDIG_DOWN );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_LEFT, (int) ETRIG_RAW_JOY0_LDIG_LEFT );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_RIGHT, (int) ETRIG_RAW_JOY0_LDIG_RIGHT );

	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_A, (int) ETRIG_RAW_ALPHA_A );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_Z, (int) ETRIG_RAW_ALPHA_Z );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_B, (int) ETRIG_RAW_ALPHA_B );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_C, (int) ETRIG_RAW_ALPHA_C );

	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_1, (int) ETRIG_RAW_NUMBER_1 );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_2, (int) ETRIG_RAW_NUMBER_2 );


	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_PLUS, (int) ETRIG_RAW_KEY_PLUS );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_MINUS, (int) ETRIG_RAW_KEY_MINUS );
	OrkSTXMapInsert( mInputMap, WPAD_BUTTON_HOME, (int) ETRIG_RAW_JOY0_START );
	OrkSTXMapInsert( mInputMap, 0x10000, (int) ETRIG_RAW_DISCONNECT );
	//OrkSTXMapInsert( mInputMap, ETRIG_RAW_JOY0_LANA_XAXIS, (int) ETRIG_RAW_JOY0_LANA_XAXIS );
	//OrkSTXMapInsert( mInputMap, ETRIG_RAW_JOY0_LANA_YAXIS, (int) ETRIG_RAW_JOY0_LANA_YAXIS );




}


CInputDeviceWii::~CInputDeviceWii()
{

}

///////////////////////////////////////////////////////////////////////////////


void CInputDeviceWii::Input_Poll()
{
	mConnectionStatus = CONN_STATUS_ACTIVE;

	MCheckPointContext( "CInputDeviceWii::Input_Poll" );

	InputState& inpstate = RefInputState();
	inpstate.BeginCycle();

	s32 status;
    u32 type;
    u32 index;


/*
	ork::FileProgressWatcher *fw =  ork::CFileEnv::GetRef().GetDefaultDevice()->GetWatcher();
	if(fw) {
		fw->SetEnable( true);
		fw->Reading( 0, 1 );
		fw->SetEnable( false);
	}
*/
	//

	/*
	PADRead( padsTmp ) ;
	if ( padsTmp[ 0 ].err == PAD_ERR_NONE ) {
	pads[ 0 ] = padsTmp[ 0 ] ;
	}
	padsTrig[ 0 ].button = (u16)( (padsTrig[ 0 ].button ^ pads[ 0 ].button) & pads[ 0 ].button ) ;
	 */
	for( orkmap<int,int>::const_iterator it= mInputMap.begin(); it!=mInputMap.end(); it++ )
	{
		std::pair<int,int> Value = *it;
		int ikey = Value.first;
		int iout = Value.second;
		inpstate.SetPressure( iout, 0 );
	}

	KPADDisableStickCrossClamp() ;
	KPADDisableDPD(m_channel);




	if (WPADProbe(m_channel, NULL) == WPAD_ERR_NO_CONTROLLER) {

		if(mdisconnectframe == 10)
			inpstate.SetPressure( ETRIG_RAW_DISCONNECT, 127);
		else
			mdisconnectframe++;

		inpstate.EndCycle();
		return;
	}

	mkpad_reads = KPADRead( m_channel, &mkpad[0], KPAD_MAX_READ_BUFS ) ;
//For some reason when you reconnect you have your state in there yuck!
    if(mdisconnectframe) {
		mdisconnectframe = 0;
		for (int i=0;i< KPAD_MAX_READ_BUFS;i++)
			mkpad[i].hold = 0;
		inpstate.EndCycle();
		return;
	}

	for( orkmap<int,int>::const_iterator it=mInputMap.begin(); it!=mInputMap.end(); it++ )
	{
		std::pair<int,int> Value = *it;
		int ikey = Value.first;
		int iout = Value.second;

		bool bkey = mkpad[0].hold & ikey ;

		if ( bkey)
		{
			if(mkpad[0].dev_type != WPAD_DEV_FREESTYLE)
			{
				U32 outrotated = iout;


				switch( iout )
				{
					case ETRIG_RAW_JOY0_LDIG_LEFT:
						outrotated = ETRIG_RAW_JOY0_LDIG_DOWN;
						break;
					case ETRIG_RAW_JOY0_LDIG_RIGHT:
						outrotated = ETRIG_RAW_JOY0_LDIG_UP;
						break;
					case ETRIG_RAW_JOY0_LDIG_DOWN:
						outrotated = ETRIG_RAW_JOY0_LDIG_RIGHT;
						break;
					case ETRIG_RAW_JOY0_LDIG_UP:
						outrotated = ETRIG_RAW_JOY0_LDIG_LEFT;
						break;
					//default:

				}
				inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_XAXIS, 0.0f);
				inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, 0.0f);

				inpstate.SetPressure( outrotated, 127);

				if(outrotated == 144)
					OSReport("IP: %x\n",(void *) &inpstate);

			}
			else
			{
				inpstate.SetPressure( iout, 127);
			}
		}

		if((mkpad[0].wpad_err == WPAD_ERR_NONE))
		{
			if(mkpad[0].dev_type == WPAD_DEV_FREESTYLE)
			{
				#define BOOL_TO_S8(v)		(S8)((v) ? 127 : 0)
				#define F32NORM_TO_S8(v)	(S8)((v) * 127.0f)

				S8 rdown = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_DOWN);
				S8 rup = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_UP);
				S8 rleft = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_LEFT);
				S8 rright = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_RIGHT);

				inpstate.SetPressure(ETRIG_RAW_JOY0_RDIG_DOWN, rdown);
				inpstate.SetPressure(ETRIG_RAW_JOY0_RDIG_UP, rup);
				inpstate.SetPressure(ETRIG_RAW_JOY0_RDIG_LEFT, rleft);
				inpstate.SetPressure(ETRIG_RAW_JOY0_RDIG_RIGHT, rright);

				S8 lthumbX = F32NORM_TO_S8(mkpad[0].ex_status.fs.stick.x);
				S8 lthumbY = F32NORM_TO_S8(mkpad[0].ex_status.fs.stick.y);

				inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_XAXIS, lthumbX);
				inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, lthumbY);

				S8 bA = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_A);
				S8 bB = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_B);
				S8 b1 = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_1);
				S8 b2 = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_2);

				inpstate.SetPressure(ETRIG_RAW_ALPHA_A, bA);
				inpstate.SetPressure(ETRIG_RAW_ALPHA_B, bB);
				inpstate.SetPressure(ETRIG_RAW_NUMBER_1, b1);
				inpstate.SetPressure(ETRIG_RAW_NUMBER_2, b2);

				S8 bPLUS = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_PLUS);
				S8 bMINUS = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_MINUS);

				inpstate.SetPressure(ETRIG_RAW_KEY_PLUS, bPLUS);
				inpstate.SetPressure(ETRIG_RAW_KEY_MINUS, bMINUS);

				S8 bC = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_C);
				S8 bZ = BOOL_TO_S8(mkpad[0].hold & KPAD_BUTTON_Z);
				
				inpstate.SetPressure(ETRIG_RAW_ALPHA_C, bC);
				inpstate.SetPressure(ETRIG_RAW_ALPHA_Z, bZ);

#if 0
				static int accum = 0;
				accum++;
				if(accum >= 128)
				{
					accum = 0;
					orkprintf("rleft<%d> rright<%d> rup<%d> rdown<%d>\n", int(rleft), int(rright), int(rup), int(rdown));
					orkprintf("lthumbX<%d> lthumbY<%d>\n", int(lthumbX), int(lthumbY));
					orkprintf("A<%d> B<%d> 1<%d> 2<%d>\n", int(bA), int(bB), int(b1), int(b2));
					orkprintf("+<%d> -<%d>\n", int(bPLUS), int(bMINUS));
					orkprintf("C<%d> Z<%d>\n", int(bC), int(bZ));
				}
#endif
			}
			else if (mkpad[0].dev_type == WPAD_DEV_CLASSIC)
			{
				F32 zaxis = 0.0f;
				/////////////////////////////////////////////////////
				// Make the 2 triggers behave like the crappy 360 PC controller
				if(mkpad[0].ex_status.cl.rtrigger >= 0.1f)
					zaxis += mkpad[0].ex_status.cl.rtrigger;
				if(mkpad[0].ex_status.cl.ltrigger >= 0.1f)
					zaxis -= mkpad[0].ex_status.cl.ltrigger;

				inpstate.SetPressure(ETRIG_RAW_JOY0_ANA_ZAXIS, zaxis * 127.0f);


				/////////////////////////////////////////////////////
				inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_XAXIS, mkpad[0].ex_status.cl.lstick.x * 127.0f);
				inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, -mkpad[0].ex_status.cl.lstick.y * 127.0f);

				inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_XAXIS, mkpad[0].ex_status.cl.rstick.x * 127.0f);
				inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_YAXIS, -mkpad[0].ex_status.cl.rstick.y * 127.0f);

				/////////////////////////////////////////////////////
				if(mkpad[0].ex_status.cl.lstick.y >= 0.8f)
					inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_UP, 127.0f);
				else
					inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_UP, 0.0f);

				if(mkpad[0].ex_status.cl.lstick.y <= -0.8f)
					inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_DOWN, 127.0f);
				else
					inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_DOWN, 0.0f);

				if(mkpad[0].ex_status.cl.lstick.x <= -0.8f)
					inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_LEFT, 127.0f);
				else
					inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_LEFT, 0.0f);

				if(mkpad[0].ex_status.cl.lstick.y >= 0.8f)
					inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_RIGHT, 127.0f);
				else
					inpstate.SetPressure(ETRIG_RAW_JOY0_LANA_RIGHT, 0.0f);

				/////////////////////////////////////////////////////
/*
				if(mkpad[0].ex_status.cl.rstick.y >= 0.8f)
					inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_UP, 127.0f);
				else
					inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_UP, 0.0f);

				if(mkpad[0].ex_status.cl.rstick.y <= -0.8f)
					inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_DOWN, 127.0f);
				else
					inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_DOWN, 0.0f);

				if(mkpad[0].ex_status.cl.rstick.x <= -0.8f)
					inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_LEFT, 127.0f);
				else
					inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_LEFT, 0.0f);

				if(mkpad[0].ex_status.cl.rstick.y >= 0.8f)
					inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_RIGHT, 127.0f);
				else
					inpstate.SetPressure(ETRIG_RAW_JOY0_RANA_RIGHT, 0.0f);

  */

			}





		}
	}

	inpstate.EndCycle();

	return;
}

void CInputManager::CreateInputDevices()
{
	new CInputWii;

	for(unsigned int i = 0; i < 4; i++)
		CInputManager::GetRef().AddDevice(new CInputDeviceWii(i));
}

} }

#endif
