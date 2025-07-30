////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2023, Michael T. Mayers.
// Distributed under the MIT License.
// see license-mit.txt in the root of the repo, and/or https://opensource.org/license/mit/
////////////////////////////////////////////////////////////////

#include <utpp/UnitTest++.h>
#include <ork/asset/catalog/catalog.h>
#include <ork/asset/catalog/manifest.h>
#include <ork/asset/catalog/packager.h>
#include <ork/asset/catalog/uploader.h>
#include <ork/asset/catalog/namespace.h>
#include <ork/kernel/string/deco.inl>
#include <ork/file/file.h>
#include <ork/file/path.h>
#include <ork/util/crypt.h>
#include <ork/util/download_manager.h>

using namespace ork;
using namespace ork::asset::catalog;

