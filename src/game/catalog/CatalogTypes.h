#pragma once

#include <string>
#include <string_view>
#include <vector>

// Product catalog: passports of serial product TYPES (ships and equipment).
//
// Three identities are deliberately distinct:
//   product identity  - WHAT the item is (stableId, stock number, part number);
//   installation code - WHERE it sits inside one ship model (e.g. "36A1A2");
//   serial number     - WHICH physical unit (per installed instance; future
//                       ObjectModuleRuntime data, never part of a passport).
// Two identical engines in two positions share one product passport.
//
// Technical parameters stay in the equipment presets (EngineUnitDescriptor,
// EngineReactorDescriptor, ...). A passport links to them through
// technicalTypeId instead of duplicating numbers.
//
// Stock numbers are an in-game catalog scheme, NOT real NATO Stock Numbers:
//   KB-<category>-<family/generation>-<serial product number>
// e.g. KB-ENG-N1-0001. Category codes: SHP ship, ENG main drive, RCT reactor,
// RCS manoeuvre nozzle. stableId is permanent even if the scheme changes.

namespace game::catalog
{

enum class ProductCategory
{
    Ship,
    MainPropulsion,
    Reactor,
    ManoeuvreThruster,
    Cooling,
    Weapon,
    Sensor,
    Drone,
    Probe,
    Missile,
    LifeSupport,
    Cargo,
    Service,
    SparePart
};

enum class PassportStatus
{
    Canonical,
    Provisional
};

enum class CompatibilityKind
{
    InstalledOn,     // standard fit on the target ship model
    CompatibleWith,  // may be fitted to the target
    Replaces,        // supersedes the target product
    ReplacementFor,  // may stand in for the target product
    OptionalFor      // optional fit on the target ship model
};

struct CompatibilityRelation
{
    CompatibilityKind kind = CompatibilityKind::CompatibleWith;
    std::string_view targetStableId {};
    // Installation positions the relation is limited to; empty = any.
    std::vector<std::string_view> installationCodes {};
};

// Human-readable expansion of an authored code (operational class,
// flight/control generation, layout).
struct CodeDescription
{
    std::string_view code {};
    std::string_view displayName {};
    std::string_view description {};
};

struct ProductPassport
{
    std::string_view stableId {};
    ProductCategory category = ProductCategory::SparePart;
    std::string_view manufacturer {};
    std::string_view designation {};
    std::string_view displayName {};
    std::string_view family {};
    std::string_view model {};
    std::string_view generation {};
    std::string_view manufacturerPartNumber {};
    std::string_view stockNumber {};
    std::string_view revision {};
    PassportStatus status = PassportStatus::Provisional;
    // Link to the technical preset type (e.g. EngineUnitDescriptor::
    // engineTypeId). Empty for products without a runtime preset yet.
    std::string_view technicalTypeId {};
    // Physical envelope; <= 0 means TBD.
    double massKg = 0.0;
    double lengthM = 0.0;
    double widthM = 0.0;
    double heightM = 0.0;
    std::string_view notes {};
    std::vector<CompatibilityRelation> compatibility {};
};

// One position in a ship model's hierarchical assembly tree.
//   "36"     ship/model assembly root
//   "36A1"   first major functional assembly (e.g. propulsion group 1)
//   "36A1A2" second component inside it (e.g. its main engine)
// The code is a LOCATION, never a product identity.
struct InstallationPosition
{
    std::string code;
    std::string description;
    // Runtime module occupying the position; empty for pure assemblies.
    std::string moduleId;
    // Standard product installed there; empty for pure assemblies.
    std::string productStableId;
};

} // namespace game::catalog
