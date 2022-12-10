////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2022, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace util { namespace dependency {
class Provider;
} } }

namespace ork { namespace asset {

util::dependency::Provider *assetLoadProvider(const Asset *asset);

} }
