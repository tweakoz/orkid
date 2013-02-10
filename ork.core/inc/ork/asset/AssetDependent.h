///////////////////////////////////////////////////////////////////////////////
// Orkid
// Copyright 1996-2010, Michael T. Mayers
///////////////////////////////////////////////////////////////////////////////

#pragma once

namespace ork { namespace util { namespace dependency {
class Provider;
} } }

namespace ork { namespace asset {

class Asset;

util::dependency::Provider *GetAssetLoadProvider(const Asset *asset);

} }
