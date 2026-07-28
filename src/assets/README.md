# Asset sources of truth

This directory contains runtime assets and source data copied into the build tree.

## Galaxy data

- `data/star_atlas/star_systems.json` is the only star-system summary catalog.
- `data/star_atlas/system_details.json` is the current detailed-system catalog until the directory-based galaxy catalog migration is completed.
- `data/galaxy/real_star_catalog.json` contains the extended real-star background catalog.
- `data/galaxy/systems.json` belongs to the separate legacy `GalaxyDatabase` graph (`actors`, `nodes`, and `routes`) and is not a duplicate of the star atlas.

## Shared shaders

Reusable shader stages live in `shaders/common`. Renderer registrations should reference these files instead of keeping byte-for-byte or functionally identical copies beside every effect.

## Retained data

Unique files are not deleted merely because no direct literal path appears in C++: environment, body-visual, and source-catalog directories are loaded by convention or tooling. Legacy galaxy datasets with unique content remain until their consumers are migrated explicitly.
