# Asset sources of truth

This directory contains runtime assets and source data copied into the build tree.

## Galaxy data

- `data/galaxy_details/systems_details/*.json` is the only physical star-system catalog. Each file owns both galactic summary data and internal celestial-body details.
- `data/galaxy_details/objects_details/*.json` contains automatically discovered free interstellar objects.
- `data/galaxy/systems.json` remains a separate political/navigation layer used by `GalaxyDatabase`; it is not a physical star-system catalog.
- `data/galaxy/real_star_catalog.json` contains only the extended real-star background. Game systems are merged into it at runtime from `galaxy_details`.

## Shared shaders

Reusable shader stages live in `shaders/common`. Renderer registrations should reference these files instead of keeping byte-for-byte or functionally identical copies beside every effect.

## Retained data

Unique files are not deleted merely because no direct literal path appears in C++: environment, body-visual, and source-catalog directories are loaded by convention or tooling. Legacy galaxy datasets with unique content remain until their consumers are migrated explicitly.
