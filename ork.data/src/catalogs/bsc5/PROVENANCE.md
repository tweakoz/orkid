# BSC5 — The Bright Star Catalogue, 5th Revised Ed.

Source data for the night sky's real star field. Committed BYTE-EXACT as
downloaded; nothing in this directory is generated, reformatted or edited.

## Compilation

Hoffleit, D. & Warren, W. H. Jr. (1991), *The Bright Star Catalogue, 5th
Revised Edition (Preliminary Version)*, Astronomical Data Center, NSSDC/ADC.
Bibcodes `1964BS....C......0H`, `1991bsc..book.....H`.

## Archive identifiers

* VizieR / CDS: **V/50**
* HEASARC: **BSC5P** (the same compilation in the HEASARC Browse archive)

## License

The BSC predates the era in which catalogs carried explicit licenses, so there
is no license text to quote. Its distribution terms are established by the
archives that hold it:

* HEASARC states no usage restrictions on the data it distributes (NASA
  archival astronomical data).
* NASA's data.gov listing classifies the holding as US-Government-Works, i.e.
  public domain in the United States.

Attribution to Hoffleit & Warren is expected as a scholarly courtesy, and this
file is that attribution.

## Download

* `catalog.gz` — https://cdsarc.cds.unistra.fr/ftp/cats/V/50/catalog.gz
  573921 bytes, sha256 `3dc44b1e90be8fbe5bcc7656032560f51275f985c7e3f783c9028e1838ec7bed`
* `ReadMe` — https://cdsarc.cds.unistra.fr/ftp/cats/V/50/ReadMe
  11571 bytes, sha256 `44fd9c73e2eecad0beb47bdfa3f01c60fd43f93d6964198e31fcd48732de5b33`

Retrieved 2026-07-28. `ReadMe` is the authority for the fixed-width byte
columns the bake parses — it is committed because the catalog is unreadable
without it, and a byte-column layout that lives only in a downstream parser is
a layout nobody can check.

## Contents

`catalog` (inside `catalog.gz`) holds **9110 records**, of which **9096 are
stars**. The remaining **14 are novae or extragalactic objects** catalogued in
the 1908 compilation and retained only to preserve the HR numbering; the ReadMe
notes their position/photometry fields are blank. Those 14 (HR 92, 95, 182,
1057, 1841, 2472, 2496, 3515, 3671, 6309, 6515, 7189, 7539, 8296) are FILTERED
AT BAKE TIME by the absence of J2000 coordinates and V magnitude — see
`obt.project/scripts/ork/hypergraph/assets/mesh/_bsc5.py`, which asserts the
9110 total so a truncated or substituted file fails loudly instead of quietly
baking a partial sky.

`notes.gz` (the remarks file) is NOT committed: nothing in the render path
reads it.
