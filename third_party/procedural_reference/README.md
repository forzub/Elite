# Procedural reference code

This directory contains **research-only snapshots** of permissively licensed upstream code relevant to terrain generation.

Production code in Elite must not include or link these files implicitly.

## Included snapshots

### Ian-Parberry/Tobler

Pinned upstream commit:

    bee7c4d77992db31a427e406a5f201246227cf04

Upstream:

    https://github.com/Ian-Parberry/Tobler

Relevant subjects:

- exponentially distributed Perlin gradients;
- amortized/infinite noise;
- terrain gradient analysis.

License notices from the upstream files are preserved.

### Ian-Parberry/DesignerWorlds

Pinned upstream commit:

    f175201ad9cebd6c1153c9a51feb3c327724b432

Upstream:

    https://github.com/Ian-Parberry/DesignerWorlds

Relevant subjects:

- value noise;
- geotypical terrain from real-world elevation statistics.

### OGRECave/scape

Pinned upstream commit:

    9c95ec7b6d8372a7a2a4eb6b96dcb5f2219e2bf2

Upstream:

    https://github.com/OGRECave/scape

Relevant subjects:

- derivative-aware procedural noise;
- ridged/billowy terrain;
- GPU terrain operations;
- lookup textures.

The upstream LICENSE is stored in this directory.

## Full upstream repositories

Run:

    powershell -ExecutionPolicy Bypass -File tools/research/fetch_procedural_references.ps1

The script also pins NoMansTerrain as a research source, but its reverse-engineered implementation is deliberately not mirrored into this reference snapshot.
