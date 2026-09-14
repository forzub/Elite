# External Asset Licenses

This directory is the Git-tracked legal/provenance registry for externally sourced art assets used by Elite.

## Storage split

Binary source assets are **not** stored in this repository.

Local working assets live under:

```text
D:\__elite\work\assets\models\
```

License, source and provenance documentation lives here in Git.

The same stable code name must be used in both places so a local binary can always be matched to its legal record.

Example:

```text
Local binary/source:
D:\__elite\work\assets\models\astronauts\lumi_001\original.glb

Git documentation:
ASSET_LICENSES/models/astronauts/lumi_001/
```

## Per-asset layout

Use one directory per external asset:

```text
ASSET_LICENSES/
  models/
    astronauts/
      lumi_001/
        README.md
        evidence/
          ... optional screenshots or saved license evidence ...
```

The asset `README.md` is the canonical record and should contain, when available:

- internal code name;
- human-readable title;
- source URL;
- source platform;
- author / uploader;
- original download format and filename;
- date downloaded;
- license name and version;
- attribution text required by the license;
- whether commercial use is allowed;
- whether modification is allowed;
- redistribution constraints;
- local working path;
- any conversion, cleanup, retopology, LOD or material changes;
- notes about screenshots or other preserved license evidence.

Do not copy the model binary into this repository merely to preserve provenance. Preserve enough documentation to identify exactly what was downloaded and under which terms.

## Naming

Use short stable lowercase Latin code names plus a three-digit index where useful, for example:

```text
lumi_001
milo_002
nori_003
piko_004
```

The code name is an internal identifier only and must not replace the original author/title in the license record.
