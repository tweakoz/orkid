////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef ORK_LEV2_INPUTDEVICEDX_H
#define ORK_LEV2_INPUTDEVICEDX_H

#if defined(_WIN32)

#include <ork/lev2/input/input.h>

#define DIRECTINPUT_VERSION         0x0800

#include <dinput.h>

namespace ork { namespace lev2 {

class CInputDeviceDX : public ork::lev2::CInputDevice
{
	friend class CInputDX;

public:
    virtual void Input_Poll();
private:
	LPDIRECTINPUTDEVICE8 mpdidDevice;
};

class CInputDX
{
public:
	
    CInputDX();
	~CInputDX();
	void Input_Configure();
	VOID UnacquireDevices();
	HRESULT AddDevice(const DIDEVICEINSTANCE *pdidi, const LPDIRECTINPUTDEVICE8 pdidDevice);

	void SetDevice(int idx, LPDIRECTINPUTDEVICE8 pdev) { mpDevice0 = pdev; }
	int GetNumDevices() const { return m_dwNumDevices; }
	HRESULT InitDirectInput();

	static CInputDX& GetRef() { return sInstance; }

private:
	static CInputDX	sInstance;

    struct DeviceInfo
    {
        LPDIRECTINPUTDEVICE8 pdidDevice;
        LPVOID pParam;
        BOOL bRelativeAxis; // TRUE if device is using relative axis //mouse
    };

    DIACTIONFORMAT m_diafGame;             // Action format for game play

	LPDIRECTINPUT8 m_pDI;
    TCHAR *m_strUserName;

    LPDIRECTINPUTDEVICE8 mpDevice0;

    DeviceInfo *m_pDevices;
    DWORD m_dwMaxDevices;
    DWORD m_dwNumDevices;

};

} }

#endif // _WIN32

#endif // ORK_LEV2_INPUTDEVICEDX_H
