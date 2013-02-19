////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 


#include <ork/pch.h>
#include <ork/file/file.h>

#include <ork/kernel/core_interface.h>
#include <ork/kernel/opq.h>

#include <fcntl.h>
#if defined(IX)
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#if defined(ORK_LINUX)
#include <linux/input.h>
#endif

///////////////////////////////////////////////////////////////////////////////

INSTANTIATE_TRANSPARENT_RTTI(ork::IUserChoiceDelegate, "IUserChoiceDelegate");
INSTANTIATE_TRANSPARENT_RTTI(ork::ItemRemovalEvent, "ItemRemovalEvent");
INSTANTIATE_TRANSPARENT_RTTI(ork::ObjectGedVisitEvent, "ObjectGedVisitEvent");
INSTANTIATE_TRANSPARENT_RTTI(ork::ObjectGedEditEvent, "ObjectGedEditEvent");
INSTANTIATE_TRANSPARENT_RTTI(ork::MapItemCreationEvent, "MapItemCreationEvent");
INSTANTIATE_TRANSPARENT_RTTI(ork::ObjectFactoryFilter, "ObjectFactoryFilter");

void ork::IUserChoiceDelegate::Describe(){}
void ork::ItemRemovalEvent::Describe(){}
void ork::ObjectGedVisitEvent::Describe(){}
void ork::ObjectGedEditEvent::Describe(){}
void ork::MapItemCreationEvent::Describe(){}
void ork::ObjectFactoryFilter::Describe(){}

const std::string gstring_noval("");

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

void CSystem::SetGlobalFloatVariable( const std::string & variable, f32 value )
{
	GetRef().mmGlobalFloatVariables[ variable ] = value;
}

f32 CSystem::GetGlobalFloatVariable( const std::string & variable )
{
	return OrkSTXFindValFromKey( GetRef().mmGlobalFloatVariables, variable, (f32) 0.0f );
}

///////////////////////////////////////////////////////////////////////////////

void CSystem::SetGlobalStringVariable( const std::string & variable, std::string value )
{
	GetRef().mmGlobalStringVariables[ variable ] = value;
}

std::string CSystem::GetGlobalStringVariable( const std::string & variable )
{
	return OrkSTXFindValFromKey( GetRef().mmGlobalStringVariables, variable, (std::string) "NoVariable" );
}

///////////////////////////////////////////////////////////////////////////////

void CSystem::SetGlobalIntVariable( const std::string & variable, int value )
{
	GetRef().mmGlobalIntVariables[ variable ] = value;
}

int CSystem::GetGlobalIntVariable( const std::string & variable )
{
	return OrkSTXFindValFromKey( GetRef().mmGlobalIntVariables, variable, 0 );
}

///////////////////////////////////////////////////////////////////////////////

CSystem::CSystem()
	: NoRttiSingleton< CSystem >()
{
#if defined( NITRO )
	OS_InitTick();
#endif

	miCalibCounter = 0;
	miBaseCycle = GetClockCycle();
	mfWallClockBaseTime = GetHiResTime();

}

///////////////////////////////////////////////////////////////////////////////

const char *CSystem::ExpandString( char *outbuf, size_t outsize, const char *pfmtstr )
{
	size_t out = 0;
	char keybuf[ 512 ];
	//////////////////////////////
	// TODO make this generic so for every $(x) variable it replaces with GetGlobalStringVariable(x)

	for(intptr_t in = 0; pfmtstr[in]; )
	{
		if(pfmtstr[in] == '$' && pfmtstr[in+1] == '(')
		{
			in += 2;

			const char *endparen = strchr( &pfmtstr[in], ')' );

			if(endparen == NULL)
			{
				OrkAssertI(0, pfmtstr);
			}
			
			intptr_t keylen = endparen - &pfmtstr[in];

			OrkAssert(keylen < sizeof(keybuf) - 1);
			strncpy(keybuf, &pfmtstr[in], size_t(keylen));
			OrkAssert(keylen<sizeof(keybuf));
			keybuf[keylen] = '\0';

			in += keylen + 1;
			
			std::string value = GetGlobalStringVariable(keybuf);

			for(std::string::size_type v = 0; v < value.size(); v++)
			{
				if(out < outsize - 1)
					outbuf[out++] = value[v];
			}
		}
		else
		{
			outbuf[out++] = pfmtstr[in++];
		}
	}

	OrkAssert(out < outsize);
	outbuf[out++] = '\0';
	return outbuf;
}


#if defined( ORK_LINUX )

static bool ix_kb_state[256];

void* ix_kb_thread( void* pctx )
{
	SetCurrentThreadName( "IxKeyboardThread" );

	for(int i=0; i<256; i++)
		ix_kb_state[i] = false;

	//int fd = open("/dev/input/event10", O_RDONLY );
	int fd = open("/dev/input/by-path/platform-i8042-serio-0-event-kbd", O_RDONLY );

	if( fd<0 )
		while(1) usleep(1<<20);
	
	struct input_event ev[64];
	
	std::queue<U8> byte_queue;
	while(1)
	{
		int rd = read(fd,ev,sizeof(input_event)*64);
		if( rd>0 )
		{
			U8* psrc = (U8*) ev;
			for( int i=0; i<rd; i++ )
			{
				byte_queue.push(psrc[i]);
			}
		}
		while(byte_queue.size()>=sizeof(input_event))
		{
			input_event oev;
			u8* pdest = (U8*) & oev;
			for( int i=0; i<sizeof(input_event); i++ )
			{
				pdest[i]=byte_queue.front();
				byte_queue.pop();
			}
			if( oev.type == 1 )
			{
				bool bkdown = (oev.value == 1);
				bool bkup = (oev.value == 0);
				bool bksta = (0!=oev.value);
				printf( "ev typ<%d> cod<%d> val<%d>\n", oev.type, oev.code, oev.value );

				switch(oev.code)
				{

					case 17: // w
						ix_kb_state['W'] = bksta;
						break;
					case 30: // a
						ix_kb_state['A'] = bksta;
						break;
					case 31: // s
						ix_kb_state['S'] = bksta;
						break;
					case 32: // d
						ix_kb_state['D'] = bksta;
						break;
					case 24: // o
						ix_kb_state['O'] = bksta;
						break;
					case 25: // p
						ix_kb_state['P'] = bksta;
						break;
					case 33: // f
						ix_kb_state['F'] = bksta;
						break;
					//case 105: // L
					//	ix_kb_state[ork::lev2::ETRIG_RAW_KEY_LEFT] = bksta;
					//	break;
					//case 106: // R
					//	ix_kb_state[ork::lev2::ETRIG_RAW_KEY_RIGHT] = bksta;
					//	break;
					//case 103: // U
					//	ix_kb_state[ork::lev2::ETRIG_RAW_KEY_UP] = bksta;
					//	break;
					//case 108: // D
					//	ix_kb_state[ork::lev2::ETRIG_RAW_KEY_DOWN] = bksta;
					//	break;
					//case 42: // lshift
					//	ix_kb_state[ork::lev2::ETRIG_RAW_KEY_LSHIFT] = bksta;
					//	break;
					//case 29: // lctrl
					//	ix_kb_state[ork::lev2::ETRIG_RAW_KEY_LCTRL] = bksta;
					//	break;
					//case 56: // lalt
					//	ix_kb_state[ork::lev2::ETRIG_RAW_KEY_LALT] = bksta;
					//	break;
//					case 28: // return
//						ix_kb_state[ork::lev2::ETRIG_RAW_KEY_ENTER] = bksta;
//						break;
					// 0 11
					// 9 10
					// [ 26
					default:
						break;
				}
			}
		}
//		printf( "rd<%d>\n", rd );
	}
}

#endif



bool CSystem::IsKeyDepressed(int ch)
{
#if 1
	return false;
#elif defined(ORK_OSX)
	KeyMap theKeys;
	GetKeys ( theKeys );
	int iword = ch/32;
	int ibit = ch%32;
	u32 imask = 1<<ibit;
	u32* uword = reinterpret_cast<u32*>(&theKeys[0]); 
	bool rv = false; //(*uword&imask);
	// f == 0x00000008
	switch( ch )
	{
		case 'W':
			rv = (uword[0]&0x00002000);
			//printf( "O: %d\n", int(rv) );
			break;
		case 'A':
			rv = (uword[0]&0x00000001);
			//printf( "O: %d\n", int(rv) );
			break;
		case 'S':
			rv = (uword[0]&0x00000002);
			//printf( "O: %d\n", int(rv) );
			break;
		case 'D':
			rv = (uword[0]&0x00000004);
			//printf( "O: %d\n", int(rv) );
			break;
		case 'O':
			rv = (uword[0]&0x80000000);
			//printf( "O: %d\n", int(rv) );
			break;
		case 'P':
			rv = (uword[1]&8);
			//printf( "P: %d\n", int(rv) );
			break;
		case 'F':
			rv = (uword[0]&8);
			//printf( "F: %d\n", int(rv) );
			break;
		case ork::lev2::ETRIG_RAW_KEY_LEFT:
			rv = (uword[3]&0x08000000);
			//printf( "F: %d\n", int(rv) );
			break;
		case ork::lev2::ETRIG_RAW_KEY_RIGHT:
			rv = (uword[3]&0x10000000);
			//printf( "F: %d\n", int(rv) );
			break;
		case ork::lev2::ETRIG_RAW_KEY_UP:
			rv = (uword[3]&0x40000000);
			//printf( "F: %d\n", int(rv) );
			break;
		case ork::lev2::ETRIG_RAW_KEY_DOWN:
			rv = (uword[3]&0x20000000);
			//printf( "F: %d\n", int(rv) );
			break;

		default:
			// curs up : uword[3] 40000000
			// curs lf : uword[3] 08000000
			// curs rt : uword[3] 10000000
			// curs dn : uword[3] 20000000



			//printf( "K: %08x\n", int(uword[3]) );
			break;
	}

	//if( false==rv )
		//printf( "Isdepressed<%d> rv<%d> KEYS<%08x:%08x:%08x:%08x>\n", ch, int(rv), theKeys[0], theKeys[1], theKeys[2], theKeys[3] );
	return rv;
#elif defined( IX )
	static pthread_t kb_thread = 0;
	if( 0 == kb_thread )
	{
		int istat = pthread_create(&kb_thread, NULL, ix_kb_thread, (void*) 0);
	}
	return ix_kb_state[ch];
#elif defined( WIN32 ) && ! defined( _XBOX )
	switch(ch)
	{
		case ork::lev2::ETRIG_RAW_KEY_LALT:
			ch = 0x10e;
			break;
		case ork::lev2::ETRIG_RAW_KEY_RALT:
			ch = 0x10f;
			break;
		case ork::lev2::ETRIG_RAW_KEY_LCTRL:
			ch = 0x10c;
			break;
		case ork::lev2::ETRIG_RAW_KEY_RCTRL:
			ch = 0x10d;
			break;
		case ork::lev2::ETRIG_RAW_KEY_LSHIFT:
			ch = 0x105;
			break;
		case ork::lev2::ETRIG_RAW_KEY_RSHIFT:
			ch = 0x10A;
			break;
		case ork::lev2::ETRIG_RAW_KEY_LEFT:
			ch = VK_LEFT;
			break;
		case ork::lev2::ETRIG_RAW_KEY_RIGHT:
			ch = VK_RIGHT;
			break;
		case ork::lev2::ETRIG_RAW_KEY_UP:
			ch = VK_UP;
			break;
		case ork::lev2::ETRIG_RAW_KEY_DOWN:
			ch = VK_DOWN;
			break;
		default:
			if( (ch>='A') && (ch<='Z') )
			{
				//ch += (int('a')-int('A'));
			}
			break;
	}
	
	return (GetAsyncKeyState(ch) & 0x8000) == 0x8000;
#else
	return false;
#endif
}

}
