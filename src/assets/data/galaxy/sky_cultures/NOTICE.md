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

## Support stars

`../constellation_support_stars.json` contains topology-only points that are absent from the approved visible top-3000 star catalog. They must never be promoted to ordinary star sprites solely because a sky culture references them. HR support points retain finite HYG positions; HIP-only support points use fixed J2000 sky directions so the renderer does not invent parallax/distance data.
