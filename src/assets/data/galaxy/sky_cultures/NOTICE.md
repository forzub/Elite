# Sky-culture data provenance

These files are local, versioned runtime catalogs. The game never downloads sky-culture topology at runtime.

## `iau-western.json`

Derived from the project's existing western constellation-line dataset (`constellation_lines.json`).
See `../constellation_lines.NOTICE.txt` for the upstream attribution/license of that topology.
Russian display names are project localization data; geometry is unchanged from the approved 88-constellation / 750-segment set.

## `chinese-28-mansions.json`

A deliberately limited subset of the traditional Chinese sky: all 28 Lunar Mansion asterisms plus 14 selected surrounding asterisms, not the full Chinese Xingguan system. The extra set is intentionally sparse and distributed north/south of the mansion belt to keep the game sky readable while avoiding large empty regions.
Topology and English/native/pronunciation names are adapted from the Stellarium `chinese` sky culture.
Upstream states **Text and line: CC BY-SA**. The adapted topology/name data in this project remains subject to those upstream CC BY-SA terms.
Source: https://github.com/Stellarium/stellarium-skycultures/tree/master/chinese
Snapshot curated for this project: 2026-08-12.

The upstream Wings topology references HIP 53975. That one source star is intentionally omitted from the bundled topology until its coordinate can be independently verified; the omission is explicit in the data and regression-tested.

## `hawaiian-starlines.json`

Topology and names are adapted from the Stellarium `hawaiian_starlines` sky culture, describing the modern Hawaiian star-line organization used for Polynesian voyaging education/navigation.
The upstream culture description states **License CC BY-SA**. The adapted topology/name data in this project remains subject to those upstream CC BY-SA terms.
Source: https://github.com/Stellarium/stellarium-skycultures/tree/master/hawaiian_starlines
Additional cultural reference: Polynesian Voyaging Society / Hōkūleʻa Hawaiian Star Lines.
Snapshot curated for this project: 2026-08-12.

## Constellation reference stars

`../constellation_support_stars.json` contains 23 catalog stars used by sky-culture topology that are absent from the restrained visible top-3000 sprite selection. They remain separate from the sprite-selection policy, but all 23 now carry finite 3D `position_ly` coordinates and therefore use the same observer-relative perspective calculation as ordinary catalog stars. There is no fixed-sky-direction runtime exception.

The pre-existing HR entries retain their curated finite project positions. HIP entries use HYG v4.1 Cartesian positions where HYG provides a finite distance. HIP 89341 (Polis / Mu Sagittarii) keeps its J2000 direction with an explicitly approximate 5000 ly system distance; that approximation is sufficient for the current Solar-neighbourhood fidelity target and is marked in the data.
