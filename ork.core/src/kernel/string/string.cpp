////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <ork/kernel/string/ResizableString.h>
#include <ork/kernel/string/string.h>
#include <ork/kernel/tempstring.h>
#include <sstream>
#include <cctype>

namespace ork {

////////////////////////////////////////////////////////////////////////////////

void SplitString(const FixedString<256>& instr, orkvector<FixedString<64>>& splitvect, const char* pdelim) {
  if (instr.length()) {
    const char* psrc = instr.c_str();

    size_t istrlen = instr.length();

    char* buffer = new char[istrlen + 1024];
    memset(buffer, 0, istrlen + 1024);
    strcpy(buffer, instr.c_str());
    char* tok = strtok(buffer, pdelim);

    splitvect.push_back(tok);

    while (tok != 0) {
      size_t ipos = (tok - buffer);

      tok = strtok(0, pdelim);
      if (tok) {
        splitvect.push_back(tok);
      }
    }
    delete[] buffer;
  }
}

////////////////////////////////////////////////////////////////////////////////

void SplitString(const std::string& instr, orkvector<std::string>& splitvect, const char* pdelim) {
  if (instr.length()) {
    const char* psrc = instr.c_str();

    size_t istrlen = instr.length();

    char* buffer = new char[istrlen + 1024];
    memset(buffer, 0, istrlen + 1024);
    strcpy(buffer, instr.c_str());
    char* tok = strtok(buffer, pdelim);

    splitvect.push_back(tok);

    while (tok != 0) {
      size_t ipos = (tok - buffer);

      tok = strtok(0, pdelim);
      if (tok) {
        splitvect.push_back(tok);
      }
    }
    delete[] buffer;
  }
}

///////////////////////////////////////////////////////////////////////////////

void TokenizeString(PieceString stringToTokenize, ConstString delimiters, orkvector<PieceString>& tokenVector) {
  PieceString::size_type startpoint = 0;
  PieceString::size_type i;

  if (stringToTokenize.empty())
    return;

  do {
    i                 = stringToTokenize.find_first_of(delimiters.c_str(), startpoint);
    PieceString token = stringToTokenize.substr(startpoint, i - startpoint);
    if (!token.empty())
      tokenVector.push_back(token);
    startpoint = i + 1;
  } while (i != ork::PieceString::npos);
}

////////////////////////////////////////////////////////////////////////////////

tokenlist CreateTokenList(const PieceString& instr, const ConstString& pdelim) {
  tokenlist rval;

  PieceString::size_type startpoint = 0;
  PieceString::size_type i;

  if (instr.empty())
    return rval;

  do {
    i                 = instr.find_first_of(pdelim.c_str(), startpoint);
    PieceString token = instr.substr(startpoint, i - startpoint);
    if (!token.empty()) {
      std::string str(token.c_str(), token.length());
      rval.push_back(str);
    }
    startpoint = i + 1;
  } while (i != ork::PieceString::npos);

  return rval;
}

///////////////////////////////////////////////////////////////////////////////
// split a string into many strings at the 'delim' boundary
//  efficient form, appends into vector passed in as an argument
///////////////////////////////////////////////////////////////////////////////

void SplitString(const std::string& s, char delim, std::vector<std::string>& tokens) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim))
    tokens.push_back(item);
}

////////////////////////////////////////////////////////////////////////////////

void SplitString(const std::string& s, const std::string& delims, std::vector<std::string>& output_tokens) {
  std::stringstream ss(s);
  std::string line;
  while (std::getline(ss, line)) {
    size_t prev = 0;
    size_t pos  = 0;
    while ((pos = line.find_first_of(" ';", prev)) != std::string::npos) {
      if (pos > prev)
        output_tokens.push_back(line.substr(prev, pos - prev));
      prev = pos + 1;
    }
    if (prev < line.length())
      output_tokens.push_back(line.substr(prev, std::string::npos));
  }
}

///////////////////////////////////////////////////////////////////////////////
// split a string into many strings at the 'delim' boundary
//  compact form, returns vector
///////////////////////////////////////////////////////////////////////////////

std::vector<std::string> SplitString(const std::string& instr, char delim) {
  std::vector<std::string> tokens;
  SplitString(instr, delim, tokens);
  return tokens;
}

////////////////////////////////////////////////////////////////////////////////

std::string JoinString(const std::vector<std::string>& strvect, const std::string& delim) {
  std::string rval;
  for (const auto& str : strvect)
    rval += (str + delim);
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

std::string JoinString(const std::set<std::string>& strset, const std::string& delim) {
  std::string rval;
  for (const auto& str : strset)
    rval += (str + delim);
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

void SplitString(const PieceString& instr, orkvector<PieceString>& splitvect, const ConstString& pdelim) {
  PieceString::size_type startpoint = 0;
  PieceString::size_type i;

  if (instr.empty())
    return;

  do {
    i                 = instr.find_first_of(pdelim.c_str(), startpoint);
    PieceString token = instr.substr(startpoint, i - startpoint);
    if (!token.empty())
      splitvect.push_back(token);
    startpoint = i + 1;
  } while (i != ork::PieceString::npos);
}

//////////////////////////////////////////////////////////////////////////////

std::string CreateFormattedString(const char* formatstring, ...) {
  std::string rval;
  char formatbuffer[512];
  va_list args;
  va_start(args, formatstring);
  vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
  va_end(args);
  rval = formatbuffer;
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

std::string FormatString(const char* formatstring, ...) {
  std::string rval;
  char formatbuffer[512];
  va_list args;
  va_start(args, formatstring);
  vsnprintf(&formatbuffer[0], sizeof(formatbuffer), formatstring, args);
  va_end(args);
  rval = formatbuffer;
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

bool ork_cstr_replace(
    const char* src,
    const char* from,
    const char* to,
    char* dest,
    const size_t idestlen,
    ork_cstr_replace_pred pred) {
  const int nummarkers = 8;

  size_t isrclen  = strlen(src);
  size_t ifromlen = strlen(from);
  size_t itolen   = strlen(to);

  bool bdone             = false;
  bool brval             = true;
  const char* src_marker = src;
  const char* src_end    = src + isrclen;
  char* dst_marker       = dest;

  // src: whatupyodiggittyyoyo
  // from: yo
  // to: damn

  while (false == bdone) {
    const char* search = strstr(src_marker, from);

    // printf( "search<%s> src_marker<%s> from<%s> to<%s> src<%s> dest<%s>\n", search, src_marker, from, to, src, dest );
    // search<yodiggittyyoyo> src_marker<whatupyodiggittyyoyo> dest<>
    // search<yoyo> src_marker<diggittyyoyo> dest<whatupdamn>

    /////////////////////////////////////
    // copy [src_marker..search] ->output
    /////////////////////////////////////
    if ((search != 0) && (search >= src_marker)) {
      size_t ilen = size_t(search - src_marker);
      OrkAssert((dst_marker + ilen) <= (dest + idestlen));
      strncpy(dst_marker, src_marker, ilen);
      dst_marker += ilen;
      src_marker += ilen;
    }
    /////////////////////////////////////
    // copy "to" -> output, advance input by ifromlen
    /////////////////////////////////////
    if ((search != 0)) {
      bool doit = true;
      if (pred) {
        doit = pred(src, src_marker, isrclen);
      }

      if (doit) {
        OrkAssert((dst_marker + itolen) <= (dest + idestlen));
        strncpy(dst_marker, to, itolen);
        dst_marker += itolen;
        src_marker += ifromlen;
      } else {
        OrkAssert(dst_marker < (dest + idestlen));
        dst_marker[0] = src_marker[0];
        dst_marker++;
        src_marker++;
      }

    }

    /////////////////////////////////////
    // copy [mkr..end] -> output
    /////////////////////////////////////
    else {
      size_t ilen = isrclen - (src_marker - src);
      strncpy(dst_marker, src_marker, ilen);
      dst_marker += ilen;
      src_marker += ilen;
    }

    bdone = (src_marker >= src_end);
  }

  dst_marker[0] = 0;
  dst_marker++;

  size_t inewlen = size_t(dst_marker - dest);
  OrkAssert(inewlen < idestlen);
  return brval;
}

////////////////////////////////////////////////////////////////////////////////

std::string toLower(const std::string& inp){
  std::string rval = inp;
  std::for_each(rval.begin(), rval.end(), [](char & c){
    c = ::tolower(c);
  });
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

std::string toUpper(const std::string& inp){
  std::string rval = inp;
  std::for_each(rval.begin(), rval.end(), [](char & c){
    c = ::toupper(c);
  });
  return rval;
}

////////////////////////////////////////////////////////////////////////////////

} // namespace ork
