////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#ifndef LPRINTF_H_
#define LPRINTF_H_

int lfprintf(ork::File *file, const char *fmt, ...);
int lvfprintf(ork::File *file, const char *fmt, va_list args);

int lprintf(const char *fmt, ...);
int lvprintf(const char *fmt, va_list args);

int lsnprintf(char *s, size_t len, const char *fmt, ...);
int lvsnprintf(char *s, size_t len, const char *fmt, va_list args);

int lwsnprintf(u16 *s, size_t len, const char *fmt, ...);
int lvwsnprintf(u16 *s, size_t len, const char *fmt, va_list args);

// prints to a temporary buffer
u16 *lwconvertf(const char *fmt, ...);
char *lconvertf(const char *fmt, ...);

size_t lwstrlen(const u16 *s);

#endif