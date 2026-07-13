# Celestial Environment Data v2

Generated source-of-truth environment profiles for every body currently present in `body_visuals/sol` and `body_visuals/tau_ceti`.

## Compatibility

Each file preserves the legacy v1 fields currently read by `CelestialEnvironmentProfile.cpp` and adds v2 data blocks:

- `atmosphere.physical`
- `atmosphere.haze`
- `clouds.global_coverage`
- `clouds.appearance`
- `clouds.layers`
- `clouds.circulation`
- `clouds.generation`
- `data_quality` and `provenance`

The current loader will ignore these additional blocks until it is extended. Existing rendering therefore remains compatible.

## Wind data and test acceleration

Wind ranges in body profiles are physical or physically motivated source values. Tau Ceti atmosphere values are explicitly marked as artistic assumptions because no atmosphere has been measured.

Do not multiply or rewrite the source wind values for testing. Runtime acceleration lives in:

`assets/data/celestial/runtime/environment_debug.json`

Use:

`effective_motion_seconds = universe_delta_seconds * wind_time_scale`

## Presence rule

A profile exists for every current body. Airless or visually negligible-exosphere bodies use:

- `atmosphere.enabled = false`
- `clouds.enabled = false`

This makes the absence of effects explicit rather than dependent on a missing file.
