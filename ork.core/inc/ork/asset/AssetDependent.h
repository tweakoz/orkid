///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2020, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace util { namespace dependency {
class Provider;
} } }

namespace ork { namespace asset {

class Asset;

util::dependency::Provider *assetLoadProvider(const Asset *asset);

} }
