#include "Config.h"

#if defined UNITTEST_POSIX
    #include "Posix/TimeHelpers.h"
#elif defined UNITTEST_PSP
    #include "PSP/TimeHelpers.h"
#elif defined UNITTEST_NDS
    #include "NDS/TimeHelpers.h"
#elif defined UNITTEST_WII
    #include "Wii/TimeHelpers.h"
#else
    #include "Win32/TimeHelpers.h"
#endif
