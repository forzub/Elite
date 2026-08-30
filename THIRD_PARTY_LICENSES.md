# Third-Party Licenses and Provenance

This file is the central index for externally sourced runtime content. Keep the
original upstream NOTICE/provenance files in place; this index points to them
and records distribution obligations that must remain visible in release builds.

## Fonts — Noto

- **Components:** Noto Sans and selected Noto script families; Noto Sans CJK; Noto Color Emoji.
- **Purpose:** deterministic glyph coverage for the Elite UI platform.
- **Upstream:** Noto project (`notofonts`) and Google Noto Emoji.
- **Pinned sources:** immutable upstream commit URLs are recorded in `src/assets/ui/font_manifest.json`; exact downloaded hashes are recorded in `third_party/fonts/noto/font-lock.json` after fetch.
- **License:** SIL Open Font License 1.1 (OFL-1.1).
- **Modification policy:** upstream binaries are distributed unmodified. Do not
  subset, rename, convert, or otherwise modify them without re-reviewing OFL
  Reserved Font Name requirements and updating this registry.
- **Bundling:** OFL permits embedding/bundling with software, including commercial
  software, provided the copyright notice and license accompany the Font Software.
- **License text:** `src/assets/licenses/fonts/SIL-OFL-1.1.txt`.

Noto project tooling/documentation is not bundled as runtime content. The Noto
Emoji repository also contains Apache-2.0 tools/images; this project fetches only
the font binary declared in the manifest, which upstream documents as OFL-1.1.

## Galaxy constellation data

- Existing provenance/terms: `src/assets/data/galaxy/constellation_lines.NOTICE.txt`.

## Sky-culture data

- Existing provenance/terms: `src/assets/data/galaxy/sky_cultures/NOTICE.md`.


## Celestial/environment provenance already present

- `src/assets/data/celestial/environment/**/*.json` contains per-preset `provenance` metadata (for example NASA observational-source identifiers). Those fields document data origin but are **not** treated as redistribution licenses for photographs, textures or 3D models.
- In the `src(20260817-132129).zip` baseline there is no separate asteroid-model or planetary-surface-image NOTICE/LICENSE file to index. If those source-license files exist outside this archive, or when such assets are added again, copy/retain their original notices and add explicit entries here before release.

## Maintenance rule

Any new third-party font, model, texture, photograph, audio asset, icon pack,
dataset, or binary dependency that is redistributed with the game must receive
an entry here before release. The entry must identify upstream, version/commit,
license, bundled files, whether we modified them, and any attribution/notice
requirements.

## Offline Model Asset Editor — libigl / Eigen / Embree

- **Components:** libigl v2.6.0 core + Embree module, Eigen and Intel Embree resolved by libigl CMake integration.
- **Purpose:** offline imported-mesh topology repair and outward orientation (`split_nonmanifold`, `reorient_facets_raycast`) in `EliteAssetEditor`; optional station diagnostic spike uses the same libraries.
- **Upstream:** `https://github.com/libigl/libigl`, pinned tag `v2.6.0`; transitive dependency versions are resolved by that pinned libigl CMake graph.
- **Scope:** authoring tools only. `EliteModelAsset`, the game client/server and runtime mesh normalizer do not link libigl or Embree.
- **Licensing:** retain upstream libigl/Eigen/Embree notices and licenses when redistributing an editor build containing these dependencies.
