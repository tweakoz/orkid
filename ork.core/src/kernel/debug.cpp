////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/kernel/debug.h>
#include <ork/kernel/tempstring.h>
#include <ork/kernel/string/deco.inl>
#include <execinfo.h>
#if defined(LINUX)
#include <cxxabi.h>
#include <dlfcn.h>
#endif
namespace ork {
std::string get_backtrace() {
  std::string rval;
  static const int kmaxdepth = 64;
  void* btbuffer[kmaxdepth];
  size_t cnt       = backtrace(btbuffer, kmaxdepth);
  char** btstrings = backtrace_symbols(btbuffer, cnt);

  rval += "/////////////////\n";
  for (size_t i = 1; i < cnt; i++) {
    FixedString<1024> tstr;
#if defined(LINUX)
		Dl_info info;
		auto callstack_line = btstrings[i];
		if (dladdr(callstack_line, &info)) {
			int status = -1;
			char* demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
			tstr.format("%d %s [%zx]\n",
					 i, 
					 status == 0 ? demangled : info.dli_sname,
					 (ptrdiff_t) ((char *) callstack_line - (char *)info.dli_saddr));
			free(demangled);
		} else {
    	FixedString<64> index_header, line_header, reset_footer;
    	deco::asciic_rgb256_inplace(index_header,255,255,0);
    	deco::asciic_rgb256_inplace(line_header,255,128,0);
    	deco::asciic_reset_inplace(reset_footer);
			tstr.format("%s %03d %s %s%s%s\n", 
				          index_header.c_str(), 
				          i, 
				          reset_footer.c_str(), 
				          line_header.c_str(), 
				          callstack_line,
				          reset_footer.c_str());
		}
#else
    tstr.format("%02d: %s\n", int(i), btstrings[i]);
#endif
    rval += tstr.c_str();
  }
  rval += "/////////////////\n";
  return rval;
}
} // namespace ork
