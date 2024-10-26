////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkprotos.h>
#include <ork/kernel/string/PieceString.h>
#include <ork/orktypes.h>
#include <string>
#include <vector>
#include <regex>
#include <list>

using tokenlist = std::list<std::string>;

///////////////////////////////////////////////////////////////////////////////
namespace ork {
///////////////////////////////////////////////////////////////////////////////

namespace string {

inline std::string replaced(std::string subject, const std::string& search, const std::string& replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
  return subject;
}

} // namespace string

///////////////////////////////////////////////////////////////////////////////

typedef bool (*ork_cstr_replace_pred)(const char* src, const char* loc, size_t isrclen);

///////////////////////////////////////////////////////////////////////////////

bool ork_cstr_replace(
    const char* src,
    const char* from,
    const char* to,
    char* dest,
    const size_t ilen,
    ork_cstr_replace_pred pred = 0);

///////////////////////////////////////////////////////////////////////////////

//! split a string into many strings at the 'delim' boundary efficient form appends into vector passed in as an argument

void SplitString(const PieceString& instr, orkvector<PieceString>& splitvect, const ConstString& pdelim);

void SplitString(const std::string& s, char delim, std::vector<std::string>& output_tokens);
void SplitString(const std::string& s, const std::string& delims, std::vector<std::string>& output_tokens);

//! split a string into many strings at the 'delim' boundary compact form, returns vector
std::vector<std::string> SplitString(const std::string& instr, char delim);

///////////////////////////////////////////////////////////////////////////////
// split regex matches into many strings
///////////////////////////////////////////////////////////////////////////////

inline std::vector<std::string> SplitAllWithRegex(const std::string haystack, std::regex& rgx) {
  std::vector<std::string> rval;
  std::smatch m;
  std::string::const_iterator searchStart(haystack.cbegin());
  while (std::regex_search(searchStart, haystack.cend(), m, rgx)) {
    rval.push_back(m[0].str());
    searchStart = m.suffix().first;
  }
  return rval;
}

///////////////////////////////////////////////////////////////////////////////

std::string JoinString(const std::vector<std::string>& strvect, const std::string& delim);
std::string JoinString(const std::set<std::string>& strvect, const std::string& delim);

///////////////////////////////////////////////////////////////////////////////

int FindSubString(const std::string& needle, const std::string& haystack);

///////////////////////////////////////////////////////////////////////////////

tokenlist CreateTokenList(const PieceString& instr, const ConstString& pdelim);

///////////////////////////////////////////////////////////////////////////////

std::string CreateFormattedString(const char* formatstring, ...);

//! like printf, for std::strings
std::string FormatString(const char* formatstring, ...);

///////////////////////////////////////////////////////////////////////////////

std::string toLower(const std::string& inp);
std::string toUpper(const std::string& inp);

///////////////////////////////////////////////////////////////////////////////
} // namespace ork
///////////////////////////////////////////////////////////////////////////////
