////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

#include <ork/orkstd.h>
#include <ork/orktypes.h>
#include <string>
#include <memory>
#include <optional>

namespace ork {

struct URL {
  std::string _scheme;
  std::string _host;
  int _port = -1;
  std::string _path;
  std::string _query;
  std::string _fragment;
  std::string _userinfo;  // username:password
  
  URL() = default;
  URL(const std::string& url_string);
  
  //////////////////////////////////////////////////////////////////////////////
  // Operations
  //////////////////////////////////////////////////////////////////////////////
  URL withQuery(const std::string& key, const std::string& value) const;
  URL withScheme(const std::string& scheme) const;
  URL withHost(const std::string& host) const;
  URL withPort(int port) const;
  URL withPath(const std::string& path) const;
  URL operator/(const std::string& segment) const;
  URL parent() const;
  
  //////////////////////////////////////////////////////////////////////////////
  // Conversion
  //////////////////////////////////////////////////////////////////////////////
  std::string toString() const;
  operator std::string() const { return toString(); }
  
  //////////////////////////////////////////////////////////////////////////////
  // Validation  
  //////////////////////////////////////////////////////////////////////////////
  bool isValid() const;
  bool isAbsolute() const;
  
  //////////////////////////////////////////////////////////////////////////////
  // Static helpers
  //////////////////////////////////////////////////////////////////////////////
  static std::string encode(const std::string& str);
  static std::string decode(const std::string& str);
  static std::string encodePath(const std::string& path);
  static std::string encodeQueryValue(const std::string& value);
  
private:
  void parse(const std::string& url_string);
  static bool isHexChar(char c);
  static char fromHex(char ch);
};

using url_ptr_t = std::shared_ptr<URL>;

} // namespace ork