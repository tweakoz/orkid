////////////////////////////////////////////////////////////////
// Orkid Media Engine
// Copyright 1996-2026, Michael T. Mayers.
// Distributed under the Boost Software License - Version 1.0 - August 17, 2003
// see http://www.boost.org/LICENSE_1_0.txt
////////////////////////////////////////////////////////////////
// REGISTRY gate for the direct-diffuse BRDF selector (pbr_common.h).
//
// The selector has TWO currencies and they are wired end to end: the crc of a
// model's NAME is what the scene DSL, an .ecs and the player's editor row carry,
// while the enum's small ORDINAL is what the shader UBO carries and what
// lib_brdf::diffuseBRDF branches on. These tests pin the seams where a silent
// mismatch would shade the wrong lobe with nothing to see:
//   * the ordinals ARE the literals the shader compares against,
//   * name -> crc -> model round-trips for every model,
//   * an unrecognized name/crc is REFUSED and leaves the caller's value alone,
//   * the shipped default is oren-nayar (the owner's ruling, in code),
//   * the editor row's crc (CrcString of the label the row saves) is the same
//     identity the engine resolves.
////////////////////////////////////////////////////////////////

#include <ork/pch.h>
#include <utpp/UnitTest++.h>

#include <ork/util/crc.h>
#include <ork/lev2/gfx/renderer/NodeCompositor/pbr_common.h>

using namespace ork;
using namespace ork::lev2::pbr;

////////////////////////////////////////////////////////////////
// The shader's private currency. lib_brdf::diffuseBRDF branches on integer
// literals (0 lambert / 1 oren-nayar / 2 burley) and the bind sends int(model),
// so a renumbering here silently repoints every lit fragment.
////////////////////////////////////////////////////////////////
TEST(DiffuseBrdfOrdinalsAreTheShaderContract) {
  CHECK_EQUAL(0, int(DiffuseBrdfModel::LAMBERT));
  CHECK_EQUAL(1, int(DiffuseBrdfModel::OREN_NAYAR));
  CHECK_EQUAL(2, int(DiffuseBrdfModel::BURLEY));
}

////////////////////////////////////////////////////////////////
TEST(DiffuseBrdfNameCrcRoundTrips) {
  const DiffuseBrdfModel all[] = {
      DiffuseBrdfModel::LAMBERT,
      DiffuseBrdfModel::OREN_NAYAR,
      DiffuseBrdfModel::BURLEY,
  };
  for (auto model : all) {
    const char* name = diffuseBrdfModelName(model);
    DiffuseBrdfModel by_name = DiffuseBrdfModel::LAMBERT;
    CHECK(diffuseBrdfModelFromName(name, by_name));
    CHECK_EQUAL(int(model), int(by_name));
    DiffuseBrdfModel by_crc = DiffuseBrdfModel::LAMBERT;
    CHECK(diffuseBrdfModelFromCrc(CrcString(name).hashed(), by_crc));
    CHECK_EQUAL(int(model), int(by_crc));
  }
}

////////////////////////////////////////////////////////////////
// A misspelling must not shade — it must be refused, with the caller's value
// left exactly as it was so the refusal can be reported instead of applied.
////////////////////////////////////////////////////////////////
TEST(DiffuseBrdfUnknownIsRefused) {
  DiffuseBrdfModel model = DiffuseBrdfModel::BURLEY;
  CHECK(not diffuseBrdfModelFromName("OREN-NAYAR", model)); // hyphen: the LABEL, not the token
  CHECK_EQUAL(int(DiffuseBrdfModel::BURLEY), int(model));
  CHECK(not diffuseBrdfModelFromName("", model));
  CHECK(not diffuseBrdfModelFromCrc(0, model));
  CHECK_EQUAL(int(DiffuseBrdfModel::BURLEY), int(model));

  auto valid = diffuseBrdfModelValidSet();
  CHECK(valid.find("LAMBERT") != std::string::npos);
  CHECK(valid.find("OREN_NAYAR") != std::string::npos);
  CHECK(valid.find("BURLEY") != std::string::npos);
}

////////////////////////////////////////////////////////////////
// The owner's ruling, in code: a scene that says nothing about its diffuse lobe
// gets oren-nayar.
////////////////////////////////////////////////////////////////
TEST(DiffuseBrdfDefaultIsOrenNayar) {
  CommonStuff pbrc;
  CHECK_EQUAL(int(DiffuseBrdfModel::OREN_NAYAR), int(pbrc._diffuseBrdfModel));
}

////////////////////////////////////////////////////////////////
// The player's POST row is an INDEX into its own labels and sends the crc of the
// model's name; the labels are the names lowercased with '_' as '-'. Both halves
// of that pairing are pinned here, because the row and the resolver live in
// different translation units and only agree by convention.
////////////////////////////////////////////////////////////////
TEST(DiffuseBrdfEditorRowIdentityMatchesEngine) {
  const char* labels[] = {"lambert", "oren-nayar", "burley"};
  for (int index = 0; index < 3; index++) {
    auto model = DiffuseBrdfModel(index);
    std::string label = diffuseBrdfModelName(model);
    for (auto& ch : label) {
      ch = char(::tolower(ch));
      if (ch == '_')
        ch = '-';
    }
    CHECK_EQUAL(std::string(labels[index]), label);
    // what the row puts on the wire, resolved by the receiver
    DiffuseBrdfModel received = DiffuseBrdfModel::LAMBERT;
    CHECK(diffuseBrdfModelFromCrc(CrcString(diffuseBrdfModelName(model)).hashed(), received));
    CHECK_EQUAL(index, int(received));
  }
}
