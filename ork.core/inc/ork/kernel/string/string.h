////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2012, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
//////////////////////////////////////////////////////////////// 

#pragma once

#include <ork/orkprotos.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/kernel/core/kerneltypes.h>
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////

typedef bool (*ork_cstr_replace_pred)( const char* src, const char* loc, size_t isrclen );

///////////////////////////////////////////////////////////////////////////////

bool ork_cstr_replace(	const char *src,
						const char *from,
						const char *to,
						char* dest,
						const size_t ilen,
						ork_cstr_replace_pred pred = 0 );

///////////////////////////////////////////////////////////////////////////////

//! split a string into many strings at the 'delim' boundary efficient form appends into vector passed in as an argument

void SplitString(const PieceString &instr,
				 orkvector<PieceString> &splitvect,
				 const ConstString &pdelim);

void SplitString(const std::string& s, char delim, std::vector<std::string>& output_tokens);
void SplitString(const std::string& s, const std::string& delims, std::vector<std::string>& output_tokens);

//! split a string into many strings at the 'delim' boundary compact form, returns vector
std::vector<std::string> SplitString(const std::string& instr, char delim);

///////////////////////////////////////////////////////////////////////////////

std::string JoinString(const std::vector<std::string>& strvect, const std::string& delim);
std::string JoinString(const std::set<std::string>& strvect, const std::string& delim);

///////////////////////////////////////////////////////////////////////////////

bool IsSubStringPresent(const std::string& needle, const std::string& haystack);
int FindSubString(const std::string& needle, const std::string& haystack);

///////////////////////////////////////////////////////////////////////////////

tokenlist CreateTokenList(const PieceString &instr, const ConstString &pdelim);

///////////////////////////////////////////////////////////////////////////////

std::string CreateFormattedString(const char* formatstring, ... );

//! like printf, for std::strings
std::string FormatString(const char* formatstring, ... );


///////////////////////////////////////////////////////////////////////////////
};
///////////////////////////////////////////////////////////////////////////////
