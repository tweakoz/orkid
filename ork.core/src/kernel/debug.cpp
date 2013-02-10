#include <ork/kernel/debug.h>
#include <ork/kernel/tempstring.h>
#include <execinfo.h>
namespace ork {
std::string get_backtrace()
{
	std::string rval;
	static const int kmaxdepth = 64;
	void* btbuffer[kmaxdepth];
	size_t cnt = backtrace(btbuffer,kmaxdepth);
	char** btstrings = backtrace_symbols(btbuffer,cnt);

	rval += "/////////////////\n";
	for( size_t i=1; i<cnt; i++ )
	{
		FixedString<1024> tstr;
		tstr.format("%02d: %s\n", int(i), btstrings[i]);
		rval += tstr.c_str();
		//int status;
        //const char *pmangle = abi::__cxa_demangle( lin.c_str(), 0, 0, & status );
        //lin = pmangle;
		//printf( "Fr%02d: %s\n", int(i), lin.c_str() );
	}
	rval += "/////////////////\n";
	return rval;
}
}
