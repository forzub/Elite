# Universe generator reference data (pinned snapshots)

Observational inputs used to normalise the stellar part of the physical
catalog (positions, stellar parameters) and to select observational anchors
(well-confirmed exoplanets). The generator never reads these files at game
runtime; the authoring tools do.

| file | source | retrieved | notes |
|---|---|---|---|
| `nasa_exoplanet_archive_pscomppars_2026-10-29.csv` | NASA Exoplanet Archive TAP, table `pscomppars`, `sy_dist < 36 pc or hostname = 'Kepler-62'` | 2026-10-29 | 669 planets; composite parameters, one row per planet |
| `hyg_v41_catalog_stars_subset.csv` | HYG v4.1 (astronexus/HYG-Database, `hyg/CURRENT/hygdata_v41.csv`, sha256 `d9f69fd86bbf90a4e4d52b4c5c53eacfa6dfc0bfdef85bfd94f095e0bebe4ebd`) | 2026-10-29 | only the rows of catalog stars and their known companions; RA in hours, distance in parsecs, J2000 |

Coordinates: ICRS / J2000 equatorial Cartesian light-years from Sol,
x = d cos(dec) cos(ra), y = d cos(dec) sin(ra), z = d sin(dec) — the frame of
`position_ly` in the catalog and of `real_star_catalog.json`.
