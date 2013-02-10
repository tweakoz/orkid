// Wiim API �2006 Eric B.
// http://digitalretrograde.com/projects/wiim/

// May be used and modified freely as long as this message is left intact

//#include "stdafx.h"
//#include "assert.h"
#if !defined(WII) && !defined(IX)

#include <ork/pch.h>

#include <wiim/Wiimote.h>
#include <wiim/Utils.h>

#include <iostream>
#include <ork/math/basicfilters.h>

using namespace std;

Wiimote::Wiimote()
{
    // Add all Wii buttons to the our internal list
    m_buttons.push_back(Button("1",                0x0002));
    m_buttons.push_back(Button("2",                0x0001));
    m_buttons.push_back(Button("A",                0x0008));
    m_buttons.push_back(Button("B",                0x0004));
    m_buttons.push_back(Button("+",                0x1000));
    m_buttons.push_back(Button("-",                0x0010));
    m_buttons.push_back(Button("Home",        0x0080));
    m_buttons.push_back(Button("Up",        0x0800));
    m_buttons.push_back(Button("Down",        0x0400));
    m_buttons.push_back(Button("Right", 0x0200));
    m_buttons.push_back(Button("Left",        0x0100));
    m_buttons.push_back(Button("NunZ",        0x2000));//e added by eggy
    m_buttons.push_back(Button("NunC",        0x4000));//e added by eggy

    m_leds.push_back(LED(1));
    m_leds.push_back(LED(2));
    m_leds.push_back(LED(3));
    m_leds.push_back(LED(4));

    m_input_listening = false;

    m_listener_thread_id = 0xffffffff;
    m_listener_thread = NULL;

    m_controller_id = -1;
	m_received_calibration = -1;
	m_nunchuck_analogx = 0;
	m_nunchuck_analogy = 0;
	m_nunchuckconnected = false;
}

void Wiimote::SetLEDs(LED::LEDSet & set)
{
    SetLEDs(set.led1, set.led2, set.led3, set.led4);
}

void Wiimote::SetLEDs(bool led1, bool led2, bool led3, bool led4)
{
    unsigned char out = 0;

    if (led1) out |= 0x10;
    if (led2) out |= 0x20;
    if (led3) out |= 0x40;
    if (led4) out |= 0x80;

    unsigned char rpt[2] = { 0x11, out };

    WriteOutputReport(rpt);
}

void Wiimote::SetRumble(bool on)
{
    unsigned char rpt[2] = { 0x13, on ? 0x01 : 0x00 };

    WriteOutputReport(rpt);
}

unsigned CALLBACK Wiimote::InputReportListener(void * param)
{
    Wiimote * device = reinterpret_cast<Wiimote*>(param);

    while (device->m_input_listening)
    {

        unsigned char input_report[256];
        ZeroMemory(input_report, 256);

        DWORD result, bytes_read;

        // Issue a read request
        if (device->m_read_handle != INVALID_HANDLE_VALUE)
        {
            result = ReadFile(device->m_read_handle,
                    input_report,
                    device->m_capabilities.InputReportByteLength,
                    &bytes_read,
                    (LPOVERLAPPED)&device->m_hid_overlapped);
        }

        // Wait for read to finish
        result = WaitForSingleObject(device->m_event_object, 300);

        ResetEvent(device->m_event_object);

        // If the wait didn't result in a sucessful read, try again
		if (result != WAIT_OBJECT_0) {
			device->m_received_calibration =1;
            continue;

		}
		device->m_received_calibration =2;

		 if (INPUT_STATUS_DATA == input_report[0])
        {
			
            int encrypted_byte = Util::GetInt2(input_report, 6);
			int decrypted_byte = (encrypted_byte ^ 0x17) + 0x17;
			if(decrypted_byte & 0x2)
				device->m_nunchuckconnected = true;
			 else
				device->m_nunchuckconnected = false;


		}
        if (INPUT_READ_DATA == input_report[0])
        {
            int encrypted_byte = Util::GetInt2(input_report, 6);
			//int decrypted_byte = (encrypted_byte ^ 0x17) + 0x17;
			if(encrypted_byte == 0xfefe)
				device->m_nunchuckconnected = true;
			else
				device->m_nunchuckconnected = false;
			
			device->ReceivedNumChuckReport();

		}
        else if (INPUT_REPORT_BUTTONS == input_report[0])
        {
		
            int key_state = Util::GetInt2(input_report, 1);

            vector<Button> changed = device->SetButtons(key_state);

            for (int i = 0; i < int(changed.size()); i++)
                    changed[i].Pressed() ? device->ButtonPressed(changed[i]) : device->ButtonReleased(changed[i]);
        }
        else if (INPUT_REPORT_MOTION == input_report[0])
        {
            static ork::avg_filter<8> filtx, filty, filtz;
            filtx.mbEnable = false;
            filty.mbEnable = false;
            filtz.mbEnable = false;

            float fx = filtx.compute( float(input_report[3]) );
            float fy = filty.compute( float(input_report[4]) );
            float fz = filtz.compute( float(input_report[5]) );

            MotionData m( (unsigned char)(fx), (unsigned char)(fy),  (unsigned char)(fz) );

/*
			  0x35: Core Buttons and Accelerometer with 16 Extension Bytes
    		  This mode returns data from the buttons and accelerometer in the Wiimote, and data from an extension controller connected to it:
			  35 BB BB AA AA AA EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE EE
			  BBBB is main wiimote button data AAAAA is accelerometer data and the EE extension data
 *            http://wiibrew.org/index.php?title=Wiimote#Memory_and_Registers
 */

		
			int key_state = Util::GetInt2(input_report, 1);
	

			u8 encrypted_byte = 0xf3;
			u8 decrypted_byte = (encrypted_byte ^ 0x17) + 0x17 ;

			if(device->m_nunchuckconnected) {
				const int NUNCHUCK_DATA = 6;
				encrypted_byte = input_report[NUNCHUCK_DATA + 5];
				decrypted_byte = (encrypted_byte ^ 0x17) + 0x17 ;

				//e only care about the last 2 bits for b and Z buttons
				key_state |= ((decrypted_byte & 0x3) << 13);

				encrypted_byte = input_report[NUNCHUCK_DATA + 0];
				decrypted_byte = (encrypted_byte ^ 0x17) + 0x17 ;
				device->m_nunchuck_analogx = decrypted_byte;

				encrypted_byte = input_report[NUNCHUCK_DATA + 1];
				decrypted_byte = (encrypted_byte ^ 0x17) + 0x17 ;
				device->m_nunchuck_analogy = decrypted_byte;
				
			}

           
			vector<Button> changed = device->SetButtons(key_state);
            device->m_last_motion = m;
            device->ReceivedMotionData(m);
        }

        else if (0x22 == input_report[0]) // reading calibration data
        {
			 device->ReceivedNumChuckInit();
			/*
			 device->m_received_calibration =2;
             MotionData zero_point(input_report[6], input_report[7], input_report[8]);
             device->m_zero_point = zero_point;
             MotionData one_g_point(input_report[10], input_report[11], input_report[12]);
             device->m_one_g_point = one_g_point;
			 */
		}

    }

    return 0;
}

void Wiimote::RequestMotionData()
{
	m_received_calibration =0;

	//Simply writing a zero to register 0xa40040 initializes the peripheral, and makes it use a very simple encryption schem
    unsigned char init_nunchuck[] = { OUTPUT_WRITE_DATA, 0x04, 0xa4, 0x00, 0x40, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    WriteOutputReport(init_nunchuck);
#if 0
	//A two-byte read of register 0xa400fe will return these byte
    unsigned char init_nunchuck_report[] = { OUTPUT_READ_DATA, 0x04, 0xa4, 0x00, 0xFE, 0x00,0x2};
    WriteOutputReport(init_nunchuck_report);

/*
    unsigned char calibration_report[] = { OUTPUT_READ_DATA, 0x00, 0x00, 0x00, 0x16, 0x00, 0x0F };
    WriteOutputReport(calibration_report);
*/
    unsigned char set_motion_report[] = { OUTPUT_REPORT_SETDATA_REPORTING_MODE, 0x00, INPUT_REPORT_MOTION };
    WriteOutputReport(set_motion_report);
#endif
}
void Wiimote::ReceivedNumChuckInit()
{
	//A two-byte read of register 0xa400fe will return these byte
    unsigned char init_nunchuck_report[] = { OUTPUT_READ_DATA, 0x04, 0xa4, 0x00, 0xFE, 0x00,0x2};
    WriteOutputReport(init_nunchuck_report);

}
void Wiimote::ReceivedNumChuckReport()
{
	mCallback(this,"nchk");
	//Piggy back these report requests. Otherwise you get weird data
    unsigned char set_motion_report[] = { OUTPUT_REPORT_SETDATA_REPORTING_MODE, 0x00, INPUT_REPORT_MOTION };
    WriteOutputReport(set_motion_report);


}

#pragma warning( disable : 4189 )        // local variable is initialized but not referenced

void Wiimote::ReceivedMotionData(MotionData & m)
{
	float fx = m.x;
        //odprintf("x: %i y: %i z: %i", m.x, m.y, m.z);

}

bool Button::SetCode(const int code)
{
    bool pressed = (code & m_code)!=0;
    bool released = m_state && !pressed;

    m_state = pressed;

    return (pressed || released);
}

vector<Button> Wiimote::SetButtons(int code)
{
    vector<Button> changed_state;

    for (int i = 0; i < int(m_buttons.size()); i++)
    {
        if (m_buttons[i].SetCode(code))
            changed_state.push_back(m_buttons[i]);
    }

    return changed_state;
}

Button Wiimote::GetButton(string label)
{
    for (int i = 0; i < int(m_buttons.size()); i++)
    {
        if (m_buttons[i].GetName().compare(label) == 0)
            return m_buttons[i];
    }

    throw exception();
}

Button Wiimote::GetButton(int code)
{
    for (int i = 0; i < int(m_buttons.size()); i++)
    {
        if (m_buttons[i].GetCode() == code)
            return m_buttons[i];
    }

    throw exception();
}

bool Wiimote::StartListening()
{
    //printf("Started listening: %i", m_controller_id);

    SetLEDs(0,0,0,0);

    m_input_listening = true;
    m_listener_thread = (HANDLE)_beginthreadex(
        0,
        0,
        InputReportListener,
        this,
        0,
        &m_listener_thread_id);

    //printf("Created thread: %i", m_listener_thread_id);

    return false;
}

bool Wiimote::StopListening()
{
    //printf("Stopped listening: %i", m_controller_id);

    m_input_listening = false;

    WaitForSingleObject(m_event_object, 300);

    // Needs to clean up thread -- Heap corruption exception?
    //CloseHandle(m_listener_thread);
    m_listener_thread = NULL;

    return true;
}

void Wiimote::Disconnect()
{
    if (IsListening())
        StopListening();

    CloseHandle(m_listener_thread);
    CloseHandle(m_device_handle);
    CloseHandle(m_read_handle);
    CloseHandle(m_write_handle);
}

bool Wiimote::NoButtonsPressed()
{
    for (int i = 0; i < int(m_buttons.size()); i++)
    {
        if (m_buttons[i].Pressed())
            return false;
    }

    return true;
}

void Wiimote::ButtonPressed(Button & b)
{
    printf("Button pressed: %s", b.GetName().c_str());
    SetLEDs(1, 0, 1, 0);

    SetRumble(true);
}

void Wiimote::ButtonReleased(Button & b)
{
    printf("Button released: %s", b.GetName().c_str());
    SetLEDs(0, 0, 0, 0);

    if (NoButtonsPressed())
        SetRumble(false);
}

#endif
