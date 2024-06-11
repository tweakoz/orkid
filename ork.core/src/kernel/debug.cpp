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
		// /home/michael/.staging-jun10/lib/libork_core.so(_Z17OrkAssertFunctionPKcz+0xf4) [0x7c2ebdc7e3d4]
		// parse address between []
		auto start = strchr(callstack_line, '[');
		auto end = strchr(start, ']');
		std::string addr_string;
		void* addr_value = nullptr;
		if (start && end) {
			addr_string = std::string(start + 1, end - start - 1);
			addr_value = (void*)std::stoul(addr_string, nullptr, 16);
		}
		bool was_demangled = false;
		std::string demangled_name;
		if (dladdr(addr_value, &info)) {
			int status = -1;
			char* demangled = abi::__cxa_demangle(info.dli_sname, NULL, 0, &status);
			if(demangled != nullptr) {
				demangled_name = demangled;
				free(demangled);
				was_demangled = true;

				// replace "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >" with "std::string"
				// replace ::__cxx11 with ""
				demangled_name = string::replaced(demangled_name, "std::__cxx11::basic_string<char, std::char_traits<char>, std::allocator<char> >", "std::string");
				demangled_name = string::replaced(demangled_name, "::__cxx11", "");
				
				demangled_name += FormatString(" [%s]", info.dli_fname);


			}
		}
		if(not was_demangled) {
			demangled_name = callstack_line;
		}
  	FixedString<64> index_header, line_header, reset_footer;
  	deco::asciic_rgb256_inplace(index_header,255,255,0);
		if(was_demangled){
			deco::asciic_rgb256_inplace(line_header,255,128,0);
		}
  	else{
			deco::asciic_rgb256_inplace(line_header,255,128,128);
		}
  	deco::asciic_reset_inplace(reset_footer);
		tstr.format("%s %03d %s %s%s%s\n", 
			          index_header.c_str(), 
			          i, 
			          reset_footer.c_str(), 
			          line_header.c_str(), 
			          demangled_name.c_str(),
			          reset_footer.c_str());
#else
    tstr.format("%02d: %s\n", int(i), btstrings[i]);
#endif
    rval += tstr.c_str();
  }
  rval += "/////////////////\n";
  return rval;
}
} // namespace ork
