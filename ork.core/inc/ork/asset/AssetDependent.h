////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace util { namespace dependency {
class Provider;
} } }

namespace ork { namespace asset {

util::dependency::Provider *assetLoadProvider(const Asset *asset);

} }
