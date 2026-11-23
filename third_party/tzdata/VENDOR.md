# tzdata 2026f (vendored)

Pinned by `tools/vendor_tzdata.sh 2026f 26189803c8b3e903dcc467404e2feefb7d190cb1870249a23577ce31e9dcbf187e4704e0b45c8d1366d1c501bb5b53a2e80f8e2828b2163be15e532f6acab52d` (spec 048, constitution 1.5.0).

- Source: <https://data.iana.org/time-zones/releases/tzdata2026f.tar.gz>
- Archive SHA-512, of the file as downloaded from data.iana.org (IANA publishes a PGP
  signature beside it, which this script does not check): `26189803c8b3e903dcc467404e2feefb7d190cb1870249a23577ce31e9dcbf187e4704e0b45c8d1366d1c501bb5b53a2e80f8e2828b2163be15e532f6acab52d`
- License: public domain. From `LICENSE`:

  > Unless specified below, all files in the tz code and data (including
  > this LICENSE file) are in the public domain.

- Updates: every toolchain release takes the newest tz release (<https://www.iana.org/time-zones>):
  run this script with the new version and checksum, `make test`, commit this directory.

## Kept files

`LICENSE` and `version` are unmodified files of the archive. `zones.bin` is derived (plan,
Complexity Tracking): the archive's files `africa antarctica asia australasia europe northamerica southamerica etcetera backward`
compiled by `zic -b fat` (zic (Ubuntu GLIBC 2.39-0ubuntu8.12) 2.39) and packed, identical zones stored once.

- `zones.bin` sha256 28d049e11eea8dfeb8ab5fe3da44fef3d7866bfa1bb7aa1ded488cddccf03a73
- `LICENSE` sha256 0613408568889f5739e5ae252b722a2659c02002839ad970a63dc5e9174b27cf
- `version` sha256 9f9a9e6bfd18cd6a0a4d65764931072577edcbb731981a068ef52bba6615b1de
