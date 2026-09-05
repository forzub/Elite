# Model Asset Editor source-folder contract

Modern editor source assets may use an asset-level folder as the source of truth.
No C++ per-mesh registration is required for such an asset.

Example:

```text
stations/
  LOD0/
    station_core.obj
    station_core.mtl
    SolPet_X0_Ym.obj
    SolPet_X0_Ym.mtl
    SolPet_X0_Yp.obj
    SolPet_X0_Yp.mtl
    variants/
      breached_a.obj
      breached_a.mtl
  LOD1/
    ... ordinary source OBJ files ...
    variants/
      ... replacement OBJ files ...
```

## Ordinary/default meshes

Every `.obj` file directly inside `LOD<N>/` is an ordinary/default mesh for that
render LOD. The editor imports it, creates a RenderGeometryDefinition and a
RenderNode, and does not consult `ObjectAssemblyRegistry` for mesh membership.
LOD0 ordinary meshes seed initial semantic parts; later LODs bind by stable
source name when possible and otherwise remain explicitly bindable in SEMANTICS.

OBJ parts use one shared authored coordinate frame. The normal Blender workflow
is therefore to keep every exported object in its assembled position relative to
the common source origin. Imported RenderNode transforms start at zero; the OBJ
vertex coordinates assemble the model. Runtime/gameplay pivots and joints are
authored later in SEMANTICS.

## Variants

Only `LOD<N>/variants/**/*.obj` is treated as additional/replacement geometry.
The directory is scanned recursively. A variant does not get a normal RenderNode
or a semantic part merely because the file exists. GEOMETRY records replacement
compatibility; DAMAGE later decides when a compatible visual is active.

## Materials

Each OBJ may reference its own MTL. Material names are stable asset-level ids, so
the same `newmtl window` name across several MTL files resolves to the same
MaterialDefinition. Keep definitions with the same material name consistent.
Visible emissive windows remain a material property; actual light sources are
semantic sockets with LightProperties.

## Runtime boundary

Source OBJ/MTL files are editor authoring inputs only. The game runtime loads the
compiled `.elmodel` / `.elmesh` package and does not reconstruct mesh membership
from this directory layout.

## Migration

Assets without an asset-level source-directory entry continue to use the legacy
runtime-assembly importer until they are migrated. The station is the first asset
using folder-authoritative SOURCE.
