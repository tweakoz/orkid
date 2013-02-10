////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#include <ork/pch.h>

#ifdef _WIN32

#include <ork/lev2/input/CInputDeviceDX.h>

#include <ork/lev2/input/CInputDeviceXInput.h>

#include <ork/kernel/CSystem.h>

#define SAFE_DELETE(p)	{ if(p) { delete (p);     (p)=NULL; } }
#define SAFE_RELEASE(p)	{ if(p) { (p)->Release(); (p)=NULL; } }

extern "C" void* __cdecl myalloc( unsigned int isize );
extern "C" void* __cdecl myrealloc( void* ptr, unsigned int isize );
extern "C" void __cdecl myfree( void* ptr );

void OrkCoInitialize();

#include <tchar.h>
#include <wbemidl.h>
#include <oleauto.h>

#pragma comment( lib, "dinput8.lib" )
#pragma comment( lib, "dxerr.lib" )
#pragma comment( lib, "dxguid.lib" )

#define SAMPLE_BUFFER_SIZE  16      // arbitrary number of buffer elements
GUID g_guidApp = { 0x67131584, 0x2938, 0x4857, { 0x8a, 0x2e, 0xd9, 0x9d, 0xc2, 0xc8, 0x20, 0x84} };

#define ACTION_MAPPING 0

// DirectInput action mapper reports events only when buttons/axis change
// so we need to remember the present state of relevant axis/buttons for
// each DirectInput device.  The CInputDeviceManager will store a
// pointer for each device that points to this struct

namespace ork { namespace lev2 {

CInputDX::~CInputDX()
{
	for(U32 i = 0; i < m_dwMaxDevices; i++)
	{
		if(m_pDevices[i].pdidDevice)
		{
			m_pDevices[i].pdidDevice->Unacquire(); //e just in case we have it..
			m_pDevices[i].pdidDevice->Release(); //e just in case we have it.......
		}

		SAFE_DELETE(m_pDevices[i].pParam);
	}

	myfree(m_pDevices);

	m_pDI->Release();
}

HRESULT CInputDX::AddDevice(const DIDEVICEINSTANCE *pdidi, const LPDIRECTINPUTDEVICE8 pdidDevice)
{
    DWORD dwDeviceType = pdidi->dwDevType;

    pdidDevice->Unacquire(); //e just in case we have it.......
	//e disable mouse and keyboard
    if((GET_DIDEVICE_TYPE(pdidi->dwDevType) == DI8DEVTYPE_KEYBOARD)
			|| (GET_DIDEVICE_TYPE(pdidi->dwDevType) == DI8DEVTYPE_MOUSE))
    	return S_OK;
	//e disable mouse and keyboard

    // Add new DeviceInfo struct to list, and resize array if needed
    m_dwNumDevices++;
    if(m_dwNumDevices > m_dwMaxDevices)
    {
        m_dwMaxDevices += 10;
		//if( m_pDevices ) myfree( m_pDevices );
        m_pDevices = (DeviceInfo*)myrealloc(m_pDevices,m_dwMaxDevices * sizeof(DeviceInfo));
        ZeroMemory(m_pDevices + m_dwMaxDevices - 10, 10 * sizeof(DeviceInfo));
    }

    DWORD dwCurrentDevice = m_dwNumDevices - 1;
    m_pDevices[dwCurrentDevice].pdidDevice = pdidDevice;

    // Check for relataive devices like mouse
    if(GET_DIDEVICE_TYPE(pdidi->dwDevType) == DI8DEVTYPE_MOUSE)
	{
	    DIPROPDWORD dipdw;
	    dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
	    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
	    dipdw.diph.dwObj        = 0;
	    dipdw.diph.dwHow        = DIPH_DEVICE;
	    dipdw.dwData            = DIPROPAXISMODE_REL;

		pdidDevice->SetProperty(DIPROP_AXISMODE, &dipdw.diph);
        m_pDevices[dwCurrentDevice].bRelativeAxis = TRUE;
	}

    // Continue enumerating suitable devices
    return S_OK;
}

void CInputDX::UnacquireDevices()
{
    for(DWORD i = 0; i < m_dwNumDevices; i++)
        m_pDevices[i].pdidDevice->Unacquire();
}

//-----------------------------------------------------------------------------
// Enum each PNP device using WMI and check each device ID to see if it contains 
// "IG_" (ex. "VID_045E&PID_028E&IG_00").  If it does, then it's an XInput device
// Unfortunately this information can not be found by just using DirectInput 
//-----------------------------------------------------------------------------
BOOL IsXInputDevice( const GUID* pGuidProductFromDirectInput )
{
    IWbemLocator*           pIWbemLocator  = NULL;
    IEnumWbemClassObject*   pEnumDevices   = NULL;
    IWbemClassObject*       pDevices[20]   = {0};
    IWbemServices*          pIWbemServices = NULL;
    BSTR                    bstrNamespace  = NULL;
    BSTR                    bstrDeviceID   = NULL;
    BSTR                    bstrClassName  = NULL;
    DWORD                   uReturned      = 0;
    bool                    bIsXinputDevice= false;
    UINT                    iDevice        = 0;
    VARIANT                 var;
    HRESULT                 hr;

	OrkCoInitialize();

    // Create WMI
    hr = CoCreateInstance( __uuidof(WbemLocator),
                           NULL,
                           CLSCTX_INPROC_SERVER,
                           __uuidof(IWbemLocator),
                           (LPVOID*) &pIWbemLocator);
    if( FAILED(hr) || pIWbemLocator == NULL )
        goto LCleanup;

    bstrNamespace = SysAllocString( L"\\\\.\\root\\cimv2" );if( bstrNamespace == NULL ) goto LCleanup;        
    bstrClassName = SysAllocString( L"Win32_PNPEntity" );   if( bstrClassName == NULL ) goto LCleanup;        
    bstrDeviceID  = SysAllocString( L"DeviceID" );          if( bstrDeviceID == NULL )  goto LCleanup;        
    
    // Connect to WMI 
    hr = pIWbemLocator->ConnectServer( bstrNamespace, NULL, NULL, 0L, 
                                       0L, NULL, NULL, &pIWbemServices );
    if( FAILED(hr) || pIWbemServices == NULL )
        goto LCleanup;

    // Switch security level to IMPERSONATE. 
    CoSetProxyBlanket( pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, 
                       RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE );                    

    hr = pIWbemServices->CreateInstanceEnum( bstrClassName, 0, NULL, &pEnumDevices ); 
    if( FAILED(hr) || pEnumDevices == NULL )
        goto LCleanup;

    // Loop over all devices
    for( ;; )
    {
        // Get 20 at a time
        hr = pEnumDevices->Next( 10000, 20, pDevices, &uReturned );
        if( FAILED(hr) )
            goto LCleanup;
        if( uReturned == 0 )
            break;

        for( iDevice=0; iDevice<uReturned; iDevice++ )
        {
            // For each device, get its device ID
            hr = pDevices[iDevice]->Get( bstrDeviceID, 0L, &var, NULL, NULL );
            if( SUCCEEDED( hr ) && var.vt == VT_BSTR && var.bstrVal != NULL )
            {
                // Check if the device ID contains "IG_".  If it does, then it's an XInput device
				    // This information can not be found from DirectInput 
                if( wcsstr( var.bstrVal, L"IG_" ) )
                {
                    // If it does, then get the VID/PID from var.bstrVal
                    DWORD dwPid = 0, dwVid = 0;
                    WCHAR* strVid = wcsstr( var.bstrVal, L"VID_" );
                    if( strVid && swscanf( strVid, L"VID_%4X", &dwVid ) != 1 )
                        dwVid = 0;
                    WCHAR* strPid = wcsstr( var.bstrVal, L"PID_" );
                    if( strPid && swscanf( strPid, L"PID_%4X", &dwPid ) != 1 )
                        dwPid = 0;

                    // Compare the VID/PID to the DInput device
                    DWORD dwVidPid = MAKELONG( dwVid, dwPid );
                    if( dwVidPid == pGuidProductFromDirectInput->Data1 )
                    {
                        bIsXinputDevice = true;
                        goto LCleanup;
                    }
                }
            }   
            SAFE_RELEASE( pDevices[iDevice] );
        }
    }

LCleanup:
    if(bstrNamespace)
        SysFreeString(bstrNamespace);
    if(bstrDeviceID)
        SysFreeString(bstrDeviceID);
    if(bstrClassName)
        SysFreeString(bstrClassName);
    for( iDevice=0; iDevice<20; iDevice++ )
        SAFE_RELEASE( pDevices[iDevice] );
    SAFE_RELEASE( pEnumDevices );
    SAFE_RELEASE( pIWbemLocator );
    SAFE_RELEASE( pIWbemServices );

    return bIsXinputDevice;
}

///////////////////////////////////////////////////////////////////////////////
// Name: EnumJoysticksCallback()
// Desc: Called once for each enumerated joystick. If we find one, create a
//       device interface on it so we can play with it.
///////////////////////////////////////////////////////////////////////////////

LPDIRECTINPUT8 g_pDI = 0;

BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE *pdidInstance
									, VOID* pContext)
{
	HRESULT hr;

	LPDIRECTINPUTDEVICE8 pJoystick = NULL;

    // Obtain an interface to the enumerated joystick.
    hr = g_pDI->CreateDevice(pdidInstance->guidInstance, &pJoystick, NULL);

#if 0
	if( strcmp( "Bluetooth HID Joystick", pdidInstance->tszProductName ) == 0 )
	{
		CSystem::SetGlobalStringVariable( "joyconfig", "wiimote" );
	}
	if( strcmp( "4 axis 16 button joystick", pdidInstance->tszProductName ) == 0 )
	{
		CSystem::SetGlobalStringVariable( "joyconfig", "ps2usb" );
	}

	if( strcmp( "Interact Gaming Device", pdidInstance->tszProductName ) == 0 )
	{
		CSystem::SetGlobalStringVariable( "joyconfig", "interact" );
		// Use Tweaks Laptop Joystick Config
	}
	else if( strcmp( "PSX/USB Pad", pdidInstance->tszProductName ) == 0 )
	{
		CSystem::SetGlobalStringVariable( "joyconfig", "ps2usb" );
		// Use PS2 USB Joystick Config
	}
#endif

	//DIDEVICEINSTANCE
    // If it failed, then we can't use this joystick. (Maybe the user unplugged
    // it while we were in the middle of enumerating it.)
    if(FAILED(hr))
        return DIENUM_CONTINUE;

    CInputDX *pInputManager = (CInputDX *)pContext;
    pInputManager->AddDevice(pdidInstance, pJoystick);

	pInputManager->SetDevice(0, pJoystick);

	// Stop enumeration. Note: we're just taking the first joystick we get. You
    // could store all the enumerated joysticks and let the user pick.
    return DIENUM_CONTINUE;
}

///////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK EnumSuitableDevicesCB(LPCDIDEVICEINSTANCE pdidi, LPDIRECTINPUTDEVICE8 pdidDevice, DWORD dwFlags
									, DWORD dwDeviceRemaining, VOID *pContext)
{
    // Add the device to the device manager's internal list
    CInputDX *pInputManager = (CInputDX *)pContext;
    pInputManager->AddDevice(pdidi, pdidDevice);

	return DIENUM_CONTINUE;
}

BOOL CALLBACK EnumSemanticDevicesCB(LPCDIDEVICEINSTANCE pdidi, LPDIRECTINPUTDEVICE8 pdidDevice, DWORD dwFlags
									, DWORD dwDeviceRemaining, VOID *pContext)
{
    // Add the device to the device manager's internal list
    CInputDX *pInputManager = (CInputDX *)pContext;
    pInputManager->AddDevice(pdidi, pdidDevice);

	return DIENUM_CONTINUE;
}

CInputDX::CInputDX()
	: m_pDevices( NULL )
{
    HRESULT hr;

	mpDevice0 = 0;
    m_dwNumDevices = 0;
    m_dwMaxDevices = 10;
	m_pDI = NULL;
	m_strUserName = NULL; //e 1 user anyhow so this set it to user that's logged in

    // Allocate DeviceInfo structs
    m_pDevices = (DeviceInfo *)myalloc(m_dwMaxDevices * sizeof(DeviceInfo));
    ZeroMemory(m_pDevices, m_dwMaxDevices * sizeof(DeviceInfo));

#if ACTION_MAPPING
    // Setup action format for the actual gameplay
    ZeroMemory( &m_diafGame, sizeof(DIACTIONFORMAT) );
    m_diafGame.dwSize          = sizeof(DIACTIONFORMAT);
    m_diafGame.dwActionSize    = sizeof(DIACTION);
    m_diafGame.dwDataSize      = NUMBER_OF_GAMEACTIONS * sizeof(DWORD);
    m_diafGame.guidActionMap   = g_guidApp;
    m_diafGame.dwGenre         = DIVIRTUAL_STRATEGY_ROLEPLAYING ;
    m_diafGame.dwNumActions    = NUMBER_OF_GAMEACTIONS;
    m_diafGame.rgoAction       = g_rgGameActionLanguage[0]; //Force  to English
    m_diafGame.lAxisMin        = -127;
    m_diafGame.lAxisMax        = 128;
    m_diafGame.dwBufferSize    = SAMPLE_BUFFER_SIZE;
    _tcscpy( m_diafGame.tszActionMap, _T("Igor: The Game") );
#endif

	/// INIT Direct Input
	if(FAILED(hr = DirectInput8Create(GetModuleHandle(NULL), DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID **)&m_pDI, NULL)))
		return;

	g_pDI = m_pDI;

	hr = m_pDI->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, this, DIEDFL_ATTACHEDONLY);

	if(FAILED(hr))
		return;

	/////////////////////////////////////////////////////////////////////////////////////
    // Set the data format to "simple joystick" - a predefined data format
    //
    // A data format specifies which controls on a device we are interested in,
    // and how they should be reported. This tells DInput that we will be
    // passing a DIJOYSTATE2 structure to IDirectInputDevice::GetDeviceState().
#if !ACTION_MAPPING
	if(mpDevice0)
		hr = mpDevice0->SetDataFormat(&c_dfDIJoystick2);
#endif

	if(FAILED(hr))
        return;

	/////////////////////////////////////////////////////////////////////////////////////
	for(int i = 0; i < GetNumDevices(); i++)
	{
		CInputDeviceDX *pref = new CInputDeviceDX();

#if ACTION_MAPPING
		hr = m_pDevices[i].pdidDevice->BuildActionMap( &m_diafGame, m_strUserName, 0 );
		if( FAILED( hr ) )
		   continue;
         hr = m_pDevices[i].pdidDevice->SetActionMap( &m_diafGame, m_strUserName, 0 );
        if( FAILED(hr) )
           continue;
#endif
		CInputManager::GetRef().AddDevice(pref);
		pref->mpdidDevice = m_pDevices[i].pdidDevice;
	}
	// Set the cooperative level to let DInput know how this device should
    // interact with the system and with other DInput applications.

	//hr = mpDevice0->SetCooperativeLevel(hDlg, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	//if(FAILED(hr))
		//return;
}

#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE  64

void CInputDeviceDX::Input_Poll()
{
	mInputState.BeginCycle();

	// clear them all :P
	for(int i = ETRIG_RAW_BEGIN; i <= ETRIG_RAW_END; ++i)
		mInputState.SetPressure(i, 0);

	HRESULT hr;
	DWORD dwObjCount = 10;
	FLOAT fScale = 128.0f;

	hr = mpdidDevice->Poll();
	if(FAILED(hr))
	{
		// Poll the device for data.
		hr = mpdidDevice->Acquire();
		while(hr == DIERR_INPUTLOST)
			hr = mpdidDevice->Acquire();

	    return;
	}

	static bool bisinteract = CSystem::GetGlobalStringVariable("joyconfig") == (std::string)"interact";
	static bool bisps2usb = CSystem::GetGlobalStringVariable("joyconfig") == (std::string)"ps2usb";
	static bool biswiimote = CSystem::GetGlobalStringVariable("joyconfig") == (std::string)"wiimote";

	if(bisinteract)
	{
		DIJOYSTATE2 js; // DInput joystick state

		if(FAILED(hr = mpdidDevice->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
			return;

		mInputState.SetPressure(ETRIG_RAW_JOY0_LANA_XAXIS, (S8)(js.lX >> 8) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, (S8)(255 - (js.lY >> 8)) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_RANA_XAXIS, (S8)(255 - (js.lZ >> 8)) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_RANA_YAXIS, (S8)(js.lRz >> 8) - 128 );

		if(js.rgbButtons[0] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_LEFT, 127);
		if(js.rgbButtons[1] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_UP, 127);
		if(js.rgbButtons[3] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_DOWN, 127);
		if(js.rgbButtons[4] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_RIGHT, 127);

		if(js.rgbButtons[6] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_L1, 127);
		if(js.rgbButtons[7] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_R1, 127);
		if(js.rgbButtons[8] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_L2, 127);
		if(js.rgbButtons[9] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_R2, 127);

		if(js.rgbButtons[10] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_SELECT, 127);
		if(js.rgbButtons[11] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_START, 127);
	}
	else if(bisps2usb)
	{
		DIJOYSTATE2 js; // DInput joystick state

		if(FAILED(hr = mpdidDevice->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
			return;

		// Digital //////////////////////////////////////////////////////////////////

		// left side directional buttons
		if(js.rgbButtons[0] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_UP, 127);
		if(js.rgbButtons[1] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_RIGHT, 127);
		if(js.rgbButtons[2] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_DOWN, 127);
		if(js.rgbButtons[3] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_LEFT, 127);

		// trigger buttons
		if(js.rgbButtons[4] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_L2, 127);
		if(js.rgbButtons[5] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_R2, 127);
		if(js.rgbButtons[6] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_L1, 127);
		if(js.rgbButtons[7] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_R1, 127);

		// central buttons
		if(js.rgbButtons[8] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_SELECT, 127);
		// todo -- Lstick down
		if(js.rgbButtons[9] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_L3, 127);
		if(js.rgbButtons[10] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_R3, 127);
		// todo -- Rstick down
		if(js.rgbButtons[11] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_START, 127);

		// right side directional buttons
		if(js.rgbButtons[12] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_UP, 127);
		if(js.rgbButtons[13] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_RIGHT, 127);
		if(js.rgbButtons[14] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_DOWN, 127);
		if(js.rgbButtons[15] >> 7)
			mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_LEFT, 127);

		// Analog //////////////////////////////////////////////////////////
		mInputState.SetPressure(ETRIG_RAW_JOY0_LANA_XAXIS, (S8)(js.lX >> 8) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, (S8)(255 - (js.lY >> 8)) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_RANA_XAXIS, (S8)(js.lZ >> 8) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_RANA_YAXIS, (S8)(255 - (js.lRz >> 8)) - 128);
	}
	else
	{
		DIJOYSTATE2 js; // DInput joystick state

		if(FAILED(hr = mpdidDevice->GetDeviceState(sizeof(DIJOYSTATE2), &js)))
			return;

		// use this code to determine what button does what; just break inside conditional
		//static DIJOYSTATE2 lastJS = js;
		//for(int i = 0; i < 128; i++)
		//	if(js.rgbButtons[i] != lastJS.rgbButtons[i])
		//		lastJS.rgbButtons[i] = js.rgbButtons[i];

		//orkprintf("%d %d \n",(js.lX >> 8),(js.lY >> 8));
		// Zero value if thumbsticks are within the dead zone
		if( ( std::abs((js.lX >> 8) - 128) <  XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) &&
			(  std::abs((js.lY >> 8) - 128) <  XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) )
		{
		   js.lX = 128 << 8;
		   js.lY = 128 << 8;
		}
		if( ( std::abs((js.lRx >> 8) - 128) <  XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) &&
			(  std::abs((js.lRy >> 8) - 128) <  XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) )
		{
		   js.lRx = 128 << 8;
		   js.lRy = 128 << 8;
		}

		if( std::abs(std::abs((js.lZ >> 8) - 128) <  XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE ) )
		{
			js.lZ = 128 << 8;
		}

		mInputState.SetPressure(ETRIG_RAW_JOY0_ANA_ZAXIS, -(S8)((U8)(js.lZ *(127.0f/32768.0f))-127) );

		mInputState.SetPressure(ETRIG_RAW_JOY0_LANA_XAXIS, (S8)(js.lX >> 8) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_LANA_YAXIS, (S8)( (js.lY >> 8)) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_RANA_XAXIS, (S8)(js.lRx >> 8) - 128);
		mInputState.SetPressure(ETRIG_RAW_JOY0_RANA_YAXIS, (S8)( (js.lRy >> 8)) - 128);

		u8 rdown = (js.rgbButtons[0]>>7) ? 127 : 0;
		u8 rright = (js.rgbButtons[1]>>7) ? 127 : 0;
		u8 rleft = (js.rgbButtons[2]>>7) ? 127 : 0;
		u8 rup = (js.rgbButtons[3]>>7) ? 127 : 0;

		u8 back = (js.rgbButtons[6]>>7) ? 127 : 0;
		u8 start = (js.rgbButtons[7]>>7) ? 127 : 0;

		//orkprintf( "rdown<%d> rright<%d> rleft<%d> rup<%d>\n", int(rdown), int(rright), int(rleft), int(rup) );

		mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_DOWN, rdown );
		mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_RIGHT, rright );
		mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_LEFT, rleft );
		mInputState.SetPressure(ETRIG_RAW_JOY0_RDIG_UP, rup );

		mInputState.SetPressure(ETRIG_RAW_JOY0_BACK, back );
		mInputState.SetPressure(ETRIG_RAW_JOY0_START, start );

		u8 ldown = (JOY_POVBACKWARD == js.rgdwPOV[0]) ? 127 : 0;
		u8 lright = (JOY_POVRIGHT == js.rgdwPOV[0]) ? 127 : 0;
		u8 lleft = (JOY_POVLEFT == js.rgdwPOV[0]) ? 127 : 0;
		u8 lup = (JOY_POVFORWARD == js.rgdwPOV[0]) ? 127 : 0;

		//orkprintf( "ldown<%d> lright<%d> lleft<%d> lup<%d>\n", int(ldown), int(lright), int(lleft), int(lup) );

		mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_DOWN, ldown );
		mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_RIGHT, lright );
		mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_LEFT, lleft );
		mInputState.SetPressure(ETRIG_RAW_JOY0_LDIG_UP, lup );
	}

	mInputState.EndCycle();
}

CInputDX CInputDX::sInstance;

void CInputDX::Input_Configure()
{
    HRESULT hr;

    // Initialize all the colors here
    DICOLORSET dics;
	DWORD dwFlags;

	dwFlags = DICD_EDIT;
    ZeroMemory(&dics, sizeof(DICOLORSET));
    dics.dwSize = sizeof(DICOLORSET);

	// Fill in all the params
    DICONFIGUREDEVICESPARAMS dicdp;
    ZeroMemory(&dicdp, sizeof(dicdp));
    dicdp.dwSize = sizeof(dicdp);
    dicdp.dwcFormats     = 1;
    dicdp.lprgFormats    = &m_diafGame;
    dicdp.hwnd           = NULL;
    dicdp.lpUnkDDSTarget = NULL;

    if(m_strUserName)
    {
        dicdp.dwcUsers       = 1;
        dicdp.lptszUserNames = m_strUserName;
    }

    // Unacquire the devices so that mouse doesn't control the game while in control panel
    UnacquireDevices();

	while(ShowCursor(TRUE) < 0);
    hr = m_pDI->ConfigureDevices(NULL, &dicdp, dwFlags, NULL);
    if(FAILED(hr))
        return;

    if(dwFlags & DICD_EDIT)
    {
        // Re-set up the devices
        UnacquireDevices();

        // Apply the new action map to the current devices.
        for(DWORD i = 0; i < m_dwNumDevices; i++)
        {
            hr = m_pDevices[i].pdidDevice->BuildActionMap(&m_diafGame, m_strUserName, 0);
            if(FAILED(hr))
                return;

            hr = m_pDevices[i].pdidDevice->SetActionMap(&m_diafGame, m_strUserName, 0);
            if(FAILED(hr))
                return;
        }
    }
}

} }

#endif
