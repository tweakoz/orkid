#pragma once

#include <ork/kernel/string/string.h>
#include <ork/math/cvector3.h>

namespace ork::deco {

///////////////////////////////////////////////////////////////////////////////

inline std::string asciic_rgb256(int r, int g, int b) {
  int _r    = int((r * 5) / 255);
  int _g    = int((g * 5) / 255);
  int _b    = int((b * 5) / 255);
  int color = 16 + 36 * _r + 6 * _g + _b;
  auto rval = FormatString("\033[38;5;%dm", color);
  // if self.bash:
  //  rval = "\[" + rval + "\]"
  return rval;
}
inline std::string asciic_rgb(const fvec3& color) {
  int r = int(color.x * 255.0f);
  int g = int(color.y * 255.0f);
  int b = int(color.z * 255.0f);
  return asciic_rgb256(r, g, b);
}
inline std::string asciic_reset() {
  return "\033[0m";
}

inline std::string string(const std::string& s, int r, int g, int b) {
  return asciic_rgb256(r, g, b) + s + asciic_reset();
}

inline std::string decorate(fvec3 color, std::string inp) {
  int r = int(color.x * 255.0);
  int g = int(color.y * 255.0);
  int b = int(color.z * 255.0);
  return asciic_rgb256(r, g, b) + inp + asciic_reset();
}

inline std::string format(int r, int g, int b, const char* formatstring, ...) {
  std::string rval;

  char formatbuffer[512];

  va_list args;
  va_start(args, formatstring);
  // buffer.vformat(formatstring, args);
#if 1 // defined(IX)
  vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
#else
  vsnprintf_s(&formatbuffer[0], sizeof(formatbuffer), sizeof(formatbuffer), formatstring, args);
#endif
  va_end(args);
  rval = formatbuffer;
  return asciic_rgb256(r, g, b) + rval + asciic_reset();
}

inline std::string format(const fvec3& color, const char* formatstring, ...) {
  std::string rval;

  char formatbuffer[512];

  va_list args;
  va_start(args, formatstring);
  // buffer.vformat(formatstring, args);
#if 1 // defined(IX)
  vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
#else
  vsnprintf_s(&formatbuffer[0], sizeof(formatbuffer), sizeof(formatbuffer), formatstring, args);
#endif
  va_end(args);
  rval = formatbuffer;
  return asciic_rgb(color) + rval + asciic_reset();
}

inline void print(const fvec3& color, const char* formatstring, ...) {

  int r = int(color.x * 255.0);
  int g = int(color.y * 255.0);
  int b = int(color.z * 255.0);

  std::string rval;

  char formatbuffer[512];

  va_list args;
  va_start(args, formatstring);
  // buffer.vformat(formatstring, args);
#if 1 // defined(IX)
  vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
#else
  vsnprintf_s(&formatbuffer[0], sizeof(formatbuffer), sizeof(formatbuffer), formatstring, args);
#endif
  va_end(args);
  rval        = formatbuffer;
  auto fmtstr = asciic_rgb256(r, g, b) + rval + asciic_reset();
  printf("%s", fmtstr.c_str());
}

inline void printe(const fvec3& color, std::string inp, bool endl) {

  int r = int(color.x * 255.0);
  int g = int(color.y * 255.0);
  int b = int(color.z * 255.0);

  std::string final = asciic_rgb256(r, g, b);
  final += inp;
  if (endl)
    final += "\n";
  final += asciic_reset();
  printf("%s", final.c_str());
}
inline void prints(std::string inp, bool endl) {
  std::string final = inp;
  if (endl)
    final += "\n";
  printf("%s", final.c_str());
}

} // namespace ork::deco
